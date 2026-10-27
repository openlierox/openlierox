/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Net menu
// Created 12/8/02
// Jason Boettcher



#include "LieroX.h"
#include "sound/SoundsBase.h"
#include "Error.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CAnimation.h"
#include "DeprecatedGUI/CProgressbar.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CChatWidget.h"
#include "AuxLib.h"
#include "IRC.h"
#include "DedicatedControl.h"
#include "Debug.h"



namespace DeprecatedGUI {

enum {
	mn_Internet=1000,
	mn_LAN,
	mn_Host,
	mn_Favourites,
	mn_News,
	mn_Chat,
};

// TODO: remove globals
int		iNetMode = net_main;
int		iHostType = 0;
SDL_Rect tLoadingRect;



///////////////////
// Initialize the net menu
bool Menu_NetInitialize(bool withSubMenu)
{
	if(bDedicated) return true; // just ignore
	
	tMenu->iMenuType = MNU_NETWORK;
	iNetMode = net_main;

	// Add all the network sockets to the notifier groups (for events)
	Menu_EnableNetEvents();

	// Create the buffer
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
	Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_NETWORK);
	Menu_RedrawMouse(true);

	// Initialize the IRC support
	InitializeIRC();

	// Return back to the menu we came from
	if(withSubMenu)
		switch(tMenu->iReturnTo)  {
		case net_internet:
			Menu_Net_NETInitialize();
			break;
		case net_lan:
			Menu_Net_LANInitialize();
			break;
		case net_favourites:
			Menu_Net_FavouritesInitialize();
			break;
		case net_news:
			Menu_Net_NewsInitialize();
			break;
		case net_chat:
			Menu_Net_ChatInitialize();
			break;
		default:
			Menu_Net_NETInitialize();
		}

	return true;
}


///////////////////
// Go to the host lobby
void Menu_Net_GotoHostLobby()
{
	if(game.state != Game::S_Lobby) {
		errors << "Menu_Net_HostGotoLobby called but we are not in lobby. game state = " << game.state << endl;
		return;
	}

	tMenu->iMenuType = MNU_NETWORK;
	iNetMode = net_host;

	if(!bDedicated) {
		// Create the buffer
		DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
		if (tMenu->tFrontendInfo.bPageBoxes)
			Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
		Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_NETWORK);
		Menu_RedrawMouse(true);
	}

	bSkipStart = true;

	Menu_Net_HostGotoLobby();
}


///////////////////
// Add the buttons for switching between the net menu tabs, called by each tab
void Menu_Net_AddTabBarButtons(CGuiLayout * layout)
{
	struct Tab { int id; int image; int x; int w; int mode; };
	static const Tab tabs[] = {
		{ mn_Internet,   BUT_INTERNET,   75,  95, net_internet },
		{ mn_LAN,        BUT_LAN,        190, 40, net_lan },
		{ mn_Host,       BUT_HOST,       255, 50, net_host },
		{ mn_Favourites, BUT_FAVOURITES, 330, 105, net_favourites },
		{ mn_News,       BUT_NEWS,       460, 50, net_news },
		{ mn_Chat,       BUT_CHAT,       535, 50, net_chat },
	};

	for (const Tab& tab : tabs) {
		CButton * button = new CButton(tab.image, tMenu->bmpButtons);
		// The current tab is drawn highlighted
		button->setSelected(tab.mode == iNetMode);
		layout->Add( button, tab.id, tab.x, 110, tab.w, 15 );
	}
}

///////////////////
// Close the current tab and open another one
static void Menu_Net_GotoTab(int mode)
{
	CChatWidget::GlobalSetEnabled(false);

	// Call a shutdown on all the highest net menu's
	Menu_Net_MainShutdown();
	Menu_Net_LANShutdown();
	Menu_Net_NETShutdown();
	Menu_Net_HostShutdown();
	Menu_Net_FavouritesShutdown();
	Menu_Net_NewsShutdown();
	Menu_Net_ChatShutdown();

	iNetMode = mode;

	// Redraw the window section
	DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, 20,140,  20,140,  620,340);

	// Initialize the appropriate menu
	switch(iNetMode) {
		case net_lan:
			Menu_Net_LANInitialize();
			break;
		case net_internet:
			Menu_Net_NETInitialize();
			break;
		case net_host:
			Menu_Net_HostInitialize();
			break;
		case net_favourites:
			Menu_Net_FavouritesInitialize();
			break;
		case net_news:
			Menu_Net_NewsInitialize();
			break;
		case net_chat:
			Menu_Net_ChatInitialize();
			break;
	}
}

///////////////////
// Switch the tab if one of the tab buttons was activated, called by each tab.
// Returns true if the tab was switched, the caller must not touch its layout anymore.
bool Menu_Net_ProcessTabBarButtons(gui_event_t *ev)
{
	if (!ev || ev->iEventMsg != BTN_CLICKED)
		return false;

	int mode;
	switch (ev->iControlID) {
		case mn_Internet:   mode = net_internet; break;
		case mn_LAN:        mode = net_lan; break;
		case mn_Host:       mode = net_host; break;
		case mn_Favourites: mode = net_favourites; break;
		case mn_News:       mode = net_news; break;
		case mn_Chat:       mode = net_chat; break;
		default:
			return false;
	}

	PlaySoundSample(sfxGeneral.smpClick);
	Menu_Net_GotoTab(mode);
	return true;
}


///////////////////
// The main net menu frame
void Menu_NetFrame()
{
	int		mouse = 0;

	// Refresh the main window of the screen
	//DrawImageAdv(VideoPostProcessor::videoSurface(), tMenu->bmpBuffer, 20,140,  20,140,  620,340);
	//DrawImageAdv(VideoPostProcessor::videoSurface(), tMenu->bmpBuffer, 120,110, 120,110, 420,30);

	// Continue in the chat tab if a chat popup was left open from a game
	if (CChatWidget::GlobalEnabled() && iNetMode != net_chat &&
		(iNetMode != net_host || iHostType == 0) && iNetMode != net_join)
		Menu_Net_GotoTab(net_chat);

	// Run the frame of the current menu screen
	switch(iNetMode) {

		// Main
		case net_main:
			Menu_Net_MainFrame(mouse);
			break;

		// Internet
		case net_internet:
			Menu_Net_NETFrame(mouse);
			break;

		// LAN
		case net_lan:
			Menu_Net_LANFrame(mouse);
			break;

		// Host
		case net_host:
			Menu_Net_HostFrame(mouse);
			break;

		// Favourites
		case net_favourites:
			Menu_Net_FavouritesFrame(mouse);
			break;

		// Join
		case net_join:
			Menu_Net_JoinFrame(mouse);
			break;

		// News
		case net_news:
			Menu_Net_NewsFrame(mouse);
			break;

		// Chat
		case net_chat:
			Menu_Net_ChatFrame(mouse);
			break;
	}
}

////////////
// Shutdown
void Menu_NetShutdown()
{
	switch(iNetMode) {

		// Main
		case net_main:
			Menu_Net_MainShutdown();
			break;

		// Internet
		case net_internet:
			Menu_Net_NETShutdown();
			break;

		// LAN
		case net_lan:
			Menu_Net_LANShutdown();
			break;

		// Host
		case net_host:
			Menu_Net_HostShutdown();
			break;

		// Favourites
		case net_favourites:
			Menu_Net_FavouritesShutdown();
			break;

		// Join
		case net_join:
			Menu_Net_JoinShutdown();
			break;

		// News
		case net_news:
			Menu_Net_NewsShutdown();
			break;

		// Chat
		case net_chat:
			Menu_Net_ChatShutdown();
			break;
	}

	// Remove all the network sockets from the notifier groups (we don't want any events anymore)
	Menu_DisableNetEvents();
}

void Menu_Net_ServerList_Refresher() {
	if(bDedicated) return; // not needed to do that

	struct Refresher : Action {
		Result handle() {
			switch(iNetMode) {
				case net_internet:
					Menu_Net_NET_ServerList_Refresher();
					break;
				case net_lan:
					Menu_Net_LAN_ServerList_Refresher();
					break;
				case net_favourites:
					Menu_Net_Favourites_ServerList_Refresher();
					break;
				default:
					// ignore
					break;
			}
			return true;
		}
	};
	mainQueue->push(new Refresher());
}
	
}; // namespace DeprecatedGUI


