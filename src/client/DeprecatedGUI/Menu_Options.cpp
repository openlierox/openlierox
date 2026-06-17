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
#include "CScriptableVars.h"
#include "DeprecatedGUI/CAnimation.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CCheckbox.h"
#include "DeprecatedGUI/CTextbox.h"
#include "DeprecatedGUI/CSlider.h"
#include "DeprecatedGUI/CLine.h"
#include "IpToCountryDB.h"
#include "ConversationLogger.h"
#include "TouchControls.h"


namespace DeprecatedGUI {

int OptionsMode = 0;
CGuiLayout	cOptions;
CGuiLayout	cOpt_Controls;        // Player 1 controls   (sub-tab 0)
CGuiLayout	cOpt_Controls2;       // Player 2 controls   (sub-tab 1)
CGuiLayout	cOpt_ControlsGen;     // General controls    (sub-tab 2)
CGuiLayout	cOpt_ControlsTouch;   // Touch-screen layout (sub-tab 3, only shown on touch devices)
CGuiLayout	cOpt_System;
CGuiLayout	cOpt_Game;

// The Controls screen is split into sub-tabs so each section gets the
// full width. ControlsSubTab selects which one is shown. The
// "Touchscreen" tab is only visible on platforms with a touch device
// (see IsControlsTabVisible).
enum {
	ct_Player1 = 0,
	ct_Player2,
	ct_General,
	ct_Touchscreen,
	ct__Count
};
int ControlsSubTab = ct_Player1;
static const char* ControlsTabNames[ct__Count] = { "Player 1", "Player 2", "General", "Touchscreen" };

// True if the given tab should appear in the tab bar. All control tabs,
// including Touchscreen, are always available — the touch layout can be
// inspected/picked anywhere, which is useful for debugging or authoring a
// new layout even on a machine without a touch device.
static bool IsControlsTabVisible(int /*t*/) {
	return true;
}
// Tab header hit-boxes, filled in when drawn (see Menu_OptionsDrawControlsTabs).
static SDL_Rect ControlsTabRects[ct__Count];
// "Reset defaults" button hit-box, filled in when drawn.
static SDL_Rect ResetBtnRect;

// Touch-screen tab state: list of available layouts (re-fetched at
// Menu_OptionsInitialize, hence whenever the user re-enters the menu)
// plus the per-tile hit-boxes filled in by Menu_OptionsDrawTouchTiles.
static std::vector<TouchControls::LayoutInfo> gTouchLayoutInfos;
static std::vector<SDL_Rect>                  gTouchLayoutTileRects;
// Hit-box of the "Double shoot to switch weapon" checkbox shown below the
// layout tiles on the Touchscreen sub-tab (filled in by Menu_OptionsDrawTouchOptions).
static SDL_Rect gTouchDoubleShootRect = {0, 0, 0, 0};

// The On / Off / Auto selector for Game.TouchscreenControls, shown on the top
// row of the Touchscreen sub-tab (above the layout tiles). The labels map to
// the tri-state option value; hit-boxes are filled in when drawn.
static const struct { const char* label; const char* value; } kTouchModeChoices[] = {
	{ "Auto-detect", "auto" }, { "Always on", "true" }, { "Always off", "false" },
};
static const int kTouchModeCount = (int)(sizeof(kTouchModeChoices) / sizeof(kTouchModeChoices[0]));
static SDL_Rect gTouchModeRects[kTouchModeCount];

// Which choice the current option value corresponds to (for highlighting).
static int Menu_CurrentTouchMode() {
	const std::string& v = tLXOptions->sTouchscreenControls;
	// Index must match kTouchModeChoices order: 0=Auto-detect, 1=Always on, 2=Always off.
	if(stringcaseequal(v, "true")  || stringcaseequal(v, "yes") || stringcaseequal(v, "on")  || v == "1") return 1;
	if(stringcaseequal(v, "false") || stringcaseequal(v, "no")  || stringcaseequal(v, "off") || v == "0") return 2;
	return 0; // "auto" or anything unrecognised
}
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
	os_UseIpToCountry,
	os_HttpProxy,
	os_ShowFPS,
	os_OpenGL,
	os_ShowPing,
	os_LogConvos,
	os_ScreenshotFormat,
	os_MaxFPS,
	os_Apply,
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
	og_AlwaysCenterWorm,
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

// General controls shown on the "General" sub-tab: display label, index into
// tLXOptions->sGeneralControls, and the row's control id.
struct GenCtrl { const char* label; int sin; int id; };
static const GenCtrl GeneralControlsList[] = {
	{ "Chat",              SIN_CHAT,          oc_Gen_Chat },
	{ "Scoreboard",        SIN_SCORE,         oc_Gen_Score },
	{ "Health Bar",        SIN_HEALTH,        oc_Gen_Health },
	{ "Current Settings",  SIN_SETTINGS,      oc_Gen_CurSettings },
	{ "Take Screenshot",   SIN_SCREENSHOTS,   oc_Gen_TakeScreenshot },
	{ "Viewport Manager",  SIN_VIEWPORTS,     oc_Gen_ViewportManager },
	{ "Switch Video Mode", SIN_SWITCHMODE,    oc_Gen_SwitchMode },
	{ "Toggle Top Bar",    SIN_TOGGLETOPBAR,  oc_Gen_ToggleTopBar },
	{ "Teamchat",          SIN_TEAMCHAT,      oc_Gen_TeamChat },
	{ "IRC chat window",   SIN_IRCCHAT,       oc_Gen_IrcChat },
	{ "Toggle console",    SIN_CONSOLETOGGLE, oc_Gen_ConsoleToggle },
};
static const int kNumGeneralControls = sizeof(GeneralControlsList) / sizeof(GeneralControlsList[0]);


// Each control can hold several bindings (a comma-separated list). On the
// Controls screen we show one input box per binding slot, side by side, so
// the keyboard (slot 0) and gamepad (slot 1) bindings can be edited
// independently. This is how many slot-columns we display per control.
static const int kControlSlots = 2;
static const char* ControlSlotNames[kControlSlots] = { "Keyboard", "Gamepad" };
// Control-id offset added per slot column so each box has a distinct id.
static const int kSlotIdStride = 100;

// Return the binding at the given 0-based slot of a (possibly comma-separated)
// control value, or "" if that slot is empty.
static std::string Menu_ControlSlotText(const std::string& full, int slot)
{
	if(slot < 0) return full;
	std::vector<std::string> parts = explode(full, ",");
	if(slot >= (int)parts.size()) return "";
	return Trimmed(parts[slot]);
}

// Return a copy of full with the binding at the given slot replaced by value,
// keeping the other slots in place. Trailing empty slots are dropped.
static std::string Menu_ControlSetSlot(const std::string& full, int slot, const std::string& value)
{
	if(slot < 0) return value;
	std::vector<std::string> parts = explode(full, ",");
	for(size_t k = 0; k < parts.size(); k++) parts[k] = Trimmed(parts[k]);
	while((int)parts.size() <= slot) parts.push_back("");
	parts[slot] = Trimmed(value);
	while(!parts.empty() && parts.back().empty()) parts.pop_back();
	std::string res;
	for(size_t k = 0; k < parts.size(); k++) {
		if(k) res += ", ";
		res += parts[k];
	}
	return res;
}

// Input-box width on the Controls screen, sized so long bindings like
// "j1_button_south" fit. The box widens via a 3-slice in CInputbox::Draw.
// Column positions account for the wider boxes (box1 300-399, box2 420-519).
static const int kControlBoxW = 99;
static const int kControlSlotX[kControlSlots] = { 300, 420 };
static const int kControlLabelX = 140;

// Adds the "Keyboard" / "Gamepad" column headers to a controls layout,
// centered over their columns (the boxes draw their text centered too).
static void Menu_AddControlHeaders(CGuiLayout& layout, int y)
{
	for(int s = 0; s < kControlSlots; s++) {
		const int colCentre = kControlSlotX[s] + kControlBoxW / 2;
		const int textW = tLX->cFont.GetWidth(ControlSlotNames[s]);
		layout.Add( new CLabel(ControlSlotNames[s], tLX->clSubHeading), Static, colCentre - textW / 2, y, 0, 0);
	}
}

// Adds one control row: a name label plus one input box per binding slot.
// sinIndex is the control's index into its options array; fullValue is its
// current (possibly comma-separated) value; baseId is the row's control id.
static void Menu_AddControlRow(CGuiLayout& layout, const std::string& label, int sinIndex,
                               const std::string& fullValue, int baseId, int y)
{
	layout.Add( new CLabel(label, tLX->clNormalLabel), Static, kControlLabelX, y, 0, 0);
	for(int s = 0; s < kControlSlots; s++) {
		const std::string boxName = label + " (" + ControlSlotNames[s] + ")";
		layout.Add( new CInputbox(sinIndex, Menu_ControlSlotText(fullValue, s), tMenu->bmpInputbox, boxName, s),
		            baseId + s * kSlotIdStride, kControlSlotX[s], y, kControlBoxW, 17 );
	}
}

// Defined further down; forward-declared so Menu_OptionsInitialize can call it.
static void Menu_OptionsRefreshTouchTiles();

///////////////////
// Initialize the options
bool Menu_OptionsInitialize()
{
	tMenu->iMenuType = MNU_OPTIONS;
	OptionsMode = 0;
    int i;

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

	cOpt_Controls2.Shutdown();
	cOpt_Controls2.Initialize();

	cOpt_ControlsGen.Shutdown();
	cOpt_ControlsGen.Initialize();

	cOpt_ControlsTouch.Shutdown();
	cOpt_ControlsTouch.Initialize();

	cOpt_Game.Shutdown();
	cOpt_Game.Initialize();


	// Add the controls
	cOptions.Add( new CButton(BUT_BACK, tMenu->bmpButtons), op_Back, 25,440, 50,15);


	// Controls — Player 1 / Player 2 each get their own sub-tab. Every control
	// shows one input box per binding slot (Keyboard, Gamepad) so the slots can
	// be edited independently.
	{
		int y = 205;
		Menu_AddControlHeaders(cOpt_Controls,  y - 20);
		Menu_AddControlHeaders(cOpt_Controls2, y - 20);
		for(i=0;i<9;i++,y+=26) {
			Menu_AddControlRow(cOpt_Controls,  InputNames[i], SIN_UP+i,
			                   tLXOptions->sPlayerControls[0][SIN_UP+i], oc_Ply1_Up+i, y);
			Menu_AddControlRow(cOpt_Controls2, InputNames[i], SIN_UP+i,
			                   tLXOptions->sPlayerControls[1][SIN_UP+i], oc_Ply2_Up+i, y);
		}
	}

	// Controls — General (third sub-tab), same per-slot columns
	{
		int y = 200;
		Menu_AddControlHeaders(cOpt_ControlsGen, y - 18);
		for(int g=0; g<kNumGeneralControls; g++, y+=22) {
			Menu_AddControlRow(cOpt_ControlsGen, GeneralControlsList[g].label, GeneralControlsList[g].sin,
			                   tLXOptions->sGeneralControls[GeneralControlsList[g].sin], GeneralControlsList[g].id, y);
		}
	}

	// Controls — Touch screen (fourth sub-tab). Layouts are picked from
	// preview tiles drawn directly onto the menu — see
	// Menu_OptionsRefreshTouchTiles / Menu_OptionsDrawTouchTiles. The
	// CGuiLayout itself stays empty for this tab; we only draw the tiles
	// on top of the normal Process()/Draw() of the (empty) cOpt_ControlsTouch.
	Menu_OptionsRefreshTouchTiles();

	// The "Reset defaults" button is drawn manually (see
	// Menu_OptionsDrawResetButton) so it can show that exact text — the atlas
	// buttons only offer a generic "Default" graphic.



	// System
	const Color lineCol = tLX->clLine; //tLX->clHeading.derived(0,0,0,-200);
	const int starty = 130;
	int y = starty;
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

	cOpt_Game.Add( new CLabel("Always center worm",tLX->clNormalLabel), Static, 330, 270, 0,0);
	cOpt_Game.Add( new CCheckbox(tLXOptions->bAlwaysCenterWorm),     og_AlwaysCenterWorm, 550, 270, 17,17);

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
// Returns the gui layout for the currently selected Controls sub-tab
static CGuiLayout& Menu_OptionsControlsLayout()
{
	switch(ControlsSubTab) {
		case ct_Player2:     return cOpt_Controls2;
		case ct_General:     return cOpt_ControlsGen;
		case ct_Touchscreen: return cOpt_ControlsTouch;
		default:             return cOpt_Controls;
	}
}

///////////////////
// Draws the Controls sub-tab headers (Player 1 / Player 2 / General) and
// records their hit-boxes in ControlsTabRects.
static void Menu_OptionsDrawControlsTabs(SDL_Surface* dest)
{
	const int tabY = 150;
	const int gap = 35;
	const int h = tLX->cFont.GetHeight();
	int x = 60;
	for(int t = 0; t < ct__Count; t++) {
		if(!IsControlsTabVisible(t)) {
			ControlsTabRects[t].w = 0;  // marks the tab as not hit-testable
			continue;
		}
		const std::string name = ControlsTabNames[t];
		const int w = tLX->cFont.GetWidth(name);

		ControlsTabRects[t].x = x;
		ControlsTabRects[t].y = tabY;
		ControlsTabRects[t].w = w;
		ControlsTabRects[t].h = h;

		const bool active = (t == ControlsSubTab);
		tLX->cFont.Draw(dest, x, tabY, active ? tLX->clHeading : tLX->clNormalLabel, name);
		if(active)
			DrawRectFill(dest, x, tabY + h + 1, x + w, tabY + h + 2, tLX->clHeading);

		x += w + gap;
	}
}

///////////////////
// Re-fetch the available touch-screen layouts (file name + display name
// + preview surface) from share/gamedir/touchscreen/. Called when the
// options menu is initialised so opening it picks up any newly-added
// YAML files without restarting the game.
static void Menu_OptionsRefreshTouchTiles()
{
	gTouchLayoutInfos = TouchControls::GetAvailableLayouts();
	gTouchLayoutTileRects.assign(gTouchLayoutInfos.size(), SDL_Rect{0, 0, 0, 0});
}

///////////////////
// Draw the "Touch controls: On / Off / Auto" selector on the top row of the
// Touchscreen sub-tab (above the layout tiles). Records each choice's hit-box
// in gTouchModeRects for click handling in Menu_OptionsFrame.
static void Menu_OptionsDrawTouchMode(SDL_Surface* dest)
{
	mouse_t* Mouse = GetMouse();
	// Sit roughly midway between the sub-tab bar (y=150) and the tiles (now
	// y=235), with generous padding above and below so the row isn't crowded.
	const int y = 190;
	const int h = tLX->cFont.GetHeight();

	const std::string label = "Use touchscreen controls:";
	tLX->cFont.Draw(dest, kControlLabelX, y, tLX->clNormalLabel, label);

	const int current = Menu_CurrentTouchMode();
	int x = kControlLabelX + tLX->cFont.GetWidth(label) + 16;
	for(int i = 0; i < kTouchModeCount; i++) {
		const std::string text = kTouchModeChoices[i].label;
		const int w = tLX->cFont.GetWidth(text);

		SDL_Rect r{(Sint16)x, (Sint16)y, (Uint16)w, (Uint16)h};
		gTouchModeRects[i] = r;

		const bool selected = (i == current);
		const bool hover = (Mouse->X >= x && Mouse->X <= x + w &&
		                    Mouse->Y >= y && Mouse->Y <= y + h);
		Color col = selected ? tLX->clHeading : (hover ? tLX->clHeading : tLX->clNormalLabel);
		tLX->cFont.Draw(dest, x, y, col, text);
		// Underline the selected choice so the active value is obvious.
		if(selected)
			DrawRectFill(dest, x, y + h, x + w, y + h + 1, tLX->clHeading);

		x += w + 24;
	}
}

// Draw the row of layout tiles on the "Touchscreen" sub-tab. Each tile
// shows the layout's preview PNG (or a "No preview" placeholder), the
// `name:` field below, and a thicker border around the currently-selected
// layout. Records each tile's screen rect in gTouchLayoutTileRects so
// click handling in Menu_OptionsFrame can resolve hits.
static void Menu_OptionsDrawTouchTiles(SDL_Surface* dest)
{
	const int n = (int)gTouchLayoutInfos.size();
	if(n == 0) {
		const std::string msg = "No layouts found in share/gamedir/touchscreen/";
		tLX->cFont.Draw(dest, kControlLabelX, 250, tLX->clNormalLabel, msg);
		return;
	}

	const int tileW = 200;
	const int tileH = 150;
	const int gap   = 20;
	const int topY  = 235; // leave padding below the "Use touchscreen controls:" row
	const int totalW = n * tileW + (n - 1) * gap;
	int x = (640 - totalW) / 2;
	if(x < 20) x = 20;  // many tiles — fall back to a left margin

	for(int i = 0; i < n; i++) {
		const TouchControls::LayoutInfo& info = gTouchLayoutInfos[i];
		const bool selected = (info.fileName == tLXOptions->sTouchscreenLayout);

		SDL_Rect r{(Sint16)x, (Sint16)topY, (Uint16)tileW, (Uint16)tileH};
		gTouchLayoutTileRects[i] = r;

		// Preview, or "No preview" placeholder.
		if(info.preview.get()) {
			DrawImageResampledAdv(dest, info.preview, 0, 0, r.x, r.y,
			                      info.preview.get()->w, info.preview.get()->h,
			                      r.w, r.h);
		} else {
			DrawRectFillA(dest, r.x, r.y, r.x + r.w, r.y + r.h, Color(60, 60, 60), 200);
			const std::string placeholder = "No preview";
			const int tw = tLX->cFont.GetWidth(placeholder);
			const int th = tLX->cFont.GetHeight();
			tLX->cFont.Draw(dest, r.x + (r.w - tw) / 2, r.y + (r.h - th) / 2,
			                tLX->clNormalLabel, placeholder);
		}

		// Border. Selected tile gets a thick highlight ring.
		const Color outline = selected ? tLX->clHeading : Color(120, 120, 120);
		DrawRect(dest, r.x - 1, r.y - 1, r.x + r.w,     r.y + r.h,     outline);
		if(selected) {
			DrawRect(dest, r.x - 2, r.y - 2, r.x + r.w + 1, r.y + r.h + 1, outline);
			DrawRect(dest, r.x - 3, r.y - 3, r.x + r.w + 2, r.y + r.h + 2, outline);
		}

		// Display name centred below the tile.
		const int nameW = tLX->cFont.GetWidth(info.displayName);
		const int nameX = r.x + (r.w - nameW) / 2;
		const int nameY = r.y + r.h + 8;
		tLX->cFont.Draw(dest, nameX, nameY,
		                selected ? tLX->clHeading : tLX->clNormalLabel,
		                info.displayName);

		x += tileW + gap;
	}
}

///////////////////
// Draw the "Double shoot to switch weapon" checkbox below the layout tiles on
// the Touchscreen sub-tab. Drawn manually (like the rest of this tab) so it
// can sit under the layout selection. Records its hit-box in
// gTouchDoubleShootRect for click handling in Menu_OptionsFrame.
static void Menu_OptionsDrawTouchOptions(SDL_Surface* dest)
{
	mouse_t* Mouse = GetMouse();
	// Sit below the layout tiles (tiles end at ~393, plus their name labels).
	const int y = 420;
	const int box = 17;
	const int x = kControlLabelX;

	const bool on = tLXOptions->bTouchscreenDoubleShootToSwitchWeapon;

	// Checkbox square.
	DrawRect(dest, x, y, x + box, y + box, tLX->clNormalLabel);
	if(on)
		DrawRectFill(dest, x + 3, y + 3, x + box - 3, y + box - 3, tLX->clHeading);

	const std::string label = "Double shoot to switch weapon";
	const int labelX = x + box + 8;
	tLX->cFont.Draw(dest, labelX, y + 2, tLX->clNormalLabel, label);

	const int w = (labelX + tLX->cFont.GetWidth(label)) - x;
	gTouchDoubleShootRect = SDL_Rect{(Sint16)x, (Sint16)y, (Uint16)w, (Uint16)box};
}

///////////////////
// Draws the "Reset defaults" button (a bordered text button, so it can show
// that exact label) and records its hit-box in ResetBtnRect.
static void Menu_OptionsDrawResetButton(SDL_Surface* dest)
{
	mouse_t* Mouse = GetMouse();
	const std::string text = "Reset defaults";
	const int textW = tLX->cFont.GetWidth(text);
	const int h = 18;
	// Left-aligned under the action-name column (same x as "Up", "Down", ...).
	const int x = kControlLabelX;
	const int y = 440;

	ResetBtnRect.x = x;
	ResetBtnRect.y = y;
	ResetBtnRect.w = textW;
	ResetBtnRect.h = h;

	// No border: match the borderless clickable text used by the sub-tabs.
	// A hover colour change signals it is clickable.
	const bool hover = (Mouse->X >= x && Mouse->X <= x + textW && Mouse->Y >= y && Mouse->Y <= y + h);
	tLX->cFont.Draw(dest, x, y + 3, hover ? tLX->clHeading : tLX->clNormalLabel, text);
}

///////////////////
// Restore a whole control section (Ply1Controls / Ply2Controls /
// GeneralControls) to its registered default values.
static void Menu_ResetControlSection(const std::string& prefix)
{
	for(CScriptableVars::const_iterator it = CScriptableVars::lower_bound(prefix);
	    it != CScriptableVars::end(); ++it) {
		if(!strStartsWith(it->first, prefix)) break;
		it->second.var.setDefault();
	}
}

// Update the per-slot input boxes of one control row to match the given value.
static void Menu_RefreshControlRow(CGuiLayout& layout, int baseId, const std::string& full)
{
	for(int s = 0; s < kControlSlots; s++) {
		CWidget* w = layout.getWidget(baseId + s * kSlotIdStride);
		if(w && w->getType() == wid_Inputbox)
			((CInputbox*)w)->setText(Menu_ControlSlotText(full, s));
	}
}

// Reset the currently shown Controls sub-tab to its defaults and refresh boxes.
static void Menu_ResetControlsTab(int tab)
{
	if(tab == ct_General) {
		Menu_ResetControlSection("GameOptions.GeneralControls.");
		for(int g = 0; g < kNumGeneralControls; g++)
			Menu_RefreshControlRow(cOpt_ControlsGen, GeneralControlsList[g].id,
			                       tLXOptions->sGeneralControls[GeneralControlsList[g].sin]);
	} else {
		const int ply = (tab == ct_Player2) ? 1 : 0;
		Menu_ResetControlSection(ply == 0 ? "GameOptions.Ply1Controls." : "GameOptions.Ply2Controls.");
		CGuiLayout& layout = (ply == 0) ? cOpt_Controls : cOpt_Controls2;
		const int baseId = (ply == 0) ? oc_Ply1_Up : oc_Ply2_Up;
		for(int i = 0; i < 9; i++)
			Menu_RefreshControlRow(layout, baseId + i, tLXOptions->sPlayerControls[ply][SIN_UP + i]);
	}
	tLX->setupInputs();
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

		if(i==OptionsMode)
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
	ev = cOptions.Process();
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

		// Sub-tabs: Player 1 / Player 2 / General — only one shown at a time
		Menu_OptionsDrawControlsTabs(VideoPostProcessor::videoSurface().get());
		if(Mouse->Up) {
			for(int t = 0; t < ct__Count; t++) {
				if(t == ControlsSubTab) continue;
				if(!IsControlsTabVisible(t)) continue;
				const SDL_Rect& r = ControlsTabRects[t];
				if(r.w == 0) continue;  // tab not drawn this frame
				if(Mouse->X >= r.x && Mouse->X <= r.x + r.w && Mouse->Y >= r.y && Mouse->Y <= r.y + r.h) {
					ControlsSubTab = t;
					// Clear the old sub-tab's widgets from the screen
					DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, 20,140, 20,140, 620,340);
					PlaySoundSample(sfxGeneral.smpClick);
					break;
				}
			}
		}

		// Active sub-tab content
		CGuiLayout& ctrlLayout = Menu_OptionsControlsLayout();
		ev = ctrlLayout.Process();
		ctrlLayout.Draw(VideoPostProcessor::videoSurface().get());

		if(ControlsSubTab == ct_Touchscreen) {
			// Top row: On / Off / Auto selector for Game.TouchscreenControls.
			Menu_OptionsDrawTouchMode(VideoPostProcessor::videoSurface().get());
			if(Mouse->Up) {
				for(int i = 0; i < kTouchModeCount; i++) {
					const SDL_Rect& r = gTouchModeRects[i];
					if(Mouse->X >= r.x && Mouse->X <= r.x + r.w &&
					   Mouse->Y >= r.y && Mouse->Y <= r.y + r.h) {
						const std::string val = kTouchModeChoices[i].value;
						if(!stringcaseequal(tLXOptions->sTouchscreenControls, val)) {
							tLXOptions->sTouchscreenControls = val;
							TouchControls::ReloadLayout();
							PlaySoundSample(sfxGeneral.smpClick);
						}
						break;
					}
				}
			}
			// Layout tiles + click handling. No "Reset defaults" here — the
			// touch-screen tab has nothing to reset.
			Menu_OptionsDrawTouchTiles(VideoPostProcessor::videoSurface().get());
			if(Mouse->Up) {
				for(size_t i = 0; i < gTouchLayoutTileRects.size(); ++i) {
					const SDL_Rect& r = gTouchLayoutTileRects[i];
					if(r.w == 0) continue;
					if(Mouse->X >= r.x && Mouse->X <= r.x + r.w &&
					   Mouse->Y >= r.y && Mouse->Y <= r.y + r.h) {
						const std::string& sel = gTouchLayoutInfos[i].fileName;
						if(sel != tLXOptions->sTouchscreenLayout) {
							tLXOptions->sTouchscreenLayout = sel;
							TouchControls::ReloadLayout();
							PlaySoundSample(sfxGeneral.smpClick);
						}
						break;
					}
				}
			}
			// "Double shoot to switch weapon" toggle below the layout tiles.
			Menu_OptionsDrawTouchOptions(VideoPostProcessor::videoSurface().get());
			if(Mouse->Up) {
				const SDL_Rect& r = gTouchDoubleShootRect;
				if(Mouse->X >= r.x && Mouse->X <= r.x + r.w &&
				   Mouse->Y >= r.y && Mouse->Y <= r.y + r.h) {
					tLXOptions->bTouchscreenDoubleShootToSwitchWeapon = !tLXOptions->bTouchscreenDoubleShootToSwitchWeapon;
					PlaySoundSample(sfxGeneral.smpClick);
				}
			}
		} else {
			// "Reset defaults" button (manual, so it can show that exact label)
			Menu_OptionsDrawResetButton(VideoPostProcessor::videoSurface().get());
			if(Mouse->Up &&
			   Mouse->X >= ResetBtnRect.x && Mouse->X <= ResetBtnRect.x + ResetBtnRect.w &&
			   Mouse->Y >= ResetBtnRect.y && Mouse->Y <= ResetBtnRect.y + ResetBtnRect.h) {
				Menu_ResetControlsTab(ControlsSubTab);
				PlaySoundSample(sfxGeneral.smpClick);
			}
		}

		if(ev) {

			if(ev->cWidget->getType() == wid_Inputbox) {

				if(ev->iEventMsg == INB_MOUSEUP) {

					// 0 = Player 1, 1 = Player 2, -1 = general controls
					int ply = (ControlsSubTab == ct_General) ? -1 : ControlsSubTab;

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
		ev = cOpt_Game.Process();
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

				case og_AlwaysCenterWorm:
					if(ev->iEventMsg == CHK_CHANGED)
						tLXOptions->bAlwaysCenterWorm = cOpt_Game.SendMessage(og_AlwaysCenterWorm, CKM_GETCHECK, (uintptr_t)0, 0) != 0;
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
		ev = cOpt_System.Process();
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
		
		cOpt_System.getWidget( os_NetworkUploadBandwidth )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );
		cOpt_System.getWidget( os_NetworkUploadBandwidthLabel )->setEnabled( tLXOptions->iNetworkSpeed >= NST_LAN );
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
	Menu_OptionsControlsLayout().Draw(tMenu->bmpBuffer.get());
	Menu_OptionsDrawControlsTabs(tMenu->bmpBuffer.get());

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

	// Change the options. The box only edits its own binding slot, so merge
	// the captured input back into the control's comma-separated value.
	if(ply >= 0) {
		std::string& ctrl = tLXOptions->sPlayerControls[ply][b->getValue()];
		ctrl = Menu_ControlSetSlot(ctrl, b->getSlot(), b->getText());
	} else {
		std::string& ctrl = tLXOptions->sGeneralControls[b->getValue()];
		ctrl = Menu_ControlSetSlot(ctrl, b->getSlot(), b->getText());
	}

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
	cOpt_Controls2.Shutdown();
	cOpt_ControlsGen.Shutdown();
	cOpt_ControlsTouch.Shutdown();
	cOpt_System.Shutdown();
	cOpt_Game.Shutdown();
}

}; // namespace DeprecatedGUI
