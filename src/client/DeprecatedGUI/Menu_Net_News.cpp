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
#include "FindFile.h"
#include "sound/SoundsBase.h"
#include "DeprecatedGUI/Graphics.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CBrowser.h"
#include "DeprecatedGUI/CListview.h"
#include "HTTP.h"
#include "StringUtils.h"
#include "AuxLib.h"


namespace DeprecatedGUI {

static CGuiLayout	cNews;
std::string strNewsPage;

enum {
	nw_Back = 0,
	nw_NewsBrowser,
	nw_Refresh,
};


///////////////////
// Initialize the news net menu
bool Menu_Net_NewsInitialize()
{
	iNetMode = net_news;

	// Setup the gui layout
	cNews.Shutdown();
	cNews.Initialize();

	cNews.Add( new CButton(BUT_BACK, tMenu->bmpButtons), nw_Back, 25,440, 50,15);
	cNews.Add( new CButton(BUT_REFRESH, tMenu->bmpButtons), nw_Refresh, 520,440, 50,15);
	cNews.Add( new CBrowser(), nw_NewsBrowser, 50, 160, 540, 260);

	Menu_Net_AddTabBarButtons(&cNews);

	// Get the news page
	FILE *fp = OpenGameFile("cfg/newsserver.txt", "r");
	if (fp)  {
		strNewsPage = ReadUntil(fp, '\n');
		fclose(fp);
	} else {
		strNewsPage = "http://openlierox.sourceforge.net/news.php"; // Default
	}

	// Load the news
	CBrowser *b = (CBrowser *)cNews.getWidget(nw_NewsBrowser);
	b->LoadFromHTTP(strNewsPage);


	return true;
}

//////////////////
// Shutdown the news menu
void Menu_Net_NewsShutdown()
{
	cNews.Shutdown();
}


///////////////////
// The net news menu frame
void Menu_Net_NewsFrame(int mouse)
{
	gui_event_t *ev = NULL;

	// Process the HTTP transfer
	CBrowser *news = (CBrowser *)cNews.getWidget(nw_NewsBrowser);
	news->ProcessHTTP();


	// Process & Draw the gui
	ev = cNews.Process();
	cNews.Draw( VideoPostProcessor::videoSurface().get() );

	if (Menu_Net_ProcessTabBarButtons(ev))
		return;

	// Process any events
	if(ev) {

		switch(ev->iControlID) {

			// Back
			case nw_Back:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					// Shutdown
					Menu_Net_NewsShutdown();

					// Back to main menu
					Menu_MainInitialize();
				}
				break;

			// Update
			case nw_Refresh:
				if(ev->iEventMsg == BTN_CLICKED) {

					// Click!
					PlaySoundSample(sfxGeneral.smpClick);

					((CBrowser *)cNews.getWidget(nw_NewsBrowser))->LoadFromHTTP(strNewsPage);
				}
				break;
		}

	}


	// Draw the mouse
	DrawCursor(VideoPostProcessor::videoSurface().get());
}

}; // namespace DeprecatedGUI
