/////
////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Floating options window file
// Created 24/9/08
// Karel Petranek

#include "Options.h"
#include "sound/SoundsBase.h"
#include "CClient.h"
#include "AuxLib.h"
#include "DeprecatedGUI/Graphics.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CCheckbox.h"
#include "DeprecatedGUI/CTextbox.h"
#include "DeprecatedGUI/CSlider.h"
#include "Music.h"
#include "ConversationLogger.h"
#include "game/Game.h"
#include "LieroX.h"
#include "game/CMap.h"


namespace DeprecatedGUI {

bool bChangedVideoMode = false;
bool bChangedAntiAliasing = false;
bool bShowFloatingOptions = false;


int iFloatingOptionsMode = 0;
CGuiLayout	cFloatingOptions;
CGuiLayout	cFloatingOpt_Controls;
CGuiLayout	cFloatingOpt_System;
CGuiLayout	cFloatingOpt_Game;
CButton		cFloatingOpt_TopButtons[3];

// Control id's
enum {
	op_Ok = -2,
	Static = -1,
	op_Controls=0,
	op_Game,
	op_System
};

enum {
	os_Fullscreen,
	os_SoundOn,
	os_SoundVolume,
	os_NetworkSpeed,
	os_NetworkUploadBandwidth,
	os_NetworkUploadBandwidthLabel,
	os_ShowFPS,
	os_ShowPing,
	os_LogConvos,
	os_ScreenshotFormat,
	os_MaxFPS
};

enum {
	og_BloodAmount,
	og_Shadows,
	og_Particles,
	og_OldSkoolRope,
	og_AIDifficulty,
	og_ShowWormHealth,
	og_ColorizeNicks,
	og_AutoTyping,
	og_Antialiasing,
	og_MouseAiming,
	og_AllowMouseAiming,
	og_MatchLogging,
	og_AntilagMovementPrediction,
	og_ScreenShaking,
	//og_AllowFileDownload,
	//og_AllowDirtUpdates
};

enum {
	oc_Ply1_Up,
	oc_Ply1_Down,
	oc_Ply1_Left,
	oc_Ply1_Right,
	oc_Ply1_Shoot,
	oc_Ply1_Jump,
	oc_Ply1_Rope,
	oc_Ply1_Strafe,
	oc_Ply1_Selweapon,
	oc_Ply1_PrevWeapon,
	oc_Ply1_NextWeapon,

	oc_Ply2_Up,
	oc_Ply2_Down,
	oc_Ply2_Left,
	oc_Ply2_Right,
	oc_Ply2_Shoot,
	oc_Ply2_Jump,
	oc_Ply2_Rope,
	oc_Ply2_Strafe,
	oc_Ply2_Selweapon,
	oc_Ply2_PrevWeapon,
	oc_Ply2_NextWeapon,

	oc_Gen_Chat,
    oc_Gen_Score,
	oc_Gen_Health,
	oc_Gen_CurSettings,
	oc_Gen_TakeScreenshot,
	oc_Gen_ViewportManager,
	oc_Gen_SwitchMode,
	oc_Gen_ToggleTopBar,
	oc_Gen_TeamChat,
};


std::string sFloatingOpt_InputNames[] = {
	"Up",
	"Down",
	"Left",
	"Right",
	"Shoot",
	"Jump",
	"Ninja Rope",
	"Strafe",
	"Select Weapon",
	"Previous Weapon",
	"Next Weapon"
};

static const int sFloatingOpt_InputNames__size = sizeof(sFloatingOpt_InputNames) / sizeof(std::string);
static_assert( sFloatingOpt_InputNames__size == __SIN_PLY_BOTTOM - 5, "inputopts__sizecheck" );


///////////////////
// Initialize the options
bool Menu_FloatingOptionsInitialize()
{
	iFloatingOptionsMode = 0;
    int i;

	// Background image is redrawn each frame, so commented this out
	// Create the buffer
	DrawImage(tMenu->bmpBuffer.get(), tMenu->bmpMainBack_common,0,0);

	// Setup the top buttons
	cFloatingOpt_TopButtons[op_Controls] =	CButton(BUT_CONTROLS,	tMenu->bmpButtons);
	cFloatingOpt_TopButtons[op_Game] =		CButton(BUT_GAME,		tMenu->bmpButtons);
	cFloatingOpt_TopButtons[op_System] =		CButton(BUT_SYSTEM,		tMenu->bmpButtons);

	cFloatingOpt_TopButtons[0].Setup(op_Controls, 180, 110, 100, 15);
	cFloatingOpt_TopButtons[1].Setup(op_Game, 310, 110, 50, 15);
	cFloatingOpt_TopButtons[2].Setup(op_System, 390, 110, 70, 15);
    for(i=op_Controls; i<=op_System; i++)
        cFloatingOpt_TopButtons[i].Create();

	cFloatingOptions.Shutdown();
	cFloatingOptions.Initialize();

	cFloatingOpt_System.Shutdown();
	cFloatingOpt_System.Initialize();

	cFloatingOpt_Controls.Shutdown();
	cFloatingOpt_Controls.Initialize();

	cFloatingOpt_Game.Shutdown();
	cFloatingOpt_Game.Initialize();


	// Add the controls

	// Controls
	cFloatingOpt_Controls.Add( new CLabel("Player Controls", tLX->clHeading), Static, 40,  150, 0,0);
	cFloatingOpt_Controls.Add( new CLabel("Player 1",tLX->clSubHeading),      Static, 163, 170, 0,0);
	cFloatingOpt_Controls.Add( new CLabel("Player 2",tLX->clSubHeading),      Static, 268, 170, 0,0);
	cFloatingOpt_Controls.Add( new CLabel("General Controls", tLX->clHeading),Static, 390, 150, 0,0);

	// Row pitch tightened to 22px so the two extra rows (Previous/Next weapon)
	// still fit within the dialog.
	int y = 188;
	for(i=0;i< sFloatingOpt_InputNames__size ;i++,y+=22) {
		cFloatingOpt_Controls.Add( new CLabel(sFloatingOpt_InputNames[i],tLX->clNormalLabel), Static, 40, y, 0,0);

		cFloatingOpt_Controls.Add( new CInputbox(SIN_UP+i, tLXOptions->sPlayerControls[0][SIN_UP+i], tMenu->bmpInputbox, sFloatingOpt_InputNames[i]),
			               oc_Ply1_Up+i, 165, y, 50,17);
		cFloatingOpt_Controls.Add( new CInputbox(SIN_UP+i, tLXOptions->sPlayerControls[1][SIN_UP+i], tMenu->bmpInputbox, sFloatingOpt_InputNames[i]),
			               oc_Ply2_Up+i, 270, y, 50,17);

	}

	// General Controls
	cFloatingOpt_Controls.Add( new CLabel("Chat", tLX->clNormalLabel), Static, 390, 190, 0,0);
	cFloatingOpt_Controls.Add( new CInputbox(SIN_CHAT, tLXOptions->sGeneralControls[SIN_CHAT], tMenu->bmpInputbox, "Chat"),
						   oc_Gen_Chat, 525, 190, 50,17);

    cFloatingOpt_Controls.Add( new CLabel("Scoreboard", tLX->clNormalLabel), Static, 390, 215, 0,0);
	cFloatingOpt_Controls.Add( new CInputbox(SIN_SCORE, tLXOptions->sGeneralControls[SIN_SCORE], tMenu->bmpInputbox, "Scoreboard"),
						   oc_Gen_Score, 525, 215, 50,17);

    cFloatingOpt_Controls.Add( new CLabel("Health Bar", tLX->clNormalLabel), Static, 390, 240, 0,0);
	cFloatingOpt_Controls.Add( new CInputbox(SIN_HEALTH, tLXOptions->sGeneralControls[SIN_HEALTH], tMenu->bmpInputbox, "Health Bar"),
						   oc_Gen_Health, 525, 240, 50,17);

    cFloatingOpt_Controls.Add( new CLabel("Current Settings", tLX->clNormalLabel), Static, 390, 265, 0,0);
	cFloatingOpt_Controls.Add( new CInputbox(SIN_SETTINGS, tLXOptions->sGeneralControls[SIN_SETTINGS], tMenu->bmpInputbox, "Current Settings"),
						   oc_Gen_CurSettings, 525, 265, 50,17);

    cFloatingOpt_Controls.Add( new CLabel("Take Screenshot", tLX->clNormalLabel), Static, 390, 290, 0,0);
	cFloatingOpt_Controls.Add( new CInputbox(SIN_SCREENSHOTS, tLXOptions->sGeneralControls[SIN_SCREENSHOTS], tMenu->bmpInputbox, "Take Screenshot"),
						   oc_Gen_TakeScreenshot, 525, 290, 50,17);

    cFloatingOpt_Controls.Add( new CLabel("Viewport Manager", tLX->clNormalLabel), Static, 390, 315, 0,0);
	cFloatingOpt_Controls.Add( new CInputbox(SIN_VIEWPORTS, tLXOptions->sGeneralControls[SIN_VIEWPORTS], tMenu->bmpInputbox, "Viewport Manager"),
						   oc_Gen_ViewportManager, 525, 315, 50,17);

    cFloatingOpt_Controls.Add( new CLabel("Switch Video Mode", tLX->clNormalLabel), Static, 390, 340, 0,0);
	cFloatingOpt_Controls.Add( new CInputbox(SIN_SWITCHMODE, tLXOptions->sGeneralControls[SIN_SWITCHMODE], tMenu->bmpInputbox, "Switch Video Mode"),
						   oc_Gen_SwitchMode, 525, 340, 50,17);

    cFloatingOpt_Controls.Add( new CLabel("Toggle Top Bar", tLX->clNormalLabel), Static, 390, 365, 0,0);
	cFloatingOpt_Controls.Add( new CInputbox(SIN_TOGGLETOPBAR, tLXOptions->sGeneralControls[SIN_TOGGLETOPBAR], tMenu->bmpInputbox, "Toggle Top Bar"),
						   oc_Gen_ToggleTopBar, 525, 365, 50,17);


	cFloatingOpt_Controls.Add( new CLabel("Teamchat", tLX->clNormalLabel), Static, 390, 390, 0,0);
	cFloatingOpt_Controls.Add( new CInputbox(SIN_TEAMCHAT, tLXOptions->sGeneralControls[SIN_TEAMCHAT], tMenu->bmpInputbox, "Teamchat"),
						   oc_Gen_TeamChat, 525, 390, 50,17);



	// System
	cFloatingOpt_System.Add( new CLabel("Video",tLX->clHeading),              Static, 40, 150, 0,0);
	cFloatingOpt_System.Add( new CLabel("Fullscreen",tLX->clNormalLabel),       Static, 60, 170, 0,0);
	
	cFloatingOpt_System.Add( new CCheckbox(tLXOptions->bFullscreen),os_Fullscreen, 140, 170, 17,17);

	cFloatingOpt_System.Add( new CLabel("Audio",tLX->clHeading),              Static, 40, 200, 0,0);
	cFloatingOpt_System.Add( new CLabel("Sound on",tLX->clNormalLabel),         Static, 60, 220, 0,0);
	cFloatingOpt_System.Add( new CCheckbox(tLXOptions->bSoundOn),   os_SoundOn, 170, 220, 17,17);
	cFloatingOpt_System.Add( new CLabel("Sound volume",tLX->clNormalLabel),     Static, 330, 220, 0,0);
	cFloatingOpt_System.Add( new CSlider(100),                      os_SoundVolume, 435, 217, 110, 20);

	cFloatingOpt_System.Add( new CLabel("Miscellanous",tLX->clHeading),       Static, 40, 265, 0,0);
	cFloatingOpt_System.Add( new CLabel("Show FPS",tLX->clNormalLabel),         Static, 60, 285, 0,0);
	cFloatingOpt_System.Add( new CCheckbox(tLXOptions->bShowFPS),   os_ShowFPS, 200, 285, 17,17);
	cFloatingOpt_System.Add( new CLabel("Log Conversations",tLX->clNormalLabel),Static, 60, 315, 0,0);
	cFloatingOpt_System.Add( new CCheckbox(tLXOptions->bLogConvos), os_LogConvos, 200,315,17,17);
	cFloatingOpt_System.Add( new CLabel("Show ping",tLX->clNormalLabel),		Static, 230, 315, 0,0);
	cFloatingOpt_System.Add( new CCheckbox(tLXOptions->bShowPing),  os_ShowPing, 365,315,17,17);
	cFloatingOpt_System.Add( new CLabel("Screenshot format",tLX->clNormalLabel),Static, 230,285, 0,0);
	cFloatingOpt_System.Add( new CLabel("Max FPS",tLX->clNormalLabel),Static, 480,285, 0,0);
	cFloatingOpt_System.Add( new CTextbox(),                        os_MaxFPS, 540, 283, 50,tLX->cFont.GetHeight());

	cFloatingOpt_System.Add( new CCombobox(), os_NetworkSpeed, 170, 342, 130,17);
	cFloatingOpt_System.Add( new CLabel("Network speed",tLX->clNormalLabel),    Static, 60,345, 0,0);

	cFloatingOpt_System.Add( new CLabel("Server max upload bandwidth",tLX->clNormalLabel),    os_NetworkUploadBandwidthLabel, 330, 345, 0,0);
	cFloatingOpt_System.Add( new CTextbox(),                        os_NetworkUploadBandwidth, 530, 342, 60,tLX->cFont.GetHeight());

	// Put the combo box after the other widgets to get around the problem with widget layering
	cFloatingOpt_System.Add( new CCombobox(), os_ScreenshotFormat, 365, 283, 70,17);

	// Set the values
	CSlider *s = (CSlider *)cFloatingOpt_System.getWidget(os_SoundVolume);
	s->setValue( tLXOptions->iSoundVolume );

	CTextbox *t = (CTextbox *)(cFloatingOpt_System.getWidget(os_MaxFPS));
	t->setText(itoa(tLXOptions->nMaxFPS));


	for(i=0; i<3; i++)
		cFloatingOpt_System.SendMessage(os_NetworkSpeed, CBS_ADDITEM, NetworkSpeedString((NetworkSpeed)i), i);

	cFloatingOpt_System.SendMessage(os_NetworkSpeed, CBM_SETCURSEL, tLXOptions->iNetworkSpeed, 0);
	cFloatingOpt_System.SendMessage(os_NetworkSpeed, CBM_SETCURINDEX, tLXOptions->iNetworkSpeed, 0);

	((CTextbox *)cFloatingOpt_System.getWidget( os_NetworkUploadBandwidth ))->setText( itoa(tLXOptions->iMaxUploadBandwidth) );
	cFloatingOpt_System.getWidget( os_NetworkUploadBandwidth )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );
	cFloatingOpt_System.getWidget( os_NetworkUploadBandwidthLabel )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );

	// Screenshot format
	cFloatingOpt_System.SendMessage(os_ScreenshotFormat, CBS_ADDITEM, "Bmp", FMT_BMP);
	cFloatingOpt_System.SendMessage(os_ScreenshotFormat, CBS_ADDITEM, "Png", FMT_PNG);
	cFloatingOpt_System.SendMessage(os_ScreenshotFormat, CBS_ADDITEM, "Gif", FMT_GIF);
	cFloatingOpt_System.SendMessage(os_ScreenshotFormat, CBS_ADDITEM, "Jpg", FMT_JPG);

	cFloatingOpt_System.SendMessage(os_ScreenshotFormat, CBM_SETCURSEL, tLXOptions->iScreenshotFormat, 0);
	cFloatingOpt_System.SendMessage(os_ScreenshotFormat, CBM_SETCURINDEX, tLXOptions->iScreenshotFormat, 0);

	// Game
	cFloatingOpt_Game.Add( new CLabel("Blood Amount",tLX->clNormalLabel),       Static, 40, 150, 0,0);
	cFloatingOpt_Game.Add( new CSlider(5000),                       og_BloodAmount, 175, 147, 210, 20);
	cFloatingOpt_Game.Add( new CLabel("Shadows",tLX->clNormalLabel),            Static, 40, 180, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(tLXOptions->bShadows),     og_Shadows, 280, 180, 17,17);
	cFloatingOpt_Game.Add( new CLabel("Particles",tLX->clNormalLabel),          Static, 40, 210, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(tLXOptions->bParticles),   og_Particles, 280, 210, 17,17);
	cFloatingOpt_Game.Add( new CLabel("Classic Rope throw",tLX->clNormalLabel), Static, 40, 240, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(tLXOptions->bOldSkoolRope),og_OldSkoolRope, 280, 240, 17,17);
	cFloatingOpt_Game.Add( new CLabel("Colorize nicks by teams",tLX->clNormalLabel), Static, 40, 270, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(tLXOptions->bColorizeNicks),og_ColorizeNicks, 280, 270, 17,17);
	cFloatingOpt_Game.Add( new CLabel("Start typing after any key press",tLX->clNormalLabel), Static, 40, 300, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(tLXOptions->bAutoTyping),og_AutoTyping, 280, 300, 17,17);
	cFloatingOpt_Game.Add( new CLabel("Use antialiasing (slow)",tLX->clNormalLabel), Static, 40, 330, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(tLXOptions->bAntiAliasing),og_Antialiasing, 280, 330, 17,17);
	cFloatingOpt_Game.Add( new CLabel("Enable mouse control (Player 1)",tLX->clNormalLabel), Static, 40, 420, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(tLXOptions->bMouseAiming),og_MouseAiming, 280, 420, 17,17);
	cFloatingOpt_Game.Add( new CLabel("Log my game results",tLX->clNormalLabel), Static, 40, 390, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(tLXOptions->bMatchLogging),og_MatchLogging, 280, 390, 17,17);

	cFloatingOpt_Game.Add( new CLabel("Network antilag prediction",tLX->clNormalLabel), Static, 40, 360, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(tLXOptions->bAntilagMovementPrediction),og_AntilagMovementPrediction, 280, 360, 17,17);


/*	cFloatingOpt_Game.Add( new CLabel("Shake screen on explosions",tLX->clNormalLabel), Static, 330, 180, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(tLXOptions->bScreenShaking),     og_ScreenShaking, 550, 180, 17,17); */

	/*cFloatingOpt_Game.Add( new CLabel("Allow mouse control (Server)",tLX->clNormalLabel), Static, 330, 360, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(tLXOptions->bAllowMouseAiming),og_AllowMouseAiming, 550, 360, 17,17);

	cFloatingOpt_Game.Add( new CLabel("Allow strafing (Server)",tLX->clNormalLabel), Static, 330, 390, 0,0);
	cFloatingOpt_Game.Add( new CCheckbox(&tLXOptions->bAllowStrafing), Static, 550, 390, 17,17);*/

	// TODO: Fix cSlider so it's value thing doesn't take up a square of 100x100 pixels.

	// The OK button
	cFloatingOptions.Add( new CButton(BUT_BACK, tMenu->bmpButtons), op_Ok, 540,440, 40,20);

	// Set the values
	cFloatingOpt_Game.SendMessage( og_BloodAmount,  SLM_SETVALUE, tLXOptions->iBloodAmount, 0);

	bChangedVideoMode = false;

	return true;
}


void Menu_FloatingOptionsOkClose()
{
	// Set the max FPS
	CTextbox *t = (CTextbox *)cFloatingOpt_System.getWidget(os_MaxFPS);
	bool fail = false;
	tLXOptions->nMaxFPS = from_string<int>(t->getText(), fail);
	tLXOptions->nMaxFPS = fail ? 0 : MAX(0, tLXOptions->nMaxFPS);
	t->setText(itoa(tLXOptions->nMaxFPS));

	// Process video mode switch
	if (bChangedVideoMode)  {
		tLXOptions->bFullscreen = ((CCheckbox *)cFloatingOpt_System.getWidget(os_Fullscreen))->getValue();

		PlaySoundSample(sfxGeneral.smpClick);

		// Set the new video mode
		doSetVideoModeInMainThread();
		EnableSystemMouseCursor(false);
	}

	Menu_FloatingOptionsShutdown();	
}


///////////////////
// Options main frame
void Menu_FloatingOptionsFrame()
{
	mouse_t		*Mouse = GetMouse();
	gui_event_t *ev = NULL;
	int			val;

	CCheckbox	*c;

	// Box dimensions
	static const int x = 30;
	static const int y = 20;
	static const int w = 565;
	static const int h = 450;

	// Redraw background image
	DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpMainBack_common, x, y, x, y, w, h);
	Menu_DrawBox(VideoPostProcessor::videoSurface().get(), x, y, x + w, y + h);
	Menu_DrawSubTitle(VideoPostProcessor::videoSurface().get(), SUB_OPTIONS);

	// Process the top buttons
	cFloatingOpt_TopButtons[iFloatingOptionsMode].MouseOver(Mouse);
	SetGameCursor(CURSOR_ARROW); // Hack: button changed the cursor to hand, we need to change it back
	for(int i=op_Controls;i<=op_System;i++) {

		cFloatingOpt_TopButtons[i].Draw(VideoPostProcessor::videoSurface().get());

		if(i==iFloatingOptionsMode)
			continue;

		if(cFloatingOpt_TopButtons[i].InBox(Mouse->X,Mouse->Y)) {
			cFloatingOpt_TopButtons[i].MouseOver(Mouse);
			if(Mouse->Up) {
				iFloatingOptionsMode = i;
				PlaySoundSample(sfxGeneral.smpClick);
			}
		}
	}

	// Fullscreen value
	c = (CCheckbox *)cFloatingOpt_System.getWidget(os_Fullscreen);
	bool fullscr = c->getValue();
	c = (CCheckbox *)cFloatingOpt_System.getWidget(og_Antialiasing);
	bool aa = c->getValue();


	// Process the gui layout
	ev = cFloatingOptions.Process();
	if (ev)  {
		switch (ev->iControlID)  {

		// Ok
		case op_Ok:
			if(ev->iEventMsg == BTN_CLICKED) {
				Menu_FloatingOptionsOkClose();
				return;
			}
			break;

		}
	}

	if(iFloatingOptionsMode == 0) {

		// Controls
		ev = cFloatingOpt_Controls.Process();
		cFloatingOpt_Controls.Draw(VideoPostProcessor::videoSurface().get());

		if(ev) {

			if(ev->cWidget->getType() == wid_Inputbox) {

				if(ev->iEventMsg == INB_MOUSEUP) {

					int ply = 0;
					if(ev->iControlID >= oc_Ply2_Up && ev->iControlID <= oc_Ply2_NextWeapon)
						ply = 1;
					if(ev->iControlID >= oc_Gen_Chat)
						ply = -1;

					// Get an input
					CInputbox *b = (CInputbox *)ev->cWidget;
					Menu_FloatingOptionsWaitInput(ply, b->getName(), b);

					tLX->setupInputs();

				}
			}
		}
	}


	if(iFloatingOptionsMode == 1) {

		// Game
		ev = cFloatingOpt_Game.Process();
		cFloatingOpt_Game.Draw(VideoPostProcessor::videoSurface().get());

		val = (int)cFloatingOpt_Game.SendMessage(og_BloodAmount, SLM_GETVALUE, (uintptr_t)0, 0);
        DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, 385,140, 385,140, 70,40);
		tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(),385, 148, tLX->clNormalLabel, itoa(val)+"%");



		if(ev) {

			switch(ev->iControlID) {

				// Blood amount
				case og_BloodAmount:
					if(ev->iEventMsg == SLD_CHANGE) {
						val = (int)cFloatingOpt_Game.SendMessage(og_BloodAmount, SLM_GETVALUE, (uintptr_t)0, 0);
						tLXOptions->iBloodAmount = val;
					}
					break;

				// Shadows
				case og_Shadows:
					if(ev->iEventMsg == CHK_CHANGED) {
						c = (CCheckbox *)cFloatingOpt_Game.getWidget(og_Shadows);
						tLXOptions->bShadows = c->getValue();
					}
					break;

				// Particles
				case og_Particles:
					if(ev->iEventMsg == CHK_CHANGED) {
						c = (CCheckbox *)cFloatingOpt_Game.getWidget(og_Particles);
						tLXOptions->bParticles = c->getValue();
					}
					break;

				// Old skool rope throw
				case og_OldSkoolRope:
					if(ev->iEventMsg == CHK_CHANGED) {
						tLXOptions->bOldSkoolRope = cFloatingOpt_Game.SendMessage(og_OldSkoolRope,CKM_GETCHECK,(uintptr_t)0,0) != 0;
					}
					break;

				// TDM nick colorizing
				case og_ColorizeNicks:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bColorizeNicks = cFloatingOpt_Game.SendMessage(og_ColorizeNicks, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;

				// Auto typing
				case og_AutoTyping:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bAutoTyping = cFloatingOpt_Game.SendMessage(og_AutoTyping, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;

				// Antialiasing
				case og_Antialiasing:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bAntiAliasing = cFloatingOpt_Game.SendMessage(og_Antialiasing, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;


				// Mouse aiming
				case og_MouseAiming:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bMouseAiming = cFloatingOpt_Game.SendMessage(og_MouseAiming, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;
/*
				case og_AllowMouseAiming:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bAllowMouseAiming = cFloatingOpt_Game.SendMessage(og_AllowMouseAiming, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;
*/

				// Match logging
				case og_MatchLogging:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bMatchLogging = cFloatingOpt_Game.SendMessage(og_MatchLogging, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;

				case og_AntilagMovementPrediction:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bAntilagMovementPrediction = cFloatingOpt_Game.SendMessage(og_AntilagMovementPrediction, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;
					
/*				case og_ScreenShaking:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bScreenShaking = cFloatingOpt_Game.SendMessage(og_ScreenShaking, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break; */
			}
		}

	}


	// Process the different pages
	if(iFloatingOptionsMode == 2) {

		// System
		ev = cFloatingOpt_System.Process();
		cFloatingOpt_System.Draw(VideoPostProcessor::videoSurface().get());

		if(ev) {

			switch(ev->iControlID) {

				// Sound on/off
				case os_SoundOn:
					if(ev->iEventMsg == CHK_CHANGED) {

						bool old = tLXOptions->bSoundOn;

						c = (CCheckbox *)cFloatingOpt_System.getWidget(os_SoundOn);
						tLXOptions->bSoundOn = c->getValue();

						if(old != tLXOptions->bSoundOn) {
							if(tLXOptions->bSoundOn)
								StartSoundSystem();
							else
								StopSoundSystem();
						}
					}
					break;

				// Sound volume
				case os_SoundVolume:
					if(ev->iEventMsg == SLD_CHANGE) {
						CSlider *s = (CSlider *)cFloatingOpt_System.getWidget(os_SoundVolume);
						tLXOptions->iSoundVolume = s->getValue();

						SetSoundVolume( tLXOptions->iSoundVolume );
					}
					break;

				// Show FPS
				case os_ShowFPS:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bShowFPS = cFloatingOpt_System.SendMessage(os_ShowFPS, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;

				// Logging
				case os_LogConvos:
					if(ev->iEventMsg == CHK_CHANGED)  {
						// check if value is really different
						if(tLXOptions->bLogConvos != (cFloatingOpt_System.SendMessage(os_LogConvos, CKM_GETCHECK, (uintptr_t)0, 0) != 0)) {
							tLXOptions->bLogConvos = ! tLXOptions->bLogConvos;
							if (convoLogger)  {
								if (tLXOptions->bLogConvos)  {
									convoLogger->startLogging();
									if (cClient && (cClient->getStatus() == NET_CONNECTED))
										convoLogger->enterServer(cClient->getServerName());
								} else
									convoLogger->endLogging();
							}
						}
					}
					break;

				// Show ping
				case os_ShowPing:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bShowPing = cFloatingOpt_System.SendMessage(os_ShowPing, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;
			}
		}


		// Get the values
		tLXOptions->iNetworkSpeed = (int)cFloatingOpt_System.SendMessage(os_NetworkSpeed, CBM_GETCURINDEX,(uintptr_t)0,0);

		cFloatingOpt_System.getWidget( os_NetworkUploadBandwidth )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );
		cFloatingOpt_System.getWidget( os_NetworkUploadBandwidthLabel )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );
		if( cFloatingOpt_System.getWidget( os_NetworkUploadBandwidth )->getEnabled() )
			tLXOptions->iMaxUploadBandwidth = atoi( ((CTextbox *)cFloatingOpt_System.getWidget( os_NetworkUploadBandwidth ))->getText().c_str() );
		if( tLXOptions->iMaxUploadBandwidth <= 0 )
			tLXOptions->iMaxUploadBandwidth = 20000;

		tLXOptions->iScreenshotFormat = (int)cFloatingOpt_System.SendMessage(os_ScreenshotFormat, CBM_GETCURINDEX,(uintptr_t)0,0);

		// Anti-aliasing and fullscreen

		bChangedVideoMode = fullscr != tLXOptions->bFullscreen;
		bChangedAntiAliasing = (aa != tLXOptions->bAntiAliasing);
	}

	// Draw the OK button
	cFloatingOptions.Draw(VideoPostProcessor::videoSurface().get());


	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());
}


///////////////////
// Process an input box waiting thing
// ply=-1 : general ; ply>=0 : normal player
void Menu_FloatingOptionsWaitInput(int ply, const std::string& name, CInputbox *b)
{
	mouse_t *Mouse = GetMouse();

	// Draw the back buffer
	DrawImage(tMenu->bmpBuffer.get(), VideoPostProcessor::videoSurface(), 0, 0);
	cFloatingOptions.Draw(tMenu->bmpBuffer.get());
	cFloatingOpt_Controls.Draw(tMenu->bmpBuffer.get());

	Menu_DrawBox(tMenu->bmpBuffer.get(), 210, 170, 430, 310);
	//DrawImageAdv(tMenu->bmpBuffer, tMenu->bmpMainBack, 212,172, 212,172, 217,137);
    DrawRectFill(tMenu->bmpBuffer.get(), 212,172,429,309,tLX->clDialogBackground);

	tLX->cFont.DrawCentre(tMenu->bmpBuffer.get(),320,180,Color(128,200,255),"Input for:");
	tLX->cFont.DrawCentre(tMenu->bmpBuffer.get(),320,205,Color(255,255,255),name);

	tLX->cFont.DrawCentre(tMenu->bmpBuffer.get(),320,270,Color(255,255,255),"Press any key/mouse");
	tLX->cFont.DrawCentre(tMenu->bmpBuffer.get(),320,285,Color(128,128,128),"(Escape to cancel)");

	cFloatingOpt_TopButtons[iFloatingOptionsMode].MouseOver(Mouse);
	for(ushort i=0;i<3;i++) {
		cFloatingOpt_TopButtons[i].Draw(tMenu->bmpBuffer.get());
	}

	Menu_redrawBufferRect(0, 0, VideoPostProcessor::get()->screenWidth(), VideoPostProcessor::get()->screenHeight());

	Mouse->Up = 0;
	Mouse->Down = 0;

	ProcessEvents();
	SetGameCursor(CURSOR_ARROW);
	CInput::InitJoysticksTemp();
	ProcessEvents(); // drop all current events in queue
	while(game.state != Game::S_Quit) {
		Menu_RedrawMouse(true);

		DrawCursor(VideoPostProcessor::videoSurface().get());

		// Escape quits the wait for user input
		if(WasKeyboardEventHappening(SDLK_ESCAPE,false))
			break;

		std::string tmp;
		if(CInput::Wait(tmp)) {
			b->setText(tmp);
			break;
		}

		doVideoFrameInMainThread();
		CapFPS();
		ProcessEvents();
	}
	CInput::UnInitJoysticksTemp();

	// Change the options
	if(ply >= 0) {
		tLXOptions->sPlayerControls[ply][b->getValue()] = b->getText();
	} else
		tLXOptions->sGeneralControls[b->getValue()] = b->getText();

	// Disable quick weapon selection keys if they collide with other keys
	for( uint ply1 = 0; ply1 < tLXOptions->sPlayerControls.size(); ply1 ++ )
	{
		for( int key1 = SIN_WEAPON1; key1 <= SIN_WEAPON5; key1 ++ )
		{
			for( uint ply2 = 0; ply2 < tLXOptions->sPlayerControls.size(); ply2 ++ )
				for( int key2 = SIN_UP; key2 < SIN_WEAPON1; key2 ++ )
					if( tLXOptions->sPlayerControls[ply1][key1] ==
						tLXOptions->sPlayerControls[ply2][key2] )
						tLXOptions->sPlayerControls[ply1][key1] = "";

			for( int key2 = SIN_CHAT; key2 < __SIN_GENERAL_BOTTOM; key2 ++ )
				if( tLXOptions->sPlayerControls[ply1][key1] ==
					tLXOptions->sGeneralControls[key2] )
					tLXOptions->sPlayerControls[ply1][key1] = "";
		}
	}


	Mouse->Down = 0;
	Mouse->Up = 0;

	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
}


///////////////////
// Shutdown the options menu
void Menu_FloatingOptionsShutdown()
{
	cFloatingOptions.Shutdown();
	cFloatingOpt_Controls.Shutdown();
	cFloatingOpt_System.Shutdown();
	cFloatingOpt_Game.Shutdown();
	bShowFloatingOptions = false;
}

}; // namespace DeprecatedGUI
