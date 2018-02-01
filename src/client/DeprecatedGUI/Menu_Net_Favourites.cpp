/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Net menu - Favourites
// Created 21/8/02
// Jason Boettcher


#include "LieroX.h"
#include "sound/SoundsBase.h"
#include "Clipboard.h"
#include "AuxLib.h"
#include "DeprecatedGUI/Graphics.h"
#include "CClient.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CListview.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CMenu.h"
#include "DeprecatedGUI/CTextbox.h"
#include "ProfileSystem.h"
#include "Debug.h"
#include "game/ServerList.h"



namespace DeprecatedGUI {

CGuiLayout	cFavourites;
std::string szFavouritesCurServer;


// Widgets
enum {
	mf_Join=0,
	mf_ServerList,
	mf_Refresh,
	mf_Add,
	mf_Back,
    mf_PopupMenu,
	mf_PlayerSelection,
	mf_Clear
};

void Menu_Net_Favourites_ServerList_Refresher() {
	ServerList::get()->fillList( (CListview *)cFavourites.getWidget( mf_ServerList ), SLFT_Favourites );
}

///////////////////
// Initialize the favourites menu
bool Menu_Net_FavouritesInitialize()
{
	iNetMode = net_favourites;

	cFavourites.Shutdown();
	cFavourites.Initialize();

	cFavourites.Add( new CButton(BUT_BACK, tMenu->bmpButtons),    mf_Back,        25, 440, 50,  15);
	cFavourites.Add( new CButton(BUT_ADD, tMenu->bmpButtons),	  mf_Add,		   170,440, 83,  15);
	cFavourites.Add( new CButton(BUT_REFRESH, tMenu->bmpButtons), mf_Refresh,	   400,440, 83,  15);
	cFavourites.Add( new CButton(BUT_CLEAR, tMenu->bmpButtons), mf_Clear,	   270,440, 83,  15);
	cFavourites.Add( new CButton(BUT_JOIN, tMenu->bmpButtons),    mf_Join,	   570,440, 43,  15);
	cFavourites.Add( new CListview(),							   mf_ServerList, 40, 180, 560, 242);
	cFavourites.Add( new CLabel("Select player:",tLX->clNormalLabel),-1,		125, 152, 180,15);
	cFavourites.Add( new CCombobox(),								mf_PlayerSelection,		225,150, 170,  19);

	Menu_Net_AddTabBarButtons(&cFavourites);

	// Fill the players box
	CCombobox* PlayerSelection = (CCombobox*) cFavourites.getWidget( mf_PlayerSelection );
	bool validName = false;
	for_each_iterator(SmartPointer<profile_t>, p, GetProfiles()) {
		int index = PlayerSelection->addItem( p->get()->sName, p->get()->sName );
		PlayerSelection->setImage( p->get()->cSkin.getPreview(), index );
		if( p->get()->sName == tLXOptions->sLastSelectedPlayer )
			validName = true;
	}

	if( ! validName )
		tLXOptions->sLastSelectedPlayer = MainHumanProfile()->sName;

	PlayerSelection->setCurSIndexItem( tLXOptions->sLastSelectedPlayer );

    Menu_redrawBufferRect(0, 0, 640, 480);


	/*
	  Server list columns

      Connection speed
	  Name
	  State
	  Players
	  Ping
	  Address
    */

	cFavourites.SendMessage( mf_ServerList, LVS_ADDCOLUMN, "", tLXOptions->iFavouritesList[0]);
	cFavourites.SendMessage( mf_ServerList, LVS_ADDCOLUMN, "Server Name", tLXOptions->iFavouritesList[1]);
	cFavourites.SendMessage( mf_ServerList, LVS_ADDCOLUMN, "State", tLXOptions->iFavouritesList[2]);
	cFavourites.SendMessage( mf_ServerList, LVS_ADDCOLUMN, "Players", tLXOptions->iFavouritesList[3]);
	cFavourites.SendMessage( mf_ServerList, LVS_ADDCOLUMN, "Ping", tLXOptions->iFavouritesList[4]);
	cFavourites.SendMessage( mf_ServerList, LVS_ADDCOLUMN, "Address", tLXOptions->iFavouritesList[5]);

	((CListview*) cFavourites.getWidget( mf_ServerList ))->SetSortColumn( tLXOptions->iFavouritesSortColumn, true ); // Sorting

	// Fill the server list
	ServerList::get()->fillList( (CListview *)cFavourites.getWidget( mf_ServerList ), SLFT_Favourites );
	ServerList::get()->refreshList();

	return true;
}


///////////////////
// Shutdown the favourites menu
void Menu_Net_FavouritesShutdown()
{
	if (tLXOptions)  {

		// Save the selected player
		CCombobox* combo = (CCombobox *) cFavourites.getWidget(mf_PlayerSelection);
		if( combo )
		{
			const GuiListItem::Pt item = combo->getSelectedItem();
			if (item.get())
				tLXOptions->sLastSelectedPlayer = item->index();
		}

		// Save the list
		if (iNetMode == net_favourites && cFavourites.getWidget(mf_ServerList))  {
			ServerList::get()->save();

			// Save the column widths
			for (int i=0;i<6;i++)
				tLXOptions->iFavouritesList[i] = (int)cFavourites.SendMessage(mf_ServerList,LVM_GETCOLUMNWIDTH,i,0);

			// Save the sorting column
			tLXOptions->iFavouritesSortColumn = ((CListview *)cFavourites.getWidget(mf_ServerList))->GetSortColumn();
		}

	}

	cFavourites.Shutdown();
}


///////////////////
// Net favourites frame
void Menu_Net_FavouritesFrame(int mouse)
{
	gui_event_t *ev = NULL;
	std::string		addr;


	// Process & Draw the gui
	ev = cFavourites.Process();
	cFavourites.Draw( VideoPostProcessor::videoSurface().get() );

	if (Menu_Net_ProcessTabBarButtons(ev))
		return;

	// Process the server list
	if( ServerList::get()->process() ) {
		// Add the servers to the listview
		ServerList::get()->fillList( (CListview *)cFavourites.getWidget( mf_ServerList ), SLFT_Favourites );
	}


	// Process any events
	if(ev) {

		switch(ev->iControlID) {

			// Back
			case mf_Back:
				if(ev->iEventMsg == BTN_CLICKED) {

					CCombobox* combo = (CCombobox *) cFavourites.getWidget(mf_PlayerSelection);
					const GuiListItem::Pt item = combo->getSelectedItem();
					if (item.get())
						tLXOptions->sLastSelectedPlayer = item->index();

					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					// Save the list
					ServerList::get()->save();

					// Shutdown
					cFavourites.Shutdown();

					// Back to main menu
					Menu_MainInitialize();
				}
				break;

			// Add
			case mf_Add:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					Menu_Net_FavouritesAddServer();
				}
				break;

			// Refresh
			case mf_Refresh:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					// Send out a ping
					ServerList::get()->refreshList();
					ServerList::get()->fillList( (CListview *)cFavourites.getWidget( mf_ServerList ), SLFT_Favourites );
				}
				break;

			// Join
			case mf_Join:
				if(ev->iEventMsg == BTN_CLICKED) {

					addr = "";
					int result = (int)cFavourites.SendMessage(mf_ServerList, LVS_GETCURSINDEX, &addr, 0);
					lv_subitem_t *sub = ((CListview *)cFavourites.getWidget(mf_ServerList))->getCurSubitem(1);
					if(result != -1 && addr != "" && sub) {

						// Click!
						PlaySoundSample(sfxGeneral.smpClick);

						// Join
						Menu_Net_FavouritesJoinServer(addr,sub->sText);
						return;
					}
				}
				break;

			// Serverlist
			case mf_ServerList:
				if(ev->iEventMsg == LV_DOUBLECLK) {

					/*
					  Now.... Should a double click refresh the server (like tribes)?
					  Or should it join the server like other games???
					*/

					// Just join for the moment
					addr = "";
					int result = (int)cFavourites.SendMessage(mf_ServerList, LVS_GETCURSINDEX, &addr, 0);
					lv_subitem_t *sub = ((CListview *)cFavourites.getWidget(mf_ServerList))->getCurSubitem(1);
					if(result != -1 && addr != "" && sub) {
						Menu_Net_FavouritesJoinServer(addr,sub->sText);
						return;
					}
				}

                // Right click
                if( ev->iEventMsg == LV_RIGHTCLK ) {
                    addr = "";
					int result = (int)cFavourites.SendMessage(mf_ServerList, LVS_GETCURSINDEX, &addr, 0);
					if(result && addr != "") {
                        // Display a menu
                        szFavouritesCurServer = addr;
                        mouse_t *m = GetMouse();

                        cFavourites.Add( new CMenu(m->X, m->Y), mf_PopupMenu, 0,0, 640,480 );
                        cFavourites.SendMessage( mf_PopupMenu, MNS_ADDITEM, "Remove from favourites",		0 );
						cFavourites.SendMessage( mf_PopupMenu, MNS_ADDITEM, "Rename server",				1 );
                        cFavourites.SendMessage( mf_PopupMenu, MNS_ADDITEM, "Refresh server",				2 );
                        cFavourites.SendMessage( mf_PopupMenu, MNS_ADDITEM, "Join server",					3 );
						cFavourites.SendMessage( mf_PopupMenu, MNS_ADDITEM, "Send \"I want to join\" message", 4 );
						cFavourites.SendMessage( mf_PopupMenu, MNS_ADDITEM, "Copy IP to clipboard",			5 );
                        cFavourites.SendMessage( mf_PopupMenu, MNS_ADDITEM, "Server details",				6 );
                    }
                }

				// Enter key
				if( ev->iEventMsg == LV_ENTER )  {
					// Join
					addr = "";
					int result = (int)cFavourites.SendMessage(mf_ServerList, LVS_GETCURSINDEX, &addr, 0);
					lv_subitem_t *sub = ((CListview *)cFavourites.getWidget(mf_ServerList))->getCurSubitem(1);
					if(result != -1 && addr != "" && sub) {
     					Menu_Net_FavouritesJoinServer(addr,sub->sText);
						return;
					}
				}

				// Delete
				if( ev->iEventMsg == LV_DELETE )  {
					//DrawImage(tMenu->bmpBuffer,VideoPostProcessor::videoSurface(),0,0);
					addr = "";
					int result = (int)cFavourites.SendMessage(mf_ServerList, LVS_GETCURSINDEX, &addr, 0);
					server_t::Ptr sv = ServerList::get()->findServerStr(addr);
					std::string buf;
					if (sv)  {
						if (Menu_MessageBox("Confirmation","Are you sure you want to remove "+sv->szName+" server from favourites?",LMB_YESNO) == MBR_YES)  {
							if(result && addr != "") {
								ServerList::get()->removeServer(addr);
								// Re-Fill the server list
								ServerList::get()->fillList( (CListview *)cFavourites.getWidget( mf_ServerList ), SLFT_Favourites );
							}
							ServerList::get()->save();
						}
						Menu_redrawBufferRect(0,0,640,480);
					}

				}

				break;

			// Clear
			case mf_Clear:
				if (ev->iEventMsg == BTN_CLICKED)  {
					if (Menu_MessageBox("Confirmation", "Do you really want to delete ALL your favourite servers?", LMB_YESNO) == MBR_YES)  {
						ServerList::get()->clear(SLFT_Favourites);
						ServerList::get()->save();
						CListview *lv = (CListview *)cFavourites.getWidget(mf_ServerList);
						if (lv)
							lv->Clear();
					}
				}
			break;

            // Popup menu
            case mf_PopupMenu:
                switch( ev->iEventMsg ) {
                     // Remove server from favourites
				case MNU_USER+0:  {
						server_t::Ptr sv = ServerList::get()->findServerStr(szFavouritesCurServer);
						std::string buf;
						if (sv)  {
							if (Menu_MessageBox("Confirmation","Are you sure you want to remove "+sv->szName+" server from favourites?",LMB_YESNO) == MBR_YES)  {
								ServerList::get()->removeServer(szFavouritesCurServer);
								ServerList::get()->save();  // Save changes
							}
							Menu_redrawBufferRect(0,0,640,480);
						}
						}
                        break;

					// Rename server
                    case MNU_USER+1:
						{
							// Remove the menu widget
							cFavourites.SendMessage(mf_PopupMenu, MNM_REDRAWBUFFER, (uintptr_t)0, 0);
							cFavourites.removeWidget(mf_PopupMenu);

							server_t::Ptr sv = ServerList::get()->findServerStr(szFavouritesCurServer);
							Menu_Net_RenameServer(sv->szName);
							ServerList::get()->save();  // Save changes
						}
                        break;

                    // Refresh the server
                    case MNU_USER+2:
                        {
							server_t::Ptr sv = ServerList::get()->findServerStr(szFavouritesCurServer);
                            if(sv)
                                ServerList::get()->refreshServer(sv);
                        }
                        break;

                    // Join a server
                    case MNU_USER+3:  {
                        // Join
						lv_subitem_t *sub = ((CListview *)cFavourites.getWidget(mf_ServerList))->getCurSubitem(1);
						if (sub)
							Menu_Net_FavouritesJoinServer(szFavouritesCurServer,sub->sText);
						}
                        return;

					// Send a "wants to join" message
                    case MNU_USER+4:
						{
							server_t::Ptr sv = ServerList::get()->findServerStr(szFavouritesCurServer);
							std::string Nick;
							cFavourites.SendMessage(mf_PlayerSelection, CBS_GETCURNAME, &Nick, 0);
							if (sv)
								ServerList::get()->wantsToJoin(Nick, sv);
						}
                        break;

					// Copy the IP to clipboard
					case MNU_USER+5:
						{
							copy_to_clipboard(szFavouritesCurServer);
						}
						break;

                    // Show server details
                    case MNU_USER+6:
						cFavourites.removeWidget(mf_PopupMenu);
                        Menu_Net_FavouritesShowServer(szFavouritesCurServer);
                        break;
                }

                // Re-Fill the server list
                ServerList::get()->fillList( (CListview *)cFavourites.getWidget( mf_ServerList ), SLFT_Favourites );

                // Remove the menu widget
                cFavourites.SendMessage(mf_PopupMenu, MNM_REDRAWBUFFER, (uintptr_t)0, 0);
                cFavourites.removeWidget(mf_PopupMenu);
                break;
		}

	}

	// F5 updates the list
	if (WasKeyboardEventHappening(SDLK_F5))  {
		ServerList::get()->refreshList();
		ServerList::get()->fillList((CListview *) cFavourites.getWidget(mf_ServerList), SLFT_Favourites);
	}

	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());
}

///////////////////
// Join a server
void Menu_Net_FavouritesJoinServer(const std::string& sAddress, const std::string& sName)
{
	// Fill in the game structure
	CCombobox* combo = (CCombobox *) cFavourites.getWidget(mf_PlayerSelection);
	const GuiListItem::Pt item = combo->getSelectedItem();
	if(!item.get()) {
		errors << "no player selected" << endl;
		return;
	}
	
	tLXOptions->sLastSelectedPlayer = item->index();
	
	if(!JoinServer(sAddress, sName, item->index()))
		return;	

	// Save the list
	ServerList::get()->save();
}

enum  {
	fd_Ok,
	fd_Refresh,
	fd_Join
};


///////////////////
// Show a server's details
// TODO: join this with Menu_Net_NETShowServer()
void Menu_Net_FavouritesShowServer(const std::string& szAddress)
{
    CGuiLayout  cDetails;

    // Create the buffer
    DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
    Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
	Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_NETWORK);
	cFavourites.Draw(tMenu->bmpBuffer.get());

	Menu_RedrawMouse(true);

	int center = VideoPostProcessor::videoSurface()->w/2;
	int y = VideoPostProcessor::videoSurface()->h/2 - INFO_H/2;

    cDetails.Initialize();
	cDetails.Add( new CButton(BUT_REFRESH, tMenu->bmpButtons),  fd_Refresh,	center - 105, y+INFO_H-20, 85,15);
    cDetails.Add( new CButton(BUT_JOIN, tMenu->bmpButtons),	    fd_Join,      center, y+INFO_H-20, 40,15);
	cDetails.Add( new CButton(BUT_OK, tMenu->bmpButtons),	    fd_Ok,      center + 60, y+INFO_H-20, 40,15);
	((CButton *)cDetails.getWidget(fd_Refresh))->setRedrawMenu(false);
	((CButton *)cDetails.getWidget(fd_Ok))->setRedrawMenu(false);
	((CButton *)cDetails.getWidget(fd_Join))->setRedrawMenu(false);

	bGotDetails = false;
	bOldLxBug = false;
	nTries = 0;
	fStart = AbsTime();

	DrawRectFillA(tMenu->bmpBuffer.get(),200,400,350,420,tLX->clDialogBackground,230); // Dirty; because of button redrawing

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
            if(ev->iControlID == fd_Ok && ev->iEventMsg == BTN_CLICKED) {
                break;
			// Refresh
            } else if (ev->iControlID == fd_Refresh && ev->iEventMsg == BTN_CLICKED)  {
				fStart = AbsTime();
				bGotDetails = false;
				bOldLxBug = false;
				nTries = 0;
			// Join
			} else if (ev->iControlID == fd_Join && ev->iEventMsg == BTN_CLICKED)  {

				lv_subitem_t *sub = ((CListview *)cFavourites.getWidget(mf_ServerList))->getCurSubitem(1);

				// Join
				if (sub)
					Menu_Net_FavouritesJoinServer(szAddress, sub->sText);

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

///////////////////
// Show an 'rename server' box
enum  {
	rs_Cancel=0,
	rs_Ok,
	rs_NewName
};

void Menu_Net_RenameServer(std::string& szName)
{
	CGuiLayout	cRename;
	gui_event_t *ev = NULL;
	bool		renameServerMsg = true;


	// Create the background
	cFavourites.Draw( tMenu->bmpBuffer.get() );
	Menu_DrawBox(tMenu->bmpBuffer.get(), 200, 210, 470, 340);
	//DrawImageAdv(tMenu->bmpBuffer.get(), tMenu->bmpMainBack, 202,222, 202,222, 237,117);
    DrawRectFill(tMenu->bmpBuffer.get(), 202,212,469,339,tLX->clDialogBackground);
	Menu_RedrawMouse(true);


	cRename.Initialize();
	cRename.Add( new CButton(BUT_OK, tMenu->bmpButtons),	rs_Ok, 220, 310, 40,15);
	cRename.Add( new CButton(BUT_CANCEL, tMenu->bmpButtons),rs_Cancel, 350, 310, 70,15);
	cRename.Add( new CLabel("Rename a server", tLX->clNormalLabel),		-1,275, 225, 0, 0);
	cRename.Add( new CLabel("New name", tLX->clNormalLabel),			-1,215, 267, 0, 0);
	cRename.Add( new CTextbox(),							rs_NewName, 300, 265, 150, tLX->cFont.GetHeight());

	cRename.SendMessage(2,TXM_SETMAX,30,0);
	cRename.SendMessage(2,TXS_SETTEXT,szName,0); // Fill in the current server name

	ProcessEvents();
	while(!WasKeyboardEventHappening(SDLK_ESCAPE,false) && renameServerMsg && game.state != Game::S_Quit) {
		Menu_RedrawMouse(true);
		DrawImageAdv(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer, 200,220, 200,220, 240, 240);

		// Process the server list
		if( ServerList::get()->process() ) {
			// Add the servers to the listview
			ServerList::get()->fillList( (CListview *)cFavourites.getWidget( mf_ServerList ), SLFT_Favourites );
		}


		cRename.Draw( VideoPostProcessor::videoSurface().get() );
		ev = cRename.Process();

		// Process any events
		if(ev) {

			switch(ev->iControlID) {

				// Ok
				case rs_Ok:
					if(ev->iEventMsg == BTN_CLICKED) {

						cRename.SendMessage(2, TXS_GETTEXT, &szName, 0);

						ServerList::get()->fillList( (CListview *)cFavourites.getWidget( mf_ServerList ), SLFT_Favourites );

						// Click!
						PlaySoundSample(sfxGeneral.smpClick);

						renameServerMsg = false;
					}
					break;

				// Cancel
				case rs_Cancel:
					if(ev->iEventMsg == BTN_CLICKED) {
						// Click!
						PlaySoundSample(sfxGeneral.smpClick);

						renameServerMsg = false;
					}
					break;
			}
		}


		DrawCursor(VideoPostProcessor::videoSurface().get());
		doVideoFrameInMainThread();
		CapFPS();
		ProcessEvents();
	}


	cRename.Shutdown();

	// Re-draw the background
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
	Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_NETWORK);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
	Menu_RedrawMouse(true);
}

///////////////////
// Show an 'add server' box to enter in an address and name
enum  {
	fa_Cancel=0,
	fa_Add,
	fa_Address,
	fa_Name
};

void Menu_Net_FavouritesAddServer()
{
	CGuiLayout	cAddSvr;
	gui_event_t *ev = NULL;
	bool		addServerMsg = true;


	// Create the background
	cFavourites.Draw( tMenu->bmpBuffer.get() );
	Menu_DrawBox(tMenu->bmpBuffer.get(), 200, 220, 440, 340);
	//DrawImageAdv(tMenu->bmpBuffer.get(), tMenu->bmpMainBack, 202,222, 202,222, 237,117);
    DrawRectFill(tMenu->bmpBuffer.get(), 202,222,439,339,tLX->clDialogBackground);
	Menu_RedrawMouse(true);


	cAddSvr.Initialize();
	cAddSvr.Add( new CButton(BUT_ADD, tMenu->bmpButtons),	fa_Add, 220, 320, 40,15);
	cAddSvr.Add( new CButton(BUT_CANCEL, tMenu->bmpButtons),fa_Cancel, 350, 320, 70,15);
	cAddSvr.Add( new CLabel("Add a server", tLX->clNormalLabel),		-1,275, 225, 0, 0);
	cAddSvr.Add( new CLabel("Address", tLX->clNormalLabel),				-1,215, 267, 0, 0);
	cAddSvr.Add( new CTextbox(),							fa_Address, 280, 265, 140, tLX->cFont.GetHeight());
	cAddSvr.Add( new CLabel("Name", tLX->clNormalLabel),				-1,215, 290, 0, 0);
	cAddSvr.Add( new CTextbox(),							fa_Name, 280, 288, 140, tLX->cFont.GetHeight());

	cAddSvr.SendMessage(2,TXM_SETMAX,21,0);
	cAddSvr.SendMessage(3,TXM_SETMAX,32,0);

	ProcessEvents();
	while(!WasKeyboardEventHappening(SDLK_ESCAPE,false) && addServerMsg && game.state != Game::S_Quit) {
		Menu_RedrawMouse(true);
		DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, 200,220, 200,220, 240, 240);

		// Process the server list
		if( ServerList::get()->process() ) {
			// Add the servers to the listview
			ServerList::get()->fillList( (CListview *)cFavourites.getWidget( mf_ServerList ), SLFT_Favourites );
		}

		cAddSvr.Draw( VideoPostProcessor::videoSurface().get() );
		ev = cAddSvr.Process();

		// Process any events
		if(ev) {

			switch(ev->iControlID) {

				// Add
				case fa_Add:
					if(ev->iEventMsg == BTN_CLICKED) {

						std::string addr;
						std::string name;
						cAddSvr.SendMessage(2, TXS_GETTEXT, &addr, 0);
						cAddSvr.SendMessage(3, TXS_GETTEXT, &name, 0);

						ServerList::get()->addFavourite(name, addr);
						ServerList::get()->fillList( (CListview *)cFavourites.getWidget( mf_ServerList ), SLFT_Favourites );

						// Click!
						PlaySoundSample(sfxGeneral.smpClick);

						addServerMsg = false;
					}
					break;

				// Cancel
				case fa_Cancel:
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
		ProcessEvents();
	}


	cAddSvr.Shutdown();

	// Re-draw the background
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
	Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_NETWORK);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
	Menu_RedrawMouse(true);
}

}; // namespace DeprecatedGUI

