/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Protocol
// Created 1/7/02
// Jason Boettcher


#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__


// enum pwns #define

enum {
	PROTOCOL_VERSION = 8
};


// Text type
enum TXT_TYPE {
	TXT_CHAT			= 0,
	TXT_NORMAL			= 1,
	TXT_NOTICE			= 2,
	TXT_IMPORTANT		= 3,
	TXT_NETWORK			= 4,
	TXT_PRIVATE			= 5,
	TXT_TEAMPM			= 6
};


// Client->Server
enum C2S_MESSAGES {
	C2S_TXTCOMMAND		= 0,
	C2S_IMREADY			= 1,
	C2S_CARVE			= 2,
	C2S_UPDATE			= 3,
	C2S_DEATH			= 4,
	C2S_CHATTEXT		= 5,
	C2S_UPDATELOBBY		= 6,
	C2S_DISCONNECT		= 7,
	C2S_GRABBONUS		= 8,
	C2S_SENDFILE        = 9, // Beta4+ only, enabled only in Beta6+
	C2S_CHATCMDCOMPLREQ	= 10, // since Beta7
	C2S_AFK				= 11, // since Beta7, if client away from keyboard
	C2S_REPORTDAMAGE	= 12, // since Beta9
	//C2S_NEWNET_KEYS	= 13, // since Beta9 - NewNet was never finished and later was removed
	//C2S_NEWNET_CHECKSUM = 14, // since Beta9 - NewNet was never finished and later was removed
	C2S_GUSANOS			= 15, // >=0.59 beta1
	C2S_GUSANOSUPDATE	= 16, // >=0.59 beta5
	C2S_GAMEATTRUPDATE	= 17, // >=0.59 beta10
};

// Server->Client
enum S2C_MESSAGES {
	S2C_PREPAREGAME		= 0,
	S2C_STARTGAME		= 1,
	S2C_SPAWNWORM		= 2,
	S2C_WORMINFO		= 3,
	S2C_TEXT			= 4,
	S2C_SCOREUPDATE		= 5,
	S2C_GAMEOVER		= 6,
	S2C_SPAWNBONUS		= 7,
	S2C_TAGUPDATE		= 8,
	S2C_CLREADY			= 9,
	S2C_UPDATELOBBY		= 10,
	S2C_WORMSOUT		= 11,
	S2C_UPDATEWORMS		= 12,
	S2C_UPDATELOBBYGAME	= 13,
	S2C_WORMDOWN		= 14,
	S2C_LEAVING			= 15,
	S2C_SINGLESHOOT		= 16,
	S2C_MULTISHOOT		= 17,
	S2C_UPDATESTATS		= 18,
	S2C_GOTOLOBBY		= 19,
	S2C_DESTROYBONUS	= 20,
	S2C_DROPPED         = 21,
	S2C_SENDFILE        = 22, // Beta4+ only, enabled only in Beta6+
	S2C_WORMWEAPONINFO	= 23, // >=Beta7
	S2C_CHATCMDCOMPLSOL	= 24, // since Beta7
	S2C_AFK				= 25, // since Beta7, if client away from keyboard
	S2C_CHATCMDCOMPLLST	= 26, // since Beta7
	S2C_REPORTDAMAGE	= 27, // since Beta9
	S2C_HIDEWORM		= 28, // since Beta9
	//S2C_NEWNET_KEYS		= 29, // since Beta9 - NewNet was never finished and later was removed
	S2C_TEAMSCOREUPDATE = 30, // >=beta9
	S2C_FLAGINFO		= 31, // >=beta9
	S2C_SETWORMPROPS	= 32, // >=beta9
	S2C_SELECTWEAPONS	= 33, // >=beta9
	S2C_GUSANOS			= 34, // >=0.59 beta1
	S2C_PLAYSOUND		= 35, // >=0.59 beta1
	S2C_GUSANOSUPDATE	= 36, // >=0.59 beta5
	S2C_GAMEATTRUPDATE	= 37, // >=0.59 beta10
};


#endif  //  __PROTOCOL_H__
