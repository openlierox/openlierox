/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Graphics header file
// Created 30/6/02
// Jason Boettcher


#include "LieroX.h"

#include "FindFile.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/Graphics.h"
#include "Cursor.h"
#include "ConfigHandler.h"


namespace DeprecatedGUI {

gfxgui_t	gfxGUI;
gfxgame_t	gfxGame;

static SmartPointer<SDL_Surface> getAlternativeCommandButtonGfx() {
	SmartPointer<SDL_Surface> gfx = gfxCreateSurface(24, 12);
	if(!gfx.get()) return NULL;
	
	// very simple, but it's not that important to have something nice here
	DrawRectFill(gfx.get(), 0, 0, 11, 11, tLX->clBoxLight);
	DrawRectFill(gfx.get(), 12, 0, 23, 11, tLX->clBoxDark);
	
	return gfx;
}

static SmartPointer<SDL_Surface> getAlternativeProgressGfx() {
	int w = 360;
	int h = 20;

	SmartPointer<SDL_Surface> gfx = gfxCreateSurface(w, 2 * h);
	if(!gfx.get()) return NULL;

	DrawRectFill(gfx.get(), 0, 0, w - 1, h - 1, tLX->clProgress);
	DrawRect(gfx.get(), 0, 0, w - 1, h - 2, tLX->clBoxDark);

	DrawRect(gfx.get(), 0, h - 1, w - 1, 2 * h - 2, tLX->clBoxDark);

	return gfx;
}

	
static SmartPointer<SDL_Surface> getAlternativeClockGfx() {
	SmartPointer<SDL_Surface> gfx = gfxCreateSurface(12, 12);
	if(!gfx.get()) return NULL;
	
	// just some dummy gfx (TODO: btw, why dont we have a DrawCircleFill?)
	DrawRectFill(gfx.get(), 4, 4, 7, 7, tLX->clWhite);
	
	return gfx;
}
	
	
	
///////////////////
// Load the graphics
bool LoadGraphics()
{
	// TODO: fix it that this works (or give some good reasons why we need these gfx in dedicated mode)
	// we don't need any of them in dedicated mode
	//if(bDedicated) return true;

	LOAD_IMAGE(gfxGUI.bmpScrollbar,"data/frontend/scrollbar.png");
	LOAD_IMAGE(gfxGUI.bmpSliderBut,"data/frontend/sliderbut.png");
	LOAD_IMAGE_WITHALPHA__OR(gfxGUI.bmpCommandBtn, "data/frontend/commandbtn.png", getAlternativeCommandButtonGfx());

	Load_Image_WithAlpha(gfxGame.bmpCrosshair, "data/gfx/crosshair.png");
	if (gfxGame.bmpCrosshair.get() == NULL)
		LOAD_IMAGE(gfxGame.bmpCrosshair,"data/gfx/crosshair.bmp");
	Load_Image_WithAlpha(gfxGame.bmpMuzzle, "data/gfx/muzzle.png");
	if (gfxGame.bmpMuzzle.get() == NULL)
		LOAD_IMAGE(gfxGame.bmpMuzzle,"data/gfx/muzzle.bmp");
	LOAD_IMAGE_WITHALPHA(gfxGame.bmpExplosion,"data/gfx/explosion.png");
	LOAD_IMAGE_WITHALPHA(gfxGame.bmpSmoke,"data/gfx/smoke.png");
	LOAD_IMAGE_WITHALPHA(gfxGame.bmpChemSmoke,"data/gfx/chemsmoke.png");
	LOAD_IMAGE_WITHALPHA(gfxGame.bmpSpawn,"data/gfx/spawn.png");
	Load_Image_WithAlpha(gfxGame.bmpHook, "data/gfx/hook.png");
	if (gfxGame.bmpHook.get() == NULL)
		LOAD_IMAGE(gfxGame.bmpHook,"data/gfx/hook.bmp");
	LOAD_IMAGE_WITHALPHA(gfxGame.bmpGameover,"data/gfx/gameover.png");
	LOAD_IMAGE_WITHALPHA(gfxGame.bmpScoreboard,"data/gfx/scoreboard.png");
	LOAD_IMAGE(gfxGame.bmpViewportMgr,"data/gfx/viewportmgr.png");
	LOAD_IMAGE(gfxGame.bmpSparkle, "data/gfx/sparkle.png");
	LOAD_IMAGE_WITHALPHA(gfxGame.bmpInfinite,"data/gfx/infinite.png");
	LOAD_IMAGE_WITHALPHA2(gfxGame.bmpFlagSpawnpointDefault,"data/gfx/flagspawnpoint.png","data/frontend/team_1.png");
	LOAD_IMAGE_WITHALPHA2(gfxGame.bmpFlagSpawnpoint[0],"data/gfx/flagspawnpoint1.png","data/frontend/team_1.png");
	LOAD_IMAGE_WITHALPHA2(gfxGame.bmpFlagSpawnpoint[1],"data/gfx/flagspawnpoint2.png","data/frontend/team_2.png");
	LOAD_IMAGE_WITHALPHA2(gfxGame.bmpFlagSpawnpoint[2],"data/gfx/flagspawnpoint3.png","data/frontend/team_3.png");
	LOAD_IMAGE_WITHALPHA2(gfxGame.bmpFlagSpawnpoint[3],"data/gfx/flagspawnpoint4.png","data/frontend/team_4.png");
	LOAD_IMAGE_WITHALPHA(gfxGame.bmpTeamColours[0], "data/frontend/team_1.png");
	LOAD_IMAGE_WITHALPHA(gfxGame.bmpTeamColours[1], "data/frontend/team_2.png");
	LOAD_IMAGE_WITHALPHA(gfxGame.bmpTeamColours[2], "data/frontend/team_3.png");
	LOAD_IMAGE_WITHALPHA(gfxGame.bmpTeamColours[3], "data/frontend/team_4.png");
	LOAD_IMAGE_WITHALPHA__OR(gfxGame.bmpClock, "data/frontend/clock.png", getAlternativeClockGfx());

	LOAD_IMAGE(gfxGame.bmpBonus, "data/gfx/bonus.png");
	LOAD_IMAGE(gfxGame.bmpHealth, "data/gfx/health.png");

	// These files don't have to be present (backward compatibility)
	gfxGame.bmpGameNetBackground = LoadGameImage("data/frontend/background_netgame.png",false);
	gfxGame.bmpGameLocalBackground = LoadGameImage("data/frontend/background_localgame.png",false);
	gfxGame.bmpGameLocalTopBar = LoadGameImage("data/frontend/top_bar_local.png",false);
	gfxGame.bmpGameNetTopBar = LoadGameImage("data/frontend/top_bar_net.png",false);
	gfxGame.bmpAI = LoadGameImage("data/frontend/cpu.png",false);

	// Cursors
	if (!InitializeCursors())
		return false;

	// Set the color keys
	SetColorKey(gfxGame.bmpCrosshair.get());
	SetColorKey(gfxGame.bmpMuzzle.get());
	SetColorKey(gfxGame.bmpExplosion.get());
	SetColorKey(gfxGame.bmpSmoke.get());
	SetColorKey(gfxGame.bmpChemSmoke.get());
	SetColorKey(gfxGame.bmpSpawn.get());
	SetColorKey(gfxGame.bmpHook.get());
	SetColorKey(gfxGame.bmpBonus.get());
	SetColorKey(gfxGame.bmpHealth.get());
	SetColorKey(gfxGame.bmpSparkle.get());
	SetColorKey(gfxGame.bmpViewportMgr.get());
	SetColorKey(gfxGame.bmpAI.get());

	// Check that the default skin is available (game can crash without this skin!!)
	if (!IsFileAvailable("skins/default.png")) {
		errors << "Main skin not found: skins/default.png" << endl;
		return false;
	}

	return true;
}

////////////////////////
// Load the colors from the config
void InitializeColors()  {
	// Init these special colors first
	tLX->clPink = Color(255,0,255);
	tLX->clWhite = Color(255,255,255);
	tLX->clBlack = Color(0,0,0);

	// Initialize the colours
	tLX->clChatText = Color(128,255,128);
	tLX->clSubHeading = Color(143,176,207);
	tLX->clHeading = Color(0,138,251);
	tLX->clNetworkText = Color(200,200,200);
	tLX->clPrivateText = Color(207,176,143);
	tLX->clNormalLabel = tLX->clWhite;
	tLX->clNotice = Color(200,200,200);
	tLX->clNormalText = tLX->clWhite;
	tLX->clDropDownText = tLX->clWhite;
	tLX->clDisabled = Color(96,96,96);
	tLX->clListView = tLX->clWhite;
	tLX->clTextBox = tLX->clWhite;
	tLX->clMouseOver = tLX->clWhite;
	tLX->clError = Color(200,50,50);
	tLX->clCredits1 = Color(150,150,150);
	tLX->clCredits2 = Color(96,96,96);
	tLX->clPopupMenu = tLX->clWhite;
	tLX->clWaiting = tLX->clWhite;
	tLX->clReady = Color(0,255,0);
	tLX->clPlayerName = Color(255,255,255);
	tLX->clBoxDark = Color(60,60,60);
	tLX->clBoxLight = Color(130,130,130);
	tLX->clWinBtnBody = Color(128,128,128);
	tLX->clWinBtnDark = Color(64,64,64);
	tLX->clWinBtnLight = Color(192,192,192);
	tLX->clMPlayerSong = tLX->clBlack;
	tLX->clMPlayerTime = tLX->clBlack;
	tLX->clDialogBackground = tLX->clBlack;
	tLX->clGameBackground = tLX->clBlack;
	tLX->clViewportSplit = tLX->clBlack;
	tLX->clScrollbarBack = Color(0,66,100);
	tLX->clScrollbarFront = Color(0,100,180);
	tLX->clScrollbarHighlight = Color(25,155,255);
	tLX->clScrollbarBackLight = Color(0,75,113);
	tLX->clScrollbarShadow = Color(0,40,65);
	tLX->clSliderDark = Color(90,90,90);
	tLX->clSliderLight = Color(115,115,115);
	tLX->clChatBoxBackground = tLX->clBlack;
	tLX->clScoreBackground = tLX->clBlack;
	tLX->clScoreHighlight = Color(0, 0, 64);
	tLX->clCurrentSettingsBg = tLX->clBlack;
	tLX->clDialogCaption = Color(0,0,64);
	tLX->clPlayerDividingLine = Color(64,64,64);
	tLX->clLine = tLX->clWhite;
	tLX->clProgress = Color(0,136,250);
	tLX->clListviewSelected = Color(0,66,102);
	tLX->clComboboxSelected = Color(0,66,102);
	tLX->clComboboxShowAllMain = Color(40,84,122);
	tLX->clComboboxShowAllBorder = Color(220,220,220);
	tLX->clMenuBackground = tLX->clBlack;
	tLX->clMenuSelected = Color(0,66,102);
	tLX->clGameChatter = tLX->clWhite;
	tLX->clSelection = Color(0, 100, 150);
	tLX->clTextboxCursor = Color(50, 150, 200);
	tLX->clGameChatCursor = tLX->clWhite;
	tLX->clConsoleCursor = tLX->clWhite;
	tLX->clConsoleNormal = tLX->clWhite;
	tLX->clConsoleNotify = Color(200,200,200);
	tLX->clConsoleError = Color(255,0,0);
	tLX->clConsoleWarning = Color(200,128,128);
	tLX->clConsoleDev = Color(100,100,255);
	tLX->clConsoleChat = Color(100,255,100);
	tLX->clReturningToLobby = Color(200,200,200);
	tLX->clTeamColors[0] = Color(2, 184, 252);  // Blue
	tLX->clTeamColors[1] = Color(255, 2, 2); // Red
	tLX->clTeamColors[2] = Color(32, 253, 0); // Green
	tLX->clTeamColors[3] = Color(253, 244, 0); // Yellow
	tLX->clTagHighlight = Color(255, 0, 0);
	tLX->clHealthLabel = Color(0, 222, 0);
	tLX->clWeaponLabel = Color(0, 0, 222);
	tLX->clLivesLabel = Color(0, 222, 0);
	tLX->clKillsLabel = Color(222, 0, 0);
	tLX->clFPSLabel = tLX->clWhite;
	tLX->clPingLabel = tLX->clWhite;
	tLX->clSpecMsgLabel = tLX->clWhite;
	tLX->clLoadingLabel = tLX->clWhite;
	tLX->clIpLoadingLabel = tLX->clWhite;
	tLX->clWeaponSelectionActive = tLX->clWhite;
	tLX->clWeaponSelectionDefault = Color(150, 150, 150);
	tLX->clWeaponSelectionTitle = tLX->clWhite;
	tLX->clRopeColors[0] = Color(160, 80, 0);
	tLX->clRopeColors[1] = Color(200, 100, 0);
	tLX->clLaserSightColors[0] = Color(190,0,0);
	tLX->clLaserSightColors[1] = Color(160,0,0);
	tLX->clTimeLeftLabel = tLX->clWhite;
	tLX->clTimeLeftWarnLabel = Color(255,50,50);

	// Load the colours from a file
	const std::string colorfile = "data/frontend/colours.cfg";
	ReadColour(colorfile,"Colours","ChatText",				tLX->clChatText,		tLX->clChatText);
	ReadColour(colorfile,"Colours","Credits1",				tLX->clCredits1,		tLX->clCredits1);
	ReadColour(colorfile,"Colours","Credits2",				tLX->clCredits2,		tLX->clCredits2);
	ReadColour(colorfile,"Colours","Disabled",				tLX->clDisabled,		tLX->clDisabled);
	ReadColour(colorfile,"Colours","DropDownText",			tLX->clDropDownText,	tLX->clDropDownText);
	ReadColour(colorfile,"Colours","Error",					tLX->clError,			tLX->clError);
	ReadColour(colorfile,"Colours","Heading",				tLX->clHeading,		tLX->clHeading);
	ReadColour(colorfile,"Colours","ListView",				tLX->clListView,		tLX->clListView);
	ReadColour(colorfile,"Colours","MouseOver",				tLX->clMouseOver,		tLX->clMouseOver);
	ReadColour(colorfile,"Colours","NetworkText",			tLX->clNetworkText,	tLX->clNetworkText);
	ReadColour(colorfile,"Colours","PrivateText",			tLX->clPrivateText,	tLX->clPrivateText);
	ReadColour(colorfile,"Colours","NormalLabel",			tLX->clNormalLabel,	tLX->clNormalLabel);
	ReadColour(colorfile,"Colours","NormalText",			tLX->clNormalText,		tLX->clNormalText);
	ReadColour(colorfile,"Colours","Notice",				tLX->clNotice,			tLX->clNotice);
	ReadColour(colorfile,"Colours","PopupMenu",				tLX->clPopupMenu,		tLX->clPopupMenu);
	ReadColour(colorfile,"Colours","SubHeading",			tLX->clSubHeading,		tLX->clSubHeading);
	ReadColour(colorfile,"Colours","TextBox",				tLX->clTextBox,		tLX->clTextBox);
	ReadColour(colorfile,"Colours","Waiting",				tLX->clWaiting,		tLX->clWaiting);
	ReadColour(colorfile,"Colours","Ready",					tLX->clReady,			tLX->clReady);
	ReadColour(colorfile,"Colours","PlayerName",			tLX->clPlayerName,		tLX->clPlayerName);
	ReadColour(colorfile,"Colours","BoxDark",				tLX->clBoxDark,		tLX->clBoxDark);
	ReadColour(colorfile,"Colours","BoxLight",				tLX->clBoxLight,		tLX->clBoxLight);
	ReadColour(colorfile,"Colours","WinButtonBody",			tLX->clWinBtnBody,		tLX->clWinBtnBody);
	ReadColour(colorfile,"Colours","WinButtonDark",		 	tLX->clWinBtnDark,		tLX->clWinBtnDark);
	ReadColour(colorfile,"Colours","WinButtonLight",		tLX->clWinBtnLight,	tLX->clWinBtnLight);
	ReadColour(colorfile,"Colours","MPlayerSongTime",		tLX->clMPlayerTime,	tLX->clMPlayerTime);
	ReadColour(colorfile,"Colours","MPlayerSongName",		tLX->clMPlayerSong,	tLX->clMPlayerSong);
	ReadColour(colorfile,"Colours","DialogBackground",		tLX->clDialogBackground,	tLX->clDialogBackground);
	ReadColour(colorfile,"Colours","GameBackground",		tLX->clGameBackground,		tLX->clGameBackground);
	ReadColour(colorfile,"Colours","ViewportSplit",			tLX->clViewportSplit,		tLX->clViewportSplit);
	ReadColour(colorfile,"Colours","ScrollbarBack",			tLX->clScrollbarBack,		tLX->clScrollbarBack);
	ReadColour(colorfile,"Colours","ScrollbarBackLight",	tLX->clScrollbarBackLight,	tLX->clScrollbarBackLight);
	ReadColour(colorfile,"Colours","ScrollbarFront",		tLX->clScrollbarFront,		tLX->clScrollbarFront);
	ReadColour(colorfile,"Colours","ScrollbarHighlight",	tLX->clScrollbarHighlight,	tLX->clScrollbarHighlight);
	ReadColour(colorfile,"Colours","ScrollbarShadow",		tLX->clScrollbarShadow,	tLX->clScrollbarShadow);
	ReadColour(colorfile,"Colours","ChatBoxBackground",		tLX->clChatBoxBackground,	tLX->clChatBoxBackground);
	ReadColour(colorfile,"Colours","ScoreBack",				tLX->clScoreBackground,	tLX->clScoreBackground);
	if(tLX->clScoreBackground.a == 255) tLX->clScoreBackground.a = 128;
	ReadColour(colorfile,"Colours","ScoreHighlight",		tLX->clScoreHighlight,		tLX->clScoreHighlight);
	ReadColour(colorfile,"Colours","CurrentSettBack",		tLX->clCurrentSettingsBg,	tLX->clCurrentSettingsBg);
	ReadColour(colorfile,"Colours","DialogCaption",			tLX->clDialogCaption,		tLX->clDialogCaption);
	ReadColour(colorfile,"Colours","SliderLight",			tLX->clSliderLight,		tLX->clSliderLight);
	ReadColour(colorfile,"Colours","SliderDark",			tLX->clSliderDark,			tLX->clSliderDark);
	ReadColour(colorfile,"Colours","PlayerDividingLine",	tLX->clPlayerDividingLine,tLX->clPlayerDividingLine);
	ReadColour(colorfile,"Colours","Line",					tLX->clLine,				tLX->clLine);
	ReadColour(colorfile,"Colours","Progress",				tLX->clProgress,			tLX->clProgress);
	ReadColour(colorfile,"Colours","ListviewSelected",		tLX->clListviewSelected,	tLX->clListviewSelected);
	ReadColour(colorfile,"Colours","DropDownSelected",		tLX->clComboboxSelected,	tLX->clComboboxSelected);
	ReadColour(colorfile,"Colours","DropDownShowAllMain",	tLX->clComboboxShowAllMain,tLX->clComboboxShowAllMain);
	ReadColour(colorfile,"Colours","DropDownShowAllBorder",	tLX->clComboboxShowAllBorder,tLX->clComboboxShowAllBorder);
	ReadColour(colorfile,"Colours","MenuBackground",		tLX->clMenuBackground,		tLX->clMenuBackground);
	ReadColour(colorfile,"Colours","MenuSelected",			tLX->clMenuSelected,		tLX->clMenuSelected);
	ReadColour(colorfile,"Colours","GameChatter",			tLX->clGameChatter,		tLX->clGameChatter);
	ReadColour(colorfile,"Colours","Selection",				tLX->clSelection,			tLX->clSelection);
	ReadColour(colorfile,"Colours","TextboxCursor",			tLX->clTextboxCursor,		tLX->clTextboxCursor);
	ReadColour(colorfile,"Colours","GameChatCursor",		tLX->clGameChatCursor,		tLX->clGameChatCursor);
	ReadColour(colorfile,"Colours","ConsoleCursor",			tLX->clConsoleCursor,		tLX->clConsoleCursor);
	ReadColour(colorfile,"Colours","ConsoleNormal",			tLX->clConsoleNormal,		tLX->clConsoleNormal);
	ReadColour(colorfile,"Colours","ConsoleNotify",			tLX->clConsoleNotify,		tLX->clConsoleNotify);
	ReadColour(colorfile,"Colours","ConsoleError",			tLX->clConsoleError,		tLX->clConsoleError);
	ReadColour(colorfile,"Colours","ConsoleWarning",		tLX->clConsoleWarning,		tLX->clConsoleWarning);
	ReadColour(colorfile,"Colours","ConsoleDev",			tLX->clConsoleDev,			tLX->clConsoleDev);
	ReadColour(colorfile,"Colours","ConsoleChat",			tLX->clConsoleChat,		tLX->clConsoleChat);
	ReadColour(colorfile,"Colours","ReturningToLobby",		tLX->clReturningToLobby,	tLX->clReturningToLobby);
	ReadColour(colorfile,"Colours","Team1",					tLX->clTeamColors[0],		tLX->clTeamColors[0]);
	ReadColour(colorfile,"Colours","Team2",					tLX->clTeamColors[1],		tLX->clTeamColors[1]);
	ReadColour(colorfile,"Colours","Team3",					tLX->clTeamColors[2],		tLX->clTeamColors[2]);
	ReadColour(colorfile,"Colours","Team4",					tLX->clTeamColors[3],		tLX->clTeamColors[3]);
	ReadColour(colorfile,"Colours","TagHighlight",			tLX->clTagHighlight,		tLX->clTagHighlight);
	ReadColour(colorfile,"Colours","HealthLabel",			tLX->clHealthLabel,		tLX->clHealthLabel);
	ReadColour(colorfile,"Colours","WeaponLabel",			tLX->clWeaponLabel,		tLX->clWeaponLabel);
	ReadColour(colorfile,"Colours","LivesLabel",			tLX->clLivesLabel,			tLX->clLivesLabel);
	ReadColour(colorfile,"Colours","KillsLabel",			tLX->clKillsLabel,			tLX->clKillsLabel);
	ReadColour(colorfile,"Colours","FPSLabel",				tLX->clFPSLabel,			tLX->clFPSLabel);
	ReadColour(colorfile,"Colours","PingLabel",				tLX->clPingLabel,			tLX->clPingLabel);
	ReadColour(colorfile,"Colours","SpecialMsgLabel",		tLX->clSpecMsgLabel,		tLX->clSpecMsgLabel);
	ReadColour(colorfile,"Colours","LoadingLabel",			tLX->clLoadingLabel,		tLX->clLoadingLabel);
	ReadColour(colorfile,"Colours","IpLoadingLabel",		tLX->clIpLoadingLabel,		tLX->clIpLoadingLabel);
	ReadColour(colorfile,"Colours","WeaponSelectionTitle",	tLX->clWeaponSelectionTitle,	tLX->clWeaponSelectionTitle);
	ReadColour(colorfile,"Colours","WeaponSelectionActive",	tLX->clWeaponSelectionActive,	tLX->clWeaponSelectionActive);
	ReadColour(colorfile,"Colours","WeaponSelectionDefault",tLX->clWeaponSelectionDefault,	tLX->clWeaponSelectionDefault);
	ReadColour(colorfile,"Colours","RopeColour1",			tLX->clRopeColors[0],			tLX->clRopeColors[0]);
	ReadColour(colorfile,"Colours","RopeColour2",			tLX->clRopeColors[1],			tLX->clRopeColors[1]);
	ReadColour(colorfile,"Colours","LaserSightColour1",		tLX->clLaserSightColors[0],	tLX->clLaserSightColors[0]);
	ReadColour(colorfile,"Colours","LaserSightColour2",		tLX->clLaserSightColors[1],	tLX->clLaserSightColors[1]);
	ReadColour(colorfile,"Colours","TimeLeftLabel",			tLX->clTimeLeftLabel,			tLX->clTimeLeftLabel);
	ReadColour(colorfile,"Colours","TimeLeftWarnLabel",		tLX->clTimeLeftWarnLabel,		tLX->clTimeLeftWarnLabel);
	
}


///////////////////
// Initialize the fonts
bool LoadFonts()  {
	tLX->cFont.SetVSpacing(0);
	tLX->cOutlineFont.SetOutline(true);
	tLX->cOutlineFont.SetSpacing(0);

	// Normal font
	if(!tLX->cFont.Load("data/gfx/font.png",true))
		return false;

	// Outline font
	if(!tLX->cOutlineFont.Load("data/gfx/out_font.png",true))
		return false;

	return true;
}


///////////////////
// Shutdown the graphics
void ShutdownGraphics()
{
	ShutdownCursors();
	if (tLX)  {
		tLX->cFont.Shutdown();
		tLX->cOutlineFont.Shutdown();
	}
}

}; // namespace DeprecatedGUI
