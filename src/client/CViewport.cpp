/////////////////////////////////////////
//
//                  OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Viewport class
// Created 22/1/02
// Jason Boettcher



#include "LieroX.h"
#include "Debug.h"
#include "CClient.h"
#include "Options.h" // for controls_t
#include "game/CWorm.h"
#include "MathLib.h"
#ifndef DEDICATED_ONLY
#include "sound/sfx.h"
#endif


CViewport& CViewport::operator=(const CViewport& o) {
	shutdown();

	bUsed = o.bUsed;

	Left = o.Left;
	Top = o.Top;
	VirtWidth = o.VirtWidth;
	VirtHeight = o.VirtHeight;
	Width = o.Width;
	Height = o.Height;
	WorldX = o.WorldX;
	WorldY = o.WorldY;

	m_origTargetWorm = o.m_origTargetWorm;
	pcTargetWorm = o.pcTargetWorm;
	nType = o.nType;
	bSmooth = o.bSmooth;
	fTimer = o.fTimer;

	if(bUsed) {
#ifndef DEDICATED_ONLY
		m_listener = new Listener();
		sfx.registerListener(m_listener);
#endif
	}

	return *this;
}


///////////////////
// Setup the viewport
void CViewport::Setup(int l, int t, int vw, int vh, int type)
{
	shutdown();
	
	bUsed = true;

	Left = l;
	Top = t;
	VirtWidth = vw;
	VirtHeight = vh;

	Width = vw/2;
	Height = vh/2;

    pcTargetWorm = NULL;
    nType = type;
	bSmooth = false;
	
#ifndef DEDICATED_ONLY
	m_listener = new Listener();
	sfx.registerListener(m_listener);
#endif
}

void CViewport::shutdown() {
	bUsed = false;
#ifndef DEDICATED_ONLY
	if (m_listener)  {
		sfx.removeListener(m_listener);
		delete m_listener;
		m_listener = NULL;
	}
#endif
	gusReset();
}


///////////////////
// Setup the keyboard inputs for freelook
void CViewport::setupInputs(const PlyControls& Inputs)
{
	cUp = new CInput();
	cDown = new CInput();
	cLeft = new CInput();
	cRight = new CInput();

    cUp.get()->Setup(		Inputs[SIN_UP] );
	cDown.get()->Setup(	Inputs[SIN_DOWN] );
	cLeft.get()->Setup(	Inputs[SIN_LEFT] );
	cRight.get()->Setup(	Inputs[SIN_RIGHT] );
}

void CViewport::setupGamepadInputs(const PlyControls& Inputs)
{
	gpUp = new CInput();
	gpDown = new CInput();
	gpLeft = new CInput();
	gpRight = new CInput();

	gpUp.get()->Setup(		Inputs[SIN_UP] );
	gpDown.get()->Setup(	Inputs[SIN_DOWN] );
	gpLeft.get()->Setup(	Inputs[SIN_LEFT] );
	gpRight.get()->Setup(	Inputs[SIN_RIGHT] );

	cUp.get()->setAlternative( gpUp.get() );
	cDown.get()->setAlternative( gpDown.get() );
	cLeft.get()->setAlternative( gpLeft.get() );
	cRight.get()->setAlternative( gpRight.get() );
}


///////////////////
// Process a viewport
void CViewport::Process(CViewport *pcViewList, int MWidth, int MHeight, int iGameMode)
{    
	if(!bUsed) {
		errors << "Unused CViewport process" << endl;
		return;
	}
	
	int hx = Width/2;
	int hy = Height/2;

    // Follow a player
    switch (nType)  {
	case VW_FOLLOW: {
	    // The player will ideally be in the centre of the viewport
	    // The viewport will then rest against the edges if that cannot happen

        // If we have no target at all, find one
        if( !pcTargetWorm ) {
			//notes << "find new worm for viewport because we have currently none" << endl;
            
            // Try and find a living worm first
            CWorm *t = findTarget(pcViewList, true);

            // If no living worms, try a dead worm (but still in the game)
            if(!t)
                t = findTarget(pcViewList, false);

            if(t) {
                pcTargetWorm = t;
                fTimer = AbsTime();
            } else {
                // If we didn't find a new worm, go into freelook mode                    
                pcTargetWorm = NULL;
                nType = VW_FREELOOK;
                fTimer = AbsTime();
                return;
            }
        }

        // Follow the worm

        if( pcTargetWorm ) {
			// Note: Use always the pos of the worm, even if it is dead. This doesn't give disadvantage
			// and it is needed when having a sizefactor!=1 and a custom viewport.
            if( true /* pcTargetWorm->getAlive() */ ) {
				if( bSmooth )
					setSmoothPosition( pcTargetWorm->getPos().x-hx, pcTargetWorm->getPos().y-hy, tLX->fDeltaTime );
				else
				{
					WorldX = (int)(pcTargetWorm->getPos().x-hx);
					WorldY = (int)(pcTargetWorm->getPos().y-hy);									
				}

                // Clear the timer
                fTimer = AbsTime();
            } /*else
            	notes << "viewport: our worm is dead" << endl; */
        }
    }
	break; // VW_FOLLOW

    // Cycle
    case VW_CYCLE: {
        // Cycles through players. If a player dies (but not necessarily out), move onto another living player

        // Check if the target is out of the game or has died
        if( pcTargetWorm ) {
			if( pcTargetWorm->getLives() == WRM_OUT )
				notes << "find new worm for viewport because current is out of game" << endl;

			if( pcTargetWorm->getLives() == WRM_OUT || !pcTargetWorm->getAlive() ) {
                // Setup the timer to wait 0.5 seconds before changing targets
                if( fTimer == AbsTime() )
                    fTimer = tLX->currentTime + 0.5f;

                // AbsTime up? Change targets
                if( tLX->currentTime > fTimer ) {

                    // Try and find a living worm first
                    CWorm *t = findTarget(pcViewList, true);

                    // If no living worms, try a dead worm (but still in the game)
                    if(!t)
                        t = findTarget(pcViewList, false);

                    if(t) {
                        pcTargetWorm = t;
                        fTimer = AbsTime();
                    } else {
                        // If we didn't find a new worm, go into freelook mode
                        pcTargetWorm = NULL;
                        nType = VW_FREELOOK;
                        fTimer = AbsTime();
						notes << "no worm found, going into freelook mode" << endl;
						return;
                    }
                }
            }
        }

        // Follow the worm
        if( pcTargetWorm ) {
            if( pcTargetWorm->getAlive() ) {
				if( bSmooth )
					setSmoothPosition( pcTargetWorm->getPos().x-hx, pcTargetWorm->getPos().y-hy, tLX->fDeltaTime );
				else
				{
					WorldX = (int)(pcTargetWorm->getPos().x-hx);
					WorldY = (int)(pcTargetWorm->getPos().y-hy);
				}

                // Clear the timer
                fTimer = AbsTime();
            }
        }
    }
	break; // VW_CYCLE


    // Action
	case VW_ACTIONCAM: {
        // Finds a group of worms and smoothly moves around to show the whole group of players in a fight

        pcTargetWorm = NULL;
        
        // Generate a score for each worm depending on closeness of other worms
        // The worm with the lowest score is used as a focus point        
		std::map<int,float> fScores;
        
        // Go through all permutations of the worms
		for_each_iterator(CWorm*, w1, game.aliveWorms()) {
            // Set a zero score because we are at least alive and well
            fScores[w1->get()->getID()] = 0;
            
			for_each_iterator(CWorm*, w2, game.aliveWorms())
                fScores[w1->get()->getID()] += CalculateDistance(w1->get()->getPos(), w2->get()->getPos());
        }
        
        int focusWormId = -1;
        float lowest = 99999;
        
		foreach(s, fScores) {
            if( focusWormId < 0 || s->second < lowest ) {
                focusWormId = s->first;
                lowest = s->second;
            }
        }
        
        // No focus point? Leave
        if( focusWormId < 0 )
            return;
        
        // The focus worm is our target
        tgtPos = game.wormById(focusWormId)->getPos();
        
        CVec dir = tgtPos - curPos;
        float l = NormalizeVector(&dir);
        
        float speed = l*2.5f;
        speed = MIN((float)300,speed);
        
        if( l > 2 )
            curPos += dir*speed * (float)tLX->fDeltaTime.seconds();
        
        WorldX = (int)floor(curPos.x-hx);
        WorldY = Round(curPos.y-hy);
    }
	break; // VW_ACTIONCAM


    // Free look
    case VW_FREELOOK: {
        float scrollSpeed = 300.0f*tLX->fDeltaTime.seconds();

        // Uses the players keys to scroll around
        if( cUp.get()->isDown() )
            curPos -= CVec(0,scrollSpeed);
        else if( cDown.get()->isDown() )
            curPos += CVec(0,scrollSpeed);
        if( cLeft.get()->isDown() )
            curPos -= CVec(scrollSpeed,0);
        else if( cRight.get()->isDown() )
            curPos += CVec(scrollSpeed,0);

        // Clamp our movement
        curPos.x=( MAX((float)0,curPos.x) );
        curPos.y=( MAX((float)0,curPos.y) );
        curPos.x=( MIN((float)MWidth-Width,curPos.x) );
        curPos.y=( MIN((float)MHeight-Height,curPos.y) );
        
        WorldX = (int)curPos.x;
        WorldY = (int)curPos.y;
    }
	break;

	}	// switch


	// Shake the viewport a bit
	if(bShaking) {
		if(tLX->currentTime - fShakestart > 0.2f) {
			bShaking = false;
			iShakeAmount = 0;
		}
		else {

            // Don't shake the action/freelook cam
            if( nType != VW_ACTIONCAM && nType != VW_FREELOOK && 
            	( cClient->getGameLobby()[FT_ScreenShaking] ) ) {

                // Clamp it to the edges, then shake. So we can still see shaking near edges
                Clamp(MWidth, MHeight);

			    // Shake
			    WorldX += (int)(GetRandomNum() * (float)iShakeAmount);
			    WorldY += (int)(GetRandomNum() * (float)iShakeAmount);
            }
		}
	}

	// Clamp it
	Clamp(MWidth, MHeight);

#ifndef DEDICATED_ONLY
	m_listener->pos = Vec((float)WorldX, (float)WorldY) + Vec((float)(Width/2), (float)(Height/2));
#endif
}


///////////////////
// Find a target worm
CWorm *CViewport::findTarget(CViewport *pcViewList, bool bAlive)
{
    // Find a worm that isn't already a target by another viewport
	for_each_iterator(CWorm*, w, game.worms()) {
        // If the worm isn't alive, and we want a living worm, skip the worm
        if( !w->get()->getAlive() && bAlive )
            continue;

        short viewcount = 0;
        for( short v=0; v<NUM_VIEWPORTS; v++ ) {

            // Make sure this isn't our viewport
			if( pcViewList[v].nID == nID )
                continue;

			if (!pcViewList[v].getUsed())
				continue;

            CWorm *t = pcViewList[v].getTarget();
            if(t) {
                if( w->get()->getID() == t->getID() )
                    viewcount++;
            }
        }

        // If this worm was in none of the viewports, use the worm
        if( !viewcount )
            return w->get();
    }

    // No good target
    return NULL;
}


///////////////////
// Resets the viewport simulations (timer, movement, etc)
void CViewport::reset()
{
    fTimer = AbsTime();
}


///////////////////
// Clamp the viewport if it exceeds any boundaries
void CViewport::Clamp(int MWidth, int MHeight)
{
	// If we have FT_InfiniteMap set, we don't want to clamp the viewport.
	// We are drawing the map (and everything) tiled together then.
	if(!cClient->getGameLobby()[FT_InfiniteMap]) {
		if(MWidth >= Width)
			WorldX = CLAMP(WorldX, 0, MWidth-Width);
		else
			WorldX = 0;
		if(MHeight >= Height)
			WorldY = CLAMP(WorldY, 0, MHeight-Height);
		else
			WorldY = 0;
	}
}




///////////////////
// Get the rectangle of the viewport
SDL_Rect CViewport::getRect()
{
	SDL_Rect r;

	r.x = Left;
	r.y = Top;
	r.w = VirtWidth;
	r.h = VirtHeight;

	return r;
}


///////////////////
// Shake the viewport
void CViewport::Shake(int amount)
{
	if( bSmooth )
		return;	// Don't shake viewport if we're spectating - it's just hurting my eyes.
	fShakestart = tLX->currentTime;
	bShaking = true;
	if(amount > iShakeAmount)
		iShakeAmount = amount;
}


///////////////////
// Check if a point is inside this viewport
bool CViewport::inView(CVec pos)
{
	return	(int)pos.x >= WorldX &&
			(int)pos.y >= WorldY &&
			(int)pos.x <= WorldX+Width &&
			(int)pos.y <= WorldY+Height;
}

void CViewport::setSmooth(bool _b)	
{
	bSmooth = _b; 
	cSmoothVel = cSmoothAccel = CVec(0,0); 
	curPos = tgtPos = CVec( (float)WorldX, (float)WorldY ); 
}

// Constants that control behavior of smoothed viewport
/* // Nice smoothy settings
const float fVelMax = 1200.0f, fAccelMax = 400.0f, 
			fVelIncrease = 20.0f, fAccelIncrease = 70.0f,
			fVelDecay = 10.0f, fAccelDecay = 30.0f;
*/
// Hard follow settings, only slightly smoothed
const float fVelMax = 2000.0f, fAccelMax = 600.0f, 
			fVelIncrease = 100.0f, fAccelIncrease = 200.0f,
			fVelDecay = 20.0f, fAccelDecay = 50.0f;

void CViewport::setSmoothPosition( float X, float Y, TimeDiff DeltaTime )
{
	// TODO: these formulas work only for FPS > 50, so we'll run simulation twice on low FPS
	if( DeltaTime > 0.015f )
	{
		setSmoothPosition( X, Y, TimeDiff(0.015f) );
		setSmoothPosition( X, Y, TimeDiff(DeltaTime) - TimeDiff(0.015f) );
		return;
	}

	CVec Coords( X, Y );
	CVec Diff = Coords - curPos;

	cSmoothAccel += Diff * ( fAccelIncrease * (float)DeltaTime.seconds() );
	cSmoothAccel -= cSmoothAccel * fAccelDecay * (float)DeltaTime.seconds();
	if( cSmoothAccel.GetLength2() > fAccelMax*fAccelMax )
		cSmoothAccel *= fAccelMax / cSmoothAccel.GetLength();
	
	cSmoothVel += cSmoothAccel * ( fVelIncrease * (float)DeltaTime.seconds() );
	cSmoothVel -= cSmoothVel * fVelDecay * (float)DeltaTime.seconds();
	if( cSmoothVel.GetLength2() > fVelMax*fVelMax )
		cSmoothVel *= fVelMax / cSmoothVel.GetLength();
	
	curPos += cSmoothVel * DeltaTime.seconds();

	WorldX = (int)curPos.x;
	WorldY = (int)curPos.y;
}

