/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Net menu - Internet
// Created 29/12/02
// Jason Boettcher


#include "LieroX.h"
#include "sound/SoundsBase.h"
#include "Clipboard.h"
#include "AuxLib.h"
#include "HTTP.h"
#include "DeprecatedGUI/Graphics.h"
#include "CClient.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CListview.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CCombobox.h"
#include "DeprecatedGUI/CMenu.h"
#include "DeprecatedGUI/CTextbox.h"
#include "DeprecatedGUI/CChatWidget.h"
#include "ProfileSystem.h"
#include "IpToCountryDB.h"
#include "Debug.h"
#include "game/ServerList.h"




namespace DeprecatedGUI {

CGuiLayout	cInternet;
std::string szNetCurServer;

// Internet widgets
enum {
	mi_Join=0,
	mi_ServerList,
	mi_Refresh,
	mi_UpdateList,
	mi_AddServer,
	mi_Back,
    mi_PopupMenu,
	mi_PlayerSelection,
};

void Menu_Net_NET_ServerList_Refresher() {
	ServerList::get()->fillList( (CListview *)cInternet.getWidget( mi_ServerList ), SLFT_CustomSettings, SvrListSettingsFilter::Load(tLXOptions->sSvrListSettingsFilterCfg) );
}
	
	
///////////////////
// Initialize the Internet menu
bool Menu_Net_NETInitialize()
{
	iNetMode = net_internet;
    szNetCurServer = "";

	cInternet.Shutdown();
	cInternet.Initialize();

	cInternet.Add( new CListview(),								mi_ServerList, 40, 180, 560, 242);
	cInternet.Add( new CButton(BUT_BACK, tMenu->bmpButtons),    mi_Back,       25, 440, 50,  15);
	cInternet.Add( new CButton(BUT_ADD, tMenu->bmpButtons),		mi_AddServer,  140,440, 40,  15);
	cInternet.Add( new CButton(BUT_REFRESH, tMenu->bmpButtons), mi_Refresh,	250,440, 83,  15);
	cInternet.Add( new CButton(BUT_UPDATELIST, tMenu->bmpButtons),	mi_UpdateList,  390,440, 125,  15);
	cInternet.Add( new CButton(BUT_JOIN, tMenu->bmpButtons),    mi_Join,		570,440, 43,  15);
	cInternet.Add( new CLabel("Select player:",tLX->clNormalLabel),-1,		125, 152, 180,15);
	cInternet.Add( new CCombobox(),								mi_PlayerSelection,		225,150, 170,  19);
	if( tLXOptions->bEnableChat && tLXOptions->bEnableMiniChat )
		cInternet.Add( new CChatWidget(),						-1,	25, 15, 585, 85 );


	/*
	  Server list columns

      Connection speed
	  Name
	  State
	  Players
	  Ping
	  Address
    */

	// Add players to the list
	bool validName = false;
	for_each_iterator(SmartPointer<profile_t>, p, GetProfiles()) {
		if(p->get()->iType == PRF_COMPUTER->toInt())
			continue;

		int i = ((CCombobox*) cInternet.getWidget( mi_PlayerSelection ))->addItem(p->get()->sName, p->get()->sName);
		((CCombobox*) cInternet.getWidget( mi_PlayerSelection ))->setImage(p->get()->cSkin.getPreview(), i);
		if( p->get()->sName == tLXOptions->sLastSelectedPlayer )
			validName = true;
	}

	if( ! validName )
		tLXOptions->sLastSelectedPlayer = MainHumanProfile()->sName;

	((CCombobox*) cInternet.getWidget( mi_PlayerSelection ))->setCurSIndexItem( tLXOptions->sLastSelectedPlayer );

    Menu_redrawBufferRect(0, 0, 640, 480);

	cInternet.SendMessage( mi_ServerList, LVS_ADDCOLUMN, "", tLXOptions->iInternetList[0]);
	cInternet.SendMessage( mi_ServerList, LVS_ADDCOLUMN, "Server Name", tLXOptions->iInternetList[1]);
	cInternet.SendMessage( mi_ServerList, LVS_ADDCOLUMN, "State", tLXOptions->iInternetList[2]);
	cInternet.SendMessage( mi_ServerList, LVS_ADDCOLUMN, "Players", tLXOptions->iInternetList[3]);
	cInternet.SendMessage( mi_ServerList, LVS_ADDCOLUMN, "Ping", tLXOptions->iInternetList[4]);

	if (tLXOptions->bUseIpToCountry)
	{	// Too lazy to update tLXOptions, so I'll calculate last column width from width of listview
		//int CountryColumnWidth = 21; // TODO: not used

		// HINT: because this column is optional, it is at the end of the array from options
		cInternet.SendMessage( mi_ServerList, LVS_ADDCOLUMN, "Country", tLXOptions->iInternetList[5]);
	}

	cInternet.SendMessage( mi_ServerList, LVS_ADDCOLUMN, "Address", tLXOptions->iInternetList[6]);

	((CListview*) cInternet.getWidget( mi_ServerList ))->SetSortColumn( tLXOptions->iInternetSortColumn, true ); // Sorting

    // Load the list
	Menu_Net_NET_ServerList_Refresher();
	ServerList::get()->updateUDPList();
	
	Timer("Menu_Net_NETInitialize serverlist timeout", null, NULL, SVRLIST_TIMEOUT, true).startHeadless();
	
	return true;
}


///////////////////
// Shutdown the internet menu
void Menu_Net_NETShutdown()
{
	if (tLXOptions)  {

		// Save the list
		if( iNetMode == net_internet )  {
			ServerList::get()->save();

			CListview* l = (CListview *)cInternet.getWidget(mi_ServerList);
			if(l) {
				// Save the column widths
				for (int i=0;i<7;i++)
					tLXOptions->iInternetList[i] = l->GetColumnWidth(i);

				// Save the sorting column
				tLXOptions->iInternetSortColumn = l->GetSortColumn();
			}
		}

		// Save the selected player
		GuiListItem::Pt item = ((CCombobox*)cInternet.getWidget(mi_PlayerSelection)) ? ((CCombobox*)cInternet.getWidget(mi_PlayerSelection))->getSelectedItem() : NULL;
		if (item.get())
			tLXOptions->sLastSelectedPlayer = item->index();
	}

	cInternet.Shutdown();
}


///////////////////
// Net Internet frame
void Menu_Net_NETFrame(int mouse)
{
	gui_event_t *ev = NULL;
	std::string	addr;


	// Process & Draw the gui
	ev = cInternet.Process();
	cInternet.Draw( VideoPostProcessor::videoSurface().get() );


	// Process the server list
	static bool wasLoadedBefore = false;
	if( ServerList::get()->process() || (tIpToCountryDB->Loaded() && !wasLoadedBefore) ) {
		// Add the servers to the listview
		Menu_Net_NET_ServerList_Refresher();
		wasLoadedBefore = tIpToCountryDB->Loaded();
	}



	// Process any events
	if(ev) {

		switch(ev->iControlID) {

			// Add Server
			case mi_AddServer:
				if(ev->iEventMsg == BTN_CLICKED) {
					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					Menu_Net_NETAddServer();
				}
				break;

			// Back
			case mi_Back:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Shutdown
					Menu_Net_NETShutdown();

					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					// Back to main menu
					Menu_MainInitialize();
				}
				break;

			// Refresh
			case mi_Refresh:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					// Refresh the currently visible servers
					ServerList::get()->refreshList();
					Menu_Net_NET_ServerList_Refresher();
				}
				break;

			// Join
			case mi_Join:
                if(ev->iEventMsg == BTN_CLICKED) {

					addr = "";
					int result = (int)cInternet.SendMessage(mi_ServerList, LVS_GETCURSINDEX, &addr, 0);
					if(result != -1 && addr != "") {

                        // Save the list
                        ServerList::get()->save();

						// Click!
						PlaySoundSample(sfxGeneral.smpClick);

						lv_subitem_t *sub = ((CListview *)cInternet.getWidget(mi_ServerList))->getCurSubitem(1);

						// Join
						if (sub)
							Menu_Net_NETJoinServer(addr,sub->sText);
						return;
					}
				}
				break;

			// Serverlist
			case mi_ServerList:

                // Double click
				if(ev->iEventMsg == LV_DOUBLECLK) {

					/*
					  Now.... Should a double click refresh the server (like tribes)?
					  Or should it join the server like other games???
					*/

					// Just join for the moment
					addr = "";
					int result = (int)cInternet.SendMessage(mi_ServerList, LVS_GETCURSINDEX, &addr, 0);
					lv_subitem_t *sub = ((CListview *)cInternet.getWidget(mi_ServerList))->getCurSubitem(1);
					if(result != -1 && addr != "" && sub) {
                        // Save the list
                        ServerList::get()->save();

						Menu_Net_NETJoinServer(addr,sub->sText);
						return;
					}
				}

                // Right click
                if( ev->iEventMsg == LV_RIGHTCLK ) {
                    addr = "";
					int result = (int)cInternet.SendMessage(mi_ServerList, LVS_GETCURSINDEX, &addr, 0);
					if(result && addr != "") {
                        // Display a menu
                        szNetCurServer = addr;
                        mouse_t *m = GetMouse();

                        cInternet.Add( new CMenu(m->X, m->Y), mi_PopupMenu, 0,0, 640,480 );
                        cInternet.SendMessage( mi_PopupMenu, MNS_ADDITEM, "Delete server",				0 );
                        cInternet.SendMessage( mi_PopupMenu, MNS_ADDITEM, "Refresh server",				1 );
                        cInternet.SendMessage( mi_PopupMenu, MNS_ADDITEM, "Join server",				2 );
						cInternet.SendMessage( mi_PopupMenu, MNS_ADDITEM, "Add to favourites",			3 );
						cInternet.SendMessage( mi_PopupMenu, MNS_ADDITEM, "Send \"I want to join message\"",4 );
						cInternet.SendMessage( mi_PopupMenu, MNS_ADDITEM, "Copy IP to clipboard",		5 );
                        cInternet.SendMessage( mi_PopupMenu, MNS_ADDITEM, "Server details",				6 );
                    }
                }


				// Enter key
				if( ev->iEventMsg == LV_ENTER )  {
					// Join
					addr = "";
					int result = (int)cInternet.SendMessage(mi_ServerList, LVS_GETCURSINDEX, &addr, 0);
					lv_subitem_t *sub = ((CListview *)cInternet.getWidget(mi_ServerList))->getCurSubitem(1);
					if(result != -1 && addr != "" && sub) {
                        // Save the list
                        ServerList::get()->save();

						Menu_Net_NETJoinServer(addr,sub->sText);
						return;
					}
				}

				// Delete
				if( ev->iEventMsg == LV_DELETE )  {
					addr = "";
					int result = (int)cInternet.SendMessage(mi_ServerList, LVS_GETCURSINDEX, &addr, 0);
					if(result && addr != "") {
						ServerList::get()->removeServer(addr);
						// Re-Fill the server list
						Menu_Net_NET_ServerList_Refresher();
					}
				}
				break;

            // Popup menu
            case mi_PopupMenu:
                switch( ev->iEventMsg ) {
                    // Delete the server
                    case MNU_USER+0:
                        ServerList::get()->removeServer(szNetCurServer);
                        break;

                    // Refresh the server
                    case MNU_USER+1:
                        {
							server_t::Ptr sv = ServerList::get()->findServerStr(szNetCurServer);
                            if(sv)
                                ServerList::get()->refreshServer(sv);
                        }
                        break;

                    // Join a server
                    case MNU_USER+2:  {
                        // Save the list
                        ServerList::get()->save();
						lv_subitem_t *sub = ((CListview *)cInternet.getWidget(mi_ServerList))->getCurSubitem(1);
						if (sub)
							Menu_Net_NETJoinServer(szNetCurServer,sub->sText);
						}
                        return;

                    // Add server to favourites
                    case MNU_USER+3:
						{
							server_t::Ptr sv = ServerList::get()->findServerStr(szNetCurServer);
							if (sv)
								ServerList::get()->addFavourite(sv->szName,sv->szAddress);
						}
                        break;

					// Send a "wants to join" message
                    case MNU_USER+4:
						{
							server_t::Ptr sv = ServerList::get()->findServerStr(szNetCurServer);
							std::string Nick;
							cInternet.SendMessage(mi_PlayerSelection, CBS_GETCURNAME, &Nick, 0);
							if (sv)
								ServerList::get()->wantsToJoin(Nick, sv);
						}
                        break;

					// Copy the IP to clipboard
					case MNU_USER+5:
						{
							copy_to_clipboard(szNetCurServer);
						}
						break;

                    // Show server details
                    case MNU_USER+6:
						cInternet.removeWidget(mi_PopupMenu);
                        Menu_Net_NETShowServer(szNetCurServer);
                        break;
                }

                // Re-Fill the server list
				Menu_Net_NET_ServerList_Refresher();

                // Remove the menu widget
                cInternet.SendMessage( mi_PopupMenu, MNM_REDRAWBUFFER, (uintptr_t)0, 0);
                cInternet.removeWidget(mi_PopupMenu);
                break;

			// Update server list
			case mi_UpdateList:
				if(ev->iEventMsg == BTN_CLICKED) {
					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					Menu_Net_NETUpdateList();
				}
				break;
		}

	}

	// F5 updates the list
	if (WasKeyboardEventHappening(SDLK_F5))
		Menu_Net_NETUpdateList();

	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());

}


///////////////////
// Join a server
void Menu_Net_NETJoinServer(const std::string& sAddress, const std::string& sName)
{
	// Fill in the game structure
	CCombobox* combo = (CCombobox *) cInternet.getWidget(mi_PlayerSelection);
	const GuiListItem::Pt item = combo->getSelectedItem();
	if(!item.get()) {
		errors << "no player selected" << endl;
		return;
	}
		
	tLXOptions->sLastSelectedPlayer = item->index();

	if(!JoinServer(sAddress, sName, item->index()))
		return;	
}



// TODO: remove this here!
extern CButton	cNetButtons[6];

///////////////////
// Show an 'add server' box to enter in an address
enum  {
	na_Cancel=0,
	na_Add,
	na_Address
};

void Menu_Net_NETAddServer()
{
	CGuiLayout	cAddSvr;
	gui_event_t *ev = NULL;
	bool		addServerMsg = true;


	// Create the background
	cInternet.Draw( tMenu->bmpBuffer.get() );
	Menu_DrawBox(tMenu->bmpBuffer.get(), 200, 220, 440, 340);
	//DrawImageAdv(tMenu->bmpBuffer, tMenu->bmpMainBack, 202,222, 202,222, 237,117);
    DrawRectFill(tMenu->bmpBuffer.get(), 202,222,439,339,tLX->clDialogBackground);
	for(int i=0;i<6;i++)
		cNetButtons[i].Draw(tMenu->bmpBuffer.get());
	Menu_RedrawMouse(true);


	cAddSvr.Initialize();
	cAddSvr.Add( new CButton(BUT_ADD, tMenu->bmpButtons),	na_Add, 220, 320, 40,15);
	cAddSvr.Add( new CButton(BUT_CANCEL, tMenu->bmpButtons),na_Cancel, 350, 320, 70,15);
	cAddSvr.Add( new CLabel("Add a server", tLX->clNormalLabel),		-1,275, 225, 0, 0);
	cAddSvr.Add( new CLabel("Address", tLX->clNormalLabel),				-1,215, 267, 0, 0);
	cAddSvr.Add( new CTextbox(),							na_Address, 280, 265, 140, tLX->cFont.GetHeight());

	ProcessEvents();
	while(!WasKeyboardEventHappening(SDLK_ESCAPE,false) && addServerMsg && game.state != Game::S_Quit) {
		Menu_RedrawMouse(true);
		DrawImageAdv(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer, 200,220, 200,220, 240, 240);

		ProcessEvents();

		// Process the server list
		if( ServerList::get()->process() ) {
			// Add the servers to the listview
			Menu_Net_NET_ServerList_Refresher();
		}

		cAddSvr.Draw( VideoPostProcessor::videoSurface().get() );
		ev = cAddSvr.Process();

		// Process any events
		if(ev) {

			switch(ev->iControlID) {

				// Add
				case na_Add:
					if(ev->iEventMsg == BTN_CLICKED) {

						std::string addr;
						cAddSvr.SendMessage(na_Address, TXS_GETTEXT, &addr, 0);

						ServerList::get()->addServer(addr, true);
						Menu_Net_NET_ServerList_Refresher();

						// Click!
						PlaySoundSample(sfxGeneral.smpClick);

						addServerMsg = false;
					}
					break;

				// Cancel
				case na_Cancel:
					if(ev->iEventMsg == BTN_CLICKED) {
						// Click!
						PlaySoundSample(sfxGeneral.smpClick);

						addServerMsg = false;
					}
					break;
			}
		}


		DrawCursor(VideoPostProcessor::videoSurface().get());
		doVideoFrameInMainThread();
		CapFPS();
	}


	cAddSvr.Shutdown();

	// Re-draw the background
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
	Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_NETWORK);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
	Menu_RedrawMouse(true);
}

	
		
///////////////////
// Update the server list
void Menu_Net_NETUpdateList()
{
	ServerList::get()->updateList();
}
	
	
enum  {
	nd_Ok=0,
	nd_Refresh,
	nd_Join
};

///////////////////
// Show a server's details
void Menu_Net_NETShowServer(const std::string& szAddress)
{
    CGuiLayout  cDetails;

    // Create the buffer
    DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
    Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
	Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_NETWORK);
	cInternet.Draw(tMenu->bmpBuffer.get());

	//for(ushort i=1;i<4;i++)
	//	cNetButtons[i].Draw(tMenu->bmpBuffer.get());

	Menu_RedrawMouse(true);

	int center = VideoPostProcessor::videoSurface()->w/2;
	int y = VideoPostProcessor::videoSurface()->h/2 - INFO_H/2;
	
    cDetails.Initialize();
	cDetails.Add( new CButton(BUT_REFRESH, tMenu->bmpButtons),  nd_Refresh,	center - 105, y+INFO_H-20, 85,15);
    cDetails.Add( new CButton(BUT_JOIN, tMenu->bmpButtons),	    nd_Join,    center, y+INFO_H-20, 40,45);
	cDetails.Add( new CButton(BUT_OK, tMenu->bmpButtons),	    nd_Ok,      center + 60, y+INFO_H-20, 40,15);
	((CButton *)cDetails.getWidget(nd_Refresh))->setRedrawMenu(false);
	((CButton *)cDetails.getWidget(nd_Ok))->setRedrawMenu(false);
	((CButton *)cDetails.getWidget(nd_Join))->setRedrawMenu(false);

	bGotDetails = false;
	bOldLxBug = false;
	nTries = 0;
	fStart = AbsTime();

	while(!WasKeyboardEventHappening(SDLK_ESCAPE,false) && game.state != Game::S_Quit) {
		tLX->currentTime = GetTime();

		Menu_RedrawMouse(true);
		ProcessEvents();
		//DrawImageAdv(VideoPostProcessor::videoSurface(),tMenu->bmpBuffer, 200,220, 200,220, 240, 240);

		Menu_SvrList_DrawInfo(szAddress, INFO_W, INFO_H);

        cDetails.Draw(VideoPostProcessor::videoSurface().get());
        gui_event_t *ev = NULL;

		ev = cDetails.Process();
        if(ev) {

			// Ok
            if(ev->iControlID == nd_Ok && ev->iEventMsg == BTN_CLICKED) {
                break;
			// Refresh
            } else if (ev->iControlID == nd_Refresh && ev->iEventMsg == BTN_CLICKED)  {
				fStart = AbsTime();
				bGotDetails = false;
				bOldLxBug = false;
				nTries = 0;
			} else if (ev->iControlID == nd_Join && ev->iEventMsg == BTN_CLICKED)  {
                // Save the list
                ServerList::get()->save();

				lv_subitem_t *sub = ((CListview *)cInternet.getWidget(mi_ServerList))->getCurSubitem(1);

				// Join
				if (sub)
					Menu_Net_NETJoinServer(szAddress, sub->sText);

				break;
			}
        }

        DrawCursor(VideoPostProcessor::videoSurface().get());
		doVideoFrameInMainThread();
		CapFPS();
    }

	cDetails.Shutdown();


    // Redraw the background
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
	Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_NETWORK);
	Menu_RedrawMouse(true);
}

}; // namespace DeprecatedGUI
