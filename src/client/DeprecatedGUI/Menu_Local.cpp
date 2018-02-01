/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Local menu
// Created 30/6/02
// Jason Boettcher


#define DEFAULT_LOADING_TIME 500

#include <assert.h>
#include <string>

#include "LieroX.h"
#include "CGameScript.h"
#include "AuxLib.h"
#include "DeprecatedGUI/Graphics.h"
#include "CClient.h"
#include "CServer.h"
#include "DeprecatedGUI/Menu.h"
#include "DeprecatedGUI/CListview.h"
#include "GfxPrimitives.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CImage.h"
#include "DeprecatedGUI/CTextbox.h"
#include "DeprecatedGUI/CSlider.h"
#include "DeprecatedGUI/CCheckbox.h"
#include "DeprecatedGUI/CTextButton.h"
#include "DeprecatedGUI/CListview.h"
#include "DeprecatedGUI/CGuiSkinnedLayout.h"
#include "DeprecatedGUI/CBrowser.h"
#include "sound/SoundsBase.h"
#include "ProfileSystem.h"
#include "FeatureList.h"
#include "Options.h"
#include "CGameMode.h"
#include "game/Mod.h"
#include "IniReader.h"
#include "game/SinglePlayer.h"
#include "game/SettingsPreset.h"
#include "client/ClientConnectionRequestInfo.h"
#include "game/CWorm.h"
#include "ConfigHandler.h"


namespace DeprecatedGUI {

CGuiLayout cLocalMenu;

// Gui layout id's
enum {
	ml_GameLabel,
	ml_Game,
		
	ml_Back,
	ml_Start,
	
	__ml_AutoRemoveBegin,
	
	ml_Playing,
	ml_PlayerList,
	ml_LevelList,
	ml_Gametype,
	ml_ModName,
	ml_SettingPreset,
	ml_GameSettings,
    ml_WeaponOptions,
	
	ml_LevelLabel,
	ml_ModLabel,
	ml_LevelDescription,
	ml_LevelNumber,
	
	__ml_LowerLimit,
};


bool	bGameSettings = false;
bool    bWeaponRest = false;

static CCombobox* ComboboxWithVar(ScriptVar_t& var) {
	CCombobox* c = new CCombobox();
	c->setAttachedVar(&var);
	return c;
}
	
static void Menu_Local_InitCustomLevel() {
	// Minimap box
	Menu_DrawBox(tMenu->bmpBuffer.get(), 133,129, 266, 230);

	cLocalMenu.Add( new CListview(), ml_PlayerList,  410,115, 200, 126);
	cLocalMenu.Add( new CListview(), ml_Playing,     310,250, 300, 185);

	cLocalMenu.getWidget(ml_Playing)->setKeyboardNavigationOrder(1);
	
	int y = 235;
    cLocalMenu.AddBack( new CLabel("Level",tLX->clNormalLabel),	    -1,         30,  y+1, 0,   0);
	cLocalMenu.AddBack( ComboboxWithVar(gameSettings.overwrite[FT_Map]),	ml_LevelList,  120, y, 170, 17);
	y += 24;
	cLocalMenu.AddBack( new CLabel("Game type",tLX->clNormalLabel),	-1,         30,  y+1, 0,   0);
	cLocalMenu.AddBack( new CCombobox(),				ml_Gametype,   120, y, 170, 17);
	y += 24;
	CCombobox* modList = NULL;
	cLocalMenu.AddBack( new CLabel("Mod",tLX->clNormalLabel),	    -1,         30,  y+1, 0,   0);
	cLocalMenu.AddBack( modList = ComboboxWithVar(gameSettings.overwrite[FT_Mod]),		ml_ModName,    120, y, 170, 17);
	y += 24;
	CCombobox* presetList = NULL;
	cLocalMenu.AddBack( new CLabel("Settings",tLX->clNormalLabel),	    -1,         30,  y+1, 0,   0);
	cLocalMenu.AddBack( presetList = ComboboxWithVar(gameSettings.overwrite[FT_SettingsPreset]),		ml_SettingPreset,    120, y, 170, 17);
	
	setupModGameSettingsPresetComboboxes(modList, presetList);
	
	y += 27;
	cLocalMenu.AddBack( new CButton(BUT_GAMESETTINGS, tMenu->bmpButtons), ml_GameSettings, 27, y, 170,15);
	y += 25;
    cLocalMenu.AddBack( new CButton(BUT_WEAPONOPTIONS,tMenu->bmpButtons), ml_WeaponOptions,27, y, 185,15);

	cLocalMenu.SendMessage(ml_Playing,		LVS_ADDCOLUMN, "Playing", 24);
	cLocalMenu.SendMessage(ml_Playing,		LVS_ADDCOLUMN, "", 300 - gfxGame.bmpTeamColours[0].get()->w - 50);
	cLocalMenu.SendMessage(ml_Playing,		LVS_ADDCOLUMN, "", (uintptr_t)-1);
	
	cLocalMenu.SendMessage(ml_Playing,		LVM_SETOLDSTYLE, (uintptr_t)1, 0);
	
	cLocalMenu.SendMessage(ml_PlayerList,	LVS_ADDCOLUMN, "Players", 24);
	cLocalMenu.SendMessage(ml_PlayerList,	LVS_ADDCOLUMN, "", 60);
	
	cLocalMenu.SendMessage(ml_PlayerList,		LVM_SETOLDSTYLE, (uintptr_t)0, 0);
	
	for(Iterator<CGameMode*>::Ref i = GameModeIterator(); i->isValid(); i->next()) {
		cLocalMenu.SendMessage(ml_Gametype,    CBS_ADDITEM, i->get()->Name(), GetGameModeIndex(i->get()));
	}		
    cLocalMenu.SendMessage(ml_Gametype,    CBM_SETCURSEL, GetGameModeIndex(gameSettings[FT_GameMode].as<GameModeInfo>()->mode), 0);
	
	// Add players to player/playing lists
	Menu_LocalAddProfiles();
	
	// Fill the level list
	CCombobox* cbLevel = (CCombobox *)cLocalMenu.getWidget(ml_LevelList);
	Menu_FillLevelList( cbLevel, true);
	cbLevel->setCurItem(cbLevel->getSIndexItem(gameSettings[FT_Map].as<LevelInfo>()->path));
	Menu_LocalShowMinimap(true);
	
	// Fill in the mod list
	CCombobox* cbMod = (CCombobox *)cLocalMenu.getWidget(ml_ModName);
	Menu_Local_FillModList( cbMod );
	cbMod->setCurItem(cbMod->getSIndexItem(gameSettings[FT_Mod].as<ModInfo>()->path));
		
}
	
static std::string selectedGameIndex() {
	CCombobox* cb = (CCombobox *)cLocalMenu.getWidget(ml_Game);
	if(cb == NULL) {
		errors << "Local menu: selectedGameIndex: game combobox not found" << endl;
		return "";
	}
	
	GuiListItem::Pt item = cb->getSelectedItem();
	if(item.get() == NULL) {
		errors << "Local menu: selectedGameIndex: game combobox has not selected anything" << endl;
		return "";
	}
	
	return item->index();
}

static std::string selectedGameName() {
	CCombobox* cb = (CCombobox *)cLocalMenu.getWidget(ml_Game);
	if(cb == NULL) {
		errors << "Local menu: selectedGameName: game combobox not found" << endl;
		return "";
	}
	
	const GuiListItem::Pt item = cb->getSelectedItem();
	if(item.get() == NULL) {
		errors << "Local menu: selectedGameName: game combobox has not selected anything" << endl;
		return "";		
	}
	
	return item->caption();
}
	
static void newGameListEntry(const std::string& game, const std::string& sindex) {
	((CCombobox *)cLocalMenu.getWidget(ml_Game)) ->addItem(-1, sindex, game);
}
	
static void setCurGameComboIndex(const std::string& sindex) {
	((CCombobox *)cLocalMenu.getWidget(ml_Game)) ->setCurSIndexItem(sindex);
}
	
static void fillGameList() {
	newGameListEntry("- Custom level/mod -", "");
	
	for_each_iterator(std::string, f, FileListIter("games", false, FM_DIR)) {
		std::string name;
		if(ReadString("games/" + f->get() + "/game.cfg", "General", "Name", name, "")) {
			if(name != "")
				newGameListEntry(name, f->get());
		}
	}

	setCurGameComboIndex(tLXOptions->sLocalPlayGame);
	tLXOptions->sLocalPlayGame = selectedGameIndex(); // in case that the game does not exist anymore
}	
	
static bool deleteWidgets(int index) {
	bool one = false;
	while(cLocalMenu.getWidget(index)) {
		cLocalMenu.removeWidget(index);
		one = true;
	}
	return one;
}
	
static void uninitCurrentGameMenu() {
	deleteWidgets(-1);
	for(int i = __ml_AutoRemoveBegin + 1; i < __ml_LowerLimit; ++i)
		deleteWidgets(i);
}

static const int boxw = 600;
static const int boxh = 150;
static const int boxx = (640 - boxw) / 2;
static const int boxy = 80;
	
static void redrawGamePreviewImage() {
	DrawImageAdv(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common, boxx, boxy, boxx, boxy, boxw, boxh);
	
	if(singlePlayerGame.image.get())
		DrawImageResampledAdv(tMenu->bmpBuffer.get(), singlePlayerGame.image, 0, 0, boxx, boxy, singlePlayerGame.image->w, singlePlayerGame.image->h, boxw, boxh);
}

static void updateGameLevel() {
	redrawGamePreviewImage();

	CLabel* lvlLb = (CLabel *)cLocalMenu.getWidget(ml_LevelLabel);
	if(lvlLb) lvlLb->setText(singlePlayerGame.levelInfo.name);

	CLabel* modLb = (CLabel *)cLocalMenu.getWidget(ml_ModLabel);
	if(modLb) modLb->setText(singlePlayerGame.modInfo.name);

	CBrowser *b = (CBrowser *)cLocalMenu.getWidget(ml_LevelDescription);
	if(b) b->LoadFromString(singlePlayerGame.description);
	
	CSlider* s = (CSlider*)cLocalMenu.getWidget(ml_LevelNumber);
	if(s) {
		s->setMin(1);
		s->setMax(singlePlayerGame.maxSelectableLevelForCurrentGame());
		s->setValue(singlePlayerGame.currentLevel);
	}
}
	
static void Menu_Local_InitGameMenu() {
	std::string game = selectedGameName();
	singlePlayerGame.setGame(game);
	
	// box for preview image
	Menu_DrawBox(tMenu->bmpBuffer.get(), boxx - 4, boxy - 4, boxx + boxw + 3, boxy + boxh + 3);
	
	cLocalMenu.Add( new CLabel("Level",tLX->clHeading),			-1,    120, boxy + boxh + 10, 0, 0);
	cLocalMenu.Add( new CLabel("",tLX->clNormalLabel),	ml_LevelLabel,   170, boxy + boxh + 10, 0, 0);
	cLocalMenu.Add( new CLabel("Mod",tLX->clHeading),			-1,	   300, boxy + boxh + 10, 0, 0);
	cLocalMenu.Add( new CLabel("",tLX->clNormalLabel),		ml_ModLabel,  350, boxy + boxh + 10, 0, 0);

	static const int browsery = boxy + boxh + 30;
	static const int bottombuttonsh = 50;
	cLocalMenu.Add( new CBrowser(),		ml_LevelDescription,	   30, boxy + boxh + 30, 640 - 2*30, 480 - browsery - bottombuttonsh);

	cLocalMenu.Add( new CLabel("Level number", tLX->clHeading),		-1,	   150, 445, 0, 0);
	cLocalMenu.Add( new CSlider(1,1,1,true,204,-3,tLX->clNormalLabel),		ml_LevelNumber,	   250, 445 + 2, 200, 10);
	
	updateGameLevel();
}
	
static void initCurrentGameMenu() {
	// Create the buffer
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
	Menu_DrawSubTitleAdv(tMenu->bmpBuffer.get(),SUB_LOCAL,15);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,100, 625, 465);

	if(tLXOptions->sLocalPlayGame == "")
		Menu_Local_InitCustomLevel();
	else
		Menu_Local_InitGameMenu();
}
	
static void handleGameSwitch() {
	std::string newGame = selectedGameIndex();
	if(newGame != tLXOptions->sLocalPlayGame) {
		uninitCurrentGameMenu();
		tLXOptions->sLocalPlayGame = newGame;
		initCurrentGameMenu();
	}
}
	
///////////////////
// Initialize the local menu
void Menu_LocalInitialize()
{
	tMenu->iMenuType = MNU_LOCAL;
	bGameSettings = false;


	// Shutdown any previous data from before
	cLocalMenu.Shutdown();
	cLocalMenu.Initialize();

    cLocalMenu.Add( new CLabel("Game",tLX->clNormalLabel),	  ml_GameLabel,  180,  15, 0,   0);
	cLocalMenu.Add( new CCombobox(),	ml_Game,  250, 15, 170, 17);
	fillGameList();

	cLocalMenu.Add( new CButton(BUT_BACK, tMenu->bmpButtons), ml_Back, 27,440, 50,15);
	cLocalMenu.Add( new CButton(BUT_START, tMenu->bmpButtons), ml_Start, 555,440, 60,15);

	cLocalMenu.getWidget(ml_Back)->setKeyboardNavigationOrder(1);
	cLocalMenu.getWidget(ml_Start)->setKeyboardNavigationOrder(1);
	
	initCurrentGameMenu();
	
	
	Menu_RedrawMouse(true);
	cLocalMenu.Draw(VideoPostProcessor::videoSurface().get());
}

//////////////
// Shutdown
void Menu_LocalShutdown()
{
	if (bGameSettings)
		Menu_GameSettingsShutdown();
	if (bWeaponRest)
		Menu_WeaponsRestrictionsShutdown();


	// Save the level and mod
	if (tLXOptions)  {
		cLocalMenu.SendMessage(ml_LevelList,CBS_GETCURSINDEX, &gameSettings.overwrite[FT_Map].as<LevelInfo>()->path.write(), 0);
		cLocalMenu.SendMessage(ml_ModName,CBS_GETCURSINDEX, &gameSettings.overwrite[FT_Mod].as<ModInfo>()->path.write(), 0);
	}

	cLocalMenu.Shutdown();
}


///////////////////
// Local frame
void Menu_LocalFrame()
{
	gui_event_t *ev = NULL;
	mouse_t *Mouse = GetMouse();
	CListview *lv;

    // Game Settings
	if(bGameSettings) {
		if(Menu_GameSettings_Frame()) {
			// Re-do the buffer
			DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
	        Menu_DrawSubTitleAdv(tMenu->bmpBuffer.get(),SUB_LOCAL,15);
			if (tMenu->tFrontendInfo.bPageBoxes)
				Menu_DrawBox(tMenu->bmpBuffer.get(), 15,100, 625, 465);
	        Menu_DrawBox(tMenu->bmpBuffer.get(), 133,129, 266, 230);

			Menu_RedrawMouse(true);
			Menu_LocalShowMinimap(false);

			bGameSettings = false;
		}
		return;
	}


    // Weapons Restrictions
    if(bWeaponRest) {
		if(Menu_WeaponsRestrictions_Frame()) {
			// Re-do the buffer
			DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
	        Menu_DrawSubTitleAdv(tMenu->bmpBuffer.get(),SUB_LOCAL,15);
			if (tMenu->tFrontendInfo.bPageBoxes)
				Menu_DrawBox(tMenu->bmpBuffer.get(), 15,100, 625, 465);
	        Menu_DrawBox(tMenu->bmpBuffer.get(), 133,129, 266, 230);

			Menu_RedrawMouse(true);
			Menu_LocalShowMinimap(false);

			bWeaponRest = false;
		}
		return;
	}

	// Reload the list if user switches back to the game
	// Do not reload when a dialog is open
	if (tLXOptions->sLocalPlayGame == "" && tLXOptions->bAutoFileCacheRefresh && bActivated)  {
		// Get the mod name
		CCombobox* cbMod = (CCombobox *)cLocalMenu.getWidget(ml_ModName);
		const GuiListItem::Pt it = cbMod->getItem(cbMod->getSelectedIndex());
		if(it.get()) gameSettings.overwrite[FT_Mod].as<ModInfo>()->path = it->index();

		// Fill in the mod list
		Menu_Local_FillModList( cbMod );
		cbMod->setCurItem(cbMod->getSIndexItem(gameSettings.overwrite[FT_Mod].as<ModInfo>()->path));

		// Fill in the levels list
		CCombobox* cbLevel = (CCombobox *)cLocalMenu.getWidget(ml_LevelList);
		const GuiListItem::Pt item = cbLevel->getItem( cbLevel->getSelectedIndex() );
		if (item.get())
			gameSettings.overwrite[FT_Map].as<LevelInfo>()->path = item->index();
		Menu_FillLevelList( cbLevel, true);
		cbLevel->setCurItem(cbLevel->getSIndexItem(gameSettings[FT_Map].as<LevelInfo>()->path));

		// Reload the minimap
		//if (gameSettings[FT_Map].as<LevelInfo>()->path != "_random_")
		Menu_LocalShowMinimap(true);
	}


    // Was the mouse clicked on the map section
	if( (tLXOptions->sLocalPlayGame == "") && (Mouse->Up & SDL_BUTTON(1)) ) {
    	// TODO: hardcoded sizes here
        if( MouseInRect(136,132,128,96) )
	    	// TODO: why are we redrawing the minimap here?
            Menu_LocalShowMinimap(true);
    }


	ev = cLocalMenu.Process();
	cLocalMenu.Draw(VideoPostProcessor::videoSurface().get());

	if(ev) {

		switch(ev->iControlID) {
			case ml_Game:
				//if(ev->iEventMsg == BTN_CLICKED) {
				handleGameSwitch();
				//}
				break;

			case ml_LevelNumber:
				if(ev->iEventMsg == SLD_CHANGE) {
					CSlider *s = (CSlider *)cLocalMenu.getWidget(ml_LevelNumber);
					singlePlayerGame.setLevel( s->getValue() );
					updateGameLevel();
				}
				break;
								
			// Back
			case ml_Back:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Shutdown
					Menu_LocalShutdown();

					// Leave
					PlaySoundSample(sfxGeneral.smpClick);
					Menu_MainInitialize();
					return;
				}
				break;

			// Start
			case ml_Start:
				if(ev->iEventMsg == BTN_CLICKED) {
					PlaySoundSample(sfxGeneral.smpClick);

					// Start the game
					Menu_LocalStartGame();
				}
				break;

			// Player list
			case ml_PlayerList:
				if(ev->iEventMsg == LV_DOUBLECLK || ev->iEventMsg == LV_RIGHTCLK || ev->iEventMsg == LV_ENTER) {
					Menu_LocalAddPlaying();
				}
				break;

			// Playing list
			case ml_Playing:
				if(ev->iEventMsg == LV_DOUBLECLK || ev->iEventMsg == LV_RIGHTCLK || ev->iEventMsg == LV_ENTER) {
					Menu_LocalRemovePlaying();
				}


				if(ev->iEventMsg == LV_WIDGETEVENT && gameSettings[FT_GameMode].as<GameModeInfo>()->mode->GameTeams() > 1) {

					// If the team colour item was clicked on, change it
					lv = (CListview *)cLocalMenu.getWidget(ml_Playing);

					ev = lv->getWidgetEvent();
					if (ev->cWidget->getType() == wid_Image && ev->iEventMsg == IMG_CLICK)  {
						lv_item_t *it = lv->getItem(ev->iControlID);
						lv_subitem_t *sub = lv->getSubItem(it, 2);

						if(sub) {
							sub->iExtra++;
							sub->iExtra %= 4;

							// Change the image
							((CImage *)ev->cWidget)->Change(DynDrawFromSurface(gfxGame.bmpTeamColours[sub->iExtra]));

							SmartPointer<profile_t> p = FindProfile(it->iIndex);
							if(p.get()) {
								p->iTeam = sub->iExtra;
								//tMenu->sLocalPlayers[ev->iControlID].ChangeGraphics(gameSettings[FT_GameMode].as<GameModeInfo>()->mode->GeneralGameType());
							}
						}
					}
				}
				break;


			// Game type
			case ml_Gametype:
				if(ev->iEventMsg == CMB_CHANGED) {
					gameSettings.overwrite[FT_GameMode].as<GameModeInfo>()->mode = GameMode((GameModeIndex)cLocalMenu.SendMessage(ml_Gametype, CBM_GETCURINDEX, (uintptr_t)0, 0));

					// Go through the items and enable/disable the team flags and update worm graphics
					bool teams_on = gameSettings[FT_GameMode].as<GameModeInfo>()->mode->GameTeams() > 1;
					CListview *lv = (CListview *)cLocalMenu.getWidget(ml_Playing);
					lv_item_t *it = lv->getItems();
					lv_subitem_t *sub = NULL;
					for (; it; it = it->tNext)  {
						sub = lv->getSubItem(it, 2);
						if (sub)
							sub->bVisible = teams_on;

						// Update the skin
						sub = lv->getSubItem(it, 0);
						if (sub)  {
//							tMenu->sLocalPlayers[it->iIndex].ChangeGraphics(gameSettings[FT_GameMode].as<GameModeInfo>()->mode->GeneralGameType());
//							sub->bmpImage = tMenu->sLocalPlayers[it->iIndex].getPicimg();
						}

					}
				}
				break;

			// Level list combo box
			case ml_LevelList:
				if(ev->iEventMsg == CMB_CHANGED) {

					Menu_LocalShowMinimap(true);
				}
				break;


			// Game settings button
			case ml_GameSettings:
				if(ev->iEventMsg == BTN_CLICKED) {

					cLocalMenu.Draw( tMenu->bmpBuffer.get() );

					bGameSettings = true;
					Menu_GameSettings();
				}
				break;


            // Weapons Restrictions button
            case ml_WeaponOptions:
                if( ev->iEventMsg == BTN_CLICKED ) {

					cLocalMenu.Draw( tMenu->bmpBuffer.get() );

                    // Get the current mod
					GuiListItem::Pt it = ((CCombobox*)cLocalMenu.getWidget(ml_ModName))->getSelectedItem();
                    if(it.get()) {

					    bWeaponRest = true;
					    Menu_WeaponsRestrictions(it->index());
                    }
                }
                break;

		}
	}

	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());
}

static void _addPlaying(const SmartPointer<profile_t>& ply) {
	int index = GetProfileId(ply);
	assert(index >= 0);
	
	// Add the item
	ply->iTeam = CLAMP(ply->iTeam, 0, MAX_TEAMS-1);
	CImage *img = new CImage(gfxGame.bmpTeamColours[ply->iTeam]);
	if (img)  {
		img->setID(index);
		img->setRedrawMenu(false);
	} else
		warnings << "Cannot load teamcolor image" << endl;

	CListview* lv = (CListview *)cLocalMenu.getWidget(ml_Playing);
	lv->AddItem("",index,tLX->clListView);
	lv->AddSubitem(LVS_IMAGE, "", ply->cSkin.getPreview(), NULL);
	lv->AddSubitem(LVS_TEXT, ply->sName, (DynDrawIntf*)NULL, NULL);
	lv->AddSubitem(LVS_WIDGET, "", (DynDrawIntf*)NULL, img);
	
	// If we're in deathmatch, make the team colour invisible
	lv_subitem_t *sub = lv->getSubItem(lv->getLastItem(), 2);
	if(sub) {
		if(gameSettings[FT_GameMode].as<GameModeInfo>()->mode->GameTeams() <= 1)
			sub->bVisible = false;
		sub->iExtra = 0;
	} else
		warnings << "Strange: did not found teamcolor subitem" << endl;		
}
	
//////////////////
// Move a worm from player list to playing list
void Menu_LocalAddPlaying(int index)
{
	// Add the item to the players list
	CListview *lv = (CListview *)cLocalMenu.getWidget(ml_PlayerList);
	if (index < 0)
		index = lv->getCurIndex();

	if(index < 0 || index >= MAX_PLAYERS) {
		errors << "Menu_LocalAddPlaying: invalid index = " << index << endl;
		return;
	}
	
	// Check if we have enough room for another player
	if(!Menu_LocalCheckPlaying(index))
		return;

	// Remove the item from the list
	lv->RemoveItem(index);

	SmartPointer<profile_t> ply = FindProfile(index);
	
	if(!ply.get())
		return;

	// Add a player onto the local players list
	tMenu->sLocalPlayers.push_back(ply);
	
	_addPlaying(ply);
}

static void _refillPlayerList() {
	CListview * w = (CListview *) cLocalMenu.getWidget(ml_PlayerList);
	w->Clear();
	
	std::set<int> playingIds;
	foreach(p, tMenu->sLocalPlayers)
		playingIds.insert(GetProfileId(*p));
	
	int i = 0;
	for_each_iterator(SmartPointer<profile_t>, p, GetProfiles()) {
		if(playingIds.count(i) == 0) {
			w->AddItem("", i, tLX->clListView);		
			//cLocalMenu.SendMessage( ml_PlayerList, LVS_ADDSUBITEM, (uintptr_t)p->bmpWorm, LVS_IMAGE); // TODO: 64bit unsafe (pointer cast)
			//cLocalMenu.SendMessage( ml_PlayerList, LVS_ADDSUBITEM, p->sName, LVS_TEXT);
			w->AddSubitem( LVS_IMAGE, "", p->get()->cSkin.getPreview(), NULL );
			w->AddSubitem( LVS_TEXT, p->get()->sName, (DynDrawIntf*)NULL, NULL );
		}
		++i;
	}	
}
	
//////////////////
// Move a player from playing list back to player list
void Menu_LocalRemovePlaying(int index)
{
	// Put the player back into the players list
	CListview *lv = (CListview *)cLocalMenu.getWidget(ml_Playing);
	if (index < 0)
		index = lv->getCurIndex();

	// Remove the item from the list
	lv->RemoveItem(index);

	SmartPointer<profile_t> removedProf = FindProfile(index);
	foreach(p, tMenu->sLocalPlayers)
		if(p->get() == removedProf.get()) {
			tMenu->sLocalPlayers.erase(p);
			break;
		}
	
	_refillPlayerList(); // refill the list
}


///////////////////
// Add the profiles to the players list
void Menu_LocalAddProfiles()
{
	foreach(p, tMenu->sLocalPlayers) {
		if(GetProfileId(*p) < 0)
			AddProfile(*p); // assure that the profile is registered
	}
	
	_refillPlayerList();

	foreach(p, tMenu->sLocalPlayers)
		_addPlaying(*p);	
}


///////////////////
// Show the minimap
void Menu_LocalShowMinimap(bool bReload)
{
	std::string buf;

	cLocalMenu.SendMessage(ml_LevelList, CBS_GETCURSINDEX, &buf, 0);

    //getGameLobby()->sMapRandom.bUsed = false;

	// Draw a background over the old minimap
	//DrawImageAdv(tMenu->bmpBuffer, tMenu->bmpMainBack, 126,132,126,132,128,96);

	// Load the map
    if( bReload ) {
		/*
        // Create a random map
        if( buf == "_random_" ) {
            if( map.Create(504,350,map.findRandomTheme(), 128, 96) ) {
				map.TileMap();
			    map.ApplyRandom();

                // Free any old random map object list
                if( getGameLobby()->sMapRandom.psObjects ) {
                    delete[] getGameLobby()->sMapRandom.psObjects;
                    getGameLobby()->sMapRandom.psObjects = NULL;
                }

                // Copy the layout
                maprandom_t *psRand = map.getRandomLayout();
                getGameLobby()->sMapRandom = *psRand;
                getGameLobby()->sMapRandom.bUsed = true;

                // Copy the objects, not link
                getGameLobby()->sMapRandom.psObjects = new object_t[getGameLobby()->sMapRandom.nNumObjects];
                if( getGameLobby()->sMapRandom.psObjects ) {
                    for( int i=0; i<getGameLobby()->sMapRandom.nNumObjects; i++ ) {
                        getGameLobby()->sMapRandom.psObjects[i] = psRand->psObjects[i];
                    }
                }

                // Draw the minimap
		        DrawImage(tMenu->bmpMiniMapBuffer.get(), map.GetMiniMap(), 0,0);
		        map.Shutdown();
            }

        } else {
        */
		
		DrawImage(tMenu->bmpMiniMapBuffer.get(), minimapForLevel(buf), 0, 0);
    }

	// Update the screen
    DrawImage(tMenu->bmpBuffer.get(), tMenu->bmpMiniMapBuffer, 136,132);
	DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, 130,130,130,130,140,110);
}

static bool Menu_LocalStartGame_CustomGame() {
	
	// Level
	cLocalMenu.SendMessage(ml_LevelList, CBS_GETCURSINDEX, &gameSettings.overwrite[FT_Map].as<LevelInfo>()->path.write(), 0);
	cLocalMenu.SendMessage(ml_LevelList, CBS_GETCURNAME, &gameSettings.overwrite[FT_Map].as<LevelInfo>()->name.write(), 0);
	
	
	//
	// Players
	//
		
	if(! cClient->Initialize() )
	{
		errors << "Could not initialize client" << endl;
		return false;
	}
	
	if(!cServer->StartServer()) {
		errors << "Could not start server" << endl;
		return false;
	}
	
	cClient->connectInfo = new ClientConnectionRequestInfo;
	
    // Add the human players onto the list
	foreach(p, tMenu->sLocalPlayers) {
		if((*p)->iType == PRF_HUMAN->toInt())
			cClient->connectInfo->worms.push_back(*p);
	}
	
    // Add the unhuman players onto the list
	foreach(p, tMenu->sLocalPlayers) {
		if((*p)->iType != PRF_HUMAN->toInt())
			cClient->connectInfo->worms.push_back(*p);
	}
	
	// Can't start a game with no-one playing
	if(cClient->connectInfo->worms.size() == 0) {
		errors << "Menu_LocalStartGame_CustomGame: strange, tMenu->sLocalPlayers is empty but playing list is not" << endl;
		// try to refill
		((CListview *)cLocalMenu.getWidget(ml_Playing))->Clear();
		Menu_LocalAddProfiles();
		return false;
	}
	
	// Save the current level in the options
	cLocalMenu.SendMessage(ml_LevelList, CBS_GETCURSINDEX, &gameSettings.overwrite[FT_Map].as<LevelInfo>()->path.write(), 0);
	
	//
	// Game Info
	//
	gameSettings.overwrite[FT_GameMode].as<GameModeInfo>()->mode = GameMode((GameModeIndex)cLocalMenu.SendMessage(ml_Gametype, CBM_GETCURINDEX, (uintptr_t)0, 0));
	
    // Get the mod name
	GuiListItem::Pt it = ((CCombobox*)cLocalMenu.getWidget(ml_ModName))->getSelectedItem();
    if(it.get()) {
        gameSettings.overwrite[FT_Mod].as<ModInfo>()->name = it->caption();
		gameSettings.overwrite[FT_Mod].as<ModInfo>()->path = it->index();
    } else {
		
		// Couldn't find a mod to load
		cLocalMenu.Draw(tMenu->bmpBuffer.get());
		Menu_MessageBox("Error","Could not find a mod to load!", LMB_OK);
		DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
        Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
		Menu_DrawSubTitle(tMenu->bmpBuffer.get(),SUB_LOCAL);
		Menu_RedrawMouse(true);
		return false;
	}

	return true;
}

///////////////////
// Start a local game
void Menu_LocalStartGame()
{
	bool doSPG = tLXOptions->sLocalPlayGame != "";

	if(!doSPG) {
		CListview *lv_playing = (CListview *)cLocalMenu.getWidget(ml_Playing);
		
		if (lv_playing->getItemCount() == 0) {
			Menu_MessageBox("Too few players", "You have to select at least one player.");
			return;
		}
		
		if(lv_playing->getItemCount() > MAX_PLAYERS) {
			Menu_MessageBox("Too many players",
							"You have selected " + itoa(lv_playing->getItemCount()) + " players "
							"but only " + itoa(MAX_PLAYERS) + " players are possible.");
			return;
		}
	}
	
	game.startServer(/* localGame */ true);

	bool ok = false;
	if(!doSPG)
		ok = Menu_LocalStartGame_CustomGame();
	else
		ok = singlePlayerGame.startGame();

	if(ok) {
		// Tell the client to connect to the server
		cClient->Connect("127.0.0.1:" + itoa(cServer->getPort()));
		
		cLocalMenu.Shutdown();
	}
	else
		game.state = Game::S_Inactive;
}

///////////////////
// Check if we can add another player to the list
bool Menu_LocalCheckPlaying(int index)
{
	// Check if there is too many players
	if(tMenu->sLocalPlayers.size() >= MAX_PLAYERS)
		return false;

	return true;
}


	class ModAdder { public:
		CCombobox* combobox;
		ModAdder(CCombobox* cb_) : combobox(cb_) {}
		bool operator() (const std::string& abs_filename) {
			ModInfo info = infoForMod(abs_filename, true);
			if(info.valid)
				combobox->addItem(info.path, info.name + " [" + info.typeShort.get() + "]");

			return true;
		}
	};


///////////////////
// Fill in the mod list
// HINT: also used by Menu_Net_Host
// TODO: move to MenuSystem
void Menu_Local_FillModList( CCombobox *cb )
{
	// Find all directories in the the lierox
	cb->clear();
	cb->setSorted(SORT_ASC);
	cb->setUnique(true);
	ModAdder adder(cb);
	FindFiles(adder,"",false,FM_DIR);
	
	cb->setCurSIndexItem(gameSettings[FT_Mod].as<ModInfo>()->path);
}




/*
=======================

	 Game Settings

 For both local & host

=======================
*/

//short			GameTabPane = 0;
CGuiLayout		cGameSettings;
//CGuiLayout		cGeneralSettings;
//CGuiLayout		cBonusSettings;

// Game Settings
enum {
	gs_Ok,
	gs_Default,
	gs_ShowModifiedOnly,
	gs_AdvancedLevel,
	gs_AdvancedLevelLabel,
	
	gs_FeaturesList,
	gs_FeaturesListLabel,
	
};

static void initFeaturesList(CListview* l);

///////////////////
// Initialize the game settings
void Menu_GameSettings()
{
	//GameTabPane = 0;
	// Setup the buffer
	Menu_DrawBox(tMenu->bmpBuffer.get(), 80 -35,120, 560+35,460);
	DrawRectFillA(tMenu->bmpBuffer.get(), 82 -35,122, 558+35,458, tLX->clDialogBackground, 245);

	Menu_RedrawMouse(true);


	// Shutdowns all 3 following instances
	Menu_GameSettingsShutdown();

	cGameSettings.Initialize();
	//cGeneralSettings.Initialize();

	// Keep text, it's the window text - the rest you can easily figure out by yourself.
	cGameSettings.Add( new CLabel("Game Settings", tLX->clNormalLabel),		    -1,	        280,135, 0, 0);

	// Game settings, stuff on each pane.

	cGameSettings.Add( new CButton(BUT_OK, DeprecatedGUI::tMenu->bmpButtons),	    gs_Ok,      180,435, 40,15);
    cGameSettings.Add( new CButton(BUT_DEFAULT, DeprecatedGUI::tMenu->bmpButtons), gs_Default, 390,435, 80,15);

	cGameSettings.Add( new CCheckbox(&tLXOptions->bShowModifiedGameSettingsOnly), gs_ShowModifiedOnly, 60, 155, 15,15);
	cGameSettings.Add( new CLabel("Show modified settings only", tLX->clNormalLabel), -1, 60+20, 155, 70, 15);

	cGameSettings.Add( new CSlider(__AdvancedLevelType_Count - 1, 0, tLXOptions->iAdvancedLevelLimit), gs_AdvancedLevel, 365+40, 155, 80,15);
	cGameSettings.Add( new CLabel("Detail Level:", tLX->clNormalLabel), -1, 285+40, 155, 70, 15);
	float warningCoeff = CLAMP((float)tLXOptions->iAdvancedLevelLimit / (__AdvancedLevelType_Count - 1), 0.0f, 1.0f);
	cGameSettings.Add( new CLabel(AdvancedLevelShortDescription((AdvancedLevel)tLXOptions->iAdvancedLevelLimit), tLX->clNormalLabel * (1.0f - warningCoeff) + tLX->clError * warningCoeff), gs_AdvancedLevelLabel, 450+40, 155, 70, 15);

	CListview* features = new CListview();
	cGameSettings.Add( features, gs_FeaturesList, 95 - 35, 170, 450 + 70, 205);

	features->setDrawBorder(true);
	features->setRedrawMenu(false);
	features->setShowSelect(false);
	features->setOldStyle(true);
	features->subItemsAreAligned() = true;
	features->setMouseOverEventEnabled(true);
	
	int maxWidth = 0; // Width of the widest item in this column + some space
	CScriptableVars::const_iterator upper_bound = CScriptableVars::upper_bound("GameOptions.");
	for( CScriptableVars::const_iterator it = CScriptableVars::lower_bound("GameOptions."); it != upper_bound; it++ ) 
	{
		if( tLX->cFont.GetWidth(it->second.shortDesc) > maxWidth )
			maxWidth = tLX->cFont.GetWidth(it->second.shortDesc);
	}
	
	features->AddColumn("", 60 + 10); 
	features->AddColumn("", maxWidth + 10); 
	features->AddColumn("", 190); 
	
	initFeaturesList(features);

	cGameSettings.Add( new CLabel("", tLX->clNormalLabel), gs_FeaturesListLabel, 95 - 35, 390, 450 + 70, 40);
}

// Features listview

static void addFeautureListGroupHeading(CListview* l, GameInfoGroup group) {
	if( l->getItemCount() > 0 )
		l->AddItem("", l->getNumItems(), tLX->clNormalLabel); // Empty line
	l->AddItem(GameInfoGroupDescriptions[group][0], l->getNumItems(), tLX->clHeading);
	l->AddSubitem(LVS_TEXT, std::string("--- ") + GameInfoGroupDescriptions[group][0] + " ---" + 
				  (tLXOptions->iGameInfoGroupsShown[group] ? " [-]" : " [+]"), (DynDrawIntf*)NULL, NULL);	
}

static int getListItemGroupInfoNr(const std::string& sindex) {
	for( int group = 0; group < GIG_Size; group++ )
		if( sindex == GameInfoGroupDescriptions[group][0] )
			return group;
	return -1;
}
	
static bool varIsSet(const ScriptVarPtr_t& var) {
	Feature* f = featureByVar(var, false);
	return f ? tLXOptions->customSettings.isSet[featureArrayIndex(f)] : false;
}

static void updateFeatureListItemColor(lv_item_t* item) {
	lv_subitem_t* sub = item->tSubitems; if(!sub) return;
	RegisteredVar* var = CScriptableVars::GetVar(item->sIndex);
	const bool isSet = var && varIsSet(var->var);
	
	if(CButton* resetBtn = dynamic_cast<CButton*> (sub->tWidget))
		resetBtn->bVisible = isSet;
	
	sub = sub->tNext; // first one is reset button
	if(!sub) return;
	
	if(((CListview*)cGameSettings.getWidget(gs_FeaturesList))->getMouseOverSIndex() == item->sIndex) {
		sub->iColour = tLX->clMouseOver;
		return;
	}
	
	int group = getListItemGroupInfoNr(item->sIndex);
	if(group >= 0) sub->iColour = tLX->clHeading;
	else {
		/*
		// Note: commented out because I am not sure if it is that nice
		float warningCoeff = 0.0f; //CLAMP((float)var->advancedLevel / (__AdvancedLevelType_Count - 1), 0.0f, 1.0f);
		sub->iColour = tLX->clNormalLabel * (1.0f - warningCoeff) + tLX->clError * warningCoeff;
		 */
		
		// now with the settings layer, we show it normal if unset and mark it, if it is set
		//sub->iColour = isSet ? tLX->clSubHeading : tLX->clNormalLabel;
		
		// ok, still just use std color; resetbtn already shows that it was set
		sub->iColour = tLX->clNormalLabel;
	}
}

static bool isSettingForGameSettingsDialog(const ScriptVarPtr_t& var) {
	if( var == &gameSettings.wrappers[FT_Mod] || 
	   var == &gameSettings.wrappers[FT_Map] ||
	   var == &gameSettings.wrappers[FT_GameMode] ||
	   var == &gameSettings.wrappers[FT_SettingsPreset] ||
	   var.ptr.i == &tLXOptions->iMaxPlayers )
		return false;	// We have nice comboboxes for them, skip them in the list	
	
	return true;
}
	
static void initFeaturesList(CListview* l)
{
	l->Clear();
	for( GameInfoGroup group = (GameInfoGroup)0; group < GIG_Size; group = (GameInfoGroup)(group + 1) )
	{
		if( group == GIG_GameModeSpecific_Start )
			continue;
		if( group > GIG_GameModeSpecific_Start && 
			gameSettings[FT_GameMode].as<GameModeInfo>()->mode->getGameInfoGroupInOptions() != group )
			continue;

		size_t countGroupOpts = 0;
		CScriptableVars::const_iterator upper_bound = CScriptableVars::upper_bound("GameOptions.");
		for( CScriptableVars::const_iterator it = CScriptableVars::lower_bound("GameOptions."); it != upper_bound; it++ ) 
		{
			if( it->second.group != group ) continue;
			if( (int)it->second.advancedLevel >= ALT_OnlyViaConfig ) continue;
			
			if(!isSettingForGameSettingsDialog(it->second.var))
				continue;

			if(tLXOptions->bShowModifiedGameSettingsOnly) {
				if(Feature* f = featureByVar(it->second.var, false)) {
					if(!tLXOptions->customSettings.isSet[featureArrayIndex(f)])
						continue;
				}
				else {
					if( (int)it->second.advancedLevel > tLXOptions->iAdvancedLevelLimit )
						continue;
				}
			}
			else {
				if( (int)it->second.advancedLevel > tLXOptions->iAdvancedLevelLimit )
					continue;				
			}
			
			if( tMenu && tMenu->iMenuType == MNU_LOCAL )
				if( it->second.var.ptr.b == &tLXOptions->bAllowConnectDuringGame )
					continue;
			
			if( !gameSettings[FT_GameMode].as<GameModeInfo>()->mode || !gameSettings[FT_GameMode].as<GameModeInfo>()->mode->isTeamGame() ) {
				if( it->second.var.ptr.i == &tLXOptions->iRandomTeamForNewWorm ) continue;
				if( it->second.var == &gameSettings.wrappers[FT_RespawnGroupTeams] ) continue;
				if( it->second.var == &gameSettings.wrappers[FT_TeamScoreLimit] ) continue;
				if( it->second.var == &gameSettings.wrappers[FT_TeamkillDecreasesScore] ) continue;
				if( it->second.var == &gameSettings.wrappers[FT_TeamInjure] ) continue;				
				if( it->second.var == &gameSettings.wrappers[FT_TeamHit] ) continue;				
			}	
			
			if(countGroupOpts == 0)
				addFeautureListGroupHeading(l, group);
			countGroupOpts++;

			if( ! tLXOptions->iGameInfoGroupsShown[group] )
				continue;
			
			lv_item_t * item = l->AddItem(it->first, l->getNumItems(), tLX->clNormalLabel);

			CButton* resetBtn = new CButton(BUT_RESET, tMenu->bmpButtons);
			l->AddSubitem(LVS_WIDGET, "", (DynDrawIntf*)NULL, resetBtn);
			resetBtn->Create();
			resetBtn->Setup(-1, 0, 0, 60, 15);			
			resetBtn->bVisible = varIsSet(it->second.var);
			
			l->AddSubitem(LVS_TEXT, it->second.shortDesc, (DynDrawIntf*)NULL, NULL); 
			item->iHeight = 24; // So checkbox / textbox will fit okay

			if( it->second.var.valueType() == SVT_BOOL )
			{
				CCheckbox * cb = new CCheckbox( it->second.var.asScriptVar().toBool() );
				l->AddSubitem(LVS_WIDGET, "", (DynDrawIntf*)NULL, cb);
				cb->Create();
				cb->Setup(-1, 0, 0, 20, 20);
			}
			else
			{
				int textboxSize = 228;
				if( it->second.haveMinMax() )
				{
					int imin=0, imax=0;
					float fScale = 1.0f;
					int iVal = 0;
					if( it->second.var.valueType() == SVT_FLOAT )
					{
						// Adding some small number to round it up correctly
						imin = int( float(it->second.min) *10.0f + 0.00001f );	// Scale them up
						imax = int( float(it->second.max) *10.0f + 0.00001f );
						iVal = int( it->second.var.asScriptVar().toFloat() * 10.0f + 0.00001f );
						fScale = 0.1f;
					} else {
						imin = it->second.min;
						imax = it->second.max;
						iVal = it->second.var.asScriptVar().toInt();
					}
					CSlider * sld = new CSlider( imax, imin, imin, false, 190, 0, tLX->clNormalLabel, fScale );
					CLAMP_DIRECT(iVal, sld->getMin(), sld->getMax() );
					sld->setValue(iVal);
					l->AddSubitem(LVS_WIDGET, "", (DynDrawIntf*)NULL, sld);
					sld->Create();
					sld->Setup(-1, 0, 0, 180, tLX->cFont.GetHeight());
					textboxSize = 40;
				}
				CTextbox * txt = new CTextbox();
				l->AddSubitem(LVS_WIDGET, "", (DynDrawIntf*)NULL, txt);
				txt->Create();
				txt->Setup(-1, 0, 0, textboxSize, tLX->cFont.GetHeight());
				if (it->second.var.asScriptVar().isNumeric() && it->second.var.isUnsigned && it->second.var.asScriptVar().getNumber() < 0)
					txt->setText("");  // Leave blank for infinite values
				else
					txt->setText( it->second.var.toString() );				
			}
			
			updateFeatureListItemColor(item);
		}
	}
}

	
static void unsetGameSettingsVar(const std::string& varname, const ScriptVarPtr_t& var) {
	Feature* f = featureByVar(var, false);
	if(f)
		// we have a game setting
		tLXOptions->customSettings.isSet[featureArrayIndex(f)] = false;
	// NOTE: Don't reset other settings, this is anyway the wrong dialog for it.
}
	
	
static void resetItemFromVar(lv_item_t* item, const ScriptVarPtr_t& var) {
	if( ! item || !item->tSubitems || !item->tSubitems->tNext )
		return;
	lv_subitem_t * si = item->tSubitems->tNext->tNext;
	if( ! si )
		return;
	CWidget * w = si->tWidget;
	if( ! w )
		return;

	switch(w->getType()) {
		case wid_Checkbox:
			((CCheckbox*)w)->setValue(var.asScriptVar().toBool());
			break;
		case wid_Textbox:
			((CTextbox*)w)->setText(var.toString());
			break;
		case wid_Slider:
			if( si->tNext && si->tNext->tWidget && si->tNext->tWidget->getType() == wid_Textbox )
			{
				CSlider *slider = (CSlider *)w;
				CTextbox *textBox = (CTextbox *)si->tNext->tWidget;
				int iVal = 0;
				
				if( var.valueType() == SVT_INT32 )
				{
					iVal = var.asScriptVar().toInt();
					textBox->setText(itoa(iVal));
					if( var.isUnsigned && iVal < 0 )
						textBox->setText("");
				}
				else if( var.valueType() == SVT_FLOAT )
				{
					iVal = int(var.asScriptVar().toFloat() * 10.0f);
					textBox->setText(to_string<float>(iVal / 10.0f));
					if( var.isUnsigned && iVal < 0 )
						textBox->setText("");
				}
				
				CLAMP_DIRECT(iVal, slider->getMin(), slider->getMax() );
				slider->setValue(iVal);
			}
			break;
		default:
			// ignore all others (should anyway not happen)
			break;
	}
}
	
// Copy values from listview to features list
static void updateFeaturesList(CListview* l) 
{
	// we only update the changed widget
	if(!l->getWidgetEvent() || !l->getWidgetEvent()->cWidget) return;

	CScriptableVars::const_iterator upper_bound = CScriptableVars::upper_bound("GameOptions.");
	for( CScriptableVars::const_iterator it = CScriptableVars::lower_bound("GameOptions."); it != upper_bound; it++ ) 
	{
		if( it->second.group == GIG_Invalid ) continue;

		lv_item_t * item = l->getItem(it->first);
		if( ! item || !item->tSubitems || !item->tSubitems->tNext )
			continue;
		lv_subitem_t * si = item->tSubitems->tNext->tNext;
		if( ! si )
			continue;
		CWidget * w = si->tWidget;
		if( ! w )
			continue;
		
		
		CButton* resetBtn = dynamic_cast<CButton*>(item->tSubitems->tWidget);
		if(resetBtn == l->getWidgetEvent()->cWidget) {
			unsetGameSettingsVar(it->first, it->second.var);
			updateFeatureListItemColor(item);
			resetItemFromVar(item, it->second.var);
			return; // no continue because there wont be another event
		}						
		
		switch(w->getType()) {
			case wid_Checkbox:
				if(l->getWidgetEvent()->cWidget == w)
					it->second.var.fromScriptVar( ScriptVar_t(((CCheckbox *)w)->getValue()) );
				break;
			case wid_Textbox:
				if(l->getWidgetEvent()->cWidget == w)
					it->second.var.fromString( ((CTextbox *)w)->getText() );
				break;
			case wid_Slider:
				if( 
				   si->tNext && si->tNext->tWidget && si->tNext->tWidget->getType() == wid_Textbox &&
				   l->getWidgetEvent() && l->getWidgetEvent()->cWidget )
				{
					CSlider *slider = (CSlider *)w;
					CTextbox *textBox = (CTextbox *)si->tNext->tWidget;
					
					if( l->getWidgetEvent()->cWidget == slider ) // User moved slider - update textbox
					{
						int iVal = slider->getValue();
						if( it->second.var.valueType() == SVT_INT32 )
						{
							it->second.var.fromScriptVar( ScriptVar_t(iVal) );
							textBox->setText(itoa(iVal));
							if( it->second.var.isUnsigned && iVal < 0 )
								textBox->setText("");
						}
						if( it->second.var.valueType() == SVT_FLOAT )
						{
							it->second.var.fromScriptVar( ScriptVar_t(float(iVal / 10.0f)) );
							textBox->setText(to_string<float>(iVal / 10.0f));
							if( it->second.var.isUnsigned && iVal < 0 )
								textBox->setText("");
						}
					}
					
					else if( l->getWidgetEvent()->cWidget == textBox ) // User typed in textbox - update slider
					{
						it->second.var.fromString(textBox->getText());
						int iVal = 0;
						if( it->second.var.valueType() == SVT_INT32 )
						{
							// Do not do min/max check on typed value, it's sole user responsibility if game crashes (though it should not)
							//CLAMP_DIRECT(* it->second.var.i, it->second.min.i, it->second.max.i );
							iVal = it->second.var.asScriptVar().toInt();
						}
						if( it->second.var.valueType() == SVT_FLOAT )
						{
							// Do not do min/max check on typed value, it's sole user responsibility if game crashes (though it should not)
							//CLAMP_DIRECT(*it->second.var.f, it->second.min.f, it->second.max.f );
							iVal = int(it->second.var.asScriptVar().toFloat() * 10.0f);
						}
						CLAMP_DIRECT(iVal, slider->getMin(), slider->getMax() );
						slider->setValue(iVal);
					}
				}
				break;
			default:
				// ignore all others (should anyway not happen)
				break;
		}
		
		updateFeatureListItemColor(item);
	}
	if( tLXOptions->customSettings.isSet[FT_Lives] && (int)tLXOptions->customSettings[FT_Lives] < 0 )
		tLXOptions->customSettings.set(FT_Lives) = (int)WRM_UNLIM;
}

/////////////
// Shutdown
void Menu_GameSettingsShutdown()
{
	cGameSettings.Shutdown();
}



///////////////////
// Game settings frame
// Returns whether of not we have finised with the game settings
bool Menu_GameSettings_Frame()
{
	gui_event_t *ev = NULL;

	ev = cGameSettings.Process();

	if(ev)
	{

		switch(ev->iControlID)
		{

			// OK, done
			case gs_Ok:
				if(ev->iEventMsg == BTN_CLICKED)
				{
					Menu_GameSettings_GrabInfo();
					Menu_GameSettingsShutdown();

					return true;
				}
				break;

            // Set the default values
            case gs_Default:
                if( ev->iEventMsg == BTN_CLICKED ) {
                    Menu_GameSettings_Default();
                }
                break;

			case gs_ShowModifiedOnly: {
				CListview* features = (CListview*)cGameSettings.getWidget(gs_FeaturesList);
				features->SaveScrollbarPos();
				initFeaturesList(features);
				features->RestoreScrollbarPos();
				break;
			}
				
			case gs_AdvancedLevel:
				if( ev->iEventMsg == SLD_CHANGE ) {
					tLXOptions->iAdvancedLevelLimit = ((CSlider*)cGameSettings.getWidget(gs_AdvancedLevel))->getValue();
					CListview* features = (CListview*)cGameSettings.getWidget(gs_FeaturesList);
					features->SaveScrollbarPos();
					initFeaturesList(features);
					features->RestoreScrollbarPos();

					CLabel* featuresLabel = (CLabel*)cGameSettings.getWidget(gs_FeaturesListLabel);
					float warningCoeff = CLAMP((float)tLXOptions->iAdvancedLevelLimit / (__AdvancedLevelType_Count - 1), 0.0f, 1.0f);
					featuresLabel->ChangeColour( tLX->clNormalLabel * (1.0f - warningCoeff) + tLX->clError * warningCoeff );
					featuresLabel->setText( splitStringWithNewLine(AdvancedLevelDescription((AdvancedLevel)tLXOptions->iAdvancedLevelLimit), (size_t)-1, 450, tLX->cFont) );		
					
					CLabel* advancenessLabel = (CLabel*)cGameSettings.getWidget(gs_AdvancedLevelLabel);
					advancenessLabel->setText( AdvancedLevelShortDescription((AdvancedLevel)tLXOptions->iAdvancedLevelLimit) );
					advancenessLabel->ChangeColour( tLX->clNormalLabel * (1.0f - warningCoeff) + tLX->clError * warningCoeff );
				}
				break;
				
			case gs_FeaturesList:
				CListview* features = (CListview*)ev->cWidget;
				if( ev->iEventMsg == LV_WIDGETEVENT )
				{
					updateFeaturesList(features);
				}
				if( ev->iEventMsg == LV_MOUSEOVER )
				{
					CLabel* featuresLabel = (CLabel*)cGameSettings.getWidget(gs_FeaturesListLabel);
					featuresLabel->ChangeColour( tLX->clNormalLabel );
					featuresLabel->setText( "" );
					for(lv_item_t* item = features->getItems(); item != NULL; item = item->tNext)
						updateFeatureListItemColor(item);
					if(	features->getMouseOverSIndex() != "" )
					{
						{
							lv_item_t* item = features->getItem(features->getMouseOverSIndex());
							if(item && item->tSubitems)
								item->tSubitems->iColour = tLX->clMouseOver;
						}
						std::string desc;
						{
							int group = getListItemGroupInfoNr(features->getMouseOverSIndex());
							if(group >= 0)
								desc = GameInfoGroupDescriptions[group][1];
						}
						if(desc == "") {
							RegisteredVar* var = CScriptableVars::GetVar( features->getMouseOverSIndex() );
							if(var) {
								desc = var->longDesc;
								float warningCoeff = CLAMP((float)var->advancedLevel / (__AdvancedLevelType_Count - 1), 0.0f, 1.0f);
								featuresLabel->ChangeColour( tLX->clNormalLabel * (1.0f - warningCoeff) + tLX->clError * warningCoeff );
							}
						}
						featuresLabel->setText( splitStringWithNewLine(desc, (size_t)-1, 450, tLX->cFont) );
					}
				}
				if( ev->iEventMsg == LV_CHANGED )
				{
					for( int group = 0; group < GIG_Size; group++ )
						if( features->getMouseOverSIndex() == GameInfoGroupDescriptions[group][0] ) {
							tLXOptions->iGameInfoGroupsShown[group] = ! tLXOptions->iGameInfoGroupsShown[group];
							features->SaveScrollbarPos();
							initFeaturesList(features);
							features->RestoreScrollbarPos();
							break;
						}
				}
				break;
		}

	}

	DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, 120,150, 120,150, 400,300);
	cGameSettings.Draw(VideoPostProcessor::videoSurface().get());

	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());

	return false;
}


///////////////////
// Grab the game settings info
void Menu_GameSettings_GrabInfo()
{
	// Stub
}

	
///////////////////
// Set the default game settings info
void Menu_GameSettings_Default()
{
	CScriptableVars::const_iterator upper_bound = CScriptableVars::upper_bound("GameOptions.");
	for( CScriptableVars::const_iterator it = CScriptableVars::lower_bound("GameOptions."); it != upper_bound; it++ ) 
	{
		if( it->second.group == GIG_Invalid ) continue;

		if(!isSettingForGameSettingsDialog(it->second.var))
			continue;
		
		unsetGameSettingsVar(it->first, it->second.var);
    }

    CListview * features = (CListview *)cGameSettings.getWidget(gs_FeaturesList);
    features->Clear();
	initFeaturesList(features);
}

/*
=======================

  Weapons Restrictions

 For both local & host

=======================
*/


static CGuiLayout		cWeaponsRest;
static CWpnRest			cWpnRestList;
static std::vector<std::string>	cWeaponList;
static std::string		sModDirectory;

// Weapons Restrictions
enum {
	wr_Ok,
    wr_Scroll,
    wr_Reset,
	wr_ListBox,
	wr_Cancel,
    wr_Random,
	wr_Load,
	wr_Save
};


///////////////////
// Initialize the weapons restrictions
void Menu_WeaponsRestrictions(const std::string& szMod)
{
	sModDirectory = szMod;
	// Setup the buffer
	DrawImageAdv(tMenu->bmpBuffer.get(), tMenu->bmpMainBack_common, 120,150,120,150, 400,330);
	Menu_DrawBox(tMenu->bmpBuffer.get(), 120,150, 520,470);
	//DrawRectFillA(tMenu->bmpBuffer, 122,152, 518,438, 0, 100);


	Menu_RedrawMouse(true);

	cWeaponsRest.Initialize();
	cWeaponsRest.Add( new CLabel("Weapon Options", tLX->clNormalLabel),     -1,        275,155, 0, 0);
    cWeaponsRest.Add( new CButton(BUT_RESET, tMenu->bmpButtons),wr_Reset,  180,420, 60,15);
    cWeaponsRest.Add( new CButton(BUT_RANDOM, tMenu->bmpButtons),wr_Random,280,420, 80,15);
    cWeaponsRest.Add( new CButton(BUT_OK, tMenu->bmpButtons),	wr_Ok,     400,420, 30,15);
    cWeaponsRest.Add( new CButton(BUT_LOAD, tMenu->bmpButtons),	wr_Load,   250,445, 60,15);
    cWeaponsRest.Add( new CButton(BUT_SAVE, tMenu->bmpButtons),	wr_Save,   330,445, 60,15);
    cWeaponsRest.Add( new CScrollbar(),                         wr_Scroll, 490,185, 14,230);

	cWpnRestList.resetToEnabled();
	
    //
    // Update the list with the currently selected mod
    //

	cWeaponList = CGameScript::LoadWeaponList(sModDirectory);
	// Load the weapons
	cWpnRestList.loadList(gameSettings[FT_WeaponRest], sModDirectory);
	cWpnRestList.updateList( cWeaponList );
}

//////////////////
// Shutdown the weapon restrictions
void Menu_WeaponsRestrictionsShutdown()
{
	cWeaponsRest.Shutdown();

    cWpnRestList.saveList("cfg/wpnrest.dat");
    cWpnRestList.Shutdown();

	cWeaponList.clear();
}


///////////////////
// Weapons Restrictions frame
// Returns whether or not we have finished with the weapons restrictions
bool Menu_WeaponsRestrictions_Frame()
{
	gui_event_t *ev = NULL;
	mouse_t *Mouse = GetMouse();
    //Uint32 blue = MakeColour(0,138,251);

    // State strings
    static const std::string    szStates[] = {"Enabled", "Bonus", "Banned"};

	DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpBuffer, 120,150, 120,150, 400,300);

    // Draw the list
    int count = (int)cWeaponsRest.SendMessage(wr_Scroll, SCM_GETVALUE,(uintptr_t)0,0);

	int w, j;
	w = j = 0;
	for_each_iterator(wpnrest_t, it, cWpnRestList.getList()) {
		if( w++ < count )
            continue;
        if( j > 10 )
            break;


        int y = 190 + (j++)*20;
        Color Colour = tLX->clNormalLabel;
		Color StateColour = Colour;
		if( it->get().nState == wpr_bonus ) // Different color will make it more comfortable for eyes
			StateColour = tLX->clSubHeading;
		if( it->get().nState == wpr_banned )
			StateColour = tLX->clDisabled;
		int state = it->get().nState;

        // If a mouse is over the line, highlight it
        if( Mouse->X > 150 && Mouse->X < 450 ) {
            if( Mouse->Y > y && Mouse->Y < y+20 ) {
                Colour = tLX->clMouseOver;
				StateColour = tLX->clMouseOver;

                // If the mouse has been clicked, cycle through the states
                if( Mouse->Up & SDL_BUTTON(1) ) {
					state++;
					state %= 3;
					cWpnRestList.setWeaponState(it->get().szName, (WpnRestrictionState)state);
    
					gameSettings.overwrite[FT_WeaponRest] = "cfg/wpnrest.dat";
				}
            }
        }

		std::string buf = it->get().szName;
		stripdot(buf,245);
        tLX->cFont.Draw( VideoPostProcessor::videoSurface().get(), 150, y, Colour, buf );
		tLX->cFont.Draw( VideoPostProcessor::videoSurface().get(), 400, y, StateColour, szStates[state] );
	}

    // Adjust the scrollbar
    cWeaponsRest.SendMessage(wr_Scroll, SCM_SETITEMSPERBOX, 12, 0);
    cWeaponsRest.SendMessage(wr_Scroll, SCM_SETMIN, (uintptr_t)0, 0);
	if(cWpnRestList.getNumWeapons() > 10)
		cWeaponsRest.SendMessage(wr_Scroll, SCM_SETMAX, cWpnRestList.getNumWeapons() + 1, 0);
    else
        cWeaponsRest.SendMessage(wr_Scroll, SCM_SETMAX, (uintptr_t)0, 0);


	ev = cWeaponsRest.Process();
	cWeaponsRest.Draw(VideoPostProcessor::videoSurface().get());

	if(ev) {


		switch(ev->iControlID) {

			case gev_MouseWheel:
				if(ev->iEventMsg == gev_MouseWheelUp)  {
					CScrollbar *tScrollbar = (CScrollbar *)cWeaponsRest.getWidget(wr_Scroll);
					tScrollbar->MouseWheelUp(NULL);
				}
				
				if(ev->iEventMsg == gev_MouseWheelDown)  {
					CScrollbar *tScrollbar = (CScrollbar *)cWeaponsRest.getWidget(wr_Scroll);
					tScrollbar->MouseWheelDown(NULL);
				}
				break;

			// OK, done
			case wr_Ok:
				if(ev->iEventMsg == BTN_CLICKED) {

					Menu_WeaponsRestrictionsShutdown();

					return true;
				}
				break;

            // Reset the list
            case wr_Reset:
                if( ev->iEventMsg == BTN_CLICKED ) {
					gameSettings.overwrite[FT_WeaponRest] = "cfg/wpnrest.dat";
                    cWpnRestList.cycleVisible(cWeaponList);
                }
                break;

            // Randomize the list
            case wr_Random:
                if(ev->iEventMsg == BTN_CLICKED) {
					gameSettings.overwrite[FT_WeaponRest] = "cfg/wpnrest.dat";
                    cWpnRestList.randomizeVisible(cWeaponList);
                }
                break;

            // Open the load dialog
            case wr_Load:
                if(ev->iEventMsg == BTN_CLICKED) {
                    Menu_WeaponPresets(false,&cWpnRestList);
                }
                break;

            // Open the save dialog
            case wr_Save:
                if(ev->iEventMsg == BTN_CLICKED) {
                    Menu_WeaponPresets(true,&cWpnRestList);
                }
                break;
		}
	}

	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());

	return false;
}


///////////////////
// Weapon presets load/save
// For both local and host
enum  {
	wp_Cancel=0,
	wp_Ok,
	wp_PresetList,
	wp_PresetName
};

CGuiLayout cWpnPresets;

	struct WeaponPresetsAdder {
		CListview* listview;
		WeaponPresetsAdder(CListview* lv_) : listview(lv_) {}
		bool operator() (const std::string& f) {
			if(stringcaseequal(GetFileExtension(f),"wps")) {
				std::vector<std::string> fnparts = ListAsVector(SplitFilename(f, 3));
				if(fnparts.size() < 3) return true; // strange
				std::string fname = fnparts[1] + "/" + fnparts[2];
				if(stringcaseequal(fnparts[0], "cfg") && stringcaseequal(fnparts[1], "presets"))
					fname = fnparts[2];
				std::string name = GetBaseFilenameWithoutExt(fname);
				if(!listview->getItem(fname)) {
					listview->AddItem(fname,0,tLX->clListView);
					listview->AddSubitem(LVS_TEXT, name, (DynDrawIntf*)NULL, NULL);
				}
			}
			return true;
		}
	};

void Menu_WeaponPresets(bool save, CWpnRest *wpnrest)
{
	if (!wpnrest)
		return;

	gui_event_t *ev = NULL;
	int quitloop = false;
	CTextbox *t;

	// Save the background
	DrawImageAdv(tMenu->bmpBuffer.get(), VideoPostProcessor::videoSurface(), 0,0, 0,0, 640,480);

	Menu_RedrawMouse(true);

	cWpnPresets.Initialize();

	cWpnPresets.Add( new CButton(BUT_CANCEL, tMenu->bmpButtons), wp_Cancel, 180,310, 75,15);
	cWpnPresets.Add( new CButton(BUT_OK, tMenu->bmpButtons),     wp_Ok, 430,310, 40,15);
	cWpnPresets.Add( new CListview(),                            wp_PresetList, 180,170, 280,110+(!save)*20);
	cWpnPresets.Add( new CTextbox(),                             wp_PresetName, 270,285, 190,tLX->cFont.GetHeight());

	cWpnPresets.SendMessage(wp_PresetList,LVM_SETOLDSTYLE,(uintptr_t)0,0);

	t = (CTextbox *)cWpnPresets.getWidget(wp_PresetName);

	// Hide the textbox for Load
	t->setEnabled(save);

	// Load the level list

	CListview *lv = (CListview *)cWpnPresets.getWidget(wp_PresetList);
	lv->AddColumn("Weapon presets",60);

	WeaponPresetsAdder adder(lv);
	FindFiles(adder, "cfg/presets/" + sModDirectory, false, FM_REG);
	FindFiles(adder, "cfg/presets", false, FM_REG);
	
	lv->SortBy( 0, true );
	
	if((std::string)gameSettings[FT_WeaponRest] != "cfg/wpnrest.dat") {
		std::string fn = gameSettings[FT_WeaponRest];
		if(fn.find(".wps") == std::string::npos )
			fn += ".wps";
		
		lv_item_t* it = lv->getItem(sModDirectory + "/" + fn);
		if(!it) it = lv->getItem(fn);;
		if(it) {
			lv->setSelectedID(it->_iID);
			lv->setFocused(true); // just looks nicer			
		}
	}
	
	ProcessEvents();
	while(!WasKeyboardEventHappening(SDLK_ESCAPE,false) && !quitloop && game.state != Game::S_Quit) {
		Menu_RedrawMouse(true);

		//DrawImageAdv(VideoPostProcessor::videoSurface(),tMenu->bmpBuffer, 170,150, 170,150, 300, 180);
		Menu_DrawBox(VideoPostProcessor::videoSurface().get(), 170, 150, 470, 330);
		DrawImageAdv(VideoPostProcessor::videoSurface().get(), tMenu->bmpMainBack_common, 172,152, 172,152, 297,177);
		DrawImageAdv(tMenu->bmpBuffer.get(), tMenu->bmpMainBack_common, 172,152, 172,152, 297,177);

		tLX->cFont.DrawCentre(VideoPostProcessor::videoSurface().get(), 320, 155, tLX->clNormalLabel, save ? "Save" : "Load");
		if (save)
			tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 180,288,tLX->clNormalLabel,"Preset name");

		ev = cWpnPresets.Process();
		cWpnPresets.Draw(VideoPostProcessor::videoSurface().get());

		// Process the widgets
		if(ev)  {

			switch(ev->iControlID) {
				// Cancel
				case wp_Cancel:
					if(ev->iEventMsg == BTN_CLICKED) {
						PlaySoundSample(sfxGeneral.smpClick);
						quitloop = true;
					}
					break;

				// Presets list
				case wp_PresetList:
					if(ev->iEventMsg != LV_NONE) {
						if(save) t->setText( GetBaseFilenameWithoutExt(lv->getCurSIndex()) );
						else t->setText( lv->getCurSIndex() );
					}
				break;
			}

			// OK and double click on listview
			if (ev->iControlID == wp_Ok || ev->iControlID == wp_PresetList)  {
				if((ev->iEventMsg == BTN_CLICKED && ev->iControlID == 1) || ev->iEventMsg == LV_DOUBLECLK || ev->iEventMsg == LV_ENTER) {

					// Play the sound only for OK button
					if (ev->iControlID == wp_Ok)
						PlaySoundSample(sfxGeneral.smpClick);

					// Don't process when nothing is selected
					if(t->getText().length() > 0) {

						quitloop = true;
						if(save) {

							// Save
							std::string fn = "cfg/presets/" + sModDirectory + "/" + t->getText(); // + ".wps";
							if(fn.find(".wps") == std::string::npos )
								fn += ".wps";

							// Check if it exists already. If so, ask user if they wanna overwrite
							if(Menu_WeaponPresetsOkSave(fn)) {
								wpnrest->saveList(fn);
								gameSettings.overwrite[FT_WeaponRest] = GetBaseFilename(fn);
							} else
								quitloop = false;
						} else {

							// Load
							std::string fn = "cfg/presets/" + t->getText();
							wpnrest->loadList(fn, "");
							wpnrest->updateList( cWeaponList );
							gameSettings.overwrite[FT_WeaponRest] = GetBaseFilename(t->getText());
						}
					}
				}
			}
		}

		// Draw mouse
		DrawCursor(VideoPostProcessor::videoSurface().get());

		// Display the dialog
		doVideoFrameInMainThread();

		CapFPS();
		ProcessEvents();
	}

	// Redraw back to normal
	DrawImageAdv(tMenu->bmpBuffer.get(), tMenu->bmpMainBack_common, 120,150,122,152, 396,316);
	DrawImage(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer,0,0);

	Menu_RedrawMouse(true);

	Menu_WeaponPresetsShutdown();
}

/////////////
// Shutdown
void Menu_WeaponPresetsShutdown()
{
	cWpnPresets.Shutdown();
}


///////////////////
// Check if there is a possible overwrite
bool Menu_WeaponPresetsOkSave(const std::string& szFilename)
{
	std::string filename = szFilename;

	// Adjust the filename
	if( stringcasecmp(GetFileExtension( szFilename ), "wps") != 0)
		filename += ".wps";

	FILE *fp = OpenGameFile(filename,"rb");
	if( fp == NULL)
		// File doesn't exist, ok to save
		return true;

	fclose(fp);


	// The file already exists, show a message box to confirm the overwrite
	int nResult = Menu_MessageBox("Confirmation","The preset already exists. Overwrite?", LMB_YESNO);
	if( nResult == MBR_YES )
		return true;


	// No overwrite
	return false;
}

}; // namespace DeprecatedGUI
