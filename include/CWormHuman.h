/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////



#ifndef __CWORMHUMAN_H__
#define __CWORMHUMAN_H__

#include "game/CWorm.h"
#include "game/WormInputHandler.h"


struct ALLEGRO_BITMAP;
class CViewport;


class CWormHumanInputHandler : public CWormInputHandler {
public:
	CWormHumanInputHandler(CWorm* w);
	virtual std::string name() { return "Human input handler"; }

	virtual void initWeaponSelection();
	virtual	void doWeaponSelectionFrame(SDL_Surface * bmpDest, CViewport *v);
	
	// simulation
	virtual void startGame();
	virtual void getInput(); 
    virtual void clearInput();
	
protected:
	virtual ~CWormHumanInputHandler();

	// Input
	CInput		cUp, cDown, cLeft, cRight,
	cShoot, cJump, cSelWeapon, cInpRope,
	cStrafe, cPrevWeapon, cNextWeapon, cWeapons[5];

	// for oldschool rope handling
	bool		bRopeDown;
	bool		bRopeDownOnce;

	// 0 = first local player (controlled by touch + Ply1Controls),
	// 1 = second local player (split-screen). -1 if not yet set.
	int			m_localPlayerSlot;

public:

	
	CInput &	getInputUp()					{ return cUp; };
	CInput &	getInputDown()					{ return cDown; };
	CInput &	getInputLeft()					{ return cLeft; };
	CInput &	getInputRight()					{ return cRight; };
	CInput &	getInputShoot()					{ return cShoot; };
	CInput &	getInputJump()					{ return cJump; };
	CInput &	getInputWeapon()				{ return cSelWeapon; };
	CInput &	getInputRope()					{ return cInpRope; };
	CInput &	getInputStrafe()				{ return cStrafe; };
	CInput &	getInputPrevWeapon()			{ return cPrevWeapon; };
	CInput &	getInputNextWeapon()			{ return cNextWeapon; };
	
	bool	canType();
	
	
	// Input
	void		setupInputs(const PlyControls& Inputs, int localPlayerSlot);
	void		initInputSystem();
	void		stopInputSystem();

	int			getLocalPlayerSlot() const { return m_localPlayerSlot; }
	
	
	
	// ---------------------- Gusanos -------------------------
	
public:
	
	enum Actions
	{
		LEFT = 0,
		RIGHT,
		UP,
		DOWN,
		FIRE,
		JUMP,
		CHANGE,
		NINJAROPE,
		ACTION_COUNT,
	};

protected:
	void OlxInputToGusEvents();
	
private:
	void gusInit();
	
public:
	void subThink();
	void actionStart( Actions action );
	void actionStop( Actions action );
	
private:	
	bool aimingUp;
	bool aimingDown;
	bool changing;
	bool jumping;
	bool walkingLeft;
	bool walkingRight;

	// Gamepad left-stick navigation in the weapon-selection menu (the analog
	// sticks aren't bindable, so the menu reads the pad directly). Tracks the
	// stick so one push moves one row, with keyboard-style auto-repeat while held.
	AbsTime	fLastWeaponSelStickMove;
	bool	weaponSelStickHeld;
	bool	weaponSelStickRepeated;
};

#endif  //  __CWORMHUMAN_H__
