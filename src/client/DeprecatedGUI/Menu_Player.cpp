/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Player menu
// Created 30/6/02
// Jason Boettcher


#include "LieroX.h"
#include "sound/SoundsBase.h"
#include "DeprecatedGUI/Graphics.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "DeprecatedGUI/CGuiLayout.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CTextbox.h"
#include "DeprecatedGUI/CSlider.h"
#include "AuxLib.h"
#include "ProfileSystem.h"


namespace DeprecatedGUI {

CGuiLayout	cNewPlayer;
CGuiLayout	cViewPlayers;
int			iPlayerMode = 0;
float       fPlayerSkinFrame=0;
float		fPlayerSkinAngle = 0;
bool		bAimingUp = true;
bool        bPlayerSkinAnimation = false;
Timer *		tAnimTimer = NULL;

// Generic
enum {
	Static=-1,
	pp_NewPlayerTab=0,
	pp_ViewPlayersTab,
	pp_Num
};

// New player widgets
enum {
	np_Back=pp_Num + 1,
	np_Create,
	np_Name,
	np_Red, np_Blue, np_Green,
	np_Type,
	np_AIDiffLbl,
	np_AIDiff,
	np_PlySkin,
	np_Username,
	np_Password
};

// View players widgets
enum {
	vp_Back=pp_Num + 1,
	vp_Name,
	vp_Red, vp_Blue, vp_Green,
	vp_Players,
	vp_Delete,
	vp_Apply,
	vp_Type,
	vp_AIDiffLbl,
	vp_AIDiff,
	vp_PlySkin
};

static void ReportInvalidPlayerName();

///////////////////
// Initialize the player menu
void Menu_PlayerInitialize()
{
	tMenu->iMenuType = MNU_PLAYER;

	iPlayerMode = 0;
	fPlayerSkinAngle = 0;
	bAimingUp = true;
	CListview *lv;

	// Create the buffer
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
	Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_PLAYER);

	Menu_RedrawMouse(true);

	tAnimTimer = new Timer("Menu_Player animation", null, NULL, 25, false);

	// New player
	cNewPlayer.Shutdown();
	cNewPlayer.Initialize();

	cNewPlayer.Add( new CButton(BUT_NEWPLAYER,	tMenu->bmpButtons), pp_NewPlayerTab, 150, 110, 120, 15);
	cNewPlayer.Add( new CButton(BUT_VIEWPLAYERS,tMenu->bmpButtons), pp_ViewPlayersTab, 370, 110, 135, 15);

	cNewPlayer.Add( new CButton(BUT_BACK, tMenu->bmpButtons),	np_Back, 25,440, 50,15);
	cNewPlayer.Add( new CButton(BUT_CREATE, tMenu->bmpButtons), np_Create, 540,440, 70,15);

	cNewPlayer.Add( new CLabel("Worm Details", tLX->clHeading),Static, 30, 170, 0,0);
	cNewPlayer.Add( new CTextbox(),		       np_Name,  120, 200,120,tLX->cFont.GetHeight());
	cNewPlayer.Add( new CLabel("Name",tLX->clNormalLabel), Static, 40, 202,0,  0);
	cNewPlayer.Add( new CLabel("Red",tLX->clNormalLabel),  Static, 40, 300,0,  0);
	cNewPlayer.Add( new CLabel("Green",tLX->clNormalLabel),Static, 40, 320,0,  0);
	cNewPlayer.Add( new CLabel("Blue",tLX->clNormalLabel), Static, 40, 340,0,  0);
	cNewPlayer.Add( new CSlider(255),	       np_Red,   115, 300,128,20);
	cNewPlayer.Add( new CSlider(255),	       np_Green, 115, 320,128,20);
	cNewPlayer.Add( new CSlider(255),	       np_Blue,  115, 340,128,20);

    cNewPlayer.Add( new CLabel("Skill",tLX->clNormalLabel),np_AIDiffLbl,40,362,0, 0);
    cNewPlayer.Add( new CSlider(3),            np_AIDiff,115, 360,128,20);
    cNewPlayer.Add( new CLabel("Skin", tLX->clNormalLabel),Static,40,  262,0,  0);
    cNewPlayer.Add( new CCombobox(),           np_PlySkin,120,260,120,20);
    cNewPlayer.Add( new CLabel("Type", tLX->clNormalLabel),Static,40,  232,0,  0);
    cNewPlayer.Add( new CCombobox(),           np_Type,  120, 230,120,17);

	cNewPlayer.SendMessage(np_Name,TXM_SETMAX,20,0);
	cNewPlayer.SendMessage( np_Name, TXM_SETFLAGS, TXF_NOUNICODE, 0); // Disable unicode for backward compatibility


	//cNewPlayer.Add( new CLabel("Multiplayer (optional)", tLX->clHeading),Static, 370, 170, 0,0);
	//cNewPlayer.Add( new CLabel("Username", tLX->clNormalLabel), Static, 380, 202, 0,0);
	//cNewPlayer.Add( new CLabel("Password", tLX->clNormalLabel), Static, 380, 232, 0,0);
	cNewPlayer.Add( new CTextbox(),			   np_Username, 470, 200, 110, tLX->cFont.GetHeight());
	cNewPlayer.Add( new CTextbox(),			   np_Password, 470, 230, 110, tLX->cFont.GetHeight());

	//cNewPlayer.Add( new CLabel("Note: To register a username, visit the OpenLieroX web site", tLX->clSubHeading),Static, 30, 410, 0,0);

	// Hide the multiplayer textboxes
	cNewPlayer.getWidget(np_Username)->setEnabled(false);
	cNewPlayer.getWidget(np_Password)->setEnabled(false);

    // Hide the AI stuff until 'Computer' type is selected
    cNewPlayer.getWidget(np_AIDiffLbl)->setEnabled(false);
	cNewPlayer.getWidget(np_AIDiff)->setEnabled(false);


	cNewPlayer.SendMessage( np_Password, TXM_SETFLAGS, TXF_PASSWORD, 0);
	cNewPlayer.SendMessage( np_Password, TXM_SETMAX, 15, 0);
	cNewPlayer.SendMessage( np_Username, TXM_SETMAX, 15, 0);

	// Set the default colour
	cNewPlayer.SendMessage( np_Red,		SLM_SETVALUE, 128, 0);
	cNewPlayer.SendMessage( np_Green,	SLM_SETVALUE, 128, 0);
	cNewPlayer.SendMessage( np_Blue,	SLM_SETVALUE, 128, 0);

	// Player type
    cNewPlayer.SendMessage( np_Type, CBS_ADDITEM, "Human", PRF_HUMAN->toInt() );
    cNewPlayer.SendMessage( np_Type, CBS_ADDITEM, "Computer", PRF_COMPUTER->toInt() );

    Menu_Player_NewPlayerInit();

	// View players
	cViewPlayers.Shutdown();
	cViewPlayers.Initialize();

	cViewPlayers.Add( new CButton(BUT_NEWPLAYER,	tMenu->bmpButtons), pp_NewPlayerTab, 150, 110, 120, 15);
	cViewPlayers.Add( new CButton(BUT_VIEWPLAYERS,	tMenu->bmpButtons), pp_ViewPlayersTab, 370, 110, 135, 15);

	cViewPlayers.Add( new CButton(BUT_BACK, tMenu->bmpButtons),     vp_Back,   25, 440, 50, 15);
	cViewPlayers.Add( new CListview(),                              vp_Players,40, 150, 200,170);
    cViewPlayers.Add( new CLabel("Name", tLX->clNormalLabel),                   Static, 350,172, 0,  0);
    cViewPlayers.Add( new CTextbox(),                               vp_Name,  400,170, 120,tLX->cFont.GetHeight());
	cViewPlayers.Add( new CButton(BUT_DELETE, tMenu->bmpButtons),   vp_Delete, 330,340, 70, 15);
    cViewPlayers.Add( new CButton(BUT_APPLY, tMenu->bmpButtons),    vp_Apply,  500,340, 55, 15);

    cViewPlayers.Add( new CLabel("Red",tLX->clNormalLabel),                     Static, 350,250,0,0);
	cViewPlayers.Add( new CLabel("Green",tLX->clNormalLabel),                   Static, 350,270,0,0);
	cViewPlayers.Add( new CLabel("Blue",tLX->clNormalLabel),                    Static, 350,290,0,0);
    cViewPlayers.Add( new CSlider(255),	                            vp_Red,    400,250, 128,20);
	cViewPlayers.Add( new CSlider(255),	                            vp_Green,  400,270, 128,20);
	cViewPlayers.Add( new CSlider(255),	                            vp_Blue,   400,290, 128,20);

    cViewPlayers.Add( new CLabel("Skill", tLX->clNormalLabel),                  vp_AIDiffLbl,350,312,0, 0);
    cViewPlayers.Add( new CSlider(3),                               vp_AIDiff, 400,310, 128,20);
    cViewPlayers.Add( new CLabel("Skin", tLX->clNormalLabel),                   Static, 350,227, 0,  0);
    cViewPlayers.Add( new CCombobox(),                              vp_PlySkin,400,225, 120,17);
    cViewPlayers.Add( new CLabel("Type", tLX->clNormalLabel),                   Static, 350,202, 0,  0);
    cViewPlayers.Add( new CCombobox(),                              vp_Type,   400,200, 120,17);

	cViewPlayers.SendMessage(vp_Name,TXM_SETMAX,20,0);
	cViewPlayers.SendMessage(vp_Name, TXM_SETFLAGS, TXF_NOUNICODE, 0); // Disable unicode for backward compatibility

	cViewPlayers.SendMessage(vp_Players,		LVM_SETOLDSTYLE, (uintptr_t)0, 0);

    // Hide the AI stuff until 'Computer' type is selected
    cViewPlayers.getWidget(vp_AIDiffLbl)->setEnabled(false);
	cViewPlayers.getWidget(vp_AIDiff)->setEnabled(false);


	lv = (CListview *)cViewPlayers.getWidget(vp_Players);
	lv->AddColumn("Players",24);
	lv->AddColumn("",60);

    cViewPlayers.SendMessage( vp_Type, CBS_ADDITEM, "Human", PRF_HUMAN->toInt() );
    cViewPlayers.SendMessage( vp_Type, CBS_ADDITEM, "Computer", PRF_COMPUTER->toInt() );
}

///////////////
// Shutdown
void Menu_PlayerShutdown()
{
	cNewPlayer.Shutdown();
	cViewPlayers.Shutdown();
	if (tAnimTimer)  {
		tAnimTimer->stop();
		delete tAnimTimer;
		tAnimTimer = NULL;
	}
}


///////////////////
// Player frame
void Menu_PlayerFrame()
{
	int mouse = 0;

	DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, 20,140, 20,140, 620,340);
	DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, 140,110,  140,110,  400,30);

	if(iPlayerMode == 0)
		Menu_Player_NewPlayer(mouse);

	else if(iPlayerMode == 1)
		Menu_Player_ViewPlayers(mouse);
}

///////////////////
// Initialize the newplayer settings
void Menu_Player_NewPlayerInit()
{
	iPlayerMode = 0;

    cNewPlayer.SendMessage( np_Name,    TXS_SETTEXT, "", 0);
    cNewPlayer.SendMessage( np_Type,    CBM_SETCURSEL, PRF_HUMAN->toInt(), 0 );
    cNewPlayer.SendMessage( np_Red,		SLM_SETVALUE, 128, 0);
	cNewPlayer.SendMessage( np_Green,	SLM_SETVALUE, 128, 0);
	cNewPlayer.SendMessage( np_Blue,	SLM_SETVALUE, 128, 0);

    // Hide the AI stuff until 'Computer' type is selected
    cNewPlayer.getWidget(np_AIDiffLbl)->setEnabled(false);
	cNewPlayer.getWidget(np_AIDiff)->setEnabled(false);

    // Fill the skin combo box
    Menu_Player_FillSkinCombo( (CCombobox *)cNewPlayer.getWidget(np_PlySkin) );

    // Load the default skin
	tMenu->cSkin.Change("default.png");
	tMenu->cSkin.setDefaultColor(Color(128,128,128));
	tMenu->cSkin.ColorizeDefault();
	fPlayerSkinFrame = 0;
    bPlayerSkinAnimation = false;
    cNewPlayer.FocusWidget(pp_NewPlayerTab);
}


///////////////////
// Initialize the viewplayer settings
void Menu_Player_ViewPlayerInit()
{
	iPlayerMode = 1;

    // Add the players to the list
	CListview *lv = (CListview *)cViewPlayers.getWidget(vp_Players);
    lv->Clear();

	int i = 0;
	for_each_iterator(SmartPointer<profile_t>, p, GetProfiles()) {
		lv->AddItem("",i++,tLX->clListView);
		if (p->get()->cSkin.getPreview().get())
			lv->AddSubitem(LVS_IMAGE, "", p->get()->cSkin.getPreview(), NULL);
		else
			lv->AddSubitem(LVS_TEXT, " ", (DynDrawIntf*)NULL, NULL);
		lv->AddSubitem(LVS_TEXT, p->get()->sName, (DynDrawIntf*)NULL, NULL);
	}


    // Fill the skin combo box
    Menu_Player_FillSkinCombo( (CCombobox *)cViewPlayers.getWidget(vp_PlySkin) );

    // Set the name of the first item in the list
    int sel = (int)cViewPlayers.SendMessage( vp_Players, LVM_GETCURINDEX,(uintptr_t)0,0);
	SmartPointer<profile_t> p = FindProfile(sel);
    if(p.get()) {
        cViewPlayers.SendMessage( vp_Name,    TXS_SETTEXT, p->sName,0);

        cViewPlayers.SendMessage( vp_Red,	    SLM_SETVALUE, p->R, 0);
	    cViewPlayers.SendMessage( vp_Green,		SLM_SETVALUE, p->G, 0);
	    cViewPlayers.SendMessage( vp_Blue,	    SLM_SETVALUE, p->B, 0);
        cViewPlayers.SendMessage( vp_Type,		CBM_SETCURSEL,  p->iType, 0);
        cViewPlayers.SendMessage( vp_AIDiff,	SLM_SETVALUE,   p->nDifficulty, 0);
		cViewPlayers.SendMessage( vp_PlySkin,	CBS_SETCURSINDEX,p->cSkin.getFileName(), 0);

        // Hide the AI stuff if it is a human type of player
        cViewPlayers.getWidget(vp_AIDiffLbl)->setEnabled(p->iType == PRF_COMPUTER->toInt());
	    cViewPlayers.getWidget(vp_AIDiff)->setEnabled(p->iType == PRF_COMPUTER->toInt());

        // Load the skin
		tMenu->cSkin = p->cSkin;
        fPlayerSkinFrame = 0;
        bPlayerSkinAnimation = false;
    }
    cViewPlayers.FocusWidget(pp_ViewPlayersTab);
}


///////////////////
// New player section
void Menu_Player_NewPlayer(int mouse)
{
	gui_event_t *ev = NULL;
	mouse_t *Mouse = GetMouse();

	// Process & draw the gui
	ev = cNewPlayer.Process();
	cNewPlayer.Draw(VideoPostProcessor::videoSurface().get());

	Uint8 r = ((CSlider *)cNewPlayer.getWidget(np_Red))->getValue();
	Uint8 g = ((CSlider *)cNewPlayer.getWidget(np_Green))->getValue();
	Uint8 b = ((CSlider *)cNewPlayer.getWidget(np_Blue))->getValue();


	if(ev) {

		switch(ev->iControlID) {

			case pp_ViewPlayersTab:
				if(ev->iEventMsg == BTN_CLICKED) {
					Menu_Player_ViewPlayerInit();
					return;
				}
				break;

			// Back button
			case np_Back:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Shutdown
					Menu_PlayerShutdown();
					SaveProfiles();

					// Leave
					PlaySoundSample(sfxGeneral.smpClick);
					Menu_MainInitialize();
					return;
				}
				break;

			// Create
			case np_Create:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Get the details
					std::string name = ((CTextbox *)cNewPlayer.getWidget(np_Name))->getText();

                                        TrimSpaces(name);

                                        // Does not allow worm to have empty name
                                        if(name == "") {
                                            ReportInvalidPlayerName();

                                            return;
                                        }

                                        PlaySoundSample(sfxGeneral.smpClick);

					name = RemoveSpecialChars(name);  // Remove any special characters (backward compatibility)
					std::string skin;
                    cNewPlayer.SendMessage(np_PlySkin, CBS_GETCURSINDEX, &skin, 0);

					// Add the profile
                    WormType* type = ( (int)cNewPlayer.SendMessage(np_Type,CBM_GETCURINDEX,(uintptr_t)0,0) == PRF_HUMAN->toInt() ) ? PRF_HUMAN : PRF_COMPUTER;
                    int level = (int)cNewPlayer.SendMessage(np_AIDiff,SLM_GETVALUE,(uintptr_t)0,0);

					AddProfile(name, skin, "", "",r, g, b, type->toInt(), level);

					// Shutdown
					//Menu_PlayerShutdown();
					SaveProfiles();

					//Menu_MainInitialize();
					Menu_Player_ViewPlayerInit();
					return;
				}
				break;

            // Type
            case np_Type:
                if(ev->iEventMsg == CMB_CHANGED) {

                    int type = (int)cNewPlayer.SendMessage(np_Type,CBM_GETCURINDEX,(uintptr_t)0,0);

                    // Hide the AI stuff if it is a human type of player
                    cNewPlayer.getWidget(np_AIDiffLbl)->setEnabled(type == PRF_COMPUTER->toInt());
	                cNewPlayer.getWidget(np_AIDiff)->setEnabled(type == PRF_COMPUTER->toInt());
                }
                break;

            // Skin
            case np_PlySkin:
                if(ev->iEventMsg == CMB_CHANGED) {
                    std::string buf;
                    cNewPlayer.SendMessage(np_PlySkin, CBS_GETCURSINDEX, &buf, 0);

                    // Load the skin
					tMenu->cSkin.Change(buf);
					tMenu->cSkin.ColorizeDefault();
                }
                break;
		}
	}


	// Draw the colour
	//DrawRectFill(VideoPostProcessor::videoSurface(), 260, 230, 280, 250, Color(r,g,b));
	DrawRectFill(VideoPostProcessor::videoSurface().get(),  255, 195, 285, 225, tLX->clBlack);
	Menu_DrawBox(VideoPostProcessor::videoSurface().get(),  255, 195, 285, 225);
	//DrawRect(VideoPostProcessor::videoSurface(),		255, 195, 285, 225, Color(128,128,128));


	// Draw the colour component values
	tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 250, 303, tLX->clNormalLabel, itoa(r));
	tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 250, 323, tLX->clNormalLabel, itoa(g));
	tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 250, 343, tLX->clNormalLabel, itoa(b));

	if(MouseInRect(255,195,30,30) && Mouse->Up)  {
		if (bPlayerSkinAnimation)  {
			bPlayerSkinAnimation = false;
			tAnimTimer->stop();
		} else {
			bPlayerSkinAnimation = true;
			tAnimTimer->start();
		}
	}

	Menu_Player_DrawWormImage(VideoPostProcessor::videoSurface().get(), (int)(fPlayerSkinFrame)*7+(int)( fPlayerSkinAngle/151 * 7 )+4, 257, 200, r,g,b);

	if(bPlayerSkinAnimation)  {
		if (bAimingUp)
			fPlayerSkinAngle += tLX->fDeltaTime.seconds()*220;
		else
			fPlayerSkinAngle -= tLX->fDeltaTime.seconds()*220;

		fPlayerSkinFrame += tLX->fDeltaTime.seconds()*7.5f;


		if ((int)fPlayerSkinAngle >= 60)  {
			fPlayerSkinAngle = 60;
			bAimingUp = !bAimingUp;
		} else if ((int)fPlayerSkinAngle <= -90) {
			fPlayerSkinAngle = -90;
			bAimingUp = !bAimingUp;
		}
	}
    if( fPlayerSkinFrame >= 3 )
        fPlayerSkinFrame = 0;


    // Draw the difficulty level
    int type = (int)cNewPlayer.SendMessage(np_Type,CBM_GETCURINDEX,(uintptr_t)0,0);
    if( type == PRF_COMPUTER->toInt() ) {
        static const std::string difflevels[] = {"Easy", "Medium", "Hard", "Xtreme"};
        int level = (int)cNewPlayer.SendMessage(np_AIDiff,SLM_GETVALUE,(uintptr_t)0,0);
        tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 250,363,tLX->clNormalLabel,difflevels[level]);

    }

	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());
}


///////////////////
// View the players screen
void Menu_Player_ViewPlayers(int mouse)
{
	mouse_t *Mouse = GetMouse();
	gui_event_t *ev = NULL;
	std::string buf;

	// Process & draw the gui
	ev = cViewPlayers.Process();
	cViewPlayers.Draw(VideoPostProcessor::videoSurface().get());

	if(ev) {

		switch(ev->iControlID) {

			case pp_NewPlayerTab:
				if(ev->iEventMsg == BTN_CLICKED) {
					Menu_Player_NewPlayerInit();
					return;
				}
				break;

			// Back button
			case vp_Back:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Shutdown
					Menu_PlayerShutdown();
					SaveProfiles();

					// Leave
					PlaySoundSample(sfxGeneral.smpClick);
					Menu_MainInitialize();
					return;
				}
				break;

			// Delete
			case vp_Delete:
				if(ev->iEventMsg == BTN_CLICKED) {

					CListview *lv = (CListview *)cViewPlayers.getWidget(vp_Players);
					if(!lv) {
						errors << "Menu_Player_ViewPlayers: playerlist not found" << endl;
						return;
					}
					
					int sel = lv->getCurIndex();
					SmartPointer<profile_t> p = FindProfile(sel);
					if(p.get()) {
						/*if(p->iType == PRF_HUMAN)*/ {

							//
							// Show a msgbox confirming deletion
							//

							// Setup the buffer
							Mouse->Button = Mouse->Up = Mouse->Down = 0;
							Mouse->X = Mouse->Y = 0;
							cViewPlayers.Draw(tMenu->bmpBuffer.get());

							// Ask if they are sure they wanna delete it
							buf = std::string("Delete player ") + p->sName;
							if(Menu_MessageBox("Confirmation",buf,LMB_YESNO) == MBR_YES) {

								// Delete the profile
								DeleteProfile(p);

								// Add the players to the list
								lv->Create();
								lv->AddColumn("Players",22);
								lv->AddColumn("",60);
								
								int i = 0;
								for_each_iterator(SmartPointer<profile_t>, p, GetProfiles()) {
									//if(p->iType == PRF_COMPUTER)
									//	continue;
									lv->AddItem("",i++,tLX->clListView);
									if (p->get()->cSkin.getPreview().get())
										lv->AddSubitem(LVS_IMAGE, "", p->get()->cSkin.getPreview(), NULL);
									else
										lv->AddSubitem(LVS_TEXT, " ", (DynDrawIntf*)NULL, NULL);
									lv->AddSubitem(LVS_TEXT, p->get()->sName, (DynDrawIntf*)NULL, NULL);
								}
							}
						}

                        // Update the details
                        int sel = (int)cViewPlayers.SendMessage(vp_Players,LVM_GETCURINDEX,(uintptr_t)0,0);
	                    p = FindProfile(sel);
                        if(p.get()) {
                            cViewPlayers.SendMessage( vp_Name,		TXS_SETTEXT,    p->sName,0);
                            cViewPlayers.SendMessage( vp_Red,	    SLM_SETVALUE,   p->R, 0);
	                        cViewPlayers.SendMessage( vp_Green,		SLM_SETVALUE,   p->G, 0);
	                        cViewPlayers.SendMessage( vp_Blue,	    SLM_SETVALUE,   p->B, 0);
                            cViewPlayers.SendMessage( vp_Type,		CBM_SETCURSEL,  p->iType, 0);
                            cViewPlayers.SendMessage( vp_AIDiff,	SLM_SETVALUE,   p->nDifficulty, 0);

                            // Hide the AI stuff if it is a human type of player
                            cViewPlayers.getWidget(vp_AIDiffLbl)->setEnabled(p->iType == PRF_COMPUTER->toInt());
	                        cViewPlayers.getWidget(vp_AIDiff)->setEnabled(p->iType == PRF_COMPUTER->toInt());
                        }



						// Re-draw the buffer again
						DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
						if (tMenu->tFrontendInfo.bPageBoxes)
							Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
						Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_PLAYER);
						Menu_RedrawMouse(true);

					}

					// Play a click
					PlaySoundSample(sfxGeneral.smpClick);
				}
				break;


            // Apply button
            case vp_Apply:
                if( ev->iEventMsg == BTN_CLICKED ) {
                    int sel = (int)cViewPlayers.SendMessage(vp_Players, LVM_GETCURINDEX, (uintptr_t)0,0);
	                SmartPointer<profile_t> p = FindProfile(sel);
	                if(p.get()) {

                        std::string name;
                        cViewPlayers.SendMessage(vp_Name, TXS_GETTEXT, &name, 0);

                        TrimSpaces(name);
                        
                        // Does not allow empty name
                        if(name == "") {
                            
                            ReportInvalidPlayerName();
                            cViewPlayers.SendMessage(vp_Name, TXS_SETTEXT, p->sName, 0);
                            
                            return;
                        }

                        p->sName = name;
                        cViewPlayers.SendMessage(vp_Name, TXS_SETTEXT, p->sName, 0);

                        p->R = (Uint8)cViewPlayers.SendMessage(vp_Red,SLM_GETVALUE,(uintptr_t)0,0);
                        p->G = (Uint8)cViewPlayers.SendMessage(vp_Green,SLM_GETVALUE,(uintptr_t)0,0);
                        p->B = (Uint8)cViewPlayers.SendMessage(vp_Blue,SLM_GETVALUE,(uintptr_t)0,0);
                        p->iType = ((CCombobox *)cViewPlayers.getWidget(vp_Type))->getSelectedIndex();
                        p->nDifficulty = (int)cViewPlayers.SendMessage(vp_AIDiff, SLM_GETVALUE,(uintptr_t)0,0);

						// Reload the graphics
						std::string buf;
						buf = ((CCombobox *)cViewPlayers.getWidget(vp_PlySkin))->getSelectedItem()->index();
						p->cSkin.Change(buf);
						p->cSkin.setDefaultColor(Color(p->R, p->G, p->B));
						p->cSkin.ColorizeDefault();

                        // Update the item
                        lv_item_t *it = (lv_item_t *)cViewPlayers.SendMessage(vp_Players, LVM_GETCURITEM, (uintptr_t)0,0);
                        if(it) {
                            if(it->tSubitems) {
								it->tSubitems->bmpImage = p->cSkin.getPreview();
                                if(it->tSubitems->tNext)
                                    it->tSubitems->tNext->sText = p->sName;
                            }
                        }

                        // Add the players to the list

                        /*CListview *lv = (CListview *)cViewPlayers.getWidget(vp_Players);
						lv->Create();
						lv->AddColumn("Players",22);
						lv->AddColumn("",60);
						p = GetProfiles();
						for(; p; p=p->tNext) {
							lv->AddItem("",p->iID,tLX->clListView);
							lv->AddSubitem(LVS_IMAGE,"",p->bmpWorm);
							lv->AddSubitem(LVS_TEXT, p->sName, NULL);
						}*/
                    }
                }
                break;


            // Player listbox
            case vp_Players:
                if( ev->iEventMsg == LV_CHANGED ) {
                    int sel = (int)cViewPlayers.SendMessage(vp_Players,LVM_GETCURINDEX,(uintptr_t)0,0);
	                SmartPointer<profile_t> p = FindProfile(sel);
                    if(p.get()) {
                        cViewPlayers.SendMessage( vp_Name,		TXS_SETTEXT,    p->sName,0);
                        cViewPlayers.SendMessage( vp_Red,	    SLM_SETVALUE,   p->R, 0);
	                    cViewPlayers.SendMessage( vp_Green,		SLM_SETVALUE,   p->G, 0);
	                    cViewPlayers.SendMessage( vp_Blue,	    SLM_SETVALUE,   p->B, 0);
                        cViewPlayers.SendMessage( vp_Type,		CBM_SETCURSEL,  p->iType, 0);
                        cViewPlayers.SendMessage( vp_AIDiff,	SLM_SETVALUE,   p->nDifficulty, 0);
						cViewPlayers.SendMessage( vp_PlySkin,	CBS_SETCURSINDEX,p->cSkin.getFileName(), 0);

                        // Load the skin
						tMenu->cSkin = p->cSkin;

                        // Hide the AI stuff if it is a human type of player
                        cViewPlayers.getWidget(vp_AIDiffLbl)->setEnabled(p->iType == PRF_COMPUTER->toInt());
	                    cViewPlayers.getWidget(vp_AIDiff)->setEnabled(p->iType == PRF_COMPUTER->toInt());
                    }
                }
                break;

            // Type
            case vp_Type:
                if( ev->iEventMsg == CMB_CHANGED ) {

                    int type = (int)cViewPlayers.SendMessage(vp_Type,CBM_GETCURINDEX,(uintptr_t)0,0);

                    // Hide the AI stuff if it is a human type of player
                    cViewPlayers.getWidget(vp_AIDiffLbl)->setEnabled(type == PRF_COMPUTER->toInt());
	                cViewPlayers.getWidget(vp_AIDiff)->setEnabled(type == PRF_COMPUTER->toInt());
                }
                break;

            // Skin
            case vp_PlySkin:
                if(ev->iEventMsg == CMB_CHANGED) {
                    std::string buf;
                    cViewPlayers.SendMessage(vp_PlySkin, CBS_GETCURSINDEX, &buf, 0);

                    // Load the skin
					tMenu->cSkin.Change(buf);
					tMenu->cSkin.ColorizeDefault();
                }
                break;
		}
	}


	// Show info about the selected player
	CListview *lv = (CListview *)cViewPlayers.getWidget(vp_Players);
	if(!lv) {
		errors << "Menu_Player_ViewPlayers: playerlist not found" << endl;
		return;
	}
	
	int sel = lv->getCurIndex();
	SmartPointer<profile_t> p = FindProfile(sel);
	if(p.get()) {

        Uint8 r = ((CSlider *)cViewPlayers.getWidget(vp_Red))->getValue();
	    Uint8 g = ((CSlider *)cViewPlayers.getWidget(vp_Green))->getValue();
	    Uint8 b = ((CSlider *)cViewPlayers.getWidget(vp_Blue))->getValue();

        tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 530, 253, tLX->clNormalLabel, itoa(r));
	    tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 530, 273, tLX->clNormalLabel, itoa(g));
	    tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 530, 293, tLX->clNormalLabel, itoa(b));

		// Draw the worm image
		DrawRectFill(VideoPostProcessor::videoSurface().get(),  300, 165, 330, 195, tLX->clBlack);
		Menu_DrawBox(VideoPostProcessor::videoSurface().get(),  300, 165, 330, 195);

		Menu_Player_DrawWormImage(VideoPostProcessor::videoSurface().get(),(int)(fPlayerSkinFrame)*7+(int)( fPlayerSkinAngle/151 * 7 )+4, 301, 170, r, g, b);
		if(MouseInRect(300,165,30,30) && Mouse->Up)  {
			if (bPlayerSkinAnimation)  {
				bPlayerSkinAnimation = false;
				tAnimTimer->stop();
			} else {
				bPlayerSkinAnimation = true;
				tAnimTimer->start();
			}
		}

        if(bPlayerSkinAnimation)  {
			if (bAimingUp)
				fPlayerSkinAngle += tLX->fDeltaTime.seconds()*200;
			else
				fPlayerSkinAngle -= tLX->fDeltaTime.seconds()*200;

			fPlayerSkinFrame += tLX->fDeltaTime.seconds()*7.5f;

			if (fPlayerSkinAngle >= 60)  {
				fPlayerSkinAngle = 60;
				bAimingUp = !bAimingUp;
			} else if (fPlayerSkinAngle <= -90) {
				fPlayerSkinAngle = -90;
				bAimingUp = !bAimingUp;
			}
		}
        if( fPlayerSkinFrame >= 3.0f )
            fPlayerSkinFrame = 0;
	}

    // Draw the difficulty level
    int type = (int)cViewPlayers.SendMessage(vp_Type,CBM_GETCURINDEX,(uintptr_t)0,0);
    if( type == PRF_COMPUTER->toInt() ) {
        static const std::string difflevels[] = {"Easy", "Medium", "Hard", "Xtreme"};
        int level = (int)(uintptr_t)CLAMP(cViewPlayers.SendMessage(vp_AIDiff,SLM_GETVALUE,(uintptr_t)0,0), (uintptr_t)0, (uintptr_t)3);
        tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 530,313,tLX->clNormalLabel, difflevels[level]);
    }


	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());
}

// Report an invalid name
static void ReportInvalidPlayerName()
{
    PlaySoundSample(sfxGeneral.smpNotify);

    if(!Menu_MessageBox("Invalid name", "Name cannot be empty!", LMB_OK)) {
        PlaySoundSample(sfxGeneral.smpClick);
    }
}

///////////////////
// Draw the worm image
void Menu_Player_DrawWormImage(SDL_Surface * bmpDest, int Frame, int dx, int dy, int ColR, int ColG, int ColB)
{
	tMenu->cSkin.Colorize(Color(ColR, ColG, ColB));
	tMenu->cSkin.Draw(bmpDest, dx + 4, dy, Frame, false, false, true);
}



	class SkinAdder { public:
	   	CCombobox* cb;
		SkinAdder(CCombobox* cb_) : cb(cb_) {}
		bool operator() (std::string file) {
			std::string ext = GetFileExtension(file);
			if(stringcasecmp(ext, "png")==0
			|| stringcasecmp(ext, "bmp")==0
			|| stringcasecmp(ext, "tga")==0
			|| stringcasecmp(ext, "pcx")==0) {
				size_t slash = findLastPathSep(file);
				if(slash != std::string::npos)
					file.erase(0, slash+1);

				std::string name = file.substr(0, file.size()-4); // the size-calcing here is safe
				cb->addItem(file, name);
			}
			return true;
		}
	};


///////////////////
// Fill the skin combo box
void Menu_Player_FillSkinCombo(CCombobox *cb) {
    if( !cb )
        return;

	cb->clear();
	cb->setSorted(SORT_ASC);
	cb->setUnique(true);

	SkinAdder skinAdder(cb);
    FindFiles(skinAdder, "skins", false, FM_REG);

    // Select the default
    cb->setCurItemByName("default");
}

}; // namespace DeprecatedGUI
