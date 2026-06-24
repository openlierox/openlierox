/////////////////////////////////////////
//
//             OpenLieroX
//
//    Worm class - input handling
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// TODO: rename this file (only input handling here)

// Created 2/8/02
// Jason Boettcher



#include "LieroX.h"
#include "GfxPrimitives.h"
#include "InputEvents.h"
#include "TouchControls.h"
#include "TouchScreenInput.h"
#include "game/CWorm.h"
#include "MathLib.h"
#include "Entity.h"
#include "CClient.h"
#include "CServerConnection.h"
#include "CWormHuman.h"
#include "ProfileSystem.h"
#include "CGameScript.h"
#include "Debug.h"
#include "CGameMode.h"
#include "Physics.h"
#include "WeaponDesc.h"
#include "AuxLib.h" // for doActionInMainThread
#include "game/Game.h"
#ifndef DEDICATED_ONLY
#include "gusanos/player_input.h"
#endif
#include "sound/SoundsBase.h"
#include "CClientNetEngine.h"

#ifdef __MINGW32_VERSION
// TODO: ugly hack, fix it - mingw stdlib seems to be broken
#define powf(x,y) ((float)pow((double)x,(double)y))
#endif

// Is this the first local human worm right now? Touch input only
// drives that worm so split-screen player 2 stays controlled by their
// own keyboard/gamepad bindings. Computed at read time so we don't
// depend on setupInputs() having run yet (the weapon picker frame can
// fire before SetupGameInputs in some startup paths).
static bool isFirstLocalHumanWorm(const CWorm* worm) {
	for_each_iterator(CWorm*, w, game.localWorms()) {
		if(dynamic_cast<CWormHumanInputHandler*>(w->get()->inputHandler()))
			return w->get() == worm;
	}
	return false;
}

// Index of this worm among the local *human* worms (0 = first human player,
// 1 = second, ...), or -1 if it isn't a local human worm. This doubles as the
// gamepad slot the player uses: human player N is driven by gamepad N, so every
// local human gets twin-stick controls on their own controller.
static int localHumanWormIndex(const CWorm* worm) {
	int idx = 0;
	for_each_iterator(CWorm*, w, game.localWorms()) {
		if(dynamic_cast<CWormHumanInputHandler*>(w->get()->inputHandler())) {
			if(w->get() == worm) return idx;
			idx++;
		}
	}
	return -1;
}

///////////////////
// Get the input from a human worm
void CWormHumanInputHandler::getInput() {
	// HINT: we are calling this from simulateWorm

	// Touch-screen actions are read as a binding-independent parallel
	// channel. We OR them into the CInput state below so a tap on the
	// touch UI fires the action regardless of what the keyboard/gamepad
	// is currently bound to. processFrame() ran in TouchControls::Update
	// earlier in the same gameloop tick, so the state is fresh.
	//
	// Touch only drives the *first* local player; in split-screen the
	// second worm ignores the touch channel so one player tapping the
	// screen doesn't move both worms.
	using TA = TouchScreenInput::Action;
	const bool touchForThisWorm = isFirstLocalHumanWorm(m_worm);
	const bool tLeft        = touchForThisWorm && TouchScreenInput::isActionDown(1, TA::Left);
	const bool tRight       = touchForThisWorm && TouchScreenInput::isActionDown(1, TA::Right);
	const bool tUp          = touchForThisWorm && TouchScreenInput::isActionDown(1, TA::Up);
	const bool tDown        = touchForThisWorm && TouchScreenInput::isActionDown(1, TA::Down);
	const bool tFire        = touchForThisWorm && TouchScreenInput::isActionDown(1, TA::Fire);
	const bool tJump        = touchForThisWorm && TouchScreenInput::isActionDown(1, TA::Jump);
	const bool tSelWeap     = touchForThisWorm && TouchScreenInput::isActionDown(1, TA::SelectWeapon);
	const bool tStrafe      = touchForThisWorm && TouchScreenInput::isActionDown(1, TA::Strafe);
	const bool tJumpTap     = touchForThisWorm && TouchScreenInput::wasActionTapped(1, TA::Jump);
	const bool tLeftTap     = touchForThisWorm && TouchScreenInput::wasActionTapped(1, TA::Left);
	const bool tRightTap    = touchForThisWorm && TouchScreenInput::wasActionTapped(1, TA::Right);
	const bool tRopeTap     = touchForThisWorm && TouchScreenInput::wasActionTapped(1, TA::Rope);
	const bool tPrevWeapTap = touchForThisWorm && TouchScreenInput::wasActionTapped(1, TA::PreviousWeapon);
	const bool tNextWeapTap = touchForThisWorm && TouchScreenInput::wasActionTapped(1, TA::NextWeapon);

	// do it here to ensure that it is called exactly once in a frame (needed because of intern handling)
	const bool jump      = cJump.isDownOnce()  || tJumpTap;
	const bool leftOnce  = cLeft.isDownOnce()  || tLeftTap;
	const bool rightOnce = cRight.isDownOnce() || tRightTap;

	if(!m_worm->getAlive()) {
		if(m_worm->bCanRespawnNow && jump)
			m_worm->bRespawnRequested = true;
		clearInput();
		return;
	}
	
	if(m_worm->bWeaponsReady)
		initInputSystem(); // if not done yet... otherwise it also wont hurt

	TimeDiff dt;
	// We may have called CWorm::getInput from outside the game inner
	// 100-fixed-FPS loop and thus have a different GetPhysicsTime()
	// from what we have there. So this case here can rarely happen.
	if(GetPhysicsTime() > m_worm->fLastInputTime) {
		dt = GetPhysicsTime() - m_worm->fLastInputTime;
		m_worm->fLastInputTime = GetPhysicsTime();
	}

	mouse_t *ms = GetMouse();

	worm_state_t *ws = &m_worm->tState.write();

	// Init the ws
	ws->bCarve = false;
	ws->bMove = false;
	ws->bShoot = false;
	if(!(bool)cClient->getGameLobby()[FT_GusanosWormPhysics]) // Gusanos worm physics behaves slightly different for jumping input
		ws->bJump = false;

	const bool mouseControl = 
			tLXOptions->bMouseAiming &&
			ApplicationHasFocus(); // if app has no focus, don't use mouseaiming, the mousewarping is pretty annoying then
	const float mouseSensity = (float)tLXOptions->iMouseSensity; // how sensitive is the mouse in X/Y-dir

	// TODO: here are width/height of the window hardcoded
	//int mouse_dx = ms->X - 640/2;
	//int mouse_dy = ms->Y - 480/2;
	int mouse_dx = 0, mouse_dy = 0;
	SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy); // Won't lose mouse movement and skip frames, also it doesn't call libX11 funcs, so it's safe to call not from video thread
	
	if(mouseControl)
	{
		// TODO: this will not work ...
		struct CenterMouse: public Action
		{
			Result handle()
			{
				// TODO: check current window, and window size
				SDL_WarpMouseInWindow(NULL, 640/2, 480/2); // Should be called from main thread, or you'll get race condition with libX11
				return true;
			} 
		};
		doActionInMainThread( new CenterMouse() );
	}

	{
/*		// only some debug output for checking the values
		if(mouseControl && (mouse_dx != 0 || mouse_dy != 0))
			notes("mousepos changed: %i, %i\n", mouse_dx, mouse_dy),
			notes("anglespeed: %f\n", fAngleSpeed),
			notes("movespeed: %f\n", fMoveSpeedX),
			notes("dt: %f\n", dt); */
	}

	// ---- Twin-stick controls (any local human player on their pad) ----
	// Left stick walks, right stick aims. Walking never turns the worm
	// (strafing is effectively always on) — the right stick alone decides the
	// facing side and the exact aim angle. Each local human player uses the
	// gamepad with the same index (player N -> pad N). Always active for a
	// local human with a connected pad: the analog sticks are reserved for
	// this and are not separately bindable.
	const int twinPad = localHumanWormIndex(m_worm);
	const bool twinStick = (twinPad >= 0) && isPadPresent(twinPad);
	const int twinDeadzone = getPadStickDeadzone();

	// Twin-stick aim source, resolved before the angle section so the keyboard
	// aim path can be shared. The right stick is the primary aim stick; if it's
	// idle the left stick drives the aim (so a single-stick player aims where
	// they walk), and when both are pushed the right stick wins.
	bool stickAiming = false;
	int aimAx = 0, aimAy = 0; // chosen stick (SDL: up/left negative, down/right positive)
	if(twinStick) {
		const int rx = getPadRightStickX(twinPad);
		const int ry = getPadRightStickY(twinPad);
		const int lx = getPadLeftStickX(twinPad);
		const int ly = getPadLeftStickY(twinPad);
		const bool rightAiming = (rx*rx + ry*ry > twinDeadzone*twinDeadzone);
		const bool leftAiming  = (lx*lx + ly*ly > twinDeadzone*twinDeadzone);
		if(rightAiming || leftAiming) {
			stickAiming = true;
			aimAx = rightAiming ? rx : lx;
			aimAy = rightAiming ? ry : ly;
			// Horizontal sign picks the facing side instantly (keyboard players
			// also turn instantly by walking).
			m_worm->setFaceDirectionSide((aimAx >= 0) ? DIR_RIGHT : DIR_LEFT);
		}
	}
	// Reset the aim cursor each frame; the stick limit-aim path below turns it on
	// when active (an instantaneous marker showing where the stick points while
	// the worm's actual aim rotates toward it at keyboard speed).
	m_worm->bAimCursorActive = false;

	// angle section: keyboard / mouse / d-pad aiming. Runs unconditionally and
	// unchanged from before this PR — the d-pad up/down keeps aiming through the
	// normal key-mapping system exactly as it always did. The stick aim is
	// layered on after this block.
	{
		float aimMaxSpeed = MAX((float)fabs(tLXOptions->fAimMaxSpeed), 20.0f); // Note: 100 was LX56 max aimspeed
		float aimAccel = MAX((float)fabs(tLXOptions->fAimAcceleration), 100.0f); // HINT: 500 is the LX56 value here (rev 1)
		float aimFriction = CLAMP(tLXOptions->fAimFriction, 0.0f, 1.0f); // we didn't had that in LX56; it behaves more natural
		bool aimLikeLX56 = tLXOptions->bAimLikeLX56;
		if(cClient->getGameLobby()[FT_ForceLX56Aim] || cClient->getServerVersion() < OLXRcVersion(0,58,3)) {
			aimMaxSpeed = 100;
			aimAccel = 500;
			aimLikeLX56 = true;
		}

		if(cUp.isDown() || tUp) { // Up
			m_worm->fAngleSpeed -= aimAccel * dt.seconds();
			if(!aimLikeLX56) CLAMP_DIRECT(m_worm->fAngleSpeed, -aimMaxSpeed, aimMaxSpeed);
		} else if(cDown.isDown() || tDown) { // Down
			m_worm->fAngleSpeed += aimAccel * dt.seconds();
			if(!aimLikeLX56) CLAMP_DIRECT(m_worm->fAngleSpeed, -aimMaxSpeed, aimMaxSpeed);
		} else {
			if(!mouseControl) {
				// HINT: this is the original order and code (before mouse patch - rev 1007)
				CLAMP_DIRECT(m_worm->fAngleSpeed, -aimMaxSpeed, aimMaxSpeed);
				if(aimLikeLX56) REDUCE_CONST(m_worm->fAngleSpeed, 200*dt.seconds());
				else m_worm->fAngleSpeed *= powf(aimFriction, dt.seconds() * 100.0f);
				RESET_SMALL(m_worm->fAngleSpeed, 5.0f);

			} else { // mouseControl for angle
				// HINT: to behave more like keyboard, we use CLAMP(..aimAccel) here
				float diff = mouse_dy * mouseSensity;
				CLAMP_DIRECT(diff, -aimAccel, aimAccel); // same limit as keyboard
				m_worm->fAngleSpeed += diff * dt.seconds();

				// this tries to be like keyboard where this code is only applied if up/down is not pressed
				if(abs(mouse_dy) < 5) {
					CLAMP_DIRECT(m_worm->fAngleSpeed, -aimMaxSpeed, aimMaxSpeed);
					if(aimLikeLX56) REDUCE_CONST(m_worm->fAngleSpeed, 200*dt.seconds());
					else m_worm->fAngleSpeed *= powf(aimFriction, dt.seconds() * 100.0f);
					RESET_SMALL(m_worm->fAngleSpeed, 5.0f);
				}
			}
		}

		m_worm->fAngle += m_worm->fAngleSpeed * dt.seconds();
		if(CLAMP_DIRECT(m_worm->fAngle.write(), -90.0f, cClient->getGameLobby()[FT_FullAimAngle] ? 90.0f : 60.0f) != 0)
			m_worm->fAngleSpeed = 0;
	}

	// Additionally, the analog sticks aim (joystick add-on). The facing side was
	// already set above. The stick direction gives an absolute target angle: the
	// angle above/below horizontal (atan2 against |aimAx| measures it from
	// horizontal; the left/right sign is carried by the facing side; SDL Y is
	// negative-up, matching the engine's positive-down convention).
	if(stickAiming) {
		const float maxAngle = cClient->getGameLobby()[FT_FullAimAngle] ? 90.0f : 60.0f;
		float ang = atan2f((float)aimAy, fabsf((float)aimAx)) * (180.0f / (float)PI);
		const float targetAngle = CLAMP(ang, -90.0f, maxAngle);

		// Rotate the actual aim toward the stick target, but no faster than a
		// keyboard player can (fAimMaxSpeed, or 100°/s under FT_ForceLX56Aim),
		// capped per frame, so a gamepad player can't aim faster than a keyboard
		// player. The worm thus "eventually ends" at the stick angle. A separate
		// instantaneous cursor (drawn in CWorm::draw) shows the target direction
		// right away, so the player still gets direct feedback of where they're
		// aiming while the worm catches up.
		float aimMaxSpeed = MAX((float)fabs(tLXOptions->fAimMaxSpeed), 20.0f);
		if(cClient->getGameLobby()[FT_ForceLX56Aim] || cClient->getServerVersion() < OLXRcVersion(0,58,3))
			aimMaxSpeed = 100; // LX56 keyboard aim max speed
		const float maxStep = aimMaxSpeed * dt.seconds();
		float diff = targetAngle - m_worm->getAngle();
		CLAMP_DIRECT(diff, -maxStep, maxStep);
		m_worm->fAngle = m_worm->getAngle() + diff;
		m_worm->fAngleSpeed = 0;

		// Feed the instantaneous cursor: store the target angle (fAngle
		// convention; the facing side carries the left/right sign) and flag it
		// on for this frame.
		m_worm->fAimCursorAngle = targetAngle;
		m_worm->bAimCursorActive = true;
	} // end angle section

	const CVec ninjaShootDir = m_worm->getFaceDirection();

	// basic mouse control (moving)
	if(mouseControl) {
		// no dt here, it's like the keyboard; and the value will be limited by dt later
		m_worm->fMoveSpeedX += mouse_dx * mouseSensity * 0.01f;

		REDUCE_CONST(m_worm->fMoveSpeedX, 1000*dt.seconds());
		//RESET_SMALL(m_worm->fMoveSpeedX, 5.0f);
		CLAMP_DIRECT(m_worm->fMoveSpeedX, -500.0f, 500.0f);

		if(fabs(m_worm->fMoveSpeedX) > 50) {
			if(m_worm->fMoveSpeedX > 0) {
				m_worm->iMoveDirectionSide = DIR_RIGHT;
				if(mouse_dx < 0) m_worm->lastMoveTime = tLX->currentTime;
			} else {
				m_worm->iMoveDirectionSide = DIR_LEFT;
				if(mouse_dx > 0) m_worm->lastMoveTime = tLX->currentTime;
			}
			ws->bMove = true;
			if(!cClient->isHostAllowingStrafing() || !(cStrafe.isDown() || tStrafe))
				m_worm->iFaceDirectionSide = m_worm->iMoveDirectionSide;

		} else {
			ws->bMove = false;
		}

	}


	if(mouseControl) { // set shooting, ninja and jumping, weapon selection for mousecontrol
		// like Jason did it
		ws->bShoot = (ms->Down & SDL_BUTTON(1)) ? true : false;
		ws->bJump = (ms->Down & SDL_BUTTON(3)) ? true : false;
		if(ws->bJump) {
			if(m_worm->cNinjaRope.get().isReleased())
				m_worm->cNinjaRope.write().Clear();
		}
		else if(ms->FirstDown & SDL_BUTTON(2)) {
			// TODO: this is bad. why isn't there a ws->iNinjaShoot ?
			m_worm->cNinjaRope.write().Shoot(ninjaShootDir);
			PlaySoundSample(sfxGame.smpNinja);
		}

		if( m_worm->getWeaponSlotsCount() > 0 && (ms->WheelScrollUp || ms->WheelScrollDown) ) {
			m_worm->bForceWeapon_Name = true;
			m_worm->fForceWeapon_Time = tLX->currentTime + 0.75f;
			if( ms->WheelScrollUp )
				m_worm->iCurrentWeapon ++;
			else
				m_worm->iCurrentWeapon --;
			if(m_worm->iCurrentWeapon >= m_worm->getWeaponSlotsCount())
				m_worm->iCurrentWeapon = 0;
			if(m_worm->iCurrentWeapon < 0)
				m_worm->iCurrentWeapon = m_worm->getWeaponSlotsCount() - 1;
		}
	}



	{ // set carving

/*		// this is a bit unfair to keyboard players
		if(mouseControl) { // mouseControl
			if(fabs(fMoveSpeedX) > 200) {
				ws->iCarve = true;
			}
		} */

		const float carveDelay = 0.2f;
		// On touch, the player can't tap-to-carve precisely, so hold-to-walk
		// auto-carves through dirt — same behaviour as joystick/mouseControl.
		const bool touchAutoCarve = TouchControls::IsActive();

		if(		(mouseControl && ws->bMove && m_worm->iMoveDirectionSide == DIR_LEFT)
			||	( cLeft.isJoystickDown() /*|| (cLeft.isKeyboard() && leftOnce)*/ && !cSelWeapon.isDown())
			||	(touchAutoCarve && (cLeft.isDown() || tLeft) && !(cSelWeapon.isDown() || tSelWeap))
			) {

			if(tLX->currentTime - m_worm->fLastCarve >= carveDelay) {
				ws->bCarve = true;
				ws->bMove = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}

		if(		(mouseControl && ws->bMove && m_worm->iMoveDirectionSide == DIR_RIGHT)
			||	( cRight.isJoystickDown() /*|| (cRight.isKeyboard() && rightOnce)*/ && !cSelWeapon.isDown())
			||	(touchAutoCarve && (cRight.isDown() || tRight) && !(cSelWeapon.isDown() || tSelWeap))
			) {

			if(tLX->currentTime - m_worm->fLastCarve >= carveDelay) {
				ws->bCarve = true;
				ws->bMove = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}
	}

	
	const bool allowCombo = cClient->getGameLobby()[FT_WeaponCombos];

	ws->bShoot = cShoot.isDown() || tFire;

	if(m_worm->getWeaponSlotsCount() > 0 && (!ws->bShoot || allowCombo)) {
		//
		// Weapon changing
		//
		if(cSelWeapon.isDown() || tSelWeap) {
			// TODO: was is the intention of this var? if weapon change, then it's wrong
			// if cSelWeapon.isDown(), then we don't need it

			// we don't want keyrepeats here, so only count the first down-event.
			// SelectWeapon is a hold-modifier on both keyboard and gamepad:
			// while it is held, left/right (or the d-pad) cycles the weapon.
			// Pressing it alone must NOT change the weapon — dedicated
			// Previous/Next weapon controls exist for direct cycling.
			int change = (rightOnce ? 1 : 0) - (leftOnce ? 1 : 0);
			m_worm->iCurrentWeapon += change;
			MOD(m_worm->iCurrentWeapon, m_worm->getWeaponSlotsCount());

			// Show the weapon name above the worm for a moment when it
			// actually cycled — same hint the SIN_WEAPON1..5 quick-keys give.
			if(change != 0) {
				m_worm->bForceWeapon_Name = true;
				m_worm->fForceWeapon_Time = tLX->currentTime + 0.75f;
			}
		}

		// Process weapon quick-selection keys
		for(size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]); i++ )
		{
			if( cWeapons[i].isDown() )
			{
				m_worm->iCurrentWeapon = (int32_t)i;
				MOD(m_worm->iCurrentWeapon, m_worm->getWeaponSlotsCount());
				// Let the weapon name show up for a short moment
				m_worm->bForceWeapon_Name = true;
				m_worm->fForceWeapon_Time = tLX->currentTime + 0.75f;
			}
		}

		// Dedicated Prev/Next-weapon controls — touch buttons or the keyboard
		// keys bound to SIN_PREVWEAPON/SIN_NEXTWEAPON — cycle directly without
		// requiring SelectWeapon to be held. No key-repeat, so we only react to
		// the first down-event.
		const bool prevWeap = cPrevWeapon.isDownOnce() || tPrevWeapTap;
		const bool nextWeap = cNextWeapon.isDownOnce() || tNextWeapTap;
		if(prevWeap || nextWeap) {
			int change = (nextWeap ? 1 : 0) - (prevWeap ? 1 : 0);
			if(change != 0) {
				m_worm->iCurrentWeapon += change;
				MOD(m_worm->iCurrentWeapon, m_worm->getWeaponSlotsCount());
				m_worm->bForceWeapon_Name = true;
				m_worm->fForceWeapon_Time = tLX->currentTime + 0.75f;
			}
		}

	}


	// Keyboard / d-pad movement. Runs unconditionally. With no aim stick active
	// it is unchanged from before this PR — the d-pad left/right walks *and
	// turns* the worm through the normal key-mapping system exactly as it always
	// did. While a stick is aiming (stickAiming), the aim stick owns the facing
	// side, so walking here only moves and never turns: the worm strafes and
	// keeps aiming where the stick points (the right stick thus overrides the
	// facing from keyboard/d-pad walking, not just from the left stick). The
	// left stick walk add-on below also only moves, never turns.
	if(!(cSelWeapon.isDown() || tSelWeap)) {
		if(cLeft.isDown() || tLeft) {
			ws->bMove = true;
			m_worm->lastMoveTime = tLX->currentTime;

			if(!(cRight.isDown() || tRight)) {
				if(!stickAiming && (!cClient->isHostAllowingStrafing() || !(cStrafe.isDown() || tStrafe))) m_worm->iFaceDirectionSide = DIR_LEFT;
				m_worm->iMoveDirectionSide = DIR_LEFT;
			}

			if(rightOnce) {
				ws->bCarve = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}

		if(cRight.isDown() || tRight) {
			ws->bMove = true;
			m_worm->lastMoveTime = tLX->currentTime;

			if(!(cLeft.isDown() || tLeft)) {
				if(!stickAiming && (!cClient->isHostAllowingStrafing() || !(cStrafe.isDown() || tStrafe))) m_worm->iFaceDirectionSide = DIR_RIGHT;
				m_worm->iMoveDirectionSide = DIR_RIGHT;
			}

			if(leftOnce) {
				ws->bCarve = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}

		// inform player about disallowed strafing
		if(!cClient->isHostAllowingStrafing() && cStrafe.isDownOnce())
			// TODO: perhaps in chat?
			hints << "strafing is not allowed on this server." << endl;
	}

	// Additionally, the left stick walks (joystick add-on). Strafe: it sets the
	// movement direction but never the facing side, so the worm keeps aiming
	// where the sticks point regardless of which way it walks. Walking always
	// digs. When the stick is idle this does nothing, leaving the keyboard/d-pad
	// movement above untouched.
	if(twinStick) {
		const float carveDelay = 0.2f; // same throttle as the mouse/touch carve below
		const int lx = getPadLeftStickX(twinPad);
		if(abs(lx) > twinDeadzone) {
			ws->bMove = true;
			m_worm->lastMoveTime = tLX->currentTime;
			m_worm->iMoveDirectionSide = (lx < 0) ? DIR_LEFT : DIR_RIGHT;

			// Walking always digs: pulse carve in the walk direction (throttled)
			// so the worm tunnels straight through dirt instead of stopping at
			// a wall. bCarve is reset to false each frame, so the throttle makes
			// it pulse, which the gus DIG action triggers on each rising edge.
			if(tLX->currentTime - m_worm->fLastCarve >= carveDelay) {
				ws->bCarve = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}
	}


	const bool oldskool = tLXOptions->bOldSkoolRope;

	// Jump
	if( !(oldskool && (cSelWeapon.isDown() || tSelWeap)) )  {
		ws->bJump |= jump;

		if(jump && m_worm->cNinjaRope.get().isReleased())
			m_worm->cNinjaRope.write().Clear();
	}

	// Ninja Rope
	if(oldskool) {
		// Old skool style rope throwing
		// Change-weapon & jump

		if(!(cSelWeapon.isDown() || tSelWeap) || !(cJump.isDown() || tJump))  {
			bRopeDown = false;
		}

		if((cSelWeapon.isDown() || tSelWeap) && (cJump.isDown() || tJump) && !bRopeDown) {
			bRopeDownOnce = true;
			bRopeDown = true;
		}

		// Down
		if(bRopeDownOnce) {
			bRopeDownOnce = false;

			m_worm->cNinjaRope.write().Shoot(ninjaShootDir);

			// Throw sound
			PlaySoundSample(sfxGame.smpNinja);
		}


	} else {
		// Newer style rope throwing
		// Seperate dedicated button for throwing the rope
		if(cInpRope.isDownOnce() || tRopeTap) {

			m_worm->cNinjaRope.write().Shoot(ninjaShootDir);
			// Throw sound
			PlaySoundSample(sfxGame.smpNinja);
		}
	}


	cUp.reset();
	cDown.reset();
	cLeft.reset();
	cRight.reset();
	cShoot.reset();
	cJump.reset();
	cSelWeapon.reset();
	cInpRope.reset();
	cStrafe.reset();
	cPrevWeapon.reset();
	cNextWeapon.reset();
	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].reset();
}


///////////////////
// Clear the input
void CWormHumanInputHandler::clearInput() {
	// clear inputs
	bRopeDown = bRopeDownOnce = false;
	cUp.reset();
	cDown.reset();
	cLeft.reset();
	cRight.reset();
	cShoot.reset();
	cJump.reset();
	cSelWeapon.reset();
	cInpRope.reset();
	cStrafe.reset();
	cPrevWeapon.reset();
	cNextWeapon.reset();
	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].reset();
}



struct HumanWormType : WormType {
	virtual CWormInputHandler* createInputHandler(CWorm* w) { return new CWormHumanInputHandler(w); }
	int toInt() { return 0; }
} PRF_HUMAN_instance;
WormType* PRF_HUMAN = &PRF_HUMAN_instance;

CWormHumanInputHandler::CWormHumanInputHandler(CWorm* w) : CWormInputHandler(w) {
	bRopeDown = bRopeDownOnce = false;
	m_localPlayerSlot = -1;
	weaponSelStickHeld = false;
	weaponSelStickRepeated = false;
	gusInit();
	
	game.onNewPlayer( this );
	game.onNewHumanPlayer( this );
	game.onNewHumanPlayer_Lua( this );
	game.onNewPlayer_Lua(this);
}

CWormHumanInputHandler::~CWormHumanInputHandler() {}

void CWormHumanInputHandler::startGame() {
	initInputSystem();
}



///////////////////
// Setup the inputs
void CWormHumanInputHandler::setupInputs(const PlyControls& Inputs, int localPlayerSlot)
{
	m_localPlayerSlot = localPlayerSlot;
	cUp.Setup(		Inputs[SIN_UP] );
	cDown.Setup(	Inputs[SIN_DOWN] );
	cLeft.Setup(	Inputs[SIN_LEFT] );
	cRight.Setup(	Inputs[SIN_RIGHT] );

	cShoot.Setup(	Inputs[SIN_SHOOT] );
	cJump.Setup(	Inputs[SIN_JUMP] );
	cSelWeapon.Setup(Inputs[SIN_SELWEAP] );
	cInpRope.Setup(	Inputs[SIN_ROPE] );

	cStrafe.Setup( Inputs[SIN_STRAFE] );
	cPrevWeapon.Setup( Inputs[SIN_PREVWEAPON] );
	cNextWeapon.Setup( Inputs[SIN_NEXTWEAPON] );

	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].Setup(Inputs[SIN_WEAPON1 + i]);
}


void CWormHumanInputHandler::initInputSystem() {
	cUp.setResetEachFrame( false );
	cDown.setResetEachFrame( false );
	cLeft.setResetEachFrame( false );
	cRight.setResetEachFrame( false );
	cShoot.setResetEachFrame( false );
	cJump.setResetEachFrame( false );
	cSelWeapon.setResetEachFrame( false );
	cInpRope.setResetEachFrame( false );
	cStrafe.setResetEachFrame( false );
	cPrevWeapon.setResetEachFrame( false );
	cNextWeapon.setResetEachFrame( false );
	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].setResetEachFrame( false );
}

void CWormHumanInputHandler::stopInputSystem() {
	cUp.setResetEachFrame( true );
	cDown.setResetEachFrame( true );
	cLeft.setResetEachFrame( true );
	cRight.setResetEachFrame( true );
	cShoot.setResetEachFrame( true );
	cJump.setResetEachFrame( true );
	cSelWeapon.setResetEachFrame( true );
	cInpRope.setResetEachFrame( true );
	cStrafe.setResetEachFrame( true );
	cPrevWeapon.setResetEachFrame( true );
	cNextWeapon.setResetEachFrame( true );
	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].setResetEachFrame( true );
}





///////////////////
// Initialize the weapon selection screen
void CWormHumanInputHandler::initWeaponSelection() {
	if(!game.gameScript()->isLoaded())
		errors << "CWormHumanInputHandler::initWeaponSelection: gamescript not loaded" << endl;

	// the way we handle the inputs in wpn selection is different
	stopInputSystem();
	
	// This is used for the menu screen as well
	m_worm->iCurrentWeapon = 0;
	
	m_worm->bWeaponsReady = false;
		
	m_worm->clearInput();
	
	// Safety
	if (!m_worm->tProfile.get())  {
		warnings << "initWeaponSelection called and tProfile is not set" << endl;
		m_worm->tProfile = new profile_t(); // not really a problem, though...
	}
	
	// Load previous settings from profile
	for(size_t i=0;i<m_worm->tWeapons.size();i++) {
		m_worm->weaponSlots.write()[i].WeaponId = (int)game.gameScript()->FindWeaponId( m_worm->tProfile->getWeaponSlot((int)i) );
		
        // If this weapon is not enabled in the restrictions, find another weapon that is enabled
		if( !m_worm->tWeapons[i].weapon() || !game.weaponRestrictions()->isEnabled( m_worm->tWeapons[i].weapon()->Name ) ) {
			m_worm->weaponSlots.write()[i].WeaponId = game.gameScript()->FindWeaponId( game.weaponRestrictions()->findEnabledWeapon( game.gameScript()->GetWeaponList() ) );
        }
	}
	
	
	for(size_t n=0;n<m_worm->tWeapons.size();n++) {
		m_worm->weaponSlots.write()[n].Charge = 1.f;
		m_worm->weaponSlots.write()[n].Reloading = false;
		m_worm->weaponSlots.write()[n].LastFire = 0.f;
	}
	
	// Skip the dialog if there's only one weapon available
	int enabledWeaponsAmount = 0;
	for( int f = 0; f < game.gameScript()->GetNumWeapons(); f++ )
		if( game.weaponRestrictions()->isEnabled( game.gameScript()->GetWeapons()[f].Name ) )
			enabledWeaponsAmount++;
	
	if( enabledWeaponsAmount <= 1 ) // server can ban ALL weapons, noone will be able to shoot then
		m_worm->bWeaponsReady = true;
}


///////////////////
// Draw/Process the weapon selection screen
void CWormHumanInputHandler::doWeaponSelectionFrame(SDL_Surface * bmpDest, CViewport *v)
{
	// TODO: this should also be used for selecting the weapons for the bot (but this in CWorm_AI then)
	// TODO: reduce local variables in this function
	// TODO: make this function shorter
	// TODO: give better names to local variables
		
	if(bDedicated) {
		warnings << "doWeaponSelectionFrame: we have a local human input in our dedicated server" << endl; 
		return; // just for safty; atm this function only handles non-bot players
	}

	// do that check here instead of initWeaponSelection() because at that time,
	// not all params of the gamemode are set
	if(cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode && !cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode->Shoot(m_worm)) {
		// just skip weapon selection in game modes where shooting is not possible (e.g. hidenseek)
		m_worm->bWeaponsReady = true;
		return;
	}
	
	int l = 0;
	int t = 0;
	// Center on the displayed view; a used viewport (split-screen) overrides
	// this with its own center. The whole-screen dark overlay behind the
	// selection is drawn once by the caller (CClient::Draw).
	int centrex = VideoPostProcessor::get()->displayScreenWidth() / 2;

    if( v && v->getUsed() ) {
        l = v->GetLeft();
        t = v->GetTop();
        centrex = v->GetLeft() + v->GetVirtW()/2;
    }
		
	// Clip all weapon-selection drawing to this worm's viewport so the (long)
	// "Key settings" text can't bleed past the viewport into the split-screen
	// separator gap. Belt-and-braces with the separator that repaints the gap
	// (see CClient::Draw). CFont and DrawImageAdv both honour dst->clip_rect.
	SDL_Rect wsClipRect = bmpDest->clip_rect;
	if( v && v->getUsed() ) {
		wsClipRect.x = l;
		wsClipRect.y = t;
		wsClipRect.w = v->GetVirtW();
		wsClipRect.h = v->GetVirtH();
	}
	ScopedSurfaceClip wsClip(bmpDest, wsClipRect);

	tLX->cFont.DrawCentre(bmpDest, centrex, t+30, tLX->clWeaponSelectionTitle, "~ Weapons Selection ~");
		
	tLX->cFont.DrawCentre(bmpDest, centrex, t+48, tLX->clWeaponSelectionTitle, "(Use up/down and left/right for selection.)");
	tLX->cFont.DrawCentre(bmpDest, centrex, t+66, tLX->clWeaponSelectionTitle, "(Go to 'Done' and press shoot then.)");
	//tLX->cOutlineFont.DrawCentre(bmpDest, centrex, t+30, tLX->clWeaponSelectionTitle, "Weapons Selection");
	//tLX->cOutlineFont.DrawCentre(bmpDest, centrex, t+30, tLX->clWeaponSelectionTitle, "Weapons Selection");
	
	bool bChat_Typing = cClient->isTyping();

	// Touch only drives the first local player in split-screen.
	const bool touchForThisWorm = isFirstLocalHumanWorm(m_worm);

	int y = t + 100;
	for(size_t i=0;i<m_worm->tWeapons.size();i++) {
		
		std::string slotDesc;
		if(m_worm->tWeapons[i].weapon()) slotDesc = m_worm->tWeapons[i].weapon()->Name;
		else slotDesc = "* INVALID WEAPON *";
		Color col = tLX->clWeaponSelectionActive;		
		if(m_worm->iCurrentWeapon != (int)i) col = tLX->clWeaponSelectionDefault;
		tLX->cOutlineFont.Draw(bmpDest, centrex-70, y, col,  slotDesc);
		
		if (bChat_Typing)  {
			y += 18;
			continue;
		}
		
		// Changing weapon
		if(m_worm->iCurrentWeapon == (int)i && !bChat_Typing && game.gameScript()->GetNumWeapons() > 0) {
			int change = cRight.wasDown() - cLeft.wasDown();
			if(touchForThisWorm) {
				// tapCount() returns (and consumes) every queued tap, so a
				// burst of taps during a slow frame still moves N rows.
				change += TouchScreenInput::tapCount(1, TouchScreenInput::Action::Right);
				change -= TouchScreenInput::tapCount(1, TouchScreenInput::Action::Left);
			}
			if(cSelWeapon.isDown()) change *= 6; // jump with multiple speed if selWeapon is pressed
			int id = m_worm->tWeapons[i].weapon() ? m_worm->tWeapons[i].weapon()->ID : 0;
			if(change > 0) while(change) {
				id++; MOD(id, game.gameScript()->GetNumWeapons());
				if( game.weaponRestrictions()->isEnabled( game.gameScript()->GetWeapons()[id].Name ) )
					change--;
				if(!m_worm->tWeapons[i].weapon() && id == 0)
					break;
				if(m_worm->tWeapons[i].weapon() && id == m_worm->tWeapons[i].weapon()->ID) // back where we were before
					break;
			} else if(change < 0) while(change) {
				id--; MOD(id, game.gameScript()->GetNumWeapons());
				if( game.weaponRestrictions()->isEnabled( game.gameScript()->GetWeapons()[id].Name ) )
					change++;
				if(!m_worm->tWeapons[i].weapon() && id == 0)
					break;
				if(m_worm->tWeapons[i].weapon() && id == m_worm->tWeapons[i].weapon()->ID) // back where we were before
					break;
			}
			m_worm->weaponSlots.write()[i].WeaponId = id;
		}
		
		y += 18;
	}
		
    // Note: The extra weapon weapon is the 'random' button
	if(m_worm->iCurrentWeapon == (int)m_worm->tWeapons.size()) {

		// Fire on the random button?
		if((cShoot.isDownOnce()) && !bChat_Typing) {
			m_worm->GetRandomWeapons();
		}
	}

	// Touch dedicated "Random" button: randomise without requiring the
	// player to first navigate to the Random row.
	if(touchForThisWorm && TouchScreenInput::wasActionTapped(1, TouchScreenInput::Action::RandomizeWeapons) && !bChat_Typing) {
		m_worm->GetRandomWeapons();
	}

	m_worm->tProfile->sWeaponSlots.resize(m_worm->tWeapons.size());
	for(size_t i=0;i<m_worm->tWeapons.size();i++)
		m_worm->tProfile->writeWeaponSlot((int)i) = m_worm->tWeapons[i].weapon() ? m_worm->tWeapons[i].weapon()->Name : "";

	// Note: The extra weapon slot is the 'done' button
	if(m_worm->iCurrentWeapon == (int)m_worm->tWeapons.size()+1) {

		// Fire on the done button?
		// we have to check isUp() here because if we continue while it is still down, we will fire after in the game
		if((cShoot.isUp()) && !bChat_Typing) {
			// we are ready with manual human weapon selection
			m_worm->bWeaponsReady = true;
			m_worm->iCurrentWeapon = 0;

			SaveProfiles();
		}
	}

	// Touch dedicated "Done" button: confirm without navigating to the
	// Done row first. Mirrors the keyboard "Fire on Done row" path.
	if(touchForThisWorm && TouchScreenInput::wasActionTapped(1, TouchScreenInput::Action::ConfirmWeapons) && !bChat_Typing) {
		m_worm->bWeaponsReady = true;
		m_worm->iCurrentWeapon = 0;
		SaveProfiles();
	}
	
	
	
    y+=5;
	if(m_worm->iCurrentWeapon == (int)m_worm->tWeapons.size())
		tLX->cOutlineFont.DrawCentre(bmpDest, centrex, y, tLX->clWeaponSelectionActive, "Random");
	else
		tLX->cOutlineFont.DrawCentre(bmpDest, centrex, y, tLX->clWeaponSelectionDefault, "Random");
	
    y+=18;
	
	if(m_worm->iCurrentWeapon == (int)m_worm->tWeapons.size()+1)
		tLX->cOutlineFont.DrawCentre(bmpDest, centrex, y, tLX->clWeaponSelectionActive, "Done");
	else
		tLX->cOutlineFont.DrawCentre(bmpDest, centrex, y, tLX->clWeaponSelectionDefault, "Done");
	
	
	// list current key settings
	// TODO: move this out here
	y += 20;
	tLX->cFont.DrawCentre(bmpDest, centrex, y += 15, tLX->clWeaponSelectionTitle, "~ Key settings ~");
	tLX->cFont.Draw(bmpDest, centrex - 150, y += 15, tLX->clWeaponSelectionTitle, "up/down: " + cUp.getEventName() + "/" + cDown.getEventName());
	tLX->cFont.Draw(bmpDest, centrex - 150, y += 15, tLX->clWeaponSelectionTitle, "left/right: " + cLeft.getEventName() + "/" + cRight.getEventName());
	tLX->cFont.Draw(bmpDest, centrex - 150, y += 15, tLX->clWeaponSelectionTitle, "shoot: " + cShoot.getEventName());
	y -= 45;
	tLX->cFont.Draw(bmpDest, centrex, y += 15, tLX->clWeaponSelectionTitle, "jump/ninja: " + cJump.getEventName() + "/" + cInpRope.getEventName());
	tLX->cFont.Draw(bmpDest, centrex, y += 15, tLX->clWeaponSelectionTitle, "select weapon: " + cSelWeapon.getEventName());
	tLX->cFont.Draw(bmpDest, centrex, y += 15, tLX->clWeaponSelectionTitle, "strafe: " + cStrafe.getEventName());
	tLX->cFont.Draw(bmpDest, centrex, y += 15, tLX->clWeaponSelectionTitle, "quick select weapon: " + cWeapons[0].getEventName() + " " + cWeapons[1].getEventName() + " " + cWeapons[2].getEventName() + " " + cWeapons[3].getEventName() + " " + cWeapons[4].getEventName() );
	
	
	if(!bChat_Typing) {
		// move selection up or down
		int change = cDown.wasDown() - cUp.wasDown();
		if(touchForThisWorm) {
			// tapCount() returns (and consumes) every queued tap, so a
			// burst of taps during a slow frame still moves N rows.
			change += TouchScreenInput::tapCount(1, TouchScreenInput::Action::Down);
			change -= TouchScreenInput::tapCount(1, TouchScreenInput::Action::Up);
		}

		// Gamepad left stick navigates up/down too. The sticks aren't bindable
		// (the in-game twin-stick path owns them), so read the pad directly via
		// the same helpers and slot mapping (player N -> pad N). Push up/down to
		// move one row, then it auto-repeats while held, mimicking key-repeat.
		const int padSlot = localHumanWormIndex(m_worm);
		if(padSlot >= 0 && isPadPresent(padSlot)) {
			const int ly = getPadLeftStickY(padSlot); // SDL: up negative, down positive
			const int dz = getPadStickDeadzone();
			const int dir = (ly < -dz) ? -1 : (ly > dz) ? 1 : 0;
			if(dir == 0) {
				weaponSelStickHeld = false;
			} else if(!weaponSelStickHeld) {
				// rising edge: move one row right away
				change += dir;
				weaponSelStickHeld = true;
				weaponSelStickRepeated = false;
				fLastWeaponSelStickMove = tLX->currentTime;
			} else {
				// held: wait longer before the first repeat, then repeat faster
				const float delay = weaponSelStickRepeated ? 0.12f : 0.4f;
				if(tLX->currentTime - fLastWeaponSelStickMove >= delay) {
					change += dir;
					weaponSelStickRepeated = true;
					fLastWeaponSelStickMove = tLX->currentTime;
				}
			}
		}

		m_worm->iCurrentWeapon += change;
		m_worm->iCurrentWeapon %= (int)m_worm->tWeapons.size() + 2;
		if(m_worm->iCurrentWeapon < 0) m_worm->iCurrentWeapon += (int)m_worm->tWeapons.size() + 2;
	}

	stopInputSystem(); // if not done yet... otherwise it will also not hurt. it will shut down the regular ingame input system
}




#ifndef DEDICATED_ONLY
#include "CViewport.h"
#endif

//#include "gusanos/allegro.h"

using namespace std;

void CWormHumanInputHandler::gusInit()
{
	aimingUp=(false);
	aimingDown=(false);
	changing=(false);
	jumping=(false);
	walkingLeft=(false);
	walkingRight=(false);
}


void CWormHumanInputHandler::subThink()
{
	if ( m_worm ) {
#ifndef DEDICATED_ONLY
	//	if ( m_viewport )
	//		m_viewport->interpolateTo(m_worm->getRenderPos(), m_options->viewportFollowFactor);
#endif

		if(changing && m_worm->getNinjaRope()->isReleased()) {
			if(aimingUp) {
				m_worm->addRopeLength(-tLXOptions->fRopeAdjustSpeed);
			}
			if(aimingDown) {
				m_worm->addRopeLength(tLXOptions->fRopeAdjustSpeed);
			}
		}
	}
}


void CWormHumanInputHandler::actionStart ( Actions action )
{
	switch (action) {
			case LEFT: {
				if ( m_worm ) {
					if(changing) {
						m_worm->changeWeaponTo( m_worm->getWeaponIndexOffset(-1) );
					} else {
						CWormInputHandler::baseActionStart(CWormInputHandler::LEFT);
						walkingLeft = true;
						if ( walkingRight )
							CWormInputHandler::baseActionStart(CWormInputHandler::DIG);
					}
				}
			}
			break;

			case RIGHT: {
				if ( m_worm ) {
					if(changing) {
						m_worm->changeWeaponTo( m_worm->getWeaponIndexOffset(1) );
					} else {
						CWormInputHandler::baseActionStart(CWormInputHandler::RIGHT);
						walkingRight = true;
						if ( walkingLeft )
							CWormInputHandler::baseActionStart(CWormInputHandler::DIG);
					}
				}
			}
			break;

			case FIRE: {
				if ( m_worm ) {
					if(!changing)
						CWormInputHandler::baseActionStart(CWormInputHandler::FIRE);
				}
			}
			break;

			case JUMP: {
				if ( m_worm ) {
					if ( m_worm->isActive() ) {
						if (tLXOptions->bOldSkoolRope && changing) {
							CWormInputHandler::baseActionStart(CWormInputHandler::NINJAROPE);
						} else {
							CWormInputHandler::baseActionStart(CWormInputHandler::JUMP);
							CWormInputHandler::baseActionStop(CWormInputHandler::NINJAROPE);
						}

						jumping = true;
					} else {
						CWormInputHandler::baseActionStart(CWormInputHandler::RESPAWN);
					}
				}
			}
			break;

			case UP: {
				if ( m_worm ) {
					aimingUp = true;
				}
			}
			break;

			case DOWN: {
				if ( m_worm ) {
					aimingDown = true;
				}
			}
			break;

			case CHANGE: {
				if ( m_worm ) {
					m_worm->actionStart(CWorm::CHANGEWEAPON);

					if (tLXOptions->bOldSkoolRope && jumping) {
						CWormInputHandler::baseActionStart(CWormInputHandler::NINJAROPE);
						jumping = false;
					} else {
						m_worm->actionStop(CWorm::FIRE); //TODO: Stop secondary fire also

						// Stop any movement
						m_worm->actionStop(CWorm::MOVELEFT);
						m_worm->actionStop(CWorm::MOVERIGHT);

					}

					changing = true;
				}
			}
			break;
			
			case NINJAROPE:
				CWormInputHandler::baseActionStart(CWormInputHandler::NINJAROPE);
				break;
			
			case ACTION_COUNT:
			break;
	}
}

void CWormHumanInputHandler::actionStop ( Actions action )
{
	switch (action) {
			case LEFT: {
				if ( m_worm ) {
					CWormInputHandler::baseActionStop(CWormInputHandler::LEFT);
					walkingLeft = false;
				}
			}
			break;

			case RIGHT: {
				if ( m_worm ) {
					CWormInputHandler::baseActionStop(CWormInputHandler::RIGHT);
					walkingRight = false;
				}
			}
			break;

			case FIRE: {
				if ( m_worm ) {
					CWormInputHandler::baseActionStop(CWormInputHandler::FIRE);
				}
			}
			break;

			case JUMP: {
				if ( m_worm ) {
					CWormInputHandler::baseActionStop(CWormInputHandler::JUMP);
					jumping = false;
				}
			}
			break;

			case UP: {
				if ( m_worm ) {
					aimingUp = false;
				}
			}
			break;

			case DOWN: {
				if ( m_worm ) {
					aimingDown = false;
				}
			}
			break;

			case CHANGE: {
				if ( m_worm ) {
					m_worm->actionStop(CWorm::CHANGEWEAPON);

					changing = false;
				}
			}
			break;

			case NINJAROPE:
			break;
			
			case ACTION_COUNT:
			break;
	}
}




void CWormHumanInputHandler::OlxInputToGusEvents()
{
#ifndef DEDICATED_ONLY
	// Note: This whole function should be removed later.
	// See the comment on CWormInputHandler::OlxInputToGusEvents.

	if(m_worm == NULL) return;

	// Note: We should use the following in all cases.
	// Right now, it doesn't really work with Gus
	// wpn selection and some other minor stuff.
	if(m_worm->getAlive() && m_worm->bWeaponsReady) {
		CWormInputHandler::OlxInputToGusEvents();
		return;
	}

	// change + jump -> ninja

	size_t i = 0;
	for(; i < game.localPlayers.size(); ++i)
		if(game.localPlayers[i] == this) break;

	if(i >= game.localPlayers.size()) {
		errors << "CWormHumanInputHandler::OlxInputToGusEvents: local player unknown" << endl;
		return;
	}
	
	//LEFT
	if(cLeft.wasDown()) eventStart(i, CWormHumanInputHandler::LEFT);
	if(cLeft.wasUp()) eventStop(i, CWormHumanInputHandler::LEFT);
	
 	//RIGHT
	if(cRight.wasDown()) eventStart(i, CWormHumanInputHandler::RIGHT);
	if(cRight.wasUp()) eventStop(i, CWormHumanInputHandler::RIGHT);
 	
	//UP
	if(cUp.wasDown()) eventStart(i, CWormHumanInputHandler::UP);
	if(cUp.wasUp()) eventStop(i, CWormHumanInputHandler::UP);
	
	//DOWN
	if(cDown.wasDown()) eventStart(i, CWormHumanInputHandler::DOWN);
	if(cDown.wasUp()) eventStop(i, CWormHumanInputHandler::DOWN);
	
	//FIRE
	if(cShoot.wasDown()) eventStart(i, CWormHumanInputHandler::FIRE);
	if(cShoot.wasUp()) eventStop(i, CWormHumanInputHandler::FIRE);
	
	//JUMP
	if(cJump.wasDown()) eventStart(i, CWormHumanInputHandler::JUMP);
	if(cJump.wasUp()) eventStop(i, CWormHumanInputHandler::JUMP);
	
	//CHANGE
	if(cSelWeapon.wasDown()) eventStart(i, CWormHumanInputHandler::CHANGE);
	if(cSelWeapon.wasUp()) eventStop(i, CWormHumanInputHandler::CHANGE);
	
	if(!tLXOptions->bOldSkoolRope) {
		if(cInpRope.isDownOnce()) eventStart(i, CWormHumanInputHandler::NINJAROPE);
		if(cInpRope.wasUp()) eventStop(i, CWormHumanInputHandler::NINJAROPE);
	}

	
	cUp.reset();
	cDown.reset();
	cLeft.reset();
	cRight.reset();
	cShoot.reset();
	cJump.reset();
	cSelWeapon.reset();
	cInpRope.reset();
	cStrafe.reset();
	cPrevWeapon.reset();
	cNextWeapon.reset();
	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].reset();
#endif // #ifndef DEDICATED_ONLY
}

