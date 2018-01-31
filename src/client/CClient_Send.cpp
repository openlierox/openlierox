/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Client class - Sending routines
// Created 22/7/02
// Jason Boettcher

#include "LieroX.h"

#include "CClientNetEngine.h"
#include "StringUtils.h"
#include "CClient.h"
#include "Protocol.h"
#include "game/CWorm.h"
#include "MathLib.h"
#include "ChatCommand.h"
#include "OLXCommand.h"
#include "EndianSwap.h"
#include "CServer.h"
#include "AuxLib.h"
#include "CChannel.h"
#include "DeprecatedGUI/Menu.h"
#include "IRC.h"
#include "OLXConsole.h"
#include "game/Game.h"
#include "gusanos/network.h"
#include "CGameScript.h"
#include "game/GameState.h"



// declare them only locally here as nobody really should use them explicitly
std::string OldLxCompatibleString(const std::string &Utf8String);


///////////////////
// Send the worm details
void CClientNetEngine::SendWormDetails()
{
	if(game.isServer()) return;

	// Don't flood packets so often
	// we are checking in w->checkPacketNeeded() if we need to send an update
	// we are checking with bandwidth if we should add an update
	/*if ((tLX->currentTime - fLastUpdateSent) <= tLXOptions->fUpdatePeriod)
		if (getGameLobby()->iGameType != GME_LOCAL)
			return; */

	// TODO: Have we always used the limitcheck from GameServer here?
	// We should perhaps move it out from GameServer. Looks a bit strange here to use something from GameServer.
	if(	game.isClient() // we are a client in a netgame
	   && !GameServer::checkUploadBandwidth(client->getChannel()->getOutgoingRate()) )
		return;

	if(!game.gameScript()->gusEngineUsed() && client->getServerVersion() < OLXBetaVersion(0,59,10)) {
		// Check if we need to write the state update
		bool update = false;
		for_each_iterator(CWorm*, w, game.localWorms())
			if (w->get()->getAlive() && w->get()->checkPacketNeeded())  {
				update = true;
				break;
			}

		if(update) {
			// Write the update
			CBytestream bs;
			bs.writeByte(C2S_UPDATE);

			for_each_iterator(CWorm*, w, game.localWorms())
				w->get()->writePacket(&bs, false, NULL);

			client->bsUnreliable.Append(&bs);

			client->fLastUpdateSent = tLX->currentTime;
		}
	}
	
	// handle Gusanos updates
	// only for join-mode because otherwise, we would handle it in CServer
	if(game.isClient() && network.getNetControl()) {
		const size_t maxBytes = size_t(-1); // TODO ?
		if(maxBytes > 0 && network.getNetControl()->olxSendNodeUpdates(NetConnID_server(), maxBytes))
			client->fLastUpdateSent = tLX->currentTime;
	}
}


void CClientNetEngine::SendGameReady()
{
	if(client->getServerVersion() >= OLXBetaVersion(0,59,10)) return;

	CBytestream bs;
	bs.writeByte(C2S_IMREADY);
	
	if(game.bServerChoosesWeapons) {
		bs.writeByte(0);
	} else {		
		// Send my worm's weapon details
		bs.writeByte(game.localWorms()->size());
		for_each_iterator(CWorm*, w, game.localWorms())
			w->get()->writeWeapons( &bs );
	}
	
	client->cNetChan->AddReliablePacketToSend(bs);
}


///////////////////
// Send a death
void CClientNetEngine::SendDeath(int victim, int killer)
{
	CBytestream bs;

	bs.writeByte(C2S_DEATH);
	bs.writeInt(victim,1);
	bs.writeInt(killer,1);

	client->cNetChan->AddReliablePacketToSend(bs);
}

void CClientNetEngineBeta9::SendDeath(int victim, int killer)
{
	// If we have some damage reports in buffer send them first so clients won't sum up updated damage score and reported damage packet sent later
	SendReportDamage(true);

	CClientNetEngine::SendDeath(victim, killer);
}


///////////////////
// Send a string of text
void CClientNetEngine::SendText(const std::string& sText, std::string sWormName)
{
	if(HandleDebugCommand(sText)) return;
	
	bool chat_command = sText.size() >= 2 && sText[0] == '/' && sText[1] != '/';

	if (sText.find("/irc ") == 0 || sText.find("/chat ") == 0)  { // Send text to IRC
		bool res = false;
		if (GetGlobalIRC())
			res = GetGlobalIRC()->sendChat(sText.substr(sText.find(' ') + 1));
		if (!res)
			client->cChatbox.AddText("Could not send the IRC message", tLX->clNetworkText, TXT_NETWORK, tLX->currentTime);
		return;
	}

	// We can safely send long messages to servers >= beta8
	if (client->getServerVersion() >= OLXBetaVersion(0,57,8))  {

		// HTML support since beta 8
		SendTextInternal(OldLxCompatibleString(sText), sWormName);


	// <=beta7 server
	} else {

		if (chat_command &&
			client->getServerVersion() < OLXBetaVersion(0,57,3) && // <beta3 clients don't have chat command support
			sText.find("/me ") != 0) // Ignores "/me" special command
		{
			// Try if we can execute the same command in console (mainly for "/suicide" command to work on all servers)
			if( !subStrCaseEqual( sText.substr(1), "suicide", 7 ) )
			    Con_Execute(sText.substr(1));
			else
				client->cChatbox.AddText("HINT: server cannot execute commands, only OLX beta3+ can", tLX->clNotice, TXT_NOTICE, tLX->currentTime);
			
			return;

		} else if (chat_command &&
				   client->getServerVersion() >= OLXBetaVersion(0,57,3)) {
			// we don't have to split chat commands
			// "/me ..." is also save, because server uses SendGlobalText for sending and it splits the text there
			SendTextInternal(OldLxCompatibleString(sText), sWormName);

		} else { // we can savely split the message (and we have to)
			
			// If the text is too long, split it in smaller pieces and then send (backward comaptibility)
			int name_w = tLX->cFont.GetWidth(sWormName + ": ");
			std::vector<std::string> split = splitstring(StripHtmlTags(sText), 63,
				client->iNetStatus == NET_CONNECTED ? 600 - name_w : 300 - name_w, tLX->cFont);

			// Check
			if (split.size() == 0)  {  // More than weird...
				SendTextInternal(OldLxCompatibleString(StripHtmlTags(sText)), sWormName);
				return;
			}

			// Send the first chunk
			SendTextInternal(OldLxCompatibleString(split[0]), sWormName);

			// Send the text
			for (std::vector<std::string>::const_iterator it = split.begin() + 1; it != split.end(); it++)
				SendTextInternal(OldLxCompatibleString(*it), "");
		}
	}
}

void CClientNetEngineBeta7::SendChatCommandCompletionRequest(const std::string& startStr) {
	CBytestream bs;
	bs.writeByte(C2S_CHATCMDCOMPLREQ);
	bs.writeString(startStr);

	client->cNetChan->AddReliablePacketToSend(bs);
}

void CClientNetEngineBeta7::SendAFK(int wormid, AFK_TYPE afkType, const std::string & message ) {
	if( client->getServerVersion() < OLXBetaVersion(0,57,7) )
		return;
	
	std::string msg = message;
	if( msg == "" )
	{
		switch(afkType) {
			case AFK_BACK_ONLINE: break;
			case AFK_TYPING_CHAT: msg = "(typing)"; break;
			case AFK_AWAY: msg = "(away)"; break;
			case AFK_SELECTING_WPNS: msg = "(selecting weapons)"; break;
			case AFK_CONSOLE: msg = "(console)"; break;
			case AFK_MENU: msg = "(menu)"; break;
		}
	}


	{
		CWorm* w = game.wormById(wormid, false);
		if(w) w->setAFK(afkType, msg);
	}
	
	CBytestream bs;
	bs.writeByte(C2S_AFK);
	bs.writeByte(wormid);
	bs.writeByte(afkType);
	if(msg.size() > 127)
		msg = msg.substr(0, 127);
	bs.writeString(msg);

	client->cNetChan->AddReliablePacketToSend(bs);
}

/////////////////////
// Internal function for text sending, does not do any checks or parsing
void CClientNetEngine::SendTextInternal(const std::string& sText, const std::string& sWormName)
{
	if(!client) {
		errors << "CClientNetEngine::SendTextInternal(" << sWormName << ": " << sText << "): my client is unset" << endl;
		return;
	}
	
	if(!client->cNetChan) {
		errors << "CClientNetEngine::SendTextInternal(" << sWormName << ": " << sText << "): cNetChan of my client (" << client->debugName() << ") is unset" << endl;
		return;
	}
	
	CBytestream bs;
	bs.writeByte(C2S_CHATTEXT);
	if (sWormName.size() == 0)
		bs.writeString(sText);
	else if( sText.find("/me ") == 0 && client->getServerVersion() < OLXBetaVersion(0,58,1) )	// "/me " chat command
		bs.writeString( "* " + sWormName + " " + sText.substr(4)); // Add star so clients with empty name won't fake others
	else	// "/me " command is server-sided on Beta9
		bs.writeString(sWormName + ": " + sText);

	client->cNetChan->AddReliablePacketToSend(bs);
}

#ifdef FUZZY_ERROR_TESTING
//////////////////
// Send a random packet to server (used for debugging)
void CClientNetEngine::SendRandomPacket()
{
	// don't send random packets from the local client to our own server
	if( getGameLobby()->iGameType != GME_JOIN ) return;

	CBytestream bs;

	int random_length = GetRandomInt(50);
	for (int i=0; i < random_length; i++)
		bs.writeByte((uchar)GetRandomInt(255));

	client->cNetChan->AddReliablePacketToSend(bs);

	bs.Clear();

	// Test connectionless packets
	if (GetRandomInt(100) >= 75)  {
		bs.writeInt(-1, 4);
		static const std::string commands[] = { "lx::getchallenge", "lx::connect", "lx::ping", "lx::query",
			"lx::getinfo", "lx::wantsjoin", "lx::traverse", "lx::registered"};
		bs.writeString(commands[GetRandomInt(500) % (sizeof(commands)/sizeof(std::string))]);
		for (int i=0; i < random_length; i++)
			bs.writeByte((uchar)GetRandomInt(255));
		SetRemoteNetAddr(client->tSocket, client->cNetChan->getAddress());
		bs.Send(client->tSocket);
	}
}
#endif

void CClientNetEngine::SendGrabBonus(int id, int wormid)
{
	CBytestream bs;
	bs.writeByte(C2S_GRABBONUS);
	bs.writeByte(id);
	bs.writeByte(wormid);
	bs.writeByte(game.wormById(wormid)->getCurrentWeapon());

	client->cNetChan->AddReliablePacketToSend(bs);
}

void CClientNetEngine::SendUpdateLobby(bool ready)
{
	CBytestream bs;
	bs.writeByte(C2S_UPDATELOBBY);
	bs.writeByte(ready);
	client->getChannel()->AddReliablePacketToSend(bs);
}

void CClientNetEngine::SendDisconnect()
{
	CBytestream bs;

	bs.writeByte(C2S_DISCONNECT);

	// Send the pack a few times to make sure the server gets the packet
	if( client->cNetChan != NULL)
		for(int i=0;i<3;i++)
			client->cNetChan->Transmit(&bs);
}

void CClientNetEngine::SendFileData()
{
	CBytestream bs;
	bs.writeByte(C2S_SENDFILE);
	client->getUdpFileDownloader()->send(&bs);
	client->getChannel()->AddReliablePacketToSend(bs);
}

void CClientNetEngineBeta9::QueueReportDamage(int victim, float damage, int offender)
{
	// Buffer up all damage and send it once per 0.1 second for LAN nettype, or once per 0.3 seconds for modem
	std::pair< int, int > dmgPair = std::make_pair( victim, offender );
	if( cDamageReport.count( dmgPair ) == 0 )
		cDamageReport[ dmgPair ] = 0;
	cDamageReport[ dmgPair ] += damage;

	SendReportDamage();
}

void CClientNetEngineBeta9::SendReportDamage(bool flush)
{
	if( ! flush && tLX->currentTime - fLastDamageReportSent < 0.1f * ( NST_LOCAL - client->getNetSpeed() ) )
		return;

	CBytestream bs;

	for( std::map< std::pair< int, int >, float > :: iterator it = cDamageReport.begin(); 
			it != cDamageReport.end(); it++ )
	{
		int victim = it->first.first;
		int offender = it->first.second;
		float damage = it->second;
		bs.writeByte(C2S_REPORTDAMAGE);
		bs.writeByte(victim);
		bs.writeFloat(damage);
		bs.writeByte(offender);
	}

	client->cNetChan->AddReliablePacketToSend(bs);

	cDamageReport.clear();
	fLastDamageReportSent = tLX->currentTime;
}

void CClient::SendGameStateUpdates() {
	if(getStatus() != NET_CONNECTED)
		return;
	if(getServerVersion() < OLXBetaVersion(0,59,10))
		return;
	if(cNetChan->getBufferFull())
		return;

	GameState& state = *serverGameState;
	GameStateUpdates updates;
	updates.diffFromStateToCurrent(state);
	if(!updates) return;

	{
		CBytestream bs;
		bs.writeByte(C2S_GAMEATTRUPDATE);
		updates.writeToBs(&bs, state);
		cNetChan->AddReliablePacketToSend(bs);
	}

	state.updateToCurrent();
}
