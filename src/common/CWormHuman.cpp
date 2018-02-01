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

///////////////////
// Get the input from a human worm
void CWormHumanInputHandler::getInput() {
	// HINT: we are calling this from simulateWorm

	// do it here to ensure that it is called exactly once in a frame (needed because of intern handling)
	const bool jump = cJump.isDownOnce();
	const bool leftOnce = cLeft.isDownOnce();
	const bool rightOnce = cRight.isDownOnce();

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

	// angle section
	{
		static const float joystickCoeff = 150.0f/65536.0f;
		static const float joystickShift = 15; // 15 degrees

		// Joystick up
		if (cDown.isJoystickThrottle())  {
			m_worm->fAngleSpeed = 0;
			m_worm->fAngle = CLAMP((float)cUp.getJoystickValue() * joystickCoeff - joystickShift, -90.0f, 60.0f);
		}

		// Joystick down
		if (cDown.isJoystickThrottle())  {
			m_worm->fAngleSpeed = 0;
			m_worm->fAngle = CLAMP((float)cUp.getJoystickValue() * joystickCoeff - joystickShift, -90.0f, 60.0f);
		}

		float aimMaxSpeed = MAX((float)fabs(tLXOptions->fAimMaxSpeed), 20.0f); // Note: 100 was LX56 max aimspeed
		float aimAccel = MAX((float)fabs(tLXOptions->fAimAcceleration), 100.0f); // HINT: 500 is the LX56 value here (rev 1)
		float aimFriction = CLAMP(tLXOptions->fAimFriction, 0.0f, 1.0f); // we didn't had that in LX56; it behaves more natural
		bool aimLikeLX56 = tLXOptions->bAimLikeLX56;
		if(cClient->getGameLobby()[FT_ForceLX56Aim] || cClient->getServerVersion() < OLXRcVersion(0,58,3)) {
			aimMaxSpeed = 100;
			aimAccel = 500;
			aimLikeLX56 = true;
		}
		
		if(cUp.isDown() && !cUp.isJoystickThrottle()) { // Up
			m_worm->fAngleSpeed -= aimAccel * dt.seconds();
			if(!aimLikeLX56) CLAMP_DIRECT(m_worm->fAngleSpeed, -aimMaxSpeed, aimMaxSpeed);
		} else if(cDown.isDown() && !cDown.isJoystickThrottle()) { // Down
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
			if(!cClient->isHostAllowingStrafing() || !cStrafe.isDown())
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

		if(		(mouseControl && ws->bMove && m_worm->iMoveDirectionSide == DIR_LEFT)
			||	( ( (cLeft.isJoystick() && cLeft.isDown()) /*|| (cLeft.isKeyboard() && leftOnce)*/ ) && !cSelWeapon.isDown())
			) {

			if(tLX->currentTime - m_worm->fLastCarve >= carveDelay) {
				ws->bCarve = true;
				ws->bMove = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}

		if(		(mouseControl && ws->bMove && m_worm->iMoveDirectionSide == DIR_RIGHT)
			||	( ( (cRight.isJoystick() && cRight.isDown()) /*|| (cRight.isKeyboard() && rightOnce)*/ ) && !cSelWeapon.isDown())
			) {

			if(tLX->currentTime - m_worm->fLastCarve >= carveDelay) {
				ws->bCarve = true;
				ws->bMove = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}
	}

	
	const bool allowCombo = cClient->getGameLobby()[FT_WeaponCombos];
	
	ws->bShoot = cShoot.isDown();

	if(m_worm->getWeaponSlotsCount() > 0 && (!ws->bShoot || allowCombo)) {
		//
		// Weapon changing
		//
		if(cSelWeapon.isDown()) {
			// TODO: was is the intention of this var? if weapon change, then it's wrong
			// if cSelWeapon.isDown(), then we don't need it

			// we don't want keyrepeats here, so only count the first down-event
			int change = (rightOnce ? 1 : 0) - (leftOnce ? 1 : 0);
			m_worm->iCurrentWeapon += change;
			MOD(m_worm->iCurrentWeapon, m_worm->getWeaponSlotsCount());

			// Joystick: if the button is pressed, change the weapon (it is annoying to move the axis for weapon changing)
			if (cSelWeapon.isJoystick() && change == 0 && cSelWeapon.isDownOnce())  {
				m_worm->iCurrentWeapon++;
				MOD(m_worm->iCurrentWeapon, m_worm->getWeaponSlotsCount());
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

	}


	if(!cSelWeapon.isDown()) {
		if(cLeft.isDown()) {
			ws->bMove = true;
			m_worm->lastMoveTime = tLX->currentTime;

			if(!cRight.isDown()) {
				if(!cClient->isHostAllowingStrafing() || !cStrafe.isDown()) m_worm->iFaceDirectionSide = DIR_LEFT;
				m_worm->iMoveDirectionSide = DIR_LEFT;
			}

			if(rightOnce) {
				ws->bCarve = true;
				m_worm->fLastCarve = tLX->currentTime;
			}
		}

		if(cRight.isDown()) {
			ws->bMove = true;
			m_worm->lastMoveTime = tLX->currentTime;

			if(!cLeft.isDown()) {
				if(!cClient->isHostAllowingStrafing() || !cStrafe.isDown()) m_worm->iFaceDirectionSide = DIR_RIGHT;
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


	const bool oldskool = tLXOptions->bOldSkoolRope;

	// Jump
	if( !(oldskool && cSelWeapon.isDown()) )  {
		ws->bJump |= jump;

		if(jump && m_worm->cNinjaRope.get().isReleased())
			m_worm->cNinjaRope.write().Clear();
	}
	
	// Ninja Rope
	if(oldskool) {
		// Old skool style rope throwing
		// Change-weapon & jump

		if(!cSelWeapon.isDown() || !cJump.isDown())  {
			bRopeDown = false;
		}

		if(cSelWeapon.isDown() && cJump.isDown() && !bRopeDown) {
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
		if(cInpRope.isDownOnce()) {

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
void CWormHumanInputHandler::setupInputs(const PlyControls& Inputs)
{
	cUp.Setup(		Inputs[SIN_UP] );
	cDown.Setup(	Inputs[SIN_DOWN] );
	cLeft.Setup(	Inputs[SIN_LEFT] );
	cRight.Setup(	Inputs[SIN_RIGHT] );

	cShoot.Setup(	Inputs[SIN_SHOOT] );
	cJump.Setup(	Inputs[SIN_JUMP] );
	cSelWeapon.Setup(Inputs[SIN_SELWEAP] );
	cInpRope.Setup(	Inputs[SIN_ROPE] );

	cStrafe.Setup( Inputs[SIN_STRAFE] );

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
	int centrex = 320; // TODO: hardcoded screen width here
	
    if( v ) {
        if( v->getUsed() ) {
            l = v->GetLeft();
	        t = v->GetTop();
            centrex = v->GetLeft() + v->GetVirtW()/2;
        }
		
		DrawRectFill(bmpDest, l, t, l + v->GetVirtW(), t + v->GetVirtH(), Color(0,0,0,100));
    }
		
	tLX->cFont.DrawCentre(bmpDest, centrex, t+30, tLX->clWeaponSelectionTitle, "~ Weapons Selection ~");
		
	tLX->cFont.DrawCentre(bmpDest, centrex, t+48, tLX->clWeaponSelectionTitle, "(Use up/down and left/right for selection.)");
	tLX->cFont.DrawCentre(bmpDest, centrex, t+66, tLX->clWeaponSelectionTitle, "(Go to 'Done' and press shoot then.)");
	//tLX->cOutlineFont.DrawCentre(bmpDest, centrex, t+30, tLX->clWeaponSelectionTitle, "Weapons Selection");
	//tLX->cOutlineFont.DrawCentre(bmpDest, centrex, t+30, tLX->clWeaponSelectionTitle, "Weapons Selection");
	
	bool bChat_Typing = cClient->isTyping();
	
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
		if (cDown.isJoystickThrottle() || cUp.isJoystickThrottle())  {
			m_worm->iCurrentWeapon = (cUp.getJoystickValue() + 32768) * (m_worm->tWeapons.size() + 2) / 65536; // We have 7 rows and 65536 throttle states
			
		} else {
			int change = cDown.wasDown() - cUp.wasDown();
			m_worm->iCurrentWeapon += change;
			m_worm->iCurrentWeapon %= (int)m_worm->tWeapons.size() + 2;
			if(m_worm->iCurrentWeapon < 0) m_worm->iCurrentWeapon += (int)m_worm->tWeapons.size() + 2;
		}
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
	for( size_t i = 0; i < sizeof(cWeapons) / sizeof(cWeapons[0]) ; i++  )
		cWeapons[i].reset();
#endif // #ifndef DEDICATED_ONLY
}

