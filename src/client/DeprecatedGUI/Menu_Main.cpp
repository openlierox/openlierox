/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Main menu
// Created 30/6/02
// Jason Boettcher


#include <assert.h>

#include "LieroX.h"
#include "AuxLib.h"
#include "FindFile.h"
#include "DeprecatedGUI/Graphics.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/CTitleButton.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CCombobox.h"
#include "DeprecatedGUI/CCheckbox.h"
#include "DeprecatedGUI/CBrowser.h"
#include "DeprecatedGUI/CTextButton.h"
#include "sound/SoundsBase.h"
#include "game/ServerList.h"


namespace DeprecatedGUI {

CGuiLayout	cMainMenu;
float		alpha = 0;
int			lastimg = -1;

enum {
	mm_LocalPlay=0,
	mm_NetPlay,
	mm_PlayerProfiles,
	mm_LevelEditor,
	mm_Options,
	mm_Quit,
	mm_LXALink,
	mm_Theme,
};

static void Menu_Main_GuiThemeComboboxCreate();

///////////////////
// Initialize the main menu
void Menu_MainInitialize()
{
	int i;
	assert(tMenu);
	tMenu->iMenuType = MNU_MAIN;

	if(bDedicated) return;
	
	// Create the buffer
	assert(tMenu->bmpBuffer.get());
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_wob,0,0);
	DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpLieroXtreme, 320 - tMenu->bmpLieroXtreme.get()->w/2, 10);
	if (tMenu->tFrontendInfo.bPageBoxes)
		Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);

	Menu_RedrawMouse(true);

	alpha = 0;
	lastimg = -1;

	// Menu buttons
	int titleheight = tMenu->bmpMainTitles.get()->h/((mm_Quit-mm_LocalPlay)*2);
	for(i=mm_LocalPlay;i<mm_Quit;i++)
		cMainMenu.Add( new CTitleButton(i, tMenu->bmpMainTitles), i, tMenu->tFrontendInfo.iMainTitlesLeft, tMenu->tFrontendInfo.iMainTitlesTop+i*(titleheight+tMenu->tFrontendInfo.iMainTitlesSpacing), tMenu->bmpMainTitles.get()->w, titleheight);

	// Quit
	cMainMenu.Add( new CButton(BUT_QUIT, tMenu->bmpButtons), mm_Quit, 25,440, 50,15);

	// selection of different themes (like default, LX56, ...)
	Menu_Main_GuiThemeComboboxCreate();

	const char * LXALinkText = "Visit our website https://openlierox.net";
	int orig_spacing = tLX->cFont.GetVSpacing();
	tLX->cFont.SetVSpacing(tMenu->tFrontendInfo.iCreditsSpacing);
	cMainMenu.Add( new CTextButton(LXALinkText, tLX->clCredits2, tLX->clCredits1 ), 
						mm_LXALink, tMenu->tFrontendInfo.iCreditsLeft - tLX->cFont.GetWidth(LXALinkText) - 30, 
						tMenu->tFrontendInfo.iCreditsTop + tLX->cFont.GetHeight() * 6, 250, 15 );
	tLX->cFont.SetVSpacing(orig_spacing);
}


///////////////////
// Main menu frame
void Menu_MainFrame()
{
	gui_event_t *ev = NULL;

	//DrawImageAdv(VideoPostProcessor::videoSurface(), tMenu->bmpBuffer, 50,160, 50,160, 320,290);
	//DrawImageAdv(VideoPostProcessor::videoSurface(), tMenu->bmpBuffer, 20,430, 20,430, 60,40);

	// Process the buttons
	ev = cMainMenu.Process();
		
	cMainMenu.Draw(VideoPostProcessor::videoSurface().get());
	
	int mouseover = false;
	int img = lastimg;

	if(ev) {

		switch(ev->iControlID) {

			// local
			case mm_LocalPlay:
                mouseover = true;
                img=0;
                if( ev->iEventMsg == TBT_CLICKED ) {
					PlaySoundSample(sfxGeneral.smpClick);
				    Menu_MainShutdown();
				    Menu_LocalInitialize();
				    return;
                }
				break;

			// Network
			case mm_NetPlay:
                mouseover = true;
                img=1;
                if( ev->iEventMsg == TBT_CLICKED ) {
					PlaySoundSample(sfxGeneral.smpClick);
				    Menu_MainShutdown();
				    // Refresh the master server lists in the background so the
				    // server list shown in the Internet menu is up to date.
				    DownloadMasterServerListFiles();
				    Menu_NetInitialize();
				    return;
                }
				break;

			// Player
			case mm_PlayerProfiles:
                if( ev->iEventMsg == TBT_CLICKED ) {
					PlaySoundSample(sfxGeneral.smpClick);
				    Menu_MainShutdown();
				    Menu_PlayerInitialize();
				    return;
                }
				break;

			// Level editor
			case mm_LevelEditor:
                if( ev->iEventMsg == TBT_CLICKED ) {
                    PlaySoundSample(sfxGeneral.smpClick);
				    Menu_MainShutdown();
				    Menu_MapEdInitialize();
				    return;
                }
				break;

			// Options
			case mm_Options:
                if( ev->iEventMsg == TBT_CLICKED ) {
					PlaySoundSample(sfxGeneral.smpClick);
				    Menu_MainShutdown();
				    Menu_OptionsInitialize();
				    return;
                }
				break;

            // Quit
			case mm_Quit:
                if( ev->iEventMsg == BTN_CLICKED ) {
			        PlaySoundSample(sfxGeneral.smpClick);

                    cMainMenu.Draw(tMenu->bmpBuffer.get());

					MessageBoxReturnType answer = Menu_MessageBox(GetGameName(), "Quit OpenLieroX?", LMB_YESNO);
					if( answer == MBR_YES || answer == MBR_INVALID ) {
						game.state = Game::S_Quit;
					    Menu_MainShutdown();
				    } else {

					    // Create the buffer
					    DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpMainBack_wob,0,0);
						if (tMenu->tFrontendInfo.bPageBoxes)
							Menu_DrawBox(tMenu->bmpBuffer.get(), 15,130, 625, 465);
					    DrawImage(tMenu->bmpBuffer.get(),tMenu->bmpLieroXtreme, 320 - tMenu->bmpLieroXtreme.get()->w/2, 10);
					    Menu_RedrawMouse(true);
				    }
				    return;
                }
                break;
            
			case mm_LXALink:
				if( ev->iEventMsg == TXB_MOUSEUP ) {
					OpenLinkInExternBrowser("https://openlierox.net");
				}
				break;

			case mm_Theme:
				if( ev->iEventMsg == CMB_CHANGED ) {
					CCombobox * cbox = (CCombobox *) cMainMenu.getWidget(mm_Theme);
					if (cbox && cbox->getSelectedItem().get() && cbox->getSelectedItem()->index() != tLXOptions->sTheme) {
						tLXOptions->sTheme = cbox->getSelectedItem()->index();
						notes("Changed theme to: " + tLXOptions->sTheme + "\n");

						// Atm it's the easiest way and we ensure that every gfx is newly loaded.
						// The problem is that we would have to clear all the caches because
						// the order of the searchpaths changed etc.

						// restart game
						game.state = Game::S_Quit;
						Menu_MainShutdown(); // cleanup for this menu
						bRestartGameAfterQuit = true; // set restart-flag
					}
				}
				break;

			default:
				break;
		}
	}

	if(mouseover) {
		alpha += tLX->fDeltaTime.seconds()*5;
		alpha = MIN(1.0f,alpha);
	} else {
		alpha -= tLX->fDeltaTime.seconds()*5;
		alpha = MAX(0.0f,alpha);
	}

	if(alpha) {

		DrawImageAdv(VideoPostProcessor::videoSurface().get(),tMenu->bmpBuffer, 410,260, 410,260, 200,64);

		switch(img) {
			case 0:
				//DrawImageAdv(VideoPostProcessor::videoSurface(), tMenu->bmpMainLocal, 0,y, 410, 260, tMenu->bmpMainLocal->w,64);
				break;
			case 1:
				//DrawImageAdv(VideoPostProcessor::videoSurface(), tMenu->bmpMainNet, 0,y, 410, 260, tMenu->bmpMainNet->w,64);
				break;
		}
		lastimg = img;
	}


	// Credits

	static const std::string credits1 = "  " + GetGameVersion().asHumanString(); // TODO: should we print revision here?

	static const std::string credits2 = std::string(
		"- Original code by Jason Boettcher\n"
		"- Ported and enhanced by\n"
		"  K. Petránek, Albert Zeyer, Daniel Sjoholm,\n" // In order of joining
		"  Martin Griffin, Sergiy Pylypenko\n"
		"- Music by Corentin Larsen\n"
		)
		+ tMenu->tFrontendInfo.sFrontendCredits;


	//
	// Draw the version number
	//

	// Set special spacing for credits
	int orig_spacing = tLX->cFont.GetVSpacing();
	tLX->cFont.SetVSpacing(tMenu->tFrontendInfo.iCreditsSpacing);

	int x = tMenu->tFrontendInfo.iCreditsLeft;
	int y = tMenu->tFrontendInfo.iCreditsTop;
	static int w = 0;
	if (!w)
		w = MAX(tLX->cFont.GetWidth(credits1), tLX->cFont.GetWidth(credits2)) + 4;

	static int h = 0;
	if (!h)
		h = tLX->cFont.GetHeight() + tLX->cFont.GetHeight(credits2) + 4;

	Menu_redrawBufferRect(x, y, w, h);
	tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), x, y, tLX->clCredits1, credits1);
	tLX->cFont.Draw(VideoPostProcessor::videoSurface().get(), x, y + tLX->cFont.GetHeight(), tLX->clCredits2, credits2);


	// Restore the original spacing
	tLX->cFont.SetVSpacing(orig_spacing);


	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());
}


///////////////////
// Shutdown the main menu
void Menu_MainShutdown()
{
	cMainMenu.Shutdown();
}

static ScriptVarPtr_t themeVar;

struct Menu_Main_GuiThemeComboboxCreate__Executer {
	CCombobox* combobox;
	
	void execute() {
		// GUI theme combobox

		cMainMenu.Add( new CLabel("Theme", tLX->clNormalLabel), -1, 465,10,0,0);
		combobox = new CCombobox();
		cMainMenu.Add(combobox, mm_Theme, 515,8,115,17);

		// Find all directories in the the lierox
		combobox->clear();
		combobox->setSorted(SORT_ASC);
		combobox->setUnique(true);
		combobox->setKeyboardNavigationOrder(1);

		combobox->addItem("", "- Default -");
		FindFiles(*this, "themes", false, FM_DIR);
		combobox->setCurSIndexItem(tLXOptions->sTheme);
	}

	// handler for FindFile
	bool operator() (std::string abs_filename) {
		size_t sep = findLastPathSep(abs_filename);
		if(sep != std::string::npos) abs_filename.erase(0, sep+1);

		if(abs_filename != "" && abs_filename[0] != '.')
			combobox->addItem(abs_filename, abs_filename);

		return true;
	}
};

void Menu_Main_GuiThemeComboboxCreate() {
	Menu_Main_GuiThemeComboboxCreate__Executer initialiser;
	initialiser.execute();
}

}; // namespace DeprecatedGUI
