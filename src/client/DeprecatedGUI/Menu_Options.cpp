/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Options
// Created 30/7/02
// Jason Boettcher


#include "LieroX.h"
#include "sound/SoundsBase.h"
#include "Music.h"
#include "AuxLib.h"
#include "DeprecatedGUI/Graphics.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "DeprecatedGUI/CAnimation.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CCheckbox.h"
#include "DeprecatedGUI/CTextbox.h"
#include "DeprecatedGUI/CSlider.h"
#include "DeprecatedGUI/CLine.h"
#include "IpToCountryDB.h"
#include "ConversationLogger.h"


namespace DeprecatedGUI {

int OptionsMode = 0;
CGuiLayout	cOptions;
CGuiLayout	cOpt_Controls;
CGuiLayout	cOpt_System;
CGuiLayout	cOpt_Game;
CButton		TopButtons[3];

// Control id's
enum {
	op_Back = -2,
	Static = -1,
	op_Controls=0,
	op_Game,
	op_System
};

enum {
	os_Fullscreen,
	os_ColourDepth,
	os_SoundOn,
	os_SoundVolume,
	os_NetworkPort,
	os_NetworkSpeed,
	os_NetworkUploadBandwidth,
	os_NetworkUploadBandwidthLabel,
	os_NetworkUploadCheck,
	os_UseIpToCountry,
	os_HttpProxy,
	os_ShowFPS,
	os_OpenGL,
	os_ShowPing,
	os_LogConvos,
	os_ScreenshotFormat,
	os_MaxFPS,
	os_Apply,
	os_TestBandwidth,
	os_TestAnimation,
	os_ShowCountryFlags,
	os_CheckForUpdates,
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
	og_DamagePopups,
	og_ColorizeDamageByWorm,
};

enum {
	oc_Ply1_Up,
	oc_Ply1_Down,
	oc_Ply1_Left,
	oc_Ply1_Right,
	oc_Ply1_Shoot,
	oc_Ply1_Jump,
	oc_Ply1_Selweapon,
	oc_Ply1_Rope,
	oc_Ply1_Strafe,

	oc_Ply2_Up,
	oc_Ply2_Down,
	oc_Ply2_Left,
	oc_Ply2_Right,
	oc_Ply2_Shoot,
	oc_Ply2_Jump,
	oc_Ply2_Selweapon,
	oc_Ply2_Rope,
	oc_Ply2_Strafe,

	oc_Gen_Chat,
    oc_Gen_Score,
	oc_Gen_Health,
	oc_Gen_CurSettings,
	oc_Gen_TakeScreenshot,
	oc_Gen_ViewportManager,
	oc_Gen_SwitchMode,
	oc_Gen_ToggleTopBar,
	oc_Gen_TeamChat,
	oc_Gen_IrcChat,
	oc_Gen_ConsoleToggle,
};


std::string InputNames[] = {
	"Up",
	"Down",
	"Left",
	"Right",
	"Shoot",
	"Jump",
	"Select Weapon",
	"Ninja Rope",
	"Strafe"
};


bool bSpeedTest = false;

///////////////////
// Initialize the options
bool Menu_OptionsInitialize()
{
	tMenu->iMenuType = MNU_OPTIONS;
	OptionsMode = 0;
    int i;
	bSpeedTest = false;

	// Create the buffer
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
	Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_OPTIONS);

	Menu_RedrawMouse(true);

	// Setup the top buttons
	TopButtons[op_Controls] =	CButton(BUT_CONTROLS,	tMenu->bmpButtons);
	TopButtons[op_Game] =		CButton(BUT_GAME,		tMenu->bmpButtons);
	TopButtons[op_System] =		CButton(BUT_SYSTEM,		tMenu->bmpButtons);

	TopButtons[0].Setup(op_Controls, 180, 110, 100, 15);
	TopButtons[1].Setup(op_Game, 310, 110, 50, 15);
	TopButtons[2].Setup(op_System, 390, 110, 70, 15);
    for(i=op_Controls; i<=op_System; i++)
        TopButtons[i].Create();

	cOptions.Shutdown();
	cOptions.Initialize();

	cOpt_System.Shutdown();
	cOpt_System.Initialize();

	cOpt_Controls.Shutdown();
	cOpt_Controls.Initialize();

	cOpt_Game.Shutdown();
	cOpt_Game.Initialize();


	// Add the controls
	cOptions.Add( new CButton(BUT_BACK, tMenu->bmpButtons), op_Back, 25,440, 50,15);


	// Controls
	cOpt_Controls.Add( new CLabel("Player Controls", tLX->clHeading), Static, 40,  150, 0,0);
	cOpt_Controls.Add( new CLabel("Player 1",tLX->clSubHeading),      Static, 163, 170, 0,0);
	cOpt_Controls.Add( new CLabel("Player 2",tLX->clSubHeading),      Static, 268, 170, 0,0);
	cOpt_Controls.Add( new CLabel("General Controls", tLX->clHeading),Static, 390, 150, 0,0);

	int y = 190;
	for(i=0;i<9;i++,y+=25) {
		cOpt_Controls.Add( new CLabel(InputNames[i],tLX->clNormalLabel), Static, 40, y, 0,0);

		cOpt_Controls.Add( new CInputbox(SIN_UP+i, tLXOptions->sPlayerControls[0][SIN_UP+i], tMenu->bmpInputbox, InputNames[i]),
			               oc_Ply1_Up+i, 165, y, 50,17);
		cOpt_Controls.Add( new CInputbox(SIN_UP+i, tLXOptions->sPlayerControls[1][SIN_UP+i], tMenu->bmpInputbox, InputNames[i]),
			               oc_Ply2_Up+i, 270, y, 50,17);

	}

	// General Controls
	cOpt_Controls.Add( new CLabel("Chat", tLX->clNormalLabel), Static, 390, 190, 0,0);
	cOpt_Controls.Add( new CInputbox(SIN_CHAT, tLXOptions->sGeneralControls[SIN_CHAT], tMenu->bmpInputbox, "Chat"),
						   oc_Gen_Chat, 525, 190, 50,17);

    cOpt_Controls.Add( new CLabel("Scoreboard", tLX->clNormalLabel), Static, 390, 215, 0,0);
	cOpt_Controls.Add( new CInputbox(SIN_SCORE, tLXOptions->sGeneralControls[SIN_SCORE], tMenu->bmpInputbox, "Scoreboard"),
						   oc_Gen_Score, 525, 215, 50,17);

    cOpt_Controls.Add( new CLabel("Health Bar", tLX->clNormalLabel), Static, 390, 240, 0,0);
	cOpt_Controls.Add( new CInputbox(SIN_HEALTH, tLXOptions->sGeneralControls[SIN_HEALTH], tMenu->bmpInputbox, "Health Bar"),
						   oc_Gen_Health, 525, 240, 50,17);

    cOpt_Controls.Add( new CLabel("Current Settings", tLX->clNormalLabel), Static, 390, 265, 0,0);
	cOpt_Controls.Add( new CInputbox(SIN_SETTINGS, tLXOptions->sGeneralControls[SIN_SETTINGS], tMenu->bmpInputbox, "Current Settings"),
						   oc_Gen_CurSettings, 525, 265, 50,17);

    cOpt_Controls.Add( new CLabel("Take Screenshot", tLX->clNormalLabel), Static, 390, 290, 0,0);
	cOpt_Controls.Add( new CInputbox(SIN_SCREENSHOTS, tLXOptions->sGeneralControls[SIN_SCREENSHOTS], tMenu->bmpInputbox, "Take Screenshot"),
						   oc_Gen_TakeScreenshot, 525, 290, 50,17);

    cOpt_Controls.Add( new CLabel("Viewport Manager", tLX->clNormalLabel), Static, 390, 315, 0,0);
	cOpt_Controls.Add( new CInputbox(SIN_VIEWPORTS, tLXOptions->sGeneralControls[SIN_VIEWPORTS], tMenu->bmpInputbox, "Viewport Manager"),
						   oc_Gen_ViewportManager, 525, 315, 50,17);

    cOpt_Controls.Add( new CLabel("Switch Video Mode", tLX->clNormalLabel), Static, 390, 340, 0,0);
	cOpt_Controls.Add( new CInputbox(SIN_SWITCHMODE, tLXOptions->sGeneralControls[SIN_SWITCHMODE], tMenu->bmpInputbox, "Switch Video Mode"),
						   oc_Gen_SwitchMode, 525, 340, 50,17);

    cOpt_Controls.Add( new CLabel("Toggle Top Bar", tLX->clNormalLabel), Static, 390, 365, 0,0);
	cOpt_Controls.Add( new CInputbox(SIN_TOGGLETOPBAR, tLXOptions->sGeneralControls[SIN_TOGGLETOPBAR], tMenu->bmpInputbox, "Toggle Top Bar"),
						   oc_Gen_ToggleTopBar, 525, 365, 50,17);

	cOpt_Controls.Add( new CLabel("Teamchat", tLX->clNormalLabel), Static, 390, 390, 0,0);
	cOpt_Controls.Add( new CInputbox(SIN_TEAMCHAT, tLXOptions->sGeneralControls[SIN_TEAMCHAT], tMenu->bmpInputbox, "Teamchat"),
						   oc_Gen_TeamChat, 525, 390, 50,17);

	cOpt_Controls.Add( new CLabel("IRC chat window", tLX->clNormalLabel), Static, 390, 415, 0,0);
	cOpt_Controls.Add( new CInputbox(SIN_IRCCHAT, tLXOptions->sGeneralControls[SIN_IRCCHAT], tMenu->bmpInputbox, "IRC chat"),
						   oc_Gen_IrcChat, 525, 415, 50,17);

	cOpt_Controls.Add( new CLabel("Toggle console", tLX->clNormalLabel), Static, 390, 440, 0,0);
	cOpt_Controls.Add( new CInputbox(SIN_CONSOLETOGGLE, tLXOptions->sGeneralControls[SIN_CONSOLETOGGLE], tMenu->bmpInputbox, "Console"),
	                   oc_Gen_ConsoleToggle, 525, 440, 50,17);
	
	

	// System
	const Color lineCol = tLX->clLine; //tLX->clHeading.derived(0,0,0,-200);
	const int starty = 130;
	y = starty;
	cOpt_System.Add( new CLabel("Video",tLX->clHeading),              Static, 40, y, 0,0);
	cOpt_System.Add( new CLine(0,0,0,0, lineCol), Static, 90, y + 8, 620 - 90, 0);
	y += 20;
	cOpt_System.Add( new CLabel("Fullscreen",tLX->clNormalLabel),       Static, 60, y, 0,0);
	cOpt_System.Add( new CLabel("Colour depth",tLX->clNormalLabel),       Static, 175, y, 0,0);
	cOpt_System.Add( new CCheckbox(tLXOptions->bFullscreen),os_Fullscreen, 140, y, 17,17);
	cOpt_System.Add( new CLabel("Use OpenGL Rendering",tLX->clNormalLabel),Static, 440, y, 0,0);
	cOpt_System.Add( new CCheckbox(tLXOptions->bOpenGL),    os_OpenGL, 590, y, 17,17);

	y += 20;
	cOpt_System.Add( new CLabel("Audio",tLX->clHeading),              Static, 40, y, 0,0);
	cOpt_System.Add( new CLine(0,0,0,0, lineCol), Static, 90, y + 8, 620 - 90, 0);
	y += 20;
	cOpt_System.Add( new CLabel("Sound on",tLX->clNormalLabel),         Static, 60, y, 0,0);
	cOpt_System.Add( new CCheckbox(tLXOptions->bSoundOn),   os_SoundOn, 170, y, 17,17);
	cOpt_System.Add( new CLabel("Sound volume",tLX->clNormalLabel),     Static, 330, y, 0,0);
	cOpt_System.Add( new CSlider(100),                      os_SoundVolume, 435, y - 2, 110, 20); y += 25;

	y += 20;
	cOpt_System.Add( new CLabel("Network",tLX->clHeading),            Static, 40, y, 0,0);
	cOpt_System.Add( new CLine(0,0,0,0, lineCol), Static, 110, y + 8, 620 - 110, 0);	
	y += 20;
	cOpt_System.Add( new CLabel("Network port",tLX->clNormalLabel),     Static, 60, y, 0,0);
	cOpt_System.Add( new CTextbox(),                        os_NetworkPort, 170, y - 2, 100,tLX->cFont.GetHeight());
	
	cOpt_System.Add( new CLabel("Use IP To Country Database",tLX->clNormalLabel),	Static, 330, y, 0,0);
	cOpt_System.Add( new CCheckbox(tLXOptions->bUseIpToCountry),  os_UseIpToCountry, 530,y,17,17); y += 30;

	cOpt_System.Add( new CLabel("Network speed",tLX->clNormalLabel),    Static, 60, y, 0,0);
	
	cOpt_System.Add( new CLabel("Show country flags",tLX->clNormalLabel),	Static, 330, y, 0,0);
	cOpt_System.Add( new CCheckbox(tLXOptions->bShowCountryFlags),  os_ShowCountryFlags, 530, y,17,17); y += 30;
	
	cOpt_System.Add( new CLabel("HTTP proxy",tLX->clNormalLabel),    Static, 60, y, 0,0);
	cOpt_System.Add( new CTextbox(),                        os_HttpProxy, 170, y - 2, 130,tLX->cFont.GetHeight());
	
	cOpt_System.Add( new CLabel("Server max upload bandwidth",tLX->clNormalLabel),    os_NetworkUploadBandwidthLabel, 330, y, 0,0);
	cOpt_System.Add( new CTextbox(),                        os_NetworkUploadBandwidth, 530, y - 2, 50,tLX->cFont.GetHeight());
	cOpt_System.Add( new CButton(BUT_TEST, tMenu->bmpButtons), os_TestBandwidth, 585, y - 2, 30, 22); y += 25;
	
	cOpt_System.Add( new CLabel("Check bandwidth sanity",tLX->clNormalLabel),    Static, 330, y, 0,0);
	cOpt_System.Add( new CCheckbox(tLXOptions->bCheckBandwidthSanity),  os_NetworkUploadCheck, 530, y,17,17);
	
	y += 20;
	cOpt_System.Add( new CLabel("Miscellanous",tLX->clHeading),       Static, 40, y, 0,0);
	cOpt_System.Add( new CLine(0,0,0,0, lineCol), Static, 130, y + 8, 620 - 130, 0);	
	y += 20;
	cOpt_System.Add( new CLabel("Show FPS",tLX->clNormalLabel),         Static, 60, y, 0,0);
	cOpt_System.Add( new CCheckbox(tLXOptions->bShowFPS),   os_ShowFPS, 200, y, 17,17);

	cOpt_System.Add( new CLabel("Screenshot format",tLX->clNormalLabel),Static, 230,y, 0,0);
	cOpt_System.Add( new CLabel("Max FPS",tLX->clNormalLabel),Static, 480,y, 0,0);
	cOpt_System.Add( new CTextbox(),                        os_MaxFPS, 550, y - 2, 50,tLX->cFont.GetHeight()); y += 30;
	
	cOpt_System.Add( new CLabel("Log Conversations",tLX->clNormalLabel),Static, 60, y, 0,0);
	cOpt_System.Add( new CCheckbox(tLXOptions->bLogConvos), os_LogConvos, 200,y,17,17);
	cOpt_System.Add( new CLabel("Check for updates",tLX->clNormalLabel),Static, 230, y, 0,0);
	cOpt_System.Add( new CCheckbox(tLXOptions->bCheckForUpdates),  os_CheckForUpdates, 365,y,17,17);
	cOpt_System.Add( new CLabel("Show ping",tLX->clNormalLabel),		Static, 480, y, 0,0);
	cOpt_System.Add( new CCheckbox(tLXOptions->bShowPing),  os_ShowPing, 550,y,17,17);
	
	
	cOpt_System.SendMessage(os_NetworkPort,TXM_SETMAX,5,0);

	cOpt_System.Add( new CButton(BUT_APPLY, tMenu->bmpButtons), os_Apply, 555,440, 60,15);

	// Put the combo box after the other widgets to get around the problem with widget layering
	cOpt_System.Add( new CCombobox(), os_NetworkSpeed, 170, starty + 155 - 3, 130,17);
	cOpt_System.Add( new CCombobox(), os_ScreenshotFormat, 365, starty + 250 - 2, 70,17);
	cOpt_System.Add( new CCombobox(), os_ColourDepth, 275, starty + 20, 145, 17);

	// Set the values
	CSlider *s = (CSlider *)cOpt_System.getWidget(os_SoundVolume);
	s->setValue( tLXOptions->iSoundVolume );

	CTextbox *t = (CTextbox *)cOpt_System.getWidget(os_NetworkPort);
	t->setText( itoa(tLXOptions->iNetworkPort) );
	t = (CTextbox *)(cOpt_System.getWidget(os_MaxFPS));
	t->setText(itoa(tLXOptions->nMaxFPS));
	t = (CTextbox *)(cOpt_System.getWidget(os_HttpProxy));
	t->setText(tLXOptions->sHttpProxy);

	// Network speed
	for(i=0; i<3; i++)
		cOpt_System.SendMessage(os_NetworkSpeed, CBS_ADDITEM, NetworkSpeedString((NetworkSpeed)i), i);

	cOpt_System.SendMessage(os_NetworkSpeed, CBM_SETCURSEL, tLXOptions->iNetworkSpeed, 0);
	cOpt_System.SendMessage(os_NetworkSpeed, CBM_SETCURINDEX, tLXOptions->iNetworkSpeed, 0);
	((CTextbox *)cOpt_System.getWidget( os_NetworkUploadBandwidth ))->setText( itoa(tLXOptions->iMaxUploadBandwidth) );
	cOpt_System.getWidget( os_NetworkUploadBandwidth )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );
	cOpt_System.getWidget( os_NetworkUploadBandwidthLabel )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );
	cOpt_System.getWidget( os_TestBandwidth )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );
	
	// Screenshot format
	cOpt_System.SendMessage(os_ScreenshotFormat, CBS_ADDITEM, "Bmp", FMT_BMP);
	cOpt_System.SendMessage(os_ScreenshotFormat, CBS_ADDITEM, "Png", FMT_PNG);
	cOpt_System.SendMessage(os_ScreenshotFormat, CBS_ADDITEM, "Gif", FMT_GIF);
	cOpt_System.SendMessage(os_ScreenshotFormat, CBS_ADDITEM, "Jpg", FMT_JPG);

	cOpt_System.SendMessage(os_ScreenshotFormat, CBM_SETCURSEL, tLXOptions->iScreenshotFormat, 0);
	cOpt_System.SendMessage(os_ScreenshotFormat, CBM_SETCURINDEX, tLXOptions->iScreenshotFormat, 0);

	// Color depth
	cOpt_System.SendMessage(os_ColourDepth, CBS_ADDITEM, "Automatic", 0);
	cOpt_System.SendMessage(os_ColourDepth, CBS_ADDITEM, "High Color (16 bit)", 1);
	cOpt_System.SendMessage(os_ColourDepth, CBS_ADDITEM, "True Color (24 bit)", 2);
	cOpt_System.SendMessage(os_ColourDepth, CBS_ADDITEM, "True Color (32 bit)", 3);

	switch (tLXOptions->iColourDepth) {
	case 0:  // Automatic
		cOpt_System.SendMessage(os_ColourDepth, CBM_SETCURSEL, (uintptr_t)0, 0);
		cOpt_System.SendMessage(os_ColourDepth, CBM_SETCURINDEX, (uintptr_t)0, 0);
		break;
	case 16:  // 16 bit
		cOpt_System.SendMessage(os_ColourDepth, CBM_SETCURSEL, (uintptr_t)1, 0);
		cOpt_System.SendMessage(os_ColourDepth, CBM_SETCURINDEX, (uintptr_t)1, 0);
		break;
	case 24:  // 24 bit
		cOpt_System.SendMessage(os_ColourDepth, CBM_SETCURSEL, (uintptr_t)2, 0);
		cOpt_System.SendMessage(os_ColourDepth, CBM_SETCURINDEX, (uintptr_t)2, 0);
		break;
	case 32:  // 32 bit
		cOpt_System.SendMessage(os_ColourDepth, CBM_SETCURSEL, (uintptr_t)3, 0);
		cOpt_System.SendMessage(os_ColourDepth, CBM_SETCURINDEX, (uintptr_t)3, 0);
		break;
	}


	// Disable apply for now
	cOpt_System.getWidget(os_Apply)->setEnabled(false);


	// Game
	cOpt_Game.Add( new CLabel("Blood Amount",tLX->clNormalLabel),       Static, 40, 150, 0,0);
	cOpt_Game.Add( new CSlider(5000),                       og_BloodAmount, 175, 147, 210, 20);
	cOpt_Game.Add( new CLabel("Shadows",tLX->clNormalLabel),            Static, 40, 180, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bShadows),     og_Shadows, 280, 180, 17,17);
	cOpt_Game.Add( new CLabel("Particles",tLX->clNormalLabel),          Static, 40, 210, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bParticles),   og_Particles, 280, 210, 17,17);
	cOpt_Game.Add( new CLabel("Classic Rope throw",tLX->clNormalLabel), Static, 40, 240, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bOldSkoolRope),og_OldSkoolRope, 280, 240, 17,17);
	//cOpt_Game.Add( new CLabel("Show worm's health",tLX->clNormalLabel), Static, 40, 270, 0,0);
	//cOpt_Game.Add( new CCheckbox(tLXOptions->bShowHealth),  og_ShowWormHealth, 280, 270, 17,17);
	cOpt_Game.Add( new CLabel("Colorize nicks by teams",tLX->clNormalLabel), Static, 40, 270, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bColorizeNicks),og_ColorizeNicks, 280, 270, 17,17);
	cOpt_Game.Add( new CLabel("Start typing after any key press",tLX->clNormalLabel), Static, 40, 300, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bAutoTyping),og_AutoTyping, 280, 300, 17,17);
	cOpt_Game.Add( new CLabel("Use antialiasing (slow)",tLX->clNormalLabel), Static, 40, 330, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bAntiAliasing),og_Antialiasing, 280, 330, 17,17);
	//cOpt_Game.Add( new CLabel("AI Difficulty",tLX->clNormalLabel), Static, 40, 270, 0,0);
	//cOpt_Game.Add( new CSlider(3), og_AIDifficulty,   175, 267, 100, 20);
/*	cOpt_Game.Add( new CLabel("Enable mouse control (Player 1)",tLX->clNormalLabel), Static, 40, 420, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bMouseAiming),og_MouseAiming, 280, 420, 17,17); */
	cOpt_Game.Add( new CLabel("Log my game results",tLX->clNormalLabel), Static, 40, 390, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bMatchLogging),og_MatchLogging, 280, 390, 17,17);

	cOpt_Game.Add( new CLabel("Network antilag prediction",tLX->clNormalLabel), Static, 40, 360, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bAntilagMovementPrediction),og_AntilagMovementPrediction, 280, 360, 17,17);

/*	cOpt_Game.Add( new CLabel("Shake screen on explosions",tLX->clNormalLabel), Static, 330, 180, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bScreenShaking),     og_ScreenShaking, 550, 180, 17,17); */
	
	cOpt_Game.Add( new CLabel("Damage popups above worms",tLX->clNormalLabel), Static, 330, 210, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bDamagePopups),     og_DamagePopups, 550, 210, 17,17);

	cOpt_Game.Add( new CLabel("Colorize damage popups by worm",tLX->clNormalLabel), Static, 330, 240, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bColorizeDamageByWorm),     og_ColorizeDamageByWorm, 550, 240, 17,17);

/*
	cOpt_Game.Add( new CLabel("Allow mouse control (Server)",tLX->clNormalLabel), Static, 330, 360, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bAllowMouseAiming),og_AllowMouseAiming, 550, 360, 17,17);

	cOpt_Game.Add( new CLabel("Allow strafing (Server)",tLX->clNormalLabel), Static, 330, 390, 0,0);
	cOpt_Game.Add( new CCheckbox(&tLXOptions->bAllowStrafing), Static, 550, 390, 17,17);
*/

	// TODO: Fix cSlider so it's value thing doesn't take up a square of 100x100 pixels.

	// Set the values
	cOpt_Game.SendMessage( og_BloodAmount,  SLM_SETVALUE, tLXOptions->iBloodAmount, 0);
	//cOpt_Game.SendMessage( og_AIDifficulty, SLM_SETVALUE, tLXOptions->iAIDifficulty, 0);


	return true;
}


bool Menu_StartWithSysOptionsMenu(void*) {
	bSkipStart = true;
	Menu_OptionsInitialize();
	OptionsMode = 2;
	return true;
}

///////////////////
// Called when the upload speed test finishes
void Menu_OptionsUpdateUpload(float speed)
{
	tLXOptions->iMaxUploadBandwidth = (int)speed / 2;  // HINT: we set only a half of the maximum bandwidth not to block the line with upload only

	// Get the textbox
	CTextbox *txt = (CTextbox *)cOpt_System.getWidget(os_NetworkUploadBandwidth);
	if (!txt)
		return;

	// Set the new value
	txt->setText(itoa((int)speed));

	// Set the network speed to LAN if appropriate
	if (tLXOptions->iMaxUploadBandwidth >= 7500)  {
		tLXOptions->iNetworkSpeed = NST_LAN;
		CCombobox *cmb = (CCombobox *)cOpt_System.getWidget(os_NetworkSpeed);
		if (cmb)
			cmb->setCurItem(cmb->getItem(NST_LAN));
	}
}


///////////////////
// Options main frame
void Menu_OptionsFrame()
{
	mouse_t		*Mouse = GetMouse();
	gui_event_t *ev = NULL;
	int			val;

	CCheckbox	*c,*c2;
	//CSlider		*s;

	//DrawImageAdv(VideoPostProcessor::videoSurface(), tMenu->bmpBuffer,  180,110,  180,110,  300,30);
	//DrawImageAdv(VideoPostProcessor::videoSurface(), tMenu->bmpBuffer, 20,140, 20,140, 620,340);


	// Process the top buttons
	TopButtons[OptionsMode].MouseOver(Mouse);
	SetGameCursor(CURSOR_ARROW); // Hack: button changed the cursor to hand, we need to change it back
	for(int i=op_Controls;i<=op_System;i++) {

		TopButtons[i].Draw(VideoPostProcessor::videoSurface().get());

		if(i==OptionsMode || bSpeedTest)
			continue;

		if(TopButtons[i].InBox(Mouse->X,Mouse->Y)) {
			TopButtons[i].MouseOver(Mouse);
			if(Mouse->Up) {
                DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, 20,140, 20,140, 620,340);
				OptionsMode = i;
				PlaySoundSample(sfxGeneral.smpClick);
			}
		}
	}

	// Process the gui layout
	ev = bSpeedTest ? NULL : cOptions.Process();
	cOptions.Draw(VideoPostProcessor::videoSurface().get());

	if(ev) {

		switch(ev->iControlID) {

			// Back button
			case op_Back:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Shutdown & save
					Menu_OptionsShutdown();
					tLXOptions->SaveToDisc();

					// Leave
					PlaySoundSample(sfxGeneral.smpClick);
					Menu_MainInitialize();
					return;
				}
				break;
		}
	}


	if(OptionsMode == 0) {

		// Controls
		ev = bSpeedTest ? NULL : cOpt_Controls.Process();
		cOpt_Controls.Draw(VideoPostProcessor::videoSurface().get());

		if(ev) {

			if(ev->cWidget->getType() == wid_Inputbox) {

				if(ev->iEventMsg == INB_MOUSEUP) {

					int ply = 0;
					if(ev->iControlID >= oc_Ply2_Up && ev->iControlID <= oc_Ply2_Strafe)
						ply = 1;
					if(ev->iControlID >= oc_Gen_Chat)
						ply = -1;

					// Get an input
					CInputbox *b = (CInputbox *)ev->cWidget;
					Menu_OptionsWaitInput(ply, b->getName(), b);

					tLX->setupInputs(); // just resetup all
				}
			}
		}
	}


	if(OptionsMode == 1) {

		// Game
		ev = bSpeedTest ? NULL : cOpt_Game.Process();
		cOpt_Game.Draw(VideoPostProcessor::videoSurface().get());

		val = (int)cOpt_Game.SendMessage(og_BloodAmount, SLM_GETVALUE, (uintptr_t)0, 0);
		//s = (CSlider *)cOpt_Game.getWidget(og_BloodAmount);
        DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, 385,140, 385,140, 70,40);
		tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(),385, 148, tLX->clNormalLabel, itoa(val)+"%");

		//val = cOpt_Game.SendMessage(og_AIDifficulty, SLM_GETVALUE, 0, 0);
        //DrawImageAdv(VideoPostProcessor::videoSurface(), tMenu->bmpBuffer, 285,260, 285,260, 100,50);
		//tLX->cFont.Draw(VideoPostProcessor::videoSurface(),285, 268, tLX->clNormalLabel,Difficulties[val]);



		if(ev) {

			switch(ev->iControlID) {

				// Blood amount
				case og_BloodAmount:
					if(ev->iEventMsg == SLD_CHANGE) {
						val = (int)cOpt_Game.SendMessage(og_BloodAmount, SLM_GETVALUE, (uintptr_t)0, 0);
						tLXOptions->iBloodAmount = val;
					}
					break;

				// Shadows
				case og_Shadows:
					if(ev->iEventMsg == CHK_CHANGED) {
						c = (CCheckbox *)cOpt_Game.getWidget(og_Shadows);
						tLXOptions->bShadows = c->getValue();
					}
					break;

				// Particles
				case og_Particles:
					if(ev->iEventMsg == CHK_CHANGED) {
						c = (CCheckbox *)cOpt_Game.getWidget(og_Particles);
						tLXOptions->bParticles = c->getValue();
					}
					break;

				// AI Difficulty
				/*case og_AIDifficulty:
					if(ev->iEventMsg == SLD_CHANGE) {
						val = cOpt_Game.SendMessage(og_AIDifficulty, SLM_GETVALUE, 0, 0);
						tLXOptions->iAIDifficulty = val;
					}
					break;*/

				// Old skool rope throw
				case og_OldSkoolRope:
					if(ev->iEventMsg == CHK_CHANGED) {
						tLXOptions->bOldSkoolRope = cOpt_Game.SendMessage(og_OldSkoolRope,CKM_GETCHECK,(uintptr_t)0,0) != 0;
					}
					break;

				// Show the worm's health below name
/*				case og_ShowWormHealth:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->iShowHealth = cOpt_Game.SendMessage(og_ShowWormHealth, CKM_GETCHECK, (uintptr_t)0, 0);
					break;*/

				// TDM nick colorizing
				case og_ColorizeNicks:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bColorizeNicks = cOpt_Game.SendMessage(og_ColorizeNicks, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;

				// Auto typing
				case og_AutoTyping:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bAutoTyping = cOpt_Game.SendMessage(og_AutoTyping, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;

				// Antialiasing
				case og_Antialiasing:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bAntiAliasing = cOpt_Game.SendMessage(og_Antialiasing, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;

				// Mouse aiming
				case og_MouseAiming:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bMouseAiming = cOpt_Game.SendMessage(og_MouseAiming, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;
/*
				case og_AllowMouseAiming:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bAllowMouseAiming = cOpt_Game.SendMessage(og_AllowMouseAiming, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;
*/

				// Match logging
				case og_MatchLogging:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bMatchLogging = cOpt_Game.SendMessage(og_MatchLogging, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;

				case og_AntilagMovementPrediction:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bAntilagMovementPrediction = cOpt_Game.SendMessage(og_AntilagMovementPrediction, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;
					
		/*		case og_ScreenShaking:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bScreenShaking = cOpt_Game.SendMessage(og_ScreenShaking, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break; */

				case og_DamagePopups:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bDamagePopups = cOpt_Game.SendMessage(og_DamagePopups, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;
					
				case og_ColorizeDamageByWorm:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bColorizeDamageByWorm = cOpt_Game.SendMessage(og_ColorizeDamageByWorm, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;
			}
		}

	}


	// Process the different pages
	if(OptionsMode == 2) {

		// Fullscreen value
		c = (CCheckbox *)cOpt_System.getWidget(os_Fullscreen);
		bool fullscr = c->getValue();
		// OpenGL accel value
		c2 = (CCheckbox *)cOpt_System.getWidget(os_OpenGL);
		bool opengl = c2->getValue () != 0;
		// Color depth
		int cdepth = ((CCombobox *)cOpt_System.getWidget(os_ColourDepth))->getSelectedIndex();
		switch (cdepth)  {
		case 0: cdepth = 0; break;
		case 1: cdepth = 16; break;
		case 2: cdepth = 24; break;
		case 3: cdepth = 32; break;
		default: cdepth = 16;
		}

		// FIXME: WARNING! If OpenGL acceleration is not supported,
		//                 this could lead to a crash!



		// System
		ev = bSpeedTest ? NULL : cOpt_System.Process();
		cOpt_System.Draw(VideoPostProcessor::videoSurface().get());

		if(ev) {

			switch(ev->iControlID) {

				// Apply
				case os_Apply:
					if(ev->iEventMsg == BTN_CLICKED) {

						bool restart = (tLXOptions->bOpenGL != opengl) || (tLXOptions->iColourDepth != cdepth);

						// Set to fullscreen / OpenGL / change colour depth
						tLXOptions->bFullscreen = fullscr;
						tLXOptions->bOpenGL = opengl;
						tLXOptions->iColourDepth = cdepth;
						bool fail = false;

						CTextbox *t = (CTextbox *)cOpt_System.getWidget(os_MaxFPS);
						tLXOptions->nMaxFPS = from_string<int>(t->getText(), fail);
						tLXOptions->nMaxFPS = fail ? 0 : MAX(0, tLXOptions->nMaxFPS);
						t->setText(itoa(tLXOptions->nMaxFPS));
						PlaySoundSample(sfxGeneral.smpClick);

						if(restart) {
							// HINT: after changing ogl or bpp, if the user changes some other gfx setting like fullscreen, there are mess ups of course
							// to workaround the problems, we simply restart the game here
							game.state = Game::S_Quit; // quit
							Menu_OptionsShutdown(); // cleanup for this menu
							bRestartGameAfterQuit = true; // set restart-flag
							startFunction = &Menu_StartWithSysOptionsMenu; // set function which loads this menu after start
							return;

						} else {
							// Set the new video mode
							doSetVideoModeInMainThread();

							Menu_RedrawMouse(true);
							EnableSystemMouseCursor(false);
						}
					}
					break;

				// Sound on/off
				case os_SoundOn:
					if(ev->iEventMsg == CHK_CHANGED) {

						bool old = tLXOptions->bSoundOn;

						c = (CCheckbox *)cOpt_System.getWidget(os_SoundOn);
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
						CSlider *s = (CSlider *)cOpt_System.getWidget(os_SoundVolume);
						
						if(tLXOptions->iSoundVolume != s->getValue()) {
						  // Volume has been changed so set new volume
						  tLXOptions->iSoundVolume = s->getValue();
						  SetSoundVolume( tLXOptions->iSoundVolume );
						  
						  // We want the user to know how loud the new volume is
						  PlaySoundSample(sfxGeneral.smpNotify);
						}
					}
					break;

				// Show FPS
				case os_ShowFPS:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bShowFPS = cOpt_System.SendMessage(os_ShowFPS, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;

				// Logging
				case os_LogConvos:
					if(ev->iEventMsg == CHK_CHANGED)  {
						tLXOptions->bLogConvos = cOpt_System.SendMessage(os_LogConvos, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
						if (convoLogger)  {
							if (tLXOptions->bLogConvos)
								convoLogger->startLogging();
							else
								convoLogger->endLogging();
						}
					}
					break;

				// Show ping
				case os_ShowPing:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bShowPing = cOpt_System.SendMessage(os_ShowPing, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					break;

				// Use Ip To Country
				case os_UseIpToCountry:
					if(ev->iEventMsg == CHK_CHANGED)  {
						tLXOptions->bUseIpToCountry = cOpt_System.SendMessage(os_UseIpToCountry, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
						if (tLXOptions->bUseIpToCountry && !tIpToCountryDB->Loaded())  {
							tIpToCountryDB->LoadDBFile(IP_TO_COUNTRY_FILE);
						}
					}
					break;

				case os_ShowCountryFlags:
					if(ev->iEventMsg == CHK_CHANGED)  {
						tLXOptions->bShowCountryFlags = cOpt_System.SendMessage(os_ShowCountryFlags, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					}
					break;

				// Test bandwidth
				case os_TestBandwidth:  {
					if (ev->iEventMsg == BTN_CLICKED)  {
						bSpeedTest = true;
						Menu_SpeedTest_Initialize();
					}
				} break;
				
				case os_CheckForUpdates:
					if(ev->iEventMsg == CHK_CHANGED)  {
						tLXOptions->bCheckForUpdates = cOpt_System.SendMessage(os_CheckForUpdates, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
					}
					break;
				
			}
		}


		// Get the values
		CTextbox *t = (CTextbox *)cOpt_System.getWidget(os_NetworkPort);
		tLXOptions->iNetworkPort = atoi(t->getText());
		t = (CTextbox *)cOpt_System.getWidget(os_HttpProxy);
		tLXOptions->sHttpProxy = t->getText();

		tLXOptions->iNetworkSpeed = (int)cOpt_System.SendMessage(os_NetworkSpeed, CBM_GETCURINDEX,(uintptr_t)0,0);
		tLXOptions->bCheckBandwidthSanity = cOpt_System.SendMessage(os_NetworkUploadCheck, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
		
		cOpt_System.getWidget( os_NetworkUploadBandwidth )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );
		cOpt_System.getWidget( os_NetworkUploadBandwidthLabel )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );
		cOpt_System.getWidget( os_TestBandwidth )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );
		if( cOpt_System.getWidget( os_NetworkUploadBandwidth )->getEnabled() )
			tLXOptions->iMaxUploadBandwidth = atoi( ((CTextbox *)cOpt_System.getWidget( os_NetworkUploadBandwidth ))->getText().c_str() );
		if( tLXOptions->iMaxUploadBandwidth <= 0 )
			tLXOptions->iMaxUploadBandwidth = 50000;

		tLXOptions->iScreenshotFormat = (int)cOpt_System.SendMessage(os_ScreenshotFormat, CBM_GETCURINDEX,(uintptr_t)0,0);

		// FPS and fullscreen
		t = (CTextbox *)cOpt_System.getWidget(os_MaxFPS);

		if(cdepth != tLXOptions->iColourDepth || opengl != tLXOptions->bOpenGL || fullscr != tLXOptions->bFullscreen || atoi(t->getText()) != tLXOptions->nMaxFPS) {
			cOpt_System.getWidget(os_Apply)->setEnabled(true);
			cOpt_System.getWidget(os_Apply)->Draw( VideoPostProcessor::videoSurface().get() );
        } else {
			cOpt_System.getWidget(os_Apply)->setEnabled(false);
			cOpt_System.getWidget(os_Apply)->redrawBuffer();
        }
	}

	// Process the speed test window
	if (bSpeedTest)  {
		if (Menu_SpeedTest_Frame())  {
			// Finished
			Menu_OptionsUpdateUpload(Menu_SpeedTest_GetSpeed());
			Menu_SpeedTest_Shutdown();
			bSpeedTest = false;
		}
	}


	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());
}


///////////////////
// Process an input box waiting thing
// ply=-1 : general ; ply>=0 : normal player
void Menu_OptionsWaitInput(int ply, const std::string& name, CInputbox *b)
{
	mouse_t *Mouse = GetMouse();

	// Draw the back buffer
	cOptions.Draw(tMenu->bmpBuffer.get());
	cOpt_Controls.Draw(tMenu->bmpBuffer.get());

	Menu_DrawBox(tMenu->bmpBuffer.get(), 210, 170, 430, 310);
	//DrawImageAdv(tMenu->bmpBuffer, tMenu->bmpMainBack, 212,172, 212,172, 217,137);
    DrawRectFill(tMenu->bmpBuffer.get(), 212,172,429,309,tLX->clDialogBackground);

	tLX->cFont.DrawCentre(tMenu->bmpBuffer.get(),320,180,Color(128,200,255),"Input for:");
	tLX->cFont.DrawCentre(tMenu->bmpBuffer.get(),320,205,Color(255,255,255),name);

	tLX->cFont.DrawCentre(tMenu->bmpBuffer.get(),320,270,Color(255,255,255),"Press any key/mouse");
	tLX->cFont.DrawCentre(tMenu->bmpBuffer.get(),320,285,Color(128,128,128),"(Escape to cancel)");

	TopButtons[OptionsMode].MouseOver(Mouse);
	for(ushort i=0;i<3;i++) {
		TopButtons[i].Draw(tMenu->bmpBuffer.get());
	}


	Menu_RedrawMouse(true);

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
		if(WasKeyboardEventHappening(SDLK_ESCAPE))
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
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
	Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_OPTIONS);

	Menu_RedrawMouse(true);
}


///////////////////
// Shutdown the options menu
void Menu_OptionsShutdown()
{
	cOptions.Shutdown();
	cOpt_Controls.Shutdown();
	cOpt_System.Shutdown();
	cOpt_Game.Shutdown();

	if (bSpeedTest)
		Menu_SpeedTest_Shutdown();
}

}; // namespace DeprecatedGUI
