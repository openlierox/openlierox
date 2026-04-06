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
	
	// Input (keyboard)
	CInput		cUp, cDown, cLeft, cRight,
	cShoot, cJump, cSelWeapon, cInpRope,
	cStrafe, cWeapons[5];

	// Input (gamepad alternative)
	CInput		gpUp, gpDown, gpLeft, gpRight,
	gpShoot, gpJump, gpSelWeapon, gpInpRope,
	gpStrafe, gpWeapons[5];
	
	// for oldschool rope handling
	bool		bRopeDown;
	bool		bRopeDownOnce;

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
	
	bool	canType();
	
	
	// Input
	void		setupInputs(const PlyControls& Inputs);
	void		setupGamepadInputs(const PlyControls& Inputs);
	void		initInputSystem();
	void		stopInputSystem();
	
	
	
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
};

#endif  //  __CWORMHUMAN_H__
