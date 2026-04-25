/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Map editor
// Created 28/7/02
// Jason Boettcher


#include "CodeAttributes.h"
#include "LieroX.h"
#include "AuxLib.h"
#include "DeprecatedGUI/Graphics.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CTextbox.h"
#include "sound/SoundsBase.h"
#include "EndianSwap.h"
#include "FileUtils.h"
#include "game/CMap.h"


namespace DeprecatedGUI {

static CGuiLayout	cMaped;

// Map editor controls
enum {
	map_new,
	map_random,
	map_load,
	map_save,
	map_quit
};

static CMap*		cMap = NULL;
static CViewport*	cMapedView = NULL;
static int			grabbed = false;
static int			grabX,  grabY;
static int			grabWX, grabWY;

///////////////////
// Initialize the map editor
bool Menu_MapEdInitialize()
{
	tMenu->iMenuType = MNU_MAPED;

	// Create the buffer
	/*DrawImage(tMenu->bmpBuffer,tMenu->bmpMainBack,0,0);
	Menu_DrawSubTitle(tMenu->bmpBuffer,SUB_MAPED);

	Menu_DrawBox(tMenu->bmpBuffer,20,171, 619,458);
	Menu_DrawBox(tMenu->bmpBuffer,20,135, 54,169);*/

    DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
    Menu_DrawSubTitleAdv(tMenu->bmpBuffer.get(),SUB_MAPED,18);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,100, 625, 465);

    Menu_DrawBox(tMenu->bmpBuffer.get(),20,105, 54,139);      // Preview box
    Menu_DrawBox(tMenu->bmpBuffer.get(),20,146, 619,459);     // Level box



	Menu_RedrawMouse(true);

	// Create a map
	if(cMap) delete cMap;
	cMap = new CMap();
	cMap->New(504,350,"dirt");
	if(cMapedView) delete cMapedView;
	cMapedView = new CViewport();
	cMapedView->Setup(22,148, 596,310,0);
	cMapedView->SetWorldX(0);
	cMapedView->SetWorldY(0);

	tMenu->iEditMode = 0;
	tMenu->iCurHole = 0;
	tMenu->iCurStone = 0;
	tMenu->iCurMisc = 0;
	tMenu->iCurDirt = 0;

	cMaped.Shutdown();
	cMaped.Initialize();

	grabbed = false;

	// Add the controls
	cMaped.Add( new CButton(BUT_NEW, tMenu->bmpButtons),	map_new,	 320,110, 50,15);
	//cMaped.Add( new CButton(BUT_RANDOM, tMenu->bmpButtons), map_random,  300,110, 80,15);
	cMaped.Add( new CButton(BUT_LOAD, tMenu->bmpButtons),   map_load,	 400,110, 50,15);
	cMaped.Add( new CButton(BUT_SAVE, tMenu->bmpButtons),   map_save,	 480,110, 50,15);
	cMaped.Add( new CButton(BUT_QUIT, tMenu->bmpButtons),   map_quit,	 550,110, 50,15);


	return true;
}

/////////////
// Shutdown
void Menu_MapEdShutdown()
{
	cMaped.Shutdown();
	if(cMap) {
		cMap->Shutdown();
		delete cMap;
		cMap = NULL;
	}
	if(cMapedView) {
		delete cMapedView;
		cMapedView = NULL;
	}
}


///////////////////
// Map editor frame
void Menu_MapEdFrame(SDL_Surface * bmpDest, int process)
{
	gui_event_t *ev = NULL;
	mouse_t *Mouse = GetMouse();
	int x,y,n,i;

	// Re-draw the buffer over buttons
	//DrawImageAdv(bmpDest, tMenu->bmpBuffer, 230,140, 230,140, 410,50);

	ev = cMaped.Process();
	cMaped.Draw(bmpDest);


	// Draw the map
	cMap->Draw(bmpDest, cMapedView);


	// Toolbar
	int down = -1;
	if(Mouse->Down || Mouse->Up) {
		if(Mouse->Y > 108 && Mouse->Y < 137) {

			for(x=58,n=0;x<155;x+=32,n++) {
				if(Mouse->X > x && Mouse->X < x+29)
					down = n;
			}
		}
		if(Mouse->Up && down >= 0) {
			PlaySoundSample(sfxGeneral.smpClick);

			tMenu->iEditMode = down;
		}
	}

	for(x=0,n=58,i=0;x<116;x+=29,n+=32,i++) {
		y = tMenu->iEditMode == i || down == i ? 29 : 0;
		DrawImageAdv(bmpDest, tMenu->bmpMapEdTool, x,y, n, 108, 29,29);
	}

	// Item clicked on?
	int Clicked = false;
	if(Mouse->Up) {
		if(Mouse->X > 20 && Mouse->X < 54)
			if(Mouse->Y > 105 && Mouse->Y < 139)
				Clicked = Mouse->Up;
	}


	DrawImageAdv(bmpDest, tMenu->bmpBuffer, 20,105, 20,105, 34,34);

	//
	// Draw item
	//
	theme_t *t = cMap->GetTheme();

	// Holes
	if(tMenu->iEditMode == 0) {

		// left mouse button
		if(Clicked & SDL_BUTTON(1)) {
			tMenu->iCurHole++;
			if(tMenu->iCurHole >= 5)
				tMenu->iCurHole = 0;
		}

		// Right mouse button
		if(Clicked & SDL_BUTTON(3)) {
			tMenu->iCurHole--;
			if(tMenu->iCurHole < 0)
				tMenu->iCurHole = 4;
		}


		int w = t->bmpHoles[ tMenu->iCurHole ].get()->w;
		int h = t->bmpHoles[ tMenu->iCurHole ].get()->h;

		DrawImageStretch(bmpDest, t->bmpHoles[ tMenu->iCurHole ], 37-w, 122-h);
	}

	// Stones
	if(tMenu->iEditMode == 1) {

		// lmb
		if(Clicked & SDL_BUTTON(1)) {
			tMenu->iCurStone++;
			if(tMenu->iCurStone >= t->NumStones)
				tMenu->iCurStone = 0;
		}

		// rmb
		if(Clicked & SDL_BUTTON(3)) {
			tMenu->iCurStone--;
			if(tMenu->iCurStone < 0)
				tMenu->iCurStone = t->NumStones-1;
		}


		int w = t->bmpStones[ tMenu->iCurStone ].get()->w;
		int h = t->bmpStones[ tMenu->iCurStone ].get()->h;

		if(w > 17 || h > 17) {
			w >>= 1;
			h >>= 1;
			DrawImage(bmpDest,t->bmpStones[ tMenu->iCurStone ], 37-w, 122-h);
			DrawRect(bmpDest,22,107, 52,137, tLX->clWhite);
		} else
			DrawImageStretchKey(bmpDest, t->bmpStones[ tMenu->iCurStone ], 37-w, 122-h);
	}


	// Miscellanous
	if(tMenu->iEditMode == 2) {

		// lmb
		if(Clicked & SDL_BUTTON(1)) {
			tMenu->iCurMisc++;
			if(tMenu->iCurMisc >= t->NumMisc)
				tMenu->iCurMisc = 0;
		}

		// rmb
		if(Clicked & SDL_BUTTON(3)) {
			tMenu->iCurMisc--;
			if(tMenu->iCurMisc < 0)
				tMenu->iCurMisc = t->NumMisc-1;
		}


		int w = t->bmpMisc[ tMenu->iCurMisc ].get()->w;
		int h = t->bmpMisc[ tMenu->iCurMisc ].get()->h;

		DrawImageStretchKey(bmpDest, t->bmpMisc[ tMenu->iCurMisc ], 37-w, 122-h);
	}

	// Dirt
	if(tMenu->iEditMode == 3) {

		// lmb
		if(Clicked & SDL_BUTTON(1)) {
			tMenu->iCurDirt++;
			if(tMenu->iCurDirt >= 5)
				tMenu->iCurDirt = 0;
		}

		// rmb
		if(Clicked & SDL_BUTTON(3)) {
			tMenu->iCurDirt--;
			if(tMenu->iCurDirt < 0)
				tMenu->iCurDirt = 4;
		}

		int w = t->bmpHoles[ tMenu->iCurDirt ].get()->w;
		int h = t->bmpHoles[ tMenu->iCurDirt ].get()->h;

		DrawImageStretch(bmpDest, t->bmpHoles[ tMenu->iCurDirt ], 37-w, 122-h);
	}

	if(!process)
		return;


	//
	// Place the objects on the map
	//
	SmartPointer<SDL_Surface> MouseImg = NULL;
	if(Mouse->X >= 22 && Mouse->X <= 618) {
		if(Mouse->Y >= 148 && Mouse->Y <= 457) {

			x = (Mouse->X - 22)/2 + cMapedView->GetWorldX();
			y = (Mouse->Y - 148)/2 + cMapedView->GetWorldY();
			CVec pos = CVec((float)x, (float)y);


			switch(tMenu->iEditMode) {

				// Hole
				case 0:
					MouseImg = t->bmpHoles[tMenu->iCurHole];
					if(Mouse->Down & SDL_BUTTON(1))
						cMap->CarveHole(tMenu->iCurHole,pos,false);
					break;

				// Stone
				case 1:
					MouseImg = t->bmpStones[tMenu->iCurStone];
					if(Mouse->Up & SDL_BUTTON(1))
						cMap->PlaceStone(tMenu->iCurStone,pos);
					break;

				// Misc
				case 2:
					MouseImg = t->bmpMisc[tMenu->iCurMisc];
					if(Mouse->Up & SDL_BUTTON(1))
						cMap->PlaceMisc(tMenu->iCurMisc,pos);
					break;

				// Dirt
				case 3:
					MouseImg = t->bmpHoles[tMenu->iCurDirt];
					if(Mouse->Down & SDL_BUTTON(1))
						cMap->PlaceDirt(tMenu->iCurDirt,pos);
					break;

				// Delete object
				/*case 4:
					if(Mouse->Up & SDL_BUTTON(1))
						cMap->DeleteObject(pos);
					break;*/
			}
		}
	}



	//
	// Process the gui
	//

	if(ev) {

		if(ev->cWidget->getType() == wid_Button) {
			if(ev->iEventMsg == BTN_CLICKED)
				PlaySoundSample(sfxGeneral.smpClick);
		}

		switch(ev->iControlID) {

			// New
			case map_new:
				if(ev->iEventMsg == BTN_CLICKED) {
					Menu_MapEd_New();
				}
				break;

			// Random
			case map_random:
				//if(ev->iEventMsg == BTN_CLICKED)
					//cMap->ApplyRandom();
				break;

			// Load
			case map_load:
				if(ev->iEventMsg == BTN_CLICKED) {
					Menu_MapEd_LoadSave(false);
				}
				break;

			// Save
			case map_save:
				if(ev->iEventMsg == BTN_CLICKED) {
					Menu_MapEd_LoadSave(true);
				}
				break;

			// Quit
			case map_quit:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Shutdown the classes
					Menu_MapEdShutdown();

					Menu_MainInitialize();
					return;
				}
				break;
		}
	}


	// Keyboard arrows
	int Scroll = 250 * (int)tLX->fDeltaTime.seconds();
	// TODO: make this event-based (don't check GetKeyboard() directly)
	if(WasKeyboardEventHappening(SDLK_UP))
		cMapedView->SetWorldY( cMapedView->GetWorldY() - Scroll );
	if(WasKeyboardEventHappening(SDLK_DOWN))
		cMapedView->SetWorldY( cMapedView->GetWorldY() + Scroll );
	if(WasKeyboardEventHappening(SDLK_LEFT))
		cMapedView->SetWorldX( cMapedView->GetWorldX() - Scroll );
	if(WasKeyboardEventHappening(SDLK_RIGHT))
		cMapedView->SetWorldX( cMapedView->GetWorldX() + Scroll );

    // Grab the level & drag it
	if(Mouse->Down & SDL_BUTTON(2)) {
		if(Mouse->X >= 22 && Mouse->X <= 618) {
			if(Mouse->Y >= 173 && Mouse->Y <= 457) {
				// Mouse button grabs and moves the map
				x = (Mouse->X - 22)/2;
				y = (Mouse->Y - 173)/2;

				// First grab
				if(!grabbed) {
					grabbed = true;
					grabX = x;
					grabY = y;
					grabWX = cMapedView->GetWorldX();
					grabWY = cMapedView->GetWorldY();
				} else {

					cMapedView->SetWorldX( grabWX + (grabX-x));
					cMapedView->SetWorldY( grabWY + (grabY-y));
				}
			}
		}
	} else
		grabbed = false;

    // When the mouse is on the edges of the screen, move the level
    int EdgeSize = 7;
    if( !Mouse->Down && !Mouse->Up ) {

        // X Axis
        if( Mouse->X < EdgeSize )
            cMapedView->SetWorldX( cMapedView->GetWorldX() - Scroll );
        if( Mouse->X > 640-EdgeSize )
            cMapedView->SetWorldX( cMapedView->GetWorldX() + Scroll );

        // Y Axis
        if( Mouse->Y < EdgeSize )
            cMapedView->SetWorldY( cMapedView->GetWorldY() - Scroll );
        if( Mouse->Y > 480-EdgeSize )
            cMapedView->SetWorldY( cMapedView->GetWorldY() + Scroll );
    }

	// Clamp the viewport
	cMapedView->Clamp(cMap->GetWidth(), cMap->GetHeight());




	// Draw the mouse
	if(MouseImg.get()) {
		int w = MouseImg.get()->w;
		int h = MouseImg.get()->h;
		//if(tMenu->iEditMode == 0 || tMenu->iEditMode == 3)
		DrawImageStretchKey(VideoPostProcessor::videoSurface().get(),MouseImg, Mouse->X-w, Mouse->Y-h);
		//else
		//	DrawImageStretchKey(VideoPostProcessor::videoSurface(),MouseImg, Mouse->X-w, Mouse->Y-h, tLX->clPink);
	}
	else
		DrawCursor(VideoPostProcessor::videoSurface().get());
}


///////////////////
// Show a 'new' dialog box
enum {
	mn_Cancel=0,
	mn_Ok,
	mn_Width,
	mn_Height,
	mn_Scheme
};



	class ComboboxFiller { public:
		CGuiLayout* gui;
		int comboindex;
		int i;
		int* dirtindex;
		ComboboxFiller(CGuiLayout* g, int c, int* d) : gui(g), comboindex(c), i(0), dirtindex(d) {}
		INLINE bool operator() (const std::string& dir) {
			if (!IsFileAvailable(dir+"/theme.txt"))
				return true;
			size_t p = findLastPathSep(dir);
			std::string f = dir.substr(p+1);
			if (((CCombobox *)gui->getWidget(comboindex))->getItem(f).get() != NULL)
				return true;

			gui->SendMessage(comboindex,CBS_ADDITEM,f,i);
			if(stringcasecmp(f,"dirt"))
				*dirtindex = i;
			i++;

			return true;
		}
	};

void Menu_MapEd_New()
{
	gui_event_t *ev = NULL;
	int quitloop = false;
	CTextbox *t1,*t2;

	// Save the background
	Menu_MapEdFrame(tMenu->bmpBuffer.get(),false);

	Menu_DrawBox(tMenu->bmpBuffer.get(), 210, 170, 430, 310);
	DrawImageAdv(tMenu->bmpBuffer.get(), tMenu->bmpMainBack_common, 212,172, 212,172, 217,137);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,100, 625, 465);
	//DrawRectFill(tMenu->bmpBuffer, 212, 172, 429, 309, tLX->clBlack);

	Menu_RedrawMouse(true);

	CGuiLayout cg;

	cg.Initialize();

	cg.Add( new CButton(BUT_CANCEL, tMenu->bmpButtons), mn_Cancel, 220,285, 75,15);
	cg.Add( new CButton(BUT_OK, tMenu->bmpButtons),     mn_Ok, 390,285, 40,15);
	cg.Add( new CTextbox(),                             mn_Width, 280,200, 100,tLX->cFont.GetHeight());
	cg.Add( new CTextbox(),                             mn_Height, 280,230, 100,tLX->cFont.GetHeight());
	cg.Add( new CCombobox(),							mn_Scheme, 280,260, 120,17);

	cg.SendMessage(mn_Width,TXM_SETMAX,64,0);
	cg.SendMessage(mn_Height,TXM_SETMAX,64,0);

	int dirtindex = -1;

	// Find directories in the theme dir
	ComboboxFiller filler(&cg, 4, &dirtindex);
	FindFiles(filler, "data/themes", false, FM_DIR);

	if(dirtindex != -1)
		cg.SendMessage(4,CBM_SETCURSEL,dirtindex,0);



	// Set initial values
	t1 = (CTextbox *)cg.getWidget(mn_Width);
	t2 = (CTextbox *)cg.getWidget(mn_Height);

	t1->setText(itoa(cMap->GetWidth(),10));
	t2->setText(itoa(cMap->GetHeight(),10));

	DrawImage(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer, 0,0);
	doVideoFrameInMainThread();
	DrawImage(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer, 0,0);

	ProcessEvents();
	while(!WasKeyboardEventHappening(SDLK_ESCAPE,false) && !quitloop) {
		Menu_RedrawMouse(false);

		DrawImageAdv(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer, 210,170, 210,170, 222, 262);

		tLX->cFont.DrawCentre(VideoPostProcessor::videoSurface().get(), 320, 175, tLX->clNormalLabel, "Level details");
		tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 220, 202, tLX->clNormalLabel, "Width");
		tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 220, 232, tLX->clNormalLabel, "Height");
		tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 220, 262, tLX->clNormalLabel, "Theme");

		ev = cg.Process();
		cg.Draw(VideoPostProcessor::videoSurface().get());

		// Process the widgets
		if(ev) {

			switch(ev->iControlID) {

				// Cancel
				case mn_Cancel:
					if(ev->iEventMsg == BTN_CLICKED) {
						PlaySoundSample(sfxGeneral.smpClick);
						quitloop = true;
					}
					break;

				// OK
				case mn_Ok:
					if(ev->iEventMsg == BTN_CLICKED) {
						PlaySoundSample(sfxGeneral.smpClick);
						int w = from_string<int>(t1->getText());
						int h = from_string<int>(t2->getText());
						std::string theme;
						theme = "dirt";
						GuiListItem::Pt it = ((CCombobox*)cg.getWidget(4))->getSelectedItem();
						if(it.get())
							theme = it->caption();


						// Check for min & max sizes
						if(w >= 350 && h >= 250 &&
							w <= 4000 && h <= 4000) {
								quitloop = true;
								cMap->New(w,h,theme);
						}
					}
					break;
			}

		}

		// Game close
		if (game.state == Game::S_Quit)
			break;


		DrawCursor(VideoPostProcessor::videoSurface().get());
		doVideoFrameInMainThread();
		CapFPS();
		ProcessEvents();
	}

	// Redraw back to normal
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
    Menu_DrawSubTitleAdv(tMenu->bmpBuffer.get(),SUB_MAPED,18);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,100, 625, 465);

    Menu_DrawBox(tMenu->bmpBuffer.get(),20,105, 54,139);      // Preview box
    Menu_DrawBox(tMenu->bmpBuffer.get(),20,146, 619,459);     // Level box

	Menu_RedrawMouse(true);

	cg.Shutdown();
}


///////////////////
// File save/load dialog
enum  {
	sl_Cancel=0,
	sl_Ok,
	sl_FileList,
	sl_FileName
};



	struct LevelListFiller {
		CListview* lv;
		LevelListFiller(CListview* l) : lv(l) {}
		bool operator() (const std::string& filename) {
			std::string mapName = CMap::GetLevelName(filename, true);
			if(mapName != "") {
				std::string baseFile = GetBaseFilename(filename);
				
				if(!lv->getItem(baseFile)) {
					lv->AddItem(baseFile, 0, tLX->clListView);
					lv->AddSubitem(LVS_TEXT, mapName, (DynDrawIntf*)NULL, NULL);
				}
			}
			
			return true;
		}
	};

void Menu_MapEd_LoadSave(int save)
{
	gui_event_t *ev = NULL;
	int quitloop = false;
	CTextbox *t;

	// Save the background
	Menu_MapEdFrame(tMenu->bmpBuffer.get(),false);

	Menu_DrawBox(tMenu->bmpBuffer.get(), 170, 150, 470, 330);
	DrawImageAdv(tMenu->bmpBuffer.get(), tMenu->bmpMainBack_common, 172,152, 172,152, 297,177);

	Menu_RedrawMouse(true);

	CGuiLayout cg;

	cg.Initialize();

	cg.Add( new CButton(BUT_CANCEL, tMenu->bmpButtons), 0, 180,310, 75,15);
	cg.Add( new CButton(BUT_OK, tMenu->bmpButtons),     1, 430,310, 40,15);
	cg.Add( new CListview(),                            2, 180,170, 280,110);
	cg.Add( new CTextbox(),                             3, 260,285, 200,tLX->cFont.GetHeight());

	cg.SendMessage(2,		LVM_SETOLDSTYLE, (uintptr_t)0, 0);

	t = (CTextbox *)cg.getWidget(3);

	// Load the level list
	CListview *lv = (CListview *)cg.getWidget(2);
	lv->AddColumn("Levels",60);

	LevelListFiller filler(lv);
	FindFiles(filler, "levels", false, FM_REG);
	lv->SortBy( 0, true );

	DrawImage(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer, 0,0);
	doVideoFrameInMainThread();
	DrawImage(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer, 0,0);

	ProcessEvents();
	while(!WasKeyboardEventHappening(SDLK_ESCAPE,false) && !quitloop && game.state != Game::S_Quit) {
		Menu_RedrawMouse(false);

		DrawImageAdv(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer, 170,150, 170,150, 302, 182);

		tLX->cFont.DrawCentre(VideoPostProcessor::videoSurface().get(), 320, 155, tLX->clNormalLabel, save ? "Save" : "Load");
		tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), 180,288,tLX->clNormalLabel, "Level name");

		ev = cg.Process();
		cg.Draw(VideoPostProcessor::videoSurface().get());

		// Process the widgets
		if(ev) {

			switch(ev->iControlID) {

				// Cancel
				case sl_Cancel:
					if(ev->iEventMsg == BTN_CLICKED) {
						PlaySoundSample(sfxGeneral.smpClick);
						quitloop = true;
					}
					break;

				// OK
				case sl_Ok:
					if(ev->iEventMsg == BTN_CLICKED) {
						PlaySoundSample(sfxGeneral.smpClick);

						if(t->getText().length() > 0) {

							quitloop = true;
							std::string buf;
							if(save) {

								// Save
								if(stringtolower(GetFileExtension(t->getText())) != "lxl")
									buf = "levels/" + t->getText() + ".lxl";
								else
									buf = "levels/" + t->getText();

								// Check if it exists already. If so, ask user if they wanna overwrite
								if(Menu_MapEd_OkSave(buf))
									cMap->Save(t->getText(),buf);
								else
									quitloop = false;
							} else {

								// Load
								buf = "levels/" + t->getText();
								cMap->Load(buf);
							}
						}
					}
					break;

				// Level list
				case sl_FileList:
					if(ev->iEventMsg != LV_NONE) {
						t->setText( lv->getCurSIndex() );
					}
					break;
			}
		}


		DrawCursor(VideoPostProcessor::videoSurface().get());
		doVideoFrameInMainThread();
		CapFPS();
		ProcessEvents();
	}

	// Redraw back to normal
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_common,0,0);
    Menu_DrawSubTitleAdv(tMenu->bmpBuffer.get(),SUB_MAPED,18);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,100, 625, 465);

    Menu_DrawBox(tMenu->bmpBuffer.get(),20,105, 54,139);      // Preview box
    Menu_DrawBox(tMenu->bmpBuffer.get(),20,146, 619,459);     // Level box

	Menu_RedrawMouse(true);

	cg.Shutdown();
}


///////////////////
// Check if there is a possible overwrite
bool Menu_MapEd_OkSave(const std::string& szFilename)
{
	std::string filename = szFilename;

	// Adjust the filename
	if( stringcasecmp(GetFileExtension( szFilename ), "lxl") != 0)
		filename += ".lxl";

	FILE *fp = OpenGameFile(filename,"rb");
	if( fp == NULL)
		// File doesn't exist, ok to save
		return true;

	fclose(fp);


	// The file already exists, show a message box to confirm the overwrite
	int nResult = Menu_MessageBox("Confirmation","The level already exists. Overwrite?", LMB_YESNO);
	if( nResult == MBR_YES )
		return true;


	// No overwrite
	// Fix the screen up
	Menu_MapEdFrame(tMenu->bmpBuffer.get(),false);
	Menu_DrawBox(tMenu->bmpBuffer.get(), 170, 150, 470, 330);
	DrawImageAdv(tMenu->bmpBuffer.get(), tMenu->bmpMainBack_common, 172,152, 172,152, 297,177);
	Menu_RedrawMouse(true);

	return false;
}

}; // namespace DeprecatedGUI
