/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Menu header file
// Created 30/6/02
// Jason Boettcher


#ifndef __MENU_H__DEPRECATED_GUI__
#define __MENU_H__DEPRECATED_GUI__

#include <SDL.h>
#include <string>
#include <list>

#include "DeprecatedGUI/CWidgetList.h"
#include "DeprecatedGUI/CCombobox.h"
#include "DeprecatedGUI/CInputBox.h"
#include "Networking.h"
#include "CBytestream.h"
#include "DeprecatedGUI/CListview.h"
#include "CWpnRest.h"
#include "CClient.h" // for MAX_PLAYERS
#include "CGameSkin.h"
#include "ProfileSystem.h"
#include "Color.h"
#include "HTTP.h"
#include "ThreadVar.h"


enum {
	// Menu sockets
	SCK_LAN = 0,
	SCK_NET = 1,
	SCK_FOO = 2,	
};

namespace DeprecatedGUI {

enum {
// Server info dialog dimensions
	INFO_W = 400,
	INFO_H = 420,
};


// Menu types
enum {
	MNU_MAIN=0,
	MNU_LOCAL,
	MNU_NETWORK,
	MNU_PLAYER,
	MNU_OPTIONS,
	MNU_MAPED,
	MNU_GUISKIN
};


// Layout IDs
enum {
	L_MAINMENU=0,
	L_LOCALPLAY,
	L_GAMESETTINGS,
	L_WEAPONOPTIONS,
	L_LOADWEAPONS,
	L_SAVEWEAPONS,
	L_NET,
	L_NETINTERNET,
	L_INTERNETDETAILS,
	L_ADDSERVER,
	L_NETLAN,
	L_LANDETAILS,
	L_NETHOST,
	L_NETFAVOURITES,
	L_FAVOURITESDETAILS,
	L_RENAMESERVER,
	L_ADDFAVOURITE,
	L_CONNECTING,
	L_NETJOINLOBBY,
	L_NETHOSTLOBBY,
	L_SERVERSETTINGS,
	L_BANLIST,
	L_PLAYERPROFILES,
	L_CREATEPLAYER,
	L_VIEWPLAYERS,
	L_LEVELEDITOR,
	L_NEWDIALOG,
	L_SAVELOADLEVEL,
	L_OPTIONS,
	L_OPTIONSCONTROLS,
	L_OPTIONSGAME,
	L_OPTIONSSYSTEM,
	L_MESSAGEBOXOK,
	L_MESSAGEBOXYESNO,
	LAYOUT_COUNT
};

// Box types
enum {
	BX_OUTSET = 0,
	BX_INSET,
	BX_SOLID
};


// Sub title id's
enum {
	SUB_LOCAL=0,
	SUB_NETWORK,
	SUB_PLAYER,
	SUB_MAPED,
	SUB_OPTIONS,
	SUB_LOBBY
};


// Message box types
enum MessageBoxType {
	LMB_OK = 0,
	LMB_YESNO
};
	
enum MessageBoxReturnType {
	// Return types
	MBR_INVALID = -1,
	MBR_OK = 0,
	MBR_YES,
	MBR_NO
};


// Buttons
enum {
	BUT_MAIN=0,
	BUT_INTERNET,
	BUT_LAN,
	BUT_HOST,
	BUT_FAVOURITES,
	BUT_BACK,
	BUT_OK,
	BUT_CREATE,
	BUT_NEW,
	BUT_RANDOM,
	BUT_LOAD,
	BUT_SAVE,
	BUT_QUIT,
	BUT_CANCEL,
	BUT_QUITGAME,
	BUT_PLAYAGAIN,
	BUT_YES,
	BUT_NO,
	BUT_CONTROLS,
	BUT_GAME,
	BUT_SYSTEM,
	BUT_APPLY,
	BUT_RESUME,
	BUT_NEWPLAYER,
	BUT_VIEWPLAYERS,
	BUT_DELETE,
	BUT_START,
	BUT_JOIN,
	BUT_REFRESH,
	BUT_ADD,
	BUT_READY,
	BUT_LEAVE,
	BUT_GAMESETTINGS,
	BUT_UPDATELIST,
    BUT_WEAPONOPTIONS,
    BUT_RESET,
    BUT_DEFAULT,
	BUT_BANNED,
	BUT_UNBAN,
	BUT_CLEAR,
	BUT_SERVERSETTINGS,
	BUT_FILTER,
	BUT_ADDTOFAVOURITES,
	BUT_NEWS,
	BUT_GENERAL,
	BUT_BONUS,
	BUT_CHAT,
	BUT_TEST,
	BUT_MORE,
	BUT_LESS
};

// Button names
typedef const char * ConstCharP_t; // Workaround for error in MSVC which won't allow type "const char * const" allowed by G++
ConstCharP_t const sButtonNames[] =  {
	"Main",
	"Internet",
	"LAN",
	"Host",
	"Favourites",
	"Back",
	"Ok",
	"Create",
	"New",
	"Random",
	"Load",
	"Save",
	"Quit",
	"Cancel",
	"Quit Game",
	"Play Again",
	"Yes",
	"No",
	"Controls",
	"Game",
	"System",
	"Apply",
	"Resume",
	"New Player",
	"View Players",
	"Delete",
	"Start",
	"Join",
	"Refresh",
	"Add",
	"Ready",
	"Leave",
	"Game Settings",
	"Update List",
	"Weapon Options",
	"Reset",
	"Default",
	"Ban List",
	"Unban",
	"Clear",
	"Server Settings",
	"Filter",
	"Add to Favourites",
	"News",
	"General",
	"Bonus",
	"Chat",
	"Test",
	"More >>",
	"<< Less"
};

// Frontend info
class frontendinfo_t { public:
	int				iMainTitlesLeft;
	int				iMainTitlesTop;
	int				iMainTitlesSpacing;
	int				iCreditsLeft;
	int				iCreditsTop;
	int				iCreditsSpacing;
	bool			bPageBoxes;
	std::string		sFrontendCredits;
	float			fLoadingAnimFrameTime;
	int				iLoadingAnimLeft;
	int				iLoadingAnimTop;
	int				iLoadingBarLeft;
	int				iLoadingBarTop;
	int				iLoadingLabelLeft;
	int				iLoadingLabelTop;
};

// Menu structure
struct menu_t {
	menu_t();

	// Graphics
	//SmartPointer<SDL_Surface> bmpMainBack;
    //SmartPointer<SDL_Surface> bmpMainBack_lg;
    SmartPointer<SDL_Surface> bmpMainBack_wob;
	SmartPointer<SDL_Surface> bmpMainBack_common;
	SmartPointer<SDL_Surface> bmpBuffer;

	SmartPointer<SDL_Surface> bmpMsgBuffer;
    SmartPointer<SDL_Surface> bmpMiniMapBuffer;

	SmartPointer<SDL_Surface> bmpLieroXtreme;
	SmartPointer<SDL_Surface> bmpMainTitles;
	SmartPointer<SDL_Surface> bmpTitles;
	SmartPointer<SDL_Surface> bmpSubTitles;
	SmartPointer<SDL_Surface> bmpButtons;
	SmartPointer<SDL_Surface> bmpCheckbox;
	SmartPointer<SDL_Surface> bmpInputbox;
	SmartPointer<SDL_Surface> bmpChatBackgroundMain;
	SmartPointer<SDL_Surface> bmpChatBackground;

	SmartPointer<SDL_Surface> bmpMainLocal;
	SmartPointer<SDL_Surface> bmpMainNet;
	SmartPointer<SDL_Surface> bmpLobbyReady;
	SmartPointer<SDL_Surface> bmpLobbyNotReady;
	SmartPointer<SDL_Surface> bmpHost;
	SmartPointer<SDL_Surface> bmpConnectionSpeeds[5];
	SmartPointer<SDL_Surface> bmpSpeech;
    SmartPointer<SDL_Surface> bmpHandicap;
	SmartPointer<SDL_Surface> bmpDownload;

	SmartPointer<SDL_Surface> bmpTriangleUp;
	SmartPointer<SDL_Surface> bmpTriangleDown;

	SmartPointer<SDL_Surface> bmpAI;
	CGameSkin cSkin;

	SmartPointer<SDL_Surface> bmpMapEdTool;

	// Other
	int				iMenuType;
	frontendinfo_t	tFrontendInfo;
	std::string		sSavedChatText;
	std::list<SmartPointer<profile_t> >	sLocalPlayers;
	bool			bForbidConsole;  // Don't show console

	// Map Editor
	int				iEditMode;
	int				iCurHole;
	int				iCurStone;
	int				iCurMisc;
	int				iCurDirt;

	int				iReturnTo;

	// Socket for pinging
	SmartPointer<NetworkSocket>	tSocket[3];
	
	CHttp			CheckForNewDevelopmentVersion_http; // I don't want to mess up with static data deallocation, so just putting this here

};


// Net menu types
enum {
	net_main=0,
	net_internet,
	net_lan,
	net_host,
	net_favourites,
	net_news,
	net_chat,
	net_join
};

	
// Menu globals
extern	menu_t		*tMenu;
extern	int			iNetMode;
extern	int			iJoinMenu;
extern	int			iHostType;
extern	bool		bSkipStart;
extern  bool		bHost_Update;
extern	bool		bJoin_Update;
extern  CWidgetList	LayoutWidgets[LAYOUT_COUNT];

extern	bool		bGotDetails;
extern	bool		bOldLxBug;
extern	int			nTries;
extern	AbsTime		fStart;
extern bool		bShowFloatingOptions;


// Routines
bool	Menu_Initialize();
void	Menu_LoadFrontendInfo();
void	Menu_Shutdown();
void    Menu_Frame();
void	Menu_RedrawMouse(bool total);
void    Menu_SetSkipStart(int s);
void	Menu_DrawSubTitle(SDL_Surface * bmpDest, int id);
void    Menu_DrawSubTitleAdv(SDL_Surface * bmpDest, int id, int y);
void	Menu_DrawBox(SDL_Surface * bmpDest, int x, int y, int x2, int y2);
void	Menu_DrawBoxAdv(SDL_Surface * bmpDest, int x, int y, int x2, int y2, int border, Color LightColour, Color DarkColour, Color BgColour, uchar type);
void    Menu_DrawBoxInset(SDL_Surface * bmpDest, int x, int y, int x2, int y2);
void    Menu_DrawWinButton(SDL_Surface * bmpDest, int x, int y, int w, int h, bool down);
bool	Menu_LoadWormGfx(profile_t *ply);
MessageBoxReturnType Menu_MessageBox(const std::string& sTitle, const std::string& sText, MessageBoxType type = LMB_OK);
void	Menu_AddDefaultWidgets();
void	Menu_FillLevelList(CCombobox *cmb, int random);
void    Menu_redrawBufferRect(int x, int y, int w, int h);
void	Menu_DisableNetEvents();
void	Menu_EnableNetEvents();

void    Menu_SvrList_DrawInfo(const std::string& szAddress, int w, int h);
	
// Main menu
void	Menu_MainInitialize();
void	Menu_MainShutdown();
void	Menu_MainFrame();
void	Menu_MainDrawTitle(int x, int y, int id, int selected);


// Local menu
void	Menu_LocalInitialize();
void	Menu_LocalFrame();
void	Menu_LocalAddProfiles();
void	Menu_LocalStartGame();
bool	Menu_LocalCheckPlaying(int index);
void	Menu_Local_FillModList( CCombobox *cb );
void	Menu_LocalShowMinimap(bool bReload);
void	Menu_LocalAddPlaying(int index = -1);
void	Menu_LocalRemovePlaying(int index = -1);
void	Menu_LocalShutdown();


// Player menu
void	Menu_PlayerInitialize();
void	Menu_PlayerFrame();
void    Menu_Player_NewPlayerInit();
void    Menu_Player_ViewPlayerInit();
void	Menu_Player_NewPlayer(int mouse);
void	Menu_Player_ViewPlayers(int mouse);
void	Menu_Player_AddPlayer(const std::string& sName, Uint8 r, Uint8 g, Uint8 b);
void    Menu_Player_DrawWormImage(SDL_Surface * bmpDest, int Frame, int dx, int dy, int ColR, int ColG, int ColB);
void	Menu_Player_DeletePlayer(int index);
void    Menu_Player_FillSkinCombo(CCombobox *cb);
void	Menu_PlayerShutdown();

// Map editor
bool	Menu_MapEdInitialize();
void	Menu_MapEdFrame(SDL_Surface * bmpDest, int process);
void	Menu_MapEd_New();
void	Menu_MapEd_LoadSave(int save);
bool	Menu_MapEd_OkSave(const std::string& szFilename);
void	Menu_MapEdShutdown();

// Game Settings
void	Menu_GameSettings();
void	Menu_GameSettingsShutdown();
bool	Menu_GameSettings_Frame();
void	Menu_GameSettings_GrabInfo();
void    Menu_GameSettings_Default();


// Weapons Restrictions
void    Menu_WeaponsRestrictions(const std::string& szMod);
bool    Menu_WeaponsRestrictions_Frame();
void	Menu_WeaponsRestrictionsShutdown();

// Load/save dialog
void	Menu_WeaponPresets(bool save, CWpnRest *gamescript);
bool	Menu_WeaponPresetsOkSave(const std::string& szFilename);
void	Menu_WeaponPresetsShutdown();

// Ban List
void	Menu_BanList();
bool	Menu_BanList_Frame();
void	Menu_BanListShutdown();

// Server settings
void	Menu_ServerSettings();
bool	Menu_ServerSettings_Frame();
void	Menu_ServerSettingsShutdown();


// Options
bool	Menu_OptionsInitialize();
void	Menu_OptionsShutdown();
void	Menu_OptionsFrame();
void	Menu_OptionsWaitInput(int ply, const std::string& name, CInputbox *b);

// Floating Options
bool	Menu_FloatingOptionsInitialize();
void	Menu_FloatingOptionsShutdown();
void	Menu_FloatingOptionsFrame();
void	Menu_FloatingOptionsWaitInput(int ply, const std::string& name, CInputbox *b);

// Speed test
void	Menu_SpeedTest_Initialize();
bool	Menu_SpeedTest_Frame();
void	Menu_SpeedTest_Shutdown();
float	Menu_SpeedTest_GetSpeed();


// Main net
bool	Menu_NetInitialize(bool withSubMenu = true);
void	Menu_Net_GotoHostLobby();
void	Menu_NetFrame();
void	Menu_NetShutdown();
void	Menu_Net_ServerList_Refresher();

// Net::Main menu
bool	Menu_Net_MainInitialize();
void	Menu_Net_MainFrame(int mouse);
void	Menu_Net_MainShutdown();
void	Menu_Net_AddTabBarButtons(CGuiLayout * layout);
bool	Menu_Net_ProcessTabBarButtons(gui_event_t *ev);


// Net::Host menu
bool	Menu_Net_HostInitialize();
void	Menu_Net_HostShutdown();
void	Menu_Net_HostFrame(int mouse);
void	Menu_Net_HostPlyFrame(int mouse);
void	Menu_Net_HostPlyShutdown();

bool	Menu_Net_HostLobbyInitialize();
void    Menu_Net_HostLobbyDraw();
void    Menu_Net_HostLobbyCreateGui();
void	Menu_Net_HostGotoLobby();
void	Menu_Net_HostLobbyFrame(int mouse);
void	Menu_Net_HostLobbyShutdown();
std::string	Menu_Net_HostLobbyGetText();
void Menu_Net_HostLobbySetText(const std::string& str);
void	Menu_HostDrawLobby(SDL_Surface * bmpDest);
void	Menu_HostShowMinimap();
void	Menu_Net_HostDeregister();
bool	Menu_Net_HostStartGame();
void	Menu_Net_HostLobbySetMod(const std::string& moddir);
void	Menu_Net_HostLobbySetLevel(const std::string& filename);

void Menu_HostActionsPopupMenuInitialize( CGuiLayout & layout, int id_PopupMenu, int id_PopupPlayerInfo, int wormid );
void Menu_HostActionsPopupMenuClick(CGuiLayout & layout, int id_PopupMenu, int id_PopupPlayerInfo, int wormid, int menuItem);
void Menu_HostActionsPopupPlayerInfoClick(CGuiLayout & layout, int id_PopupMenu, int id_PopupPlayerInfo, int wormid, int menuItem);

	
// Net::LAN menu
bool	Menu_Net_LANInitialize();
void	Menu_Net_LANShutdown();
void	Menu_Net_LANFrame(int mouse);
void	Menu_Net_LANSendPing();
void	Menu_Net_LANJoinServer(const std::string& sAddress, const std::string& sName);
void    Menu_Net_LanShowServer(const std::string& szAddress);
void	Menu_Net_LAN_ServerList_Refresher();


// Net::Joining menu
bool	Menu_Net_JoinInitialize();
void	Menu_Net_JoinShutdown();
void	Menu_Net_JoinFrame(int mouse);

// Net::Favourites menu
bool	Menu_Net_FavouritesInitialize();
void	Menu_Net_FavouritesShutdown();
void	Menu_Net_FavouritesFrame(int mouse);
void	Menu_Net_FavouritesJoinServer(const std::string& sAddress, const std::string& sName);
void	Menu_Net_FavouritesShowServer(const std::string& szAddress);
void	Menu_Net_RenameServer(std::string& szName);
void	Menu_Net_FavouritesAddServer();
void	Menu_Net_Favourites_ServerList_Refresher();

// Net::News menu
bool	Menu_Net_NewsInitialize();
void	Menu_Net_NewsShutdown();
void	Menu_Net_NewsFrame(int mouse);

// Net::Chat menu
bool	Menu_Net_ChatInitialize();
void	Menu_Net_ChatShutdown();
void	Menu_Net_ChatFrame(int mouse);

bool	Menu_Net_JoinConnectionInitialize();
void	Menu_Net_JoinConnectionFrame(int mouse);
void	Menu_Net_JoinConnectionShutdown();

bool	Menu_Net_JoinLobbyInitialize();
void    Menu_Net_JoinDrawLobby();
void    Menu_Net_JoinLobbyCreateGui();
void	Menu_Net_JoinGotoLobby();
std::string	Menu_Net_JoinLobbyGetText();
void	Menu_Net_JoinLobbySetText(const std::string& str);
void	Menu_Net_JoinLobbyFrame(int mouse);
void	Menu_Net_JoinLobbyShutdown();


// Net::Internet menu
bool	Menu_Net_NETInitialize();
void	Menu_Net_NETShutdown();
void	Menu_Net_NETFrame(int mouse);
void	Menu_Net_NETJoinServer(const std::string& sAddress, const std::string& sName);
void	Menu_Net_NETAddServer();
void	Menu_Net_NETUpdateList();
void    Menu_Net_NETShowServer(const std::string& szAddress);
void	Menu_Net_NET_ServerList_Refresher();

void	Menu_Current_Shutdown();
	
} // namespace DeprecatedGUI

#endif  //  __MENU_H__DEPRECATED_GUI__
