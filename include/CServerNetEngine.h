/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


#ifndef __CSERVER_NET_ENGINE_H__
#define __CSERVER_NET_ENGINE_H__

#include "game/CWorm.h"
#include "CVec.h"

class GameServer;
class CServerConnection;

// This class is not finished yet (and I think it never will be),
// so look at it as to incorporation of all differences between OLX versions.
// Big part of net protocol that have not changed since 0.56b is scattered around GameServer and CWorm classes

class CServerNetEngine
{
public:
	// Constructor
	CServerNetEngine( GameServer * _server, CServerConnection * _client ):
		server( _server ), cl( _client )
		{ }

	virtual ~CServerNetEngine() { }
	
	// TODO: move here all net code from CServer
	// Do not move here ParseConnectionlessPacket(), or make it static, 'cause client is not available for connectionless packet
	
	// Parsing

	void		ParsePacket(CBytestream *bs);

	virtual void ParseChatCommandCompletionRequest(CBytestream *bs) { return; };
	virtual void ParseAFK(CBytestream *bs) { return; };
	virtual void ParseReportDamage(CBytestream *bs) { return; };

	void		 ParseImReady(CBytestream *bs);
	void		 ParseUpdate(CBytestream *bs);
	void		 ParseDeathPacket(CBytestream *bs);
	void		 ParseChatText(CBytestream *bs);
	void		 ParseUpdateLobby(CBytestream *bs);
	void		 ParseDisconnect();
	void		 ParseGrabBonus(CBytestream *bs);
	void		 ParseSendFile(CBytestream *bs);

	bool		 ParseChatCommand(const std::string& message);
	
	// Sending
	
	void		SendPacket(CBytestream *bs);

	void		 SendPrepareGame();
	virtual void SendText(const std::string& text, int type);
	virtual void SendChatCommandCompletionSolution(const std::string& startStr, const std::string& solution) { return; };
	virtual void SendChatCommandCompletionList(const std::string& startStr, const std::list<std::string>& solutions) { return; };
	virtual int  SendFiles() { return 0; }; // Returns client ping, or 0 if no packet was sent
	virtual void SendReportDamage(bool flush = false) { return; }
	virtual void QueueReportDamage(int victim, float damage, int offender) { return; }
	
	virtual void SendWormScore(CWorm *Worm);
	void		 SendUpdateLobbyGame();
	void		 SendUpdateLobby(CServerConnection *target = NULL);

	void SendClientReady(CServerConnection* receiver); // If Receiver != NULL we're sending to worm connected during game
	void SendWormsOut(const std::list<byte>& ids);
	void SendWormDied(CWorm *Worm);
	void SendWeapons(CWorm* w = NULL);
	void SendSpawnWorm(CWorm *Worm, CVec pos);
	virtual void SendHideWorm(CWorm *worm, int forworm, bool show = false, bool immediate = false);
	virtual void SendTeamScoreUpdate() {}
	virtual void SendWormProperties(CWorm* worm);
	void SendWormProperties(bool onlyIfNotDef); // for all worms
	static bool isWormPropertyDefault(CWorm* worm);
	virtual void SendSelectWeapons(CWorm* worm);
	virtual void SendUpdateWorm(CWorm* w);
	void SendPlaySound(const std::string& name);
		
	int getConnectionArrayIndex();
	
protected:
	// Attributes
	GameServer 	*server;
	CServerConnection *cl;
	
	virtual void WritePrepareGame(CBytestream *bs);
	virtual void WriteUpdateLobbyGame(CBytestream *bs);
};

class CServerNetEngineBeta3: public CServerNetEngine 
{

public:
	CServerNetEngineBeta3( GameServer * _server, CServerConnection * _client ):
		CServerNetEngine( _server, _client )
		{ }

	virtual void SendText(const std::string& text, int type);
	virtual void SendHideWorm(CWorm *worm, int forworm, bool show = false, bool immediate = false);

};

class CServerNetEngineBeta5: public CServerNetEngineBeta3
{

public:
	CServerNetEngineBeta5( GameServer * _server, CServerConnection * _client ):
		CServerNetEngineBeta3( _server, _client )
		{ }
		
	virtual int SendFiles();

};

class CServerNetEngineBeta7: public CServerNetEngineBeta5
{

public:
	CServerNetEngineBeta7( GameServer * _server, CServerConnection * _client ):
		CServerNetEngineBeta5( _server, _client )
		{ }

	virtual void ParseChatCommandCompletionRequest(CBytestream *bs);
	virtual void ParseAFK(CBytestream *bs);

	virtual void SendChatCommandCompletionSolution(const std::string& startStr, const std::string& solution);
	virtual void SendChatCommandCompletionList(const std::string& startStr, const std::list<std::string>& solutions);

protected:
	virtual void WritePrepareGame(CBytestream *bs);
	void WriteUpdateLobbyGame(CBytestream *bs);
};

class CServerNetEngineBeta8: public CServerNetEngineBeta7
{

public:
	CServerNetEngineBeta8( GameServer * _server, CServerConnection * _client ):
		CServerNetEngineBeta7( _server, _client )
		{ }

	virtual void SendText(const std::string& text, int type);

};

class CServerNetEngineBeta9: public CServerNetEngineBeta8
{

public:
	CServerNetEngineBeta9( GameServer * _server, CServerConnection * _client ):
		CServerNetEngineBeta8( _server, _client )
	{ 
		fLastDamageReportSent = AbsTime();
	}

	virtual void ParseReportDamage(CBytestream *bs);
	virtual void SendReportDamage(bool flush = false);
	virtual void QueueReportDamage(int victim, float damage, int offender);
	virtual void SendWormScore(CWorm *Worm);
	virtual void SendHideWorm(CWorm *worm, int forworm, bool show = false, bool immediate = false);
	virtual void SendTeamScoreUpdate();
	virtual void SendWormProperties(CWorm* worm);
	virtual void SendSelectWeapons(CWorm* worm);
	virtual void SendUpdateWorm(CWorm* w);
	
	static void WriteFeatureSettings(CBytestream* bs, const Version& compatVer);

protected:
	virtual void WritePrepareGame(CBytestream *bs);
	void WriteUpdateLobbyGame(CBytestream *bs);
	
private:
    AbsTime fLastDamageReportSent;
    std::map< std::pair< int, int >, float > cDamageReport;
};


#endif  //  __CSERVER_NET_ENGINE_H__
