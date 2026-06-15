/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////



#ifndef __CCLIENT_NET_ENGINE_H__
#define __CCLIENT_NET_ENGINE_H__


#include <string>
#include "Consts.h"
#include "SmartPointer.h"


class CClient;
class CBytestream;
struct ClientConnectionRequestInfo;

class CClientNetEngine {

protected:

	CClient * client;
	
public:
	// Constructor
	CClientNetEngine(CClient * _client): client(_client) { }
	virtual ~CClientNetEngine();

	// Parsing
	void				ParseConnectionlessPacket(CBytestream *bs);
	bool				ParsePacket(CBytestream *bs);

	// Sending
	virtual void		SendGameReady();
	virtual void		SendDeath(int victim, int killer);
	virtual void		SendText(const std::string& sText, std::string sWormName);
	virtual void		SendWormDetails();
	virtual void		SendGrabBonus(int id, int wormid);
	virtual void		SendUpdateLobby(bool ready = true);
	virtual void		SendDisconnect();
	virtual void		SendFileData();
	virtual void		SendChatCommandCompletionRequest(const std::string& startStr) { return; }
	virtual void		SendAFK(int wormid, AFK_TYPE afkType, const std::string & message = "") { return; }
	virtual void		SendReportDamage(bool flush = false) { return; }
	virtual void		QueueReportDamage(int victim, float damage, int offender) { return; }
#ifdef FUZZY_ERROR_TESTING
	void				SendRandomPacket();
#endif

private:
	void		SendTextInternal(const std::string& sText, const std::string& sWormName);

protected:

	// Internal details that may be used by child class
	// I expect child class will redirect most in-lobby messages to parent class
	void		 ParseChallenge(CBytestream *bs);
	void		 ParseConnected(CBytestream *bs);
	void		 ParsePong();
	void		 ParseTimeIs(CBytestream *bs);
	void		 ParseTraverse(CBytestream *bs);
	void		 ParseConnectHere(CBytestream *bs);

	virtual bool ParsePrepareGame(CBytestream *bs);
	virtual void ParseScoreUpdate(CBytestream *bs);
	virtual void ParseUpdateLobbyGame(CBytestream *bs);
	virtual void ParseChatCommandCompletionSolution(CBytestream* bs) { return; }
	virtual void ParseChatCommandCompletionList(CBytestream* bs) { return; }
	virtual void ParseAFK(CBytestream* bs) { return; }
    virtual void ParseReportDamage(CBytestream *bs) { return; }
	virtual void ParseHideWorm(CBytestream *bs) { return; }
	virtual void ParseGotoLobby(CBytestream *bs);
	virtual void ParseStartGame(CBytestream *bs);
	virtual void ParseSpawnWorm(CBytestream *bs);
	virtual void ParseWormDown(CBytestream *bs);
	virtual void ParseUpdateWorms(CBytestream *bs);
	virtual int  ParseWormInfo(CBytestream *bs);

	void		 ParseUpdateLobby(CBytestream *bs);
	void		 ParseWormWeaponInfo(CBytestream *bs);
	void		 ParseText(CBytestream *bs);
	void		 ParseGameOver(CBytestream *bs);
	void		 ParseSpawnBonus(CBytestream *bs);
	void		 ParseTagUpdate(CBytestream *bs);
	void		 ParseCLReady(CBytestream *bs);
	void		 ParseWormsOut(CBytestream *bs);
	void		 ParseServerLeaving(CBytestream *bs);
	void		 ParseSingleShot(CBytestream *bs);
	void		 ParseMultiShot(CBytestream *bs);
	void		 ParseUpdateStats(CBytestream *bs);
	void		 ParseDestroyBonus(CBytestream *bs);
    void		 ParseDropped(CBytestream *bs);
    void		 ParseSendFile(CBytestream *bs);
	virtual void ParseFlagInfo(CBytestream* bs);
	virtual void ParseTeamScoreUpdate(CBytestream* bs);
	virtual void ParseWormProps(CBytestream* bs);
	virtual void ParseSelectWeapons(CBytestream* bs);
	virtual void ParsePlaySound(CBytestream* bs) {}

	void		 ParseUpdateLobby_Internal(CBytestream *bs, std::vector<byte> * updatedWorms = NULL); // Second parameter is used only in CClientNetEngineBeta9::ParseUpdateLobby()
	
};

// TODO: maybe add CClientNetEngineBeta5 for map/mod downloading - SendFileData() and ParseSendFile() funcs
class CClientNetEngineBeta7: public CClientNetEngine {
public:
	CClientNetEngineBeta7( CClient * _client ): CClientNetEngine( _client ) { }

	virtual void SendChatCommandCompletionRequest(const std::string& startStr);
	virtual void SendAFK(int wormid, AFK_TYPE afkType, const std::string & message = "");

	virtual bool ParsePrepareGame(CBytestream *bs);
	virtual void ParseUpdateLobbyGame(CBytestream *bs);
	virtual void ParseChatCommandCompletionSolution(CBytestream* bs);
	virtual void ParseChatCommandCompletionList(CBytestream* bs);
	virtual void ParseAFK(CBytestream* bs);
};

class CClientNetEngineBeta9: public CClientNetEngineBeta7 {
public:
	CClientNetEngineBeta9( CClient * _client ): CClientNetEngineBeta7( _client ) 
	{ 
		fLastDamageReportSent = AbsTime();
	}

	virtual bool ParsePrepareGame(CBytestream *bs);
	virtual void ParseUpdateLobbyGame(CBytestream *bs);
	virtual void ParseReportDamage(CBytestream *bs);
	virtual void ParseScoreUpdate(CBytestream *bs);
	virtual void ParseHideWorm(CBytestream *bs);
	virtual int  ParseWormInfo(CBytestream *bs);
	virtual void ParseStartGame(CBytestream *bs);
	virtual void ParseFlagInfo(CBytestream* bs);
	virtual void ParseTeamScoreUpdate(CBytestream* bs);
	virtual void ParseWormProps(CBytestream* bs);
	virtual void ParseSelectWeapons(CBytestream* bs);
	virtual void ParsePlaySound(CBytestream* bs);

    void		 ParseFeatureSettings(CBytestream* bs);

	virtual void SendDeath(int victim, int killer);
	virtual void SendReportDamage(bool flush = false);
	virtual void QueueReportDamage(int victim, float damage, int offender);
	
private:
    AbsTime fLastDamageReportSent;
    std::map< std::pair< int, int >, float > cDamageReport;
};

#endif  //  __CCLIENT_NET_ENGINE_H__
