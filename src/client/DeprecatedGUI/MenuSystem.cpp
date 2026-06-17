/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Menu System functions
// Created 30/6/02
// Jason Boettcher


#include <assert.h>

#include <set>

#include "Debug.h"
#include "LieroX.h"
#include "OLXConsole.h"
#include "EndianSwap.h"
#include "AuxLib.h"
#include "Error.h"
#include "ConfigHandler.h"
#include "CClient.h"
#include "IpToCountryDB.h"
#include "DeprecatedGUI/Graphics.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "game/CWorm.h"
#include "Cursor.h"
#include "DeprecatedGUI/CButton.h"
#include "DedicatedControl.h"
#include "OLXG15.h"
#include "Timer.h"
#include "IRC.h"
#include "FileUtils.h"
#include "OLXCommand.h"
#include "HTTP.h"
#include "Version.h"
#include "CrashHandler.h"
#include "game/Level.h"
#ifndef DEDICATED_ONLY
#include "sound/sfx.h"
#endif
#include "TaskManager.h"
#include "game/ServerList.h"


// TODO: move this out here
// declare them only locally here as nobody really should use them explicitly
std::string Utf8String(const std::string &OldLxString);




namespace DeprecatedGUI {

menu_t	*tMenu = NULL;


bool		bSkipStart = false;
CWidgetList	LayoutWidgets[LAYOUT_COUNT];

static bool Menu_InitSockets() {
	for (size_t i = 0; i < sizeof(tMenu->tSocket)/sizeof(tMenu->tSocket[0]); ++i)
		tMenu->tSocket[i] = new NetworkSocket();

	// HACK: open an unreliable foo socket
	// Some routers simply ignore first open socket and don't let any data through, this is a workaround
	tMenu->tSocket[SCK_FOO]->OpenUnreliable(0);
	// Open a socket for broadcasting over a LAN (UDP)
	tMenu->tSocket[SCK_LAN]->OpenBroadcast(0);
	// Open a socket for communicating over the net (UDP)
	tMenu->tSocket[SCK_NET]->OpenUnreliable(0);	
	
	if(!tMenu->tSocket[SCK_LAN]->isOpen() || !tMenu->tSocket[SCK_NET]->isOpen()) {
#if defined(__EMSCRIPTEN__)
		// Browsers can't open raw UDP sockets — sockets() returns
		// SOCK_DGRAM = -1 in Emscripten. The single-player menu still
		// works without them; just skip the abort and continue with
		// closed sockets. The server browser / master server query
		// paths will be no-ops at runtime.
		warnings << "Menu_InitSockets: UDP sockets unavailable on Emscripten — "
		            "continuing without networking" << endl;
#else
		SystemError("Error: Failed to open a socket for networking");
		return false;
#endif
	}

	// Send some random data to some random IP
	if (tMenu->tSocket[SCK_FOO]->isOpen())  {
		NetworkAddr a; StringToNetAddr("1.2.3.4:5678", a);
		// For example, if no network is connected, you likely only have 127.* in your routing table.
		if(IsNetAddrAvailable(a)) {
			tMenu->tSocket[SCK_FOO]->setRemoteAddress(a);
			tMenu->tSocket[SCK_FOO]->Write("foo");
		}
	}

	return true;
}

menu_t::menu_t() : cSkin(CGameSkin::WormSkin(), true) {}

///////////////////
// Initialize the menu system
bool Menu_Initialize()
{
	bJoin_Update = true;
	bHost_Update = true;

	// Allocate the menu structure
	tMenu = new menu_t;
    if(tMenu == NULL) {
        SystemError("Error: Out of memory in for menu");
		return false;
    }

	if(!Menu_InitSockets()) return false;

	if(bDedicated) return true;
	
	// Load the frontend info
	Menu_LoadFrontendInfo();

	tMenu->iReturnTo = net_internet;
	tMenu->bForbidConsole = false;

	// Load the images
	//LOAD_IMAGE(tMenu->bmpMainBack,"data/frontend/background.png");
    //LOAD_IMAGE(tMenu->bmpMainBack_lg,"data/frontend/background_lg.png");
    LOAD_IMAGE(tMenu->bmpMainBack_wob,"data/frontend/background_wob.png");

	// bmpMainBack_common, for backward compatibility: if it doesn't exist, we use bmpMainBack_wob
	tMenu->bmpMainBack_common = LoadGameImage("data/frontend/background_common.png");
	if (!tMenu->bmpMainBack_common.get())
		tMenu->bmpMainBack_common = tMenu->bmpMainBack_wob;


	tMenu->bmpBuffer = gfxCreateSurface(640,480);
    if(tMenu->bmpBuffer.get() == NULL) {
        SystemError("Error: Out of memory back buffer");
		return false;
    }


	tMenu->bmpMsgBuffer = gfxCreateSurface(640,480);
    if(tMenu->bmpMsgBuffer.get() == NULL) {
        SystemError("Error: Out of memory in MsgBuffer");
		return false;
	}

    tMenu->bmpMiniMapBuffer = gfxCreateSurface(128,96);
    if(tMenu->bmpMiniMapBuffer.get() == NULL) {
        SystemError("Error: Out of memory in MiniMapBuffer");
		return false;
    }

	SmartPointer<SDL_Surface> lobby_state = NULL;
	LOAD_IMAGE_WITHALPHA(tMenu->bmpMainTitles,"data/frontend/maintitles.png");
	LOAD_IMAGE_WITHALPHA(tMenu->bmpLieroXtreme,"data/frontend/lierox.png");
	LOAD_IMAGE_WITHALPHA(tMenu->bmpSubTitles,"data/frontend/subtitles.png");
	LOAD_IMAGE_WITHALPHA(tMenu->bmpButtons,"data/frontend/buttons.png");
	LOAD_IMAGE_WITHALPHA(tMenu->bmpMapEdTool,"data/frontend/map_toolbar.png");
	LOAD_IMAGE_WITHALPHA(tMenu->bmpCheckbox,"data/frontend/checkbox.png");
	LOAD_IMAGE_WITHALPHA(tMenu->bmpInputbox,"data/frontend/inputbox.png");
	//LOAD_IMAGE_WITHALPHA(tMenu->bmpAI,"data/frontend/cpu.png");
	LOAD_IMAGE_WITHALPHA(lobby_state, "data/frontend/lobbyready.png");;
	LOAD_IMAGE_WITHALPHA(tMenu->bmpConnectionSpeeds[0], "data/frontend/con_good.png");
	LOAD_IMAGE_WITHALPHA(tMenu->bmpConnectionSpeeds[1], "data/frontend/con_average.png");
	LOAD_IMAGE_WITHALPHA(tMenu->bmpConnectionSpeeds[2], "data/frontend/con_bad.png");
	LOAD_IMAGE_WITHALPHA(tMenu->bmpConnectionSpeeds[3], "data/frontend/con_none.png");
	LOAD_IMAGE_WITHALPHA2(tMenu->bmpConnectionSpeeds[4], "data/frontend/con_nat.png", "data/frontend/con_bad.png");
	LOAD_IMAGE_WITHALPHA(tMenu->bmpTriangleUp, "data/frontend/triangle_up.png");
	LOAD_IMAGE_WITHALPHA(tMenu->bmpTriangleDown, "data/frontend/triangle_down.png");
	tMenu->bmpDownload = LoadGameImage("data/frontend/download.png", true); // Doesn't have to exist
	tMenu->bmpChatBackground = LoadGameImage("data/frontend/background_chat.png", true);
	tMenu->bmpChatBackgroundMain = LoadGameImage("data/frontend/background_chat_main.png", true);

	// Split up the lobby ready image
	tMenu->bmpLobbyReady = gfxCreateSurfaceAlpha(lobby_state.get()->w, 12);
	if (!tMenu->bmpLobbyReady.get())  {
		errors << "Out of memory while creating tMenu->bmpLobbyReady" << endl;
		return false;
	}
	CopySurface(tMenu->bmpLobbyReady.get(), lobby_state, 0, 0, 0, 0, lobby_state.get()->w, 12);

	tMenu->bmpLobbyNotReady = gfxCreateSurfaceAlpha(lobby_state.get()->w, 12);
	if (!tMenu->bmpLobbyNotReady.get())  {
		errors << "Out of memory while creating tMenu->bmpLobbyNotReady" << endl;
		return false;
	}
	CopySurface(tMenu->bmpLobbyNotReady.get(), lobby_state, 0, 12, 0, 0, lobby_state.get()->w, 12);


	// Add default widget IDs to the widget list
	//Menu_AddDefaultWidgets();

	return true;
}

/////////////////////////
// Load the infor about frontend
void Menu_LoadFrontendInfo()
{
	ReadInteger("data/frontend/frontend.cfg","MainTitles","X",&tMenu->tFrontendInfo.iMainTitlesLeft,50);
	ReadInteger("data/frontend/frontend.cfg","MainTitles","Y",&tMenu->tFrontendInfo.iMainTitlesTop,160);
	ReadInteger("data/frontend/frontend.cfg","Credits","X",&tMenu->tFrontendInfo.iCreditsLeft,370);
	ReadInteger("data/frontend/frontend.cfg","Credits","Y",&tMenu->tFrontendInfo.iCreditsTop,379);
	ReadInteger("data/frontend/frontend.cfg","Credits","Spacing",&tMenu->tFrontendInfo.iCreditsSpacing,0);
	ReadString ("data/frontend/frontend.cfg","Credits","FrontendCredits", tMenu->tFrontendInfo.sFrontendCredits, " ");
	ReadInteger("data/frontend/frontend.cfg","MainTitles","Spacing",&tMenu->tFrontendInfo.iMainTitlesSpacing,15);
	ReadKeyword("data/frontend/frontend.cfg","PageBoxes","Visible",&tMenu->tFrontendInfo.bPageBoxes,true);
	ReadInteger("data/frontend/frontend.cfg","IpToCountryLoading","AnimX",&tMenu->tFrontendInfo.iLoadingAnimLeft,5);
	ReadInteger("data/frontend/frontend.cfg","IpToCountryLoading","AnimY",&tMenu->tFrontendInfo.iLoadingAnimTop,5);
	ReadFloat("data/frontend/frontend.cfg","IpToCountryLoading","AnimFrameTime",&tMenu->tFrontendInfo.fLoadingAnimFrameTime,0.2f);
	ReadInteger("data/frontend/frontend.cfg","IpToCountryLoading","BarX",&tMenu->tFrontendInfo.iLoadingBarLeft,5);
	ReadInteger("data/frontend/frontend.cfg","IpToCountryLoading","BarY",&tMenu->tFrontendInfo.iLoadingBarTop,80);
	ReadInteger("data/frontend/frontend.cfg","IpToCountryLoading","LabelX",&tMenu->tFrontendInfo.iLoadingLabelLeft,5);
	ReadInteger("data/frontend/frontend.cfg","IpToCountryLoading","LabelY",&tMenu->tFrontendInfo.iLoadingLabelTop,60);
}

///////////////////
// Shutdown the menu
void Menu_Shutdown()
{
	Menu_Current_Shutdown();
	
	if(tMenu) {
		// The rest get free'd in the cache
		delete tMenu;
		tMenu = NULL;
	}

	// Shutdown the layouts
	//for (int i=0; i<LAYOUT_COUNT; i++)
	//	LayoutWidgets[i].Shutdown();
}



///////////////////
// Set the skip start bit
void Menu_SetSkipStart(int s)
{
    bSkipStart = s;
}
	
void Menu_Frame() {
	if(bDedicated) {
		ServerList::get()->process();
		return;
	}

#ifndef DEDICATED_ONLY
	sfx.think();
#endif

	if(game.state >= Game::S_Preparing) return; // could be already quitted

	// Menus are authored for menuWidth and presented centered on the screen.
	// (Overwritten by the gameplay draw, see CClient::Draw.)
	VideoPostProcessor::get()->setDisplayScreenWidth(VideoPostProcessor::menuWidth);

	// Check if user pressed screenshot key
	if (tLX->cTakeScreenshot.isDownOnce())  {
		PushScreenshot("scrshots", "");
	}
	
	Menu_RedrawMouse(true);

#ifdef WITH_G15
	if (OLXG15)
		OLXG15->menuFrame();
#endif //WITH_G15
	
	switch(tMenu->iMenuType) {

		// Main
		case MNU_MAIN:
			Menu_MainFrame();
			break;

		// Local
		case MNU_LOCAL:
			Menu_LocalFrame();
			break;

		// Network
		case MNU_NETWORK:
			Menu_NetFrame();
			break;

		// Player
		case MNU_PLAYER:
			Menu_PlayerFrame();
			break;

		// Map editor
		case MNU_MAPED:
			Menu_MapEdFrame(VideoPostProcessor::videoSurface().get(),true);
			break;

		// Options
		case MNU_OPTIONS:
			Menu_OptionsFrame();
			break;
	}

	// In network menu, we do the update anyway.
	// But we also want to have it everywhere else.
	if(tMenu->iMenuType != MNU_NETWORK)
		ServerList::get()->process();
	
	// DEBUG: show FPS
#ifdef DEBUG
	if(tLX->fDeltaTime != TimeDiff()) {
		Menu_redrawBufferRect(0, 0, 100, 20);
		tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 0, 0, tLX->clWhite, "FPS: " + itoa((int)(1.0f/tLX->fDeltaTime.seconds())));
	}
#endif

	taskManager->renderTasksStatus(VideoPostProcessor::videoSurface().get());
	
	if (!tMenu->bForbidConsole)  {
		Con_Process(tLX->fDeltaTime);
		Con_Draw(VideoPostProcessor::videoSurface().get());
	}
	tMenu->bForbidConsole = false; // Reset it here, it might get recovered next frame

	// we need to clone the screen buffer because of the current way we are drawing the menu
	struct CopyScreenBuffer : Action {
		Result handle() { VideoPostProcessor::cloneBuffer(); return true; }
	};
	doVppOperation(new CopyScreenBuffer());
}



///////////////////
// Redraw the rectangle under the mouse (total means a total buffer redraw)
// TODO: rename this function (one would expect that it redraws the mouse)
void Menu_RedrawMouse(bool total)
{
	if(total) {
		SDL_BlitSurface(tMenu->bmpBuffer.get(),NULL,VideoPostProcessor::videoSurface().get(),NULL);
		return;
	}

	int hw = GetMaxCursorWidth() / 2 - 1;
	int hh = GetMaxCursorHeight() / 2 - 1;

	mouse_t *m = GetMouse();
	DrawImageAdv(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer,
				m->X - hw - m->deltaX,
				m->Y - hh - m->deltaY,

				m->X - hw - m->deltaX,
				m->Y - hh - m->deltaY,
				GetMaxCursorWidth() * 2, GetMaxCursorHeight() * 2);
}


///////////////////
// Draw a sub title
void Menu_DrawSubTitle(SDL_Surface * bmpDest, int id)
{
	// Center within the menu canvas (menuWidth), not the full screen: the
	// whole menu is presented centered, so menu-local coordinates are used.
	int x = VideoPostProcessor::menuWidth/2;
	x -= tMenu->bmpSubTitles.get()->w/2;

	DrawImageAdv(bmpDest,tMenu->bmpSubTitles, 0, id*70, x,30, tMenu->bmpSubTitles.get()->w, 65);
}


///////////////////
// Draw a sub title advanced
void Menu_DrawSubTitleAdv(SDL_Surface * bmpDest, int id, int y)
{
	// Center within the menu canvas (menuWidth), not the full screen: the
	// whole menu is presented centered, so menu-local coordinates are used.
	int x = VideoPostProcessor::menuWidth/2;
	x -= tMenu->bmpSubTitles.get()->w/2;

	DrawImageAdv(bmpDest,tMenu->bmpSubTitles, 0, id*70, x,y, tMenu->bmpSubTitles.get()->w, 65);
}

	
////////////////
// Draws advanced box
void Menu_DrawBoxAdv(SDL_Surface * bmpDest, int x, int y, int x2, int y2, int border, Color LightColour, Color DarkColour, Color BgColour, uchar type)
{
	// First draw the background
	if (BgColour != tLX->clPink)
		DrawRectFill(bmpDest,x+border,y+border,x2-border+1,y2-border+1,BgColour);

	if (!border)
		return;

	int i;

	// Switch the light and dark colour when inset
	if (type == BX_INSET)  {
		Color tmp = LightColour;
		LightColour = DarkColour;
		DarkColour = tmp;
	}

	// Create gradient when needed
	int r_step,g_step,b_step;
	const Uint8 r1 = DarkColour.r, g1 = DarkColour.g, b1 = DarkColour.b;
	const Uint8 r2 = LightColour.r, g2 = LightColour.b, b2 = LightColour.b;

	if (type != BX_SOLID)  {
		r_step = (r2-r1)/border;
		g_step = (g2-g1)/border;
		b_step = (b2-b1)/border;
	}
	else {
		r_step = g_step = b_step = 0;
	}


	// Draw the box
	for (i=0;i<border;i++)
		DrawRect(bmpDest,x+i,y+i,x2-i,y2-i,Color(r1+r_step*i,g1+g_step*i,b1+b_step*i));
}


///////////////////
// Draw a box
void Menu_DrawBox(SDL_Surface * bmpDest, int x, int y, int x2, int y2)
{
	if(bmpDest == NULL) {
		errors << "Menu_DrawBox: bmpDest == NULL" << endl;
		return;
	}
	
	DrawRect( bmpDest,x+1, y+1, x2-1,y2-1, tLX->clBoxLight);
    //DrawRect( bmpDest,x+2, y+2, x2-2,y2-2, tLX->clBoxDark);
	DrawHLine(bmpDest,x+2, x2-1,y,  tLX->clBoxDark);
	DrawHLine(bmpDest,x+2, x2-1,y2, tLX->clBoxDark);

	DrawVLine(bmpDest,y+2, y2-1,x,  tLX->clBoxDark);
	DrawVLine(bmpDest,y+2, y2-1,x2, tLX->clBoxDark);

    Uint32 dark = tLX->clBoxDark.get(bmpDest->format);

	LOCK_OR_QUIT(bmpDest);
	PutPixel( bmpDest,x+1, y+1,     dark);
	PutPixel( bmpDest,x2-1,y+1,     dark);
	PutPixel( bmpDest,x+1, y2-1,    dark);
	PutPixel( bmpDest,x2-1,y2-1,    dark);
	UnlockSurface(bmpDest);
}


///////////////////
// Draw an inset box
void Menu_DrawBoxInset(SDL_Surface * bmpDest, int x, int y, int x2, int y2)
{
	if(bmpDest == NULL) {
		errors << "Menu_DrawBoxInset: bmpDest is NULL" << endl;
		return;
	}
	
	// Clipping
	if (x < 0) { x2 += x; x = 0; }
	if (x2 >= bmpDest->w) { x2 = bmpDest->w - 1; }
	if (y < 0) { y2 += y; y = 0; }
	if (y2 >= bmpDest->h) { y2 = bmpDest->h - 1; }

	DrawRect( bmpDest,x+1, y+1, x2-1,y2-1, tLX->clBoxDark);
	DrawHLine(bmpDest,x+2, x2-1,y,  tLX->clBoxLight);
	DrawHLine(bmpDest,x+2, x2-1,y2, tLX->clBoxLight);

	DrawVLine(bmpDest,y+2, y2-1,x,  tLX->clBoxLight);
	DrawVLine(bmpDest,y+2, y2-1,x2, tLX->clBoxLight);

	Uint32 light = tLX->clBoxLight.get(bmpDest->format);

	LOCK_OR_QUIT(bmpDest);
	if(PointInRect(x+1,y+1,bmpDest->clip_rect)) PutPixel( bmpDest,x+1, y+1,     light);
	if(PointInRect(x2-1,y+1,bmpDest->clip_rect)) PutPixel( bmpDest,x2-1,y+1,     light);
	if(PointInRect(x+1,y2-1,bmpDest->clip_rect)) PutPixel( bmpDest,x+1, y2-1,    light);
	if(PointInRect(x2-1,y2-1,bmpDest->clip_rect)) PutPixel( bmpDest,x2-1,y2-1,    light);
	UnlockSurface(bmpDest);
}


///////////////////
// Draw a windows style button
void Menu_DrawWinButton(SDL_Surface * bmpDest, int x, int y, int w, int h, bool down)
{
    DrawRectFill(bmpDest, x,y, x+w, y+h, tLX->clWinBtnBody);
    const Color dark = tLX->clWinBtnDark;
    const Color light = tLX->clWinBtnLight;
    if(down) {
        DrawHLine(bmpDest, x, x+w, y, dark);
        DrawHLine(bmpDest, x, x+w, y+h, light);
        DrawVLine(bmpDest, y, y+h, x, dark);
        DrawVLine(bmpDest, y, y+h, x+w, light);
    } else {
        DrawHLine(bmpDest, x, x+w, y, light);
        DrawHLine(bmpDest, x, x+w, y+h, dark);
        DrawVLine(bmpDest, y, y+h, x, light);
        DrawVLine(bmpDest, y, y+h, x+w, dark);
    }
}


///////////////////
// Show a message box
MessageBoxReturnType Menu_MessageBox(const std::string& sTitle, const std::string& sText, MessageBoxType type)
{
	if(bDedicated) {
		hints << "Menu_MessageBox: " << sTitle << ": " << sText << endl;
		switch(type) {
			case LMB_OK: return MBR_OK;
			case LMB_YESNO:
				hints << "Dedicated server is positive and says YES." << endl;
				return MBR_YES;
		}
		return MBR_OK;
	}

	MessageBoxReturnType ret = MBR_INVALID;
	gui_event_t *ev = NULL;

	SetGameCursor(CURSOR_ARROW);

	int minw = 350;
	int maxw = 500;
	int x = 160;
	int y = 170;
	int w = minw; // the whole box
	int h = 140; // including caption and button, the whole box

	// Adjust the width
	int longest_line = w;
	std::vector<std::string> lines = explode(sText, "\n");
	std::vector<std::string>::const_iterator it;
	for (it=lines.begin(); it!=lines.end(); it++)  {
		int tw = tLX->cFont.GetWidth(*it);
		if (tw > longest_line)
			longest_line = tw;
	}
	w = CLAMP(longest_line + 40, minw, maxw);
	// Center within the menu canvas (menuWidth); the menu itself is centered.
	x = (VideoPostProcessor::menuWidth - w) / 2;

	// Handle multiline messages
	lines = splitstring(sText, (size_t)-1, w - 2, tLX->cFont);
	
	const int line_hspace = 2;
	const int button_h = 24;
	const int caption_h = 25;

	if( (tLX->cFont.GetHeight() + line_hspace) * (int)lines.size() + button_h + caption_h + 2 > h ) {
		// TODO: hardcoded screen height (480)
		h = (int)MIN( (tLX->cFont.GetHeight() + line_hspace) * lines.size() + 90, (size_t)478);		
		y = 240-h/2;
	}

	int cx = x+w/2;
	int cy = y + caption_h;
	if(lines.size() > 0) {
		cy += (h - button_h - caption_h) / 2;
		cy -= ((int)(lines.size() - 1) * (tLX->cFont.GetHeight() + line_hspace)) / 2;
		cy -= tLX->cFont.GetHeight() / 2;
	}

	//
	// Setup the gui
	//
	CGuiLayout msgbox;

	msgbox.Initialize();

	if(type == LMB_OK)
		msgbox.Add( new CButton(BUT_OK,tMenu->bmpButtons), 0, cx-20,y+h-button_h, 40,15);
	else if(type == LMB_YESNO) {
		msgbox.Add( new CButton(BUT_YES,tMenu->bmpButtons), 1, x+15,y+h-button_h, 35,15);
		msgbox.Add( new CButton(BUT_NO,tMenu->bmpButtons),  2, x+w-35,y+h-button_h, 30,15);
	}

	// Store the old buffer into a temp buffer to keep it
	SDL_BlitSurface(tMenu->bmpBuffer.get(), NULL, tMenu->bmpMsgBuffer.get(), NULL);


	// Draw to the buffer
	//DrawImage(tMenu->bmpBuffer, shadow, 177,167);
	Menu_DrawBox(tMenu->bmpBuffer.get(), x, y, x+w, y+h);
	DrawRectFill(tMenu->bmpBuffer.get(), x+2,y+2, x+w-1,y+h-1,tLX->clDialogBackground);
	DrawRectFill(tMenu->bmpBuffer.get(), x+2,y+2, x+w-1,y+caption_h,tLX->clDialogCaption);

	tLX->cFont.DrawCentre(tMenu->bmpBuffer.get(), cx, y+5, tLX->clNormalLabel,sTitle);
	for (it=lines.begin(); it!=lines.end(); it++)  {
		cx = x+w/2;//-(tLX->cFont.GetWidth(lines[i])+30)/2;
		tLX->cFont.DrawCentre(tMenu->bmpBuffer.get(), cx, cy, tLX->clNormalLabel, *it);
		cy += tLX->cFont.GetHeight()+line_hspace;
	}

	Menu_RedrawMouse(true);


	ProcessEvents();

	
	while(true) {
		Menu_RedrawMouse(true);

		SetGameCursor(CURSOR_ARROW);

		DrawImageAdv(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer, x,y, x,y, w, h);

		// Process the gui
		ev = msgbox.Process();
		msgbox.Draw(VideoPostProcessor::videoSurface().get());

		if(ev) {

			if(ev->cWidget->getType() == wid_Button)
				SetGameCursor(CURSOR_HAND);
			if(ev->cWidget->getType() == wid_Textbox)
				SetGameCursor(CURSOR_TEXT);

			if(ev->iEventMsg == BTN_CLICKED) {
				switch(ev->iControlID) {

					// OK
					case 0:
						ret = MBR_OK;
						break;

					// Yes
					case 1:
						ret = MBR_YES;
						break;

					// No
					case 2:
						ret = MBR_NO;
						break;
				}
			}
		}

		// Handle the Enter key
		if (WasKeyboardEventHappening(SDLK_RETURN) || WasKeyboardEventHappening(SDLK_KP_ENTER))
		{
			if (type == LMB_YESNO)  {
				ret = MBR_YES;
				break;
			}
			else  {
				ret = MBR_OK;
				break;
			}
		}

		if(!WasKeyboardEventHappening(SDLK_ESCAPE) && game.state != Game::S_Quit && ret == MBR_INVALID) {
			DrawCursor(VideoPostProcessor::videoSurface().get());
			doVideoFrameInMainThread();
			CapFPS();
			tLX->currentTime = GetTime(); // we need this for CapFPS()
			ProcessEvents();
		} else
			break;
	}

	SetGameCursor(CURSOR_ARROW);
	msgbox.Shutdown();

	// Restore the old buffer
	SDL_BlitSurface(tMenu->bmpMsgBuffer.get(), NULL, tMenu->bmpBuffer.get(), NULL);
	//Menu_RedrawMouse(true);
	//doVideoFrameInMainThread();

	return ret;
}

///////////////////
// Add all the default widgets
void Menu_AddDefaultWidgets()
{
// 34 layouts total

// L_MAINMENU: 6 widgets
    LayoutWidgets[L_MAINMENU].Add("LocalPlay");
    LayoutWidgets[L_MAINMENU].Add("NetPlay");
    LayoutWidgets[L_MAINMENU].Add("PlayerProfiles");
    LayoutWidgets[L_MAINMENU].Add("LevelEditor");
    LayoutWidgets[L_MAINMENU].Add("Options");
    LayoutWidgets[L_MAINMENU].Add("Quit");

// L_LOCALPLAY: 9 widgets
    LayoutWidgets[L_LOCALPLAY].Add("Back");
    LayoutWidgets[L_LOCALPLAY].Add("Start");
    LayoutWidgets[L_LOCALPLAY].Add("Playing");
    LayoutWidgets[L_LOCALPLAY].Add("PlayerList");
    LayoutWidgets[L_LOCALPLAY].Add("LevelList");
    LayoutWidgets[L_LOCALPLAY].Add("Gametype");
    LayoutWidgets[L_LOCALPLAY].Add("ModName");
    LayoutWidgets[L_LOCALPLAY].Add("GameSettings");
    LayoutWidgets[L_LOCALPLAY].Add("WeaponOptions");

// L_GAMESETTINGS: 9 widgets
    LayoutWidgets[L_GAMESETTINGS].Add("gs_Ok");
    LayoutWidgets[L_GAMESETTINGS].Add("gs_Default");
    LayoutWidgets[L_GAMESETTINGS].Add("Lives");
    LayoutWidgets[L_GAMESETTINGS].Add("MaxKills");
    LayoutWidgets[L_GAMESETTINGS].Add("LoadingTime");
    LayoutWidgets[L_GAMESETTINGS].Add("LoadingTimeLabel");
    LayoutWidgets[L_GAMESETTINGS].Add("Bonuses");
    LayoutWidgets[L_GAMESETTINGS].Add("ShowBonusNames");
    LayoutWidgets[L_GAMESETTINGS].Add("MaxTime");

// L_WEAPONOPTIONS: 8 widgets
    LayoutWidgets[L_WEAPONOPTIONS].Add("wr_Ok");
    LayoutWidgets[L_WEAPONOPTIONS].Add("wr_Scroll");
    LayoutWidgets[L_WEAPONOPTIONS].Add("wr_Reset");
    LayoutWidgets[L_WEAPONOPTIONS].Add("wr_ListBox");
    LayoutWidgets[L_WEAPONOPTIONS].Add("wr_Cancel");
    LayoutWidgets[L_WEAPONOPTIONS].Add("wr_Random");
    LayoutWidgets[L_WEAPONOPTIONS].Add("wr_Load");
    LayoutWidgets[L_WEAPONOPTIONS].Add("wr_Save");

// L_LOADWEAPONS: 4 widgets
    LayoutWidgets[L_LOADWEAPONS].Add("wp_Cancel");
    LayoutWidgets[L_LOADWEAPONS].Add("wp_Ok");
    LayoutWidgets[L_LOADWEAPONS].Add("wp_PresetList");
    LayoutWidgets[L_LOADWEAPONS].Add("wp_PresetName");

// L_SAVEWEAPONS: 4 widgets
    LayoutWidgets[L_SAVEWEAPONS].Add("wp_Cancel");
    LayoutWidgets[L_SAVEWEAPONS].Add("wp_Ok");
    LayoutWidgets[L_SAVEWEAPONS].Add("wp_PresetList");
    LayoutWidgets[L_SAVEWEAPONS].Add("wp_PresetName");

// L_NET: 4 widgets
    LayoutWidgets[L_NET].Add("InternetTab");
    LayoutWidgets[L_NET].Add("LANTab");
    LayoutWidgets[L_NET].Add("HostTab");
    LayoutWidgets[L_NET].Add("FavouritesTab");

// L_NETINTERNET: 8 widgets
    LayoutWidgets[L_NETINTERNET].Add("Join");
    LayoutWidgets[L_NETINTERNET].Add("ServerList");
    LayoutWidgets[L_NETINTERNET].Add("Refresh");
    LayoutWidgets[L_NETINTERNET].Add("UpdateList");
    LayoutWidgets[L_NETINTERNET].Add("AddServer");
    LayoutWidgets[L_NETINTERNET].Add("Back");
    LayoutWidgets[L_NETINTERNET].Add("PopupMenu");
    LayoutWidgets[L_NETINTERNET].Add("PlayerSelection");

// L_INTERNETDETAILS: 1 widgets
    LayoutWidgets[L_INTERNETDETAILS].Add("id_Ok");

// L_ADDSERVER: 3 widgets
    LayoutWidgets[L_ADDSERVER].Add("na_Cancel");
    LayoutWidgets[L_ADDSERVER].Add("na_Add");
    LayoutWidgets[L_ADDSERVER].Add("na_Address");

// L_NETLAN: 6 widgets
    LayoutWidgets[L_NETLAN].Add("Join");
    LayoutWidgets[L_NETLAN].Add("ServerList");
    LayoutWidgets[L_NETLAN].Add("Refresh");
    LayoutWidgets[L_NETLAN].Add("Back");
    LayoutWidgets[L_NETLAN].Add("PopupMenu");
    LayoutWidgets[L_NETLAN].Add("PlayerSelection");

// L_LANDETAILS: 1 widgets
    LayoutWidgets[L_LANDETAILS].Add("ld_Ok");

// L_NETHOST: 10 widgets
    LayoutWidgets[L_NETHOST].Add("Back");
    LayoutWidgets[L_NETHOST].Add("Ok");
    LayoutWidgets[L_NETHOST].Add("PlayerList");
    LayoutWidgets[L_NETHOST].Add("Playing");
    LayoutWidgets[L_NETHOST].Add("Servername");
    LayoutWidgets[L_NETHOST].Add("MaxPlayers");
    LayoutWidgets[L_NETHOST].Add("Register");
    LayoutWidgets[L_NETHOST].Add("Password");
    LayoutWidgets[L_NETHOST].Add("WelcomeMessage");
    LayoutWidgets[L_NETHOST].Add("AllowWantsJoin");

// L_NETFAVOURITES: 7 widgets
    LayoutWidgets[L_NETFAVOURITES].Add("Join");
    LayoutWidgets[L_NETFAVOURITES].Add("ServerList");
    LayoutWidgets[L_NETFAVOURITES].Add("Refresh");
    LayoutWidgets[L_NETFAVOURITES].Add("Add");
    LayoutWidgets[L_NETFAVOURITES].Add("Back");
    LayoutWidgets[L_NETFAVOURITES].Add("PopupMenu");
    LayoutWidgets[L_NETFAVOURITES].Add("PlayerSelection");

// L_FAVOURITESDETAILS: 1 widgets
    LayoutWidgets[L_FAVOURITESDETAILS].Add("fd_Ok");

// L_RENAMESERVER: 3 widgets
    LayoutWidgets[L_RENAMESERVER].Add("rs_Cancel");
    LayoutWidgets[L_RENAMESERVER].Add("rs_Ok");
    LayoutWidgets[L_RENAMESERVER].Add("rs_NewName");

// L_ADDFAVOURITE: 4 widgets
    LayoutWidgets[L_ADDFAVOURITE].Add("fa_Cancel");
    LayoutWidgets[L_ADDFAVOURITE].Add("fa_Add");
    LayoutWidgets[L_ADDFAVOURITE].Add("fa_Address");
    LayoutWidgets[L_ADDFAVOURITE].Add("fa_Name");

// L_CONNECTING: 1 widgets
    LayoutWidgets[L_CONNECTING].Add("Cancel");

// L_NETJOINLOBBY: 4 widgets
    LayoutWidgets[L_NETJOINLOBBY].Add("Back2");
    LayoutWidgets[L_NETJOINLOBBY].Add("Ready");
    LayoutWidgets[L_NETJOINLOBBY].Add("ChatText");
    LayoutWidgets[L_NETJOINLOBBY].Add("ChatList");

// L_NETHOSTLOBBY: 14 widgets
    LayoutWidgets[L_NETHOSTLOBBY].Add("Back2");
    LayoutWidgets[L_NETHOSTLOBBY].Add("Start");
    LayoutWidgets[L_NETHOSTLOBBY].Add("ChatText");
    LayoutWidgets[L_NETHOSTLOBBY].Add("ChatList");
    LayoutWidgets[L_NETHOSTLOBBY].Add("LevelList");
    LayoutWidgets[L_NETHOSTLOBBY].Add("Lives");
    LayoutWidgets[L_NETHOSTLOBBY].Add("MaxKills");
    LayoutWidgets[L_NETHOSTLOBBY].Add("ModName");
    LayoutWidgets[L_NETHOSTLOBBY].Add("Gametype");
    LayoutWidgets[L_NETHOSTLOBBY].Add("GameSettings");
    LayoutWidgets[L_NETHOSTLOBBY].Add("WeaponOptions");
    LayoutWidgets[L_NETHOSTLOBBY].Add("PopupMenu");
    LayoutWidgets[L_NETHOSTLOBBY].Add("Banned");
    LayoutWidgets[L_NETHOSTLOBBY].Add("ServerSettings");

// L_SERVERSETTINGS: 7 widgets
    LayoutWidgets[L_SERVERSETTINGS].Add("ss_Ok");
    LayoutWidgets[L_SERVERSETTINGS].Add("ss_Cancel");
    LayoutWidgets[L_SERVERSETTINGS].Add("ss_AllowOnlyList");
    LayoutWidgets[L_SERVERSETTINGS].Add("ss_WelcomeMessage");
    LayoutWidgets[L_SERVERSETTINGS].Add("ss_ServerName");
    LayoutWidgets[L_SERVERSETTINGS].Add("ss_AllowWantsJoin");
    LayoutWidgets[L_SERVERSETTINGS].Add("ss_MaxPlayers");

// L_BANLIST: 4 widgets
    LayoutWidgets[L_BANLIST].Add("bl_Close");
    LayoutWidgets[L_BANLIST].Add("bl_Clear");
    LayoutWidgets[L_BANLIST].Add("bl_Unban");
    LayoutWidgets[L_BANLIST].Add("bl_ListBox");

// L_PLAYERPROFILES: 2 widgets
    LayoutWidgets[L_PLAYERPROFILES].Add("NewPlayerTab");
    LayoutWidgets[L_PLAYERPROFILES].Add("ViewPlayersTab");

// L_CREATEPLAYER: 12 widgets
    LayoutWidgets[L_CREATEPLAYER].Add("np_Back");
    LayoutWidgets[L_CREATEPLAYER].Add("np_Create");
    LayoutWidgets[L_CREATEPLAYER].Add("np_Name");
    LayoutWidgets[L_CREATEPLAYER].Add("np_Red");
    LayoutWidgets[L_CREATEPLAYER].Add("np_Blue");
    LayoutWidgets[L_CREATEPLAYER].Add("np_Green");
    LayoutWidgets[L_CREATEPLAYER].Add("np_Type");
    LayoutWidgets[L_CREATEPLAYER].Add("np_AIDiffLbl");
    LayoutWidgets[L_CREATEPLAYER].Add("np_AIDiff");
    LayoutWidgets[L_CREATEPLAYER].Add("np_PlySkin");
    LayoutWidgets[L_CREATEPLAYER].Add("np_Username");
    LayoutWidgets[L_CREATEPLAYER].Add("np_Password");

// L_VIEWPLAYERS: 12 widgets
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_Back");
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_Name");
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_Red");
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_Blue");
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_Green");
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_Players");
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_Delete");
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_Apply");
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_Type");
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_AIDiffLbl");
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_AIDiff");
    LayoutWidgets[L_VIEWPLAYERS].Add("vp_PlySkin");

// L_LEVELEDITOR: 5 widgets
    LayoutWidgets[L_LEVELEDITOR].Add("map_new");
    LayoutWidgets[L_LEVELEDITOR].Add("map_random");
    LayoutWidgets[L_LEVELEDITOR].Add("map_load");
    LayoutWidgets[L_LEVELEDITOR].Add("map_save");
    LayoutWidgets[L_LEVELEDITOR].Add("map_quit");

// L_NEWDIALOG: 5 widgets
    LayoutWidgets[L_NEWDIALOG].Add("mn_Cancel");
    LayoutWidgets[L_NEWDIALOG].Add("mn_Ok");
    LayoutWidgets[L_NEWDIALOG].Add("mn_Width");
    LayoutWidgets[L_NEWDIALOG].Add("mn_Height");
    LayoutWidgets[L_NEWDIALOG].Add("mn_Scheme");

// L_SAVELOADLEVEL: 4 widgets
    LayoutWidgets[L_SAVELOADLEVEL].Add("sl_Cancel");
    LayoutWidgets[L_SAVELOADLEVEL].Add("sl_Ok");
    LayoutWidgets[L_SAVELOADLEVEL].Add("sl_FileList");
    LayoutWidgets[L_SAVELOADLEVEL].Add("sl_FileName");

// L_OPTIONS: 3 widgets
    LayoutWidgets[L_OPTIONS].Add("ControlsTab");
    LayoutWidgets[L_OPTIONS].Add("GameTab");
    LayoutWidgets[L_OPTIONS].Add("SystemTab");

// L_OPTIONSCONTROLS: 23 widgets
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply1_Up");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply1_Down");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply1_Left");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply1_Right");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply1_Shoot");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply1_Jump");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply1_Selweapon");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply1_Rope");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply2_Up");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply2_Down");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply2_Left");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply2_Right");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply2_Shoot");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply2_Jump");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply2_Selweapon");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Ply2_Rope");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Gen_Chat");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Gen_Score");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Gen_Health");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Gen_CurSettings");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Gen_TakeScreenshot");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Gen_ViewportManager");
    LayoutWidgets[L_OPTIONSCONTROLS].Add("Gen_SwitchMode");

// L_OPTIONSGAME: 9 widgets
    LayoutWidgets[L_OPTIONSGAME].Add("BloodAmount");
    LayoutWidgets[L_OPTIONSGAME].Add("Shadows");
    LayoutWidgets[L_OPTIONSGAME].Add("Particles");
    LayoutWidgets[L_OPTIONSGAME].Add("OldSkoolRope");
    LayoutWidgets[L_OPTIONSGAME].Add("AIDifficulty");
    LayoutWidgets[L_OPTIONSGAME].Add("ShowWormHealth");
    LayoutWidgets[L_OPTIONSGAME].Add("ColorizeNicks");
    LayoutWidgets[L_OPTIONSGAME].Add("AutoTyping");
    LayoutWidgets[L_OPTIONSGAME].Add("ScreenshotFormat");

// L_OPTIONSSYSTEM: 12 widgets
    LayoutWidgets[L_OPTIONSSYSTEM].Add("Back");
    LayoutWidgets[L_OPTIONSSYSTEM].Add("Fullscreen");
    LayoutWidgets[L_OPTIONSSYSTEM].Add("OpenGL");
    LayoutWidgets[L_OPTIONSSYSTEM].Add("SoundOn");
    LayoutWidgets[L_OPTIONSSYSTEM].Add("SoundVolume");
    LayoutWidgets[L_OPTIONSSYSTEM].Add("NetworkPort");
    LayoutWidgets[L_OPTIONSSYSTEM].Add("NetworkSpeed");
    LayoutWidgets[L_OPTIONSSYSTEM].Add("ShowFPS");
    LayoutWidgets[L_OPTIONSSYSTEM].Add("ShowPing");
    LayoutWidgets[L_OPTIONSSYSTEM].Add("Filtered");
    LayoutWidgets[L_OPTIONSSYSTEM].Add("LogConvos");
    LayoutWidgets[L_OPTIONSSYSTEM].Add("Apply");

// L_MESSAGEBOXOK: 1 widgets
    LayoutWidgets[L_MESSAGEBOXOK].Add("mb_Ok");

// L_MESSAGEBOXYESNO: 2 widgets
    LayoutWidgets[L_MESSAGEBOXYESNO].Add("mb_Yes");
    LayoutWidgets[L_MESSAGEBOXYESNO].Add("mb_No");
}


	// Load the level list
	struct LevelComboFiller {
		CCombobox* cmb;
		LevelComboFiller(CCombobox* c) : cmb(c) {}
		bool operator() (const std::string& filename) {
			LevelInfo info = infoForLevel(filename, true);
			if(info.valid)
				cmb->addItem(info.path, info.name + " [" + info.typeShort.get() + "]");

			return true;
		}
	};


///////////////////
// Fill a listbox with the levels
void Menu_FillLevelList(CCombobox *cmb, int random)
{
	cmb->clear();
	cmb->setSorted(SORT_ASC);
	cmb->setUnique(true);

	LevelComboFiller filler(cmb);
	FindFiles(filler, "levels", false);

	// Disable sorting and add the random level at the beginning
	cmb->setSorted(SORT_NONE);
	//if(random) // If random is true, we add the 'random' level to the list
	//	cmb->addItem(0, "_random_", "- Random level -");

	cmb->setCurSIndexItem(gameSettings[FT_Map].as<LevelInfo>()->path);
}


///////////////////
// Redraw a section from the buffer to the screen
void Menu_redrawBufferRect(int x, int y, int w, int h)
{
    DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, x,y, x,y, w,h);
}



void Menu_DisableNetEvents()
{
	for (size_t i = 0; i < sizeof(tMenu->tSocket)/sizeof(tMenu->tSocket[0]); ++i)
		if(i != SCK_FOO)
			tMenu->tSocket[i]->setWithEvents(false);
}

void Menu_EnableNetEvents()
{
	for (size_t i = 0; i < sizeof(tMenu->tSocket)/sizeof(tMenu->tSocket[0]); ++i)
		if(i != SCK_FOO)
			tMenu->tSocket[i]->setWithEvents(true);
}

	
	
bool bGotDetails = false;
bool bOldLxBug = false;
int nTries = 0;
AbsTime fStart;
CListview lvInfo;


///////////////////
// Draw a 'server info' box
void Menu_SvrList_DrawInfo(const std::string& szAddress, int w, int h)
{
	int y = tMenu->bmpBuffer.get()->h/2 - h/2;
	int x = tMenu->bmpBuffer.get()->w/2 - w/2;

	Menu_redrawBufferRect(x,y,w,h);

    Menu_DrawBox(VideoPostProcessor::videoSurface().get(), x,y, x+w, y+h);
	DrawRectFillA(VideoPostProcessor::videoSurface().get(), x+2,y+2, x+w-1, y+h-1, tLX->clDialogBackground, 230);
    tLX->cFont.DrawCentre(VideoPostProcessor::videoSurface().get(), x+w/2, y+5, tLX->clNormalLabel, "Server Details");


	server_t::Ptr svr = ServerList::get()->findServerStr(szAddress);
	NetworkAddr origAddr;
	if(svr) {
		if(IsNetAddrValid(svr->sAddress)) {
			origAddr = svr->sAddress;
		} else {
			tLX->cFont.DrawCentre(VideoPostProcessor::videoSurface().get(), x+w/2, y+h/2-8, tLX->clNormalLabel,  "Resolving domain ...");
			return;
		}
	} else {
		warnings << "Querying server not from svr list: " << szAddress << endl;
		std::string tmp_addr = szAddress;
		TrimSpaces(tmp_addr);
		if(!StringToNetAddr(tmp_addr, origAddr)) {
			// TODO: this happens also, if the server is not in the serverlist
			// we should do the domain resolving also here by ourselfs
			tLX->cFont.DrawCentre(VideoPostProcessor::videoSurface().get(), x+w/2,y+tLX->cFont.GetHeight()+10, tLX->clError, "DNS not resolved");
			return;
		}
	}


    // Get the server details
    std::string		szName;
    int				nMaxWorms = 0;
    int				nState = 0;
    std::string		szMapName;
    std::string		szModName;
    int				nGameMode = 0;
    int				nLives = 0;
    int				nMaxKills = 0;
    int				nLoadingTime = 0;
    int				nBonuses = 0;
    int				nNumPlayers = 0;
	IpInfo			tIpInfo;
	std::string		sIP;
    CWorm			cWorms[MAX_WORMS];
	bool			bHaveLives = false;
	std::string		sServerVersion;
	bool			bHaveGameSpeed = false;
	float			fGameSpeed = 1.0f;
	FeatureCompatibleSettingList features;
	
    CBytestream inbs;
    NetworkAddr   addr;

    if(nTries < 3 && !bGotDetails && !bOldLxBug) {

		tLX->cFont.DrawCentre(VideoPostProcessor::videoSurface().get(), x+w/2, y+h/2-8, tLX->clNormalLabel,  "Loading info...");

        if (inbs.Read(tMenu->tSocket[SCK_NET])) {
            // Check for connectionless packet header
	        if(inbs.readInt(4) == -1) {
                std::string cmd = inbs.readString();

		        addr = tMenu->tSocket[SCK_NET]->remoteAddress();
				
				if(cmd == "lx::traverse") // Response from UDP masterserver
				{
					sIP = inbs.readString();
					StringToNetAddr(sIP, addr);
					if( !inbs.isPosAtEnd() )
						cmd = inbs.readString();
				}
					
		        // Check for server info
		        if(cmd == "lx::serverinfo") {
                    bGotDetails = true;

					sServerVersion = "LieroX 0.56";

					// Get the IP info
					if (NetAddrToString(addr, sIP))
						tIpInfo = tIpToCountryDB->GetInfoAboutIP(sIP);
					else  {
						tIpInfo.countryName = "Hackerland";
						tIpInfo.continent = "Hackerland";
						sIP = "x.x.x.x";
					}


                    // Read the info
                    szName = Utf8String(inbs.readString(64));
	                nMaxWorms = MIN(MAX_PLAYERS, MAX((int)inbs.readByte(), 0));
	                nState = inbs.readByte();

					if (nState < 0)  {
						bOldLxBug = true;
					}

                    szMapName = inbs.readString(256);
					// Adjust the map name
					if (szMapName.find("levels/") == 0)
						szMapName.erase(0,7); // Remove the path if present
					szMapName = CMap::GetLevelName(szMapName);


                    szModName = inbs.readString(256);
	                nGameMode = inbs.readByte();
	                nLives = inbs.readInt16();
	                nMaxKills = inbs.readInt16();
	                nLoadingTime = inbs.readInt16();
					if(nLoadingTime < 0)  {
						bOldLxBug = true;
					}
                    nBonuses = inbs.readByte();

                    // Worms
                    nNumPlayers = inbs.readByte();
					if (nNumPlayers < 0)  {
						bOldLxBug = true;
					}

					// Check
					nNumPlayers = MIN(nMaxWorms,nNumPlayers);

					int i;
                    for(i=0; i<nNumPlayers; i++) {
                        cWorms[i].setName(inbs.readString());
                        cWorms[i].setKills(inbs.readInt(2));
                    }

					if (nState == 1 && !bOldLxBug)  { // Loading and no bug? Must be a fixed version -> LXP/OLX b1 or 2
						sServerVersion = "LXP or OLX beta1/beta2";
					}

					// Lives (only OLX servers)
					if(!inbs.isPosAtEnd())  {
						sServerVersion = "OpenLieroX/0.57_Beta3";
						bHaveLives = true;
						for(i=0; i<nNumPlayers; i++)
							cWorms[i].setLives(inbs.readInt(2));
					}

					// IPs
					if(!inbs.isPosAtEnd())  {
						sServerVersion = "OpenLieroX/0.57_Beta4";
						for(i=0; i<nNumPlayers; i++)
							inbs.readString(); // ignore
					}

					if(!inbs.isPosAtEnd())  {
						sServerVersion = inbs.readString();
					}

					if(!inbs.isPosAtEnd())  {
						bHaveGameSpeed = true;
						fGameSpeed = inbs.readFloat();
					}

					// since Beta9
					if(!inbs.isPosAtEnd())  {
						int ftC = inbs.readInt(2);
						for(int i = 0; i < ftC; ++i) {
							std::string name = inbs.readString();
							std::string humanName = inbs.readString();
							ScriptVar_t value; inbs.readVar(value);
							bool olderClientsSupported = inbs.readBool();
							Feature* f = featureByName(name);
							if(f) {
								features.set(name, humanName, value, FeatureCompatibleSettingList::Feature::FCSL_SUPPORTED);
							} else if(!olderClientsSupported) {
								features.set(name, humanName, value, FeatureCompatibleSettingList::Feature::FCSL_JUSTUNKNOWN);
							} else {
								features.set(name, humanName, value, FeatureCompatibleSettingList::Feature::FCSL_INCOMPATIBLE);
							}
						}
					}
					
                }
            }
        }

        if((tLX->currentTime - fStart > 1) && !bGotDetails) {
            nTries++;
            fStart = tLX->currentTime;
			bGotDetails = false;
			bOldLxBug = false;

			if(svr)
				ServerList::get()->getServerInfo(svr);
        }

		// Got details, fill in the listview
		if (bGotDetails && !bOldLxBug)  {

			// States and gamemodes
   			const std::string states[] = {"Open", "Loading", "Playing", "Unknown"};
			const std::string gamemodes[] = {"Deathmatch","Team Deathmatch", "Tag", "Demolitions", "Unknown"};


			// Checks
			if (nState < 0 || nState > 2)
				nState = 3;
			if (nGameMode < 0 || nGameMode > 3)
				nGameMode = 4;
			nNumPlayers = MIN(nNumPlayers, MAX_WORMS-1);


			// Setup the listview
			lvInfo.Setup(0, x + 15, y+5, w - 30, h - 25);
			lvInfo.setDrawBorder(false);
			lvInfo.setRedrawMenu(false);
			lvInfo.setShowSelect(false);
			lvInfo.setOldStyle(true);
			// TODO: will the listview have scrollbars if too long? if not, please add this

			lvInfo.Destroy(); // Clear any old info

			// Columns
			int first_column_width = tLX->cFont.GetWidth("Loading Times:") + 30; // Width of the widest item in this column + some space
			int last_column_width = tLX->cFont.GetWidth("999"); // Kills width
			lvInfo.AddColumn("", first_column_width);
			lvInfo.AddColumn("", lvInfo.getWidth() - first_column_width - (last_column_width*2) - gfxGUI.bmpScrollbar.get()->w); // The rest
			lvInfo.AddColumn("", last_column_width);
			lvInfo.AddColumn("", last_column_width);

			int index = 0;  // Current item index

			// Server name
			lvInfo.AddItem("servername", index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "Server name:", (DynDrawIntf*)NULL, NULL);
			lvInfo.AddSubitem(LVS_TEXT, szName, (DynDrawIntf*)NULL, NULL);

			// server version
			lvInfo.AddItem("serverversion", index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "Server version:", (DynDrawIntf*)NULL, NULL);
			lvInfo.AddSubitem(LVS_TEXT, sServerVersion, (DynDrawIntf*)NULL, NULL);

			// Country and continent
			lvInfo.AddItem("country", ++index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "Location:", (DynDrawIntf*)NULL, NULL);
			if (tIpInfo.hasCityLevel)
				lvInfo.AddSubitem(LVS_TEXT, tIpInfo.city + ", " + tIpInfo.countryName + " (" + tIpInfo.continent + ")", (DynDrawIntf*)NULL, NULL);
			else
				lvInfo.AddSubitem(LVS_TEXT, tIpInfo.countryName + " (" + tIpInfo.continent + ")", (DynDrawIntf*)NULL, NULL);

			// IP address
			lvInfo.AddItem("ip", ++index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "IP Address:", (DynDrawIntf*)NULL, NULL);
			lvInfo.AddSubitem(LVS_TEXT, sIP, (DynDrawIntf*)NULL, NULL);

			// Map name
			lvInfo.AddItem("mapname", ++index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "Level name:", (DynDrawIntf*)NULL, NULL);
			lvInfo.AddSubitem(LVS_TEXT, szMapName, (DynDrawIntf*)NULL, NULL);

			// Mod name
			lvInfo.AddItem("modname", ++index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "Mod name:", (DynDrawIntf*)NULL, NULL);
			lvInfo.AddSubitem(LVS_TEXT, szModName, (DynDrawIntf*)NULL, NULL);

			// State
			lvInfo.AddItem("state", ++index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "State:", (DynDrawIntf*)NULL, NULL);
			lvInfo.AddSubitem(LVS_TEXT, states[nState], (DynDrawIntf*)NULL, NULL);

			// Playing
			lvInfo.AddItem("playing", ++index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "Playing:", (DynDrawIntf*)NULL, NULL);
			lvInfo.AddSubitem(LVS_TEXT, itoa(nNumPlayers) + " / " + itoa(nMaxWorms), (DynDrawIntf*)NULL, NULL);

			// Game type
			lvInfo.AddItem("game type", ++index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "Game Type:", (DynDrawIntf*)NULL, NULL);
			lvInfo.AddSubitem(LVS_TEXT, gamemodes[nGameMode], (DynDrawIntf*)NULL, NULL);

			// Lives
			lvInfo.AddItem("lives", ++index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "Lives:", (DynDrawIntf*)NULL, NULL);
			if (nLives < 0)
				lvInfo.AddSubitem(LVS_IMAGE, "", gfxGame.bmpInfinite, NULL);
			else
				lvInfo.AddSubitem(LVS_TEXT, itoa(nLives), (DynDrawIntf*)NULL, NULL);

			// Max kills
			lvInfo.AddItem("maxkills", ++index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "Max Kills:", (DynDrawIntf*)NULL, NULL);
			if (nMaxKills < 0)
				lvInfo.AddSubitem(LVS_IMAGE, "", gfxGame.bmpInfinite, NULL);
			else
				lvInfo.AddSubitem(LVS_TEXT, itoa(nMaxKills), (DynDrawIntf*)NULL, NULL);

			// Loading times
			lvInfo.AddItem("loading", ++index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "Loading Times:", (DynDrawIntf*)NULL, NULL);
			lvInfo.AddSubitem(LVS_TEXT, itoa(nLoadingTime) + " %", (DynDrawIntf*)NULL, NULL);

			// Separator
			lvInfo.AddItem("", ++index, tLX->clNormalLabel);

			// Players / kills / lives
			lvInfo.AddItem("players", ++index, tLX->clNormalLabel);
			if (nState)  {
				lvInfo.AddSubitem(LVS_TEXT, "Players/Kills/Lives:", (DynDrawIntf*)NULL, NULL);

				// First player (located next to the Players/Kills/Lives label)
				lvInfo.AddSubitem(LVS_TEXT, cWorms[0].getName(), (DynDrawIntf*)NULL, NULL);
				lvInfo.AddSubitem(LVS_TEXT, itoa(cWorms[0].getKills()), (DynDrawIntf*)NULL, NULL);
				if (bHaveLives)  {
					switch ((short)cWorms[0].getLives())  {
					case -1:  // Out
						lvInfo.AddSubitem(LVS_TEXT, "Out", (DynDrawIntf*)NULL, NULL);
						break;
					case -2:  // Unlim
						lvInfo.AddSubitem(LVS_IMAGE, "", gfxGame.bmpInfinite, NULL);
						break;
					default:
						lvInfo.AddSubitem(LVS_TEXT, itoa(cWorms[0].getLives()), (DynDrawIntf*)NULL, NULL);
					}
				}

				// Rest of the players
				for (int i=1; i < nNumPlayers; i++)  {
					lvInfo.AddItem("players"+itoa(i+1), ++index, tLX->clNormalLabel);
					lvInfo.AddSubitem(LVS_TEXT, "", (DynDrawIntf*)NULL, NULL);
					lvInfo.AddSubitem(LVS_TEXT, cWorms[i].getName(), (DynDrawIntf*)NULL, NULL);
					lvInfo.AddSubitem(LVS_TEXT, itoa(cWorms[i].getKills()), (DynDrawIntf*)NULL, NULL);
					if (bHaveLives)  {
						switch ((short)cWorms[i].getLives())  {
						case -1:  // Out
							lvInfo.AddSubitem(LVS_TEXT, "Out", (DynDrawIntf*)NULL, NULL);
							break;
						case -2:  // Unlim
							lvInfo.AddSubitem(LVS_IMAGE, "", gfxGame.bmpInfinite, NULL);
							break;
						default:
							lvInfo.AddSubitem(LVS_TEXT, itoa(cWorms[i].getLives()), (DynDrawIntf*)NULL, NULL);
						}
					}
				}
			}

			else  { // Don't draw kills when the server is open
				lvInfo.AddSubitem(LVS_TEXT, "Players:", (DynDrawIntf*)NULL, NULL);

				// First player (located next to the Players/Kills label)
				lvInfo.AddSubitem(LVS_TEXT, cWorms[0].getName(), (DynDrawIntf*)NULL, NULL);

				// Rest of the players
				for (int i = 1; i < nNumPlayers; i++)  {
					lvInfo.AddItem("players"+itoa(i+1), ++index, tLX->clNormalLabel);
					lvInfo.AddSubitem(LVS_TEXT, "", (DynDrawIntf*)NULL, NULL);
					lvInfo.AddSubitem(LVS_TEXT, cWorms[i].getName(), (DynDrawIntf*)NULL, NULL);
				}
			}

			// Separator
			lvInfo.AddItem("", ++index, tLX->clNormalLabel);
			
			// Bonuses
			lvInfo.AddItem("bonuses", ++index, tLX->clNormalLabel);
			lvInfo.AddSubitem(LVS_TEXT, "Bonuses:", (DynDrawIntf*)NULL, NULL);
			lvInfo.AddSubitem(LVS_TEXT, nBonuses ? "On" : "Off", (DynDrawIntf*)NULL, NULL);

			if(bHaveGameSpeed) {
				// Loading times
				lvInfo.AddItem("gamespeed", ++index, tLX->clNormalLabel);
				lvInfo.AddSubitem(LVS_TEXT, "Game speed:", (DynDrawIntf*)NULL, NULL);
				lvInfo.AddSubitem(LVS_TEXT, ftoa(fGameSpeed), (DynDrawIntf*)NULL, NULL);
			}
			
			for_each_iterator( FeatureCompatibleSettingList::Feature&, it, GetIteratorRef_second(features.list) ){
				Color col;
				switch(it->get().type) {
					case FeatureCompatibleSettingList::Feature::FCSL_JUSTUNKNOWN: col = tLX->clDisabled; break;
					case FeatureCompatibleSettingList::Feature::FCSL_INCOMPATIBLE: col = tLX->clError; break;
					default: col = tLX->clNormalLabel;
				}
				lvInfo.AddItem("feature:" + it->get().name, ++index, col);
				if(tLX->cFont.GetWidth(it->get().humanName + ":") + 20 <= first_column_width) {
					lvInfo.AddSubitem(LVS_TEXT, it->get().humanName + ":", (DynDrawIntf*)NULL, NULL);
					lvInfo.AddSubitem(LVS_TEXT, it->get().var.toString(), (DynDrawIntf*)NULL, NULL);					
				} else
					lvInfo.AddSubitem(LVS_TEXT, it->get().humanName + ":      " + it->get().var.toString(), (DynDrawIntf*)NULL, NULL);
			}

		}
		else // No details yet
			return;
    }

	// No details, server down
    if(!bGotDetails) {
        tLX->cFont.DrawCentre(VideoPostProcessor::videoSurface().get(), x+w/2,y+tLX->cFont.GetHeight()+10, tLX->clError, "Unable to query server");
        return;
    }

	// Old bug
    if(bOldLxBug) {
        tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), x+15,y+tLX->cFont.GetHeight()+10, tLX->clError, "You can't view details\nof this server because\nLieroX v0.56 contains a bug.\n\nPlease wait until the server\nchanges its state to Playing\nand try again.");
        return;
    }

	// Process the listview events
	mouse_t *Mouse = GetMouse();
	if (lvInfo.InBox(Mouse->X, Mouse->Y))  {
		lvInfo.MouseOver(Mouse);
		if (Mouse->Down)
			lvInfo.MouseDown(Mouse, true);
		else if (Mouse->Up)
			lvInfo.MouseUp(Mouse, false);

		if (Mouse->WheelScrollUp)
			lvInfo.MouseWheelUp(Mouse);
		else if (Mouse->WheelScrollDown)
			lvInfo.MouseWheelDown(Mouse);
	}

	// All ok, draw the details
	lvInfo.Draw( VideoPostProcessor::videoSurface().get() );
}

void Menu_Current_Shutdown() {
	if(!tMenu) return;
	
	// Shutdown all sub-menus
	if(!bDedicated)
		switch(tMenu->iMenuType) {
				
				// Main
			case MNU_MAIN:
				Menu_MainShutdown();
				break;
				
				// Local
			case MNU_LOCAL:
				Menu_LocalShutdown();
				break;
				
				// News
			case MNU_NETWORK:
				Menu_NetShutdown();
				break;
				
				// Player
			case MNU_PLAYER:
				Menu_PlayerShutdown();
				break;
				
				// Map editor
			case MNU_MAPED:
				Menu_MapEdShutdown();
				break;
				
				// Options
			case MNU_OPTIONS:
				Menu_OptionsShutdown();
				break;
		}
	
}
	
} // namespace DeprecatedGUI



void TaskManager::renderTasksStatus(SDL_Surface* s) {
	std::list<std::string> statusTxts;
	
	static const unsigned int MaxEntries = 4;
	{
		ScopedLock lock(mutex);
		for(std::set<Task*>::const_iterator i = runningTasks.begin(); i != runningTasks.end(); ++i) {
			std::string statusTxt = (*i)->statusText();
			if(statusTxt != "") {
				statusTxts.push_back(statusTxt);
				if(statusTxts.size() >= MaxEntries)
					break;
			}
		}
	}
	
	if(!statusTxts.empty()) {
		static const int StatusLoadingLeft = 5;
		static const int StatusLeft = 30;
		static const int StatusTop = 5;
		const int StatusHeight = tLX->cFont.GetHeight() + 5;
		DrawRectFill(s, 0, 0, s->w, StatusTop + StatusHeight * (int)statusTxts.size(), Color(42,73,145,180));
		
		int y = StatusTop;
		for(std::list<std::string>::iterator i = statusTxts.begin(); i != statusTxts.end(); ++i) {
			tLX->cFont.Draw(s, StatusLeft, y, Color(255,255,255), *i);
			y += StatusHeight;
		}
	
		static const int StatusLoadingSize = 9;
		DrawLoadingAni(s, StatusLoadingLeft + StatusLoadingSize, StatusHeight * (int)statusTxts.size() - StatusLoadingSize, StatusLoadingSize, StatusLoadingSize, Color(255,255,255), Color(128,128,128), LAT_CIRCLES);
	}
}
