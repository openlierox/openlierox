/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Net menu - News
// Created 12/8/02
// Jason Boettcher


#include "LieroX.h"
#include "sound/SoundsBase.h"
#include "DeprecatedGUI/Graphics.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CBrowser.h"
#include "DeprecatedGUI/CListview.h"
#include "DeprecatedGUI/CTextbox.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CCheckbox.h"
#include "DeprecatedGUI/CChatWidget.h"
#include "StringUtils.h"
#include "AuxLib.h"
#include "NotifyUser.h"
#include "IRC.h"


namespace DeprecatedGUI {

CGuiLayout	cChat;
static bool cChatGuiInitialized;

enum {
	nc_Back = 0,
	nc_EnableChat,
	nc_EnableMiniChat,
	nc_Chat
};


///////////////////
// Initialize the news net menu
bool Menu_Net_ChatInitialize()
{
	iNetMode = net_chat;

	// Setup the gui layout
	cChat.Shutdown();
	cChat.Initialize();

	cChat.Add( new CButton(BUT_BACK, tMenu->bmpButtons), nc_Back, 25,440, 50,15);

	cChat.Add( new CChatWidget(), nc_Chat, 25, 140, 585, 280);
	cChat.Add( new CCheckbox(tLXOptions->bEnableChat), nc_EnableChat, 100, 440, 20, 20);
	cChat.Add( new CLabel("Enable", tLX->clNormalLabel), -1, 130, 440, 0, 0);

	cChat.Add( new CCheckbox(tLXOptions->bEnableMiniChat), nc_EnableMiniChat, 350, 440, 20, 20);
	cChat.Add( new CLabel("Mini chat in server list", tLX->clNormalLabel), -1, 380, 440, 0, 0);

	Menu_Net_AddTabBarButtons(&cChat);

	cChatGuiInitialized = true;
	return true;
}

//////////////////
// Shutdown the news menu
void Menu_Net_ChatShutdown()
{
	cChatGuiInitialized = false;
	cChat.Shutdown();
}


///////////////////
// The net news menu frame
void Menu_Net_ChatFrame(int mouse)
{
	gui_event_t *ev = NULL;


	// Process & Draw the gui
	ev = cChat.Process();
	cChat.Draw( VideoPostProcessor::videoSurface().get() );

	if (Menu_Net_ProcessTabBarButtons(ev))
		return;

	// Process any events
	if(ev) {

		switch(ev->iControlID) {

			// Back
			case nc_Back:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					// Shutdown
					Menu_Net_ChatShutdown();

					// Back to main menu
					Menu_MainInitialize();
				}
				break;


			case nc_EnableChat:
				if(ev->iEventMsg == CHK_CHANGED) {
					
					PlaySoundSample(sfxGeneral.smpClick);
					tLXOptions->bEnableChat = cChat.SendMessage(nc_EnableChat, CKM_GETCHECK, 1, 1) != 0;
					if (!tLXOptions->bEnableChat)
					{
						((CChatWidget *)cChat.getWidget(nc_Chat))->DisableChat();
						ShutdownIRC();
					}
					else
					{
						InitializeIRC();
						((CChatWidget *)cChat.getWidget(nc_Chat))->EnableChat();
					}
				}
				break;

			case nc_EnableMiniChat:
				if(ev->iEventMsg == CHK_CHANGED) {
					PlaySoundSample(sfxGeneral.smpClick);
					tLXOptions->bEnableMiniChat = cChat.SendMessage(nc_EnableMiniChat, CKM_GETCHECK, 1, 1) != 0;
				}
				break;
		}

	}


	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());
}

}; // namespace DeprecatedGUI
