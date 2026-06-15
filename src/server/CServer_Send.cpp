/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Server class - Sending
// Created 1/7/02
// Jason Boettcher

#include <vector>
#include <boost/lambda/lambda.hpp>
#include <boost/bind.hpp>

#include "LieroX.h"
#include "CServer.h"
#include "Debug.h"
#include "StringUtils.h"
#include "CServerConnection.h"
#include "CServerNetEngine.h"
#include "Protocol.h"
#include "game/CWorm.h"
#include "Timer.h"
#include "Consts.h"
#include "CChannel.h"
#include "game/CMap.h"
#ifdef DEBUG
#include "MathLib.h"
#endif
#include "CGameMode.h"
#include "game/Mod.h"
#include "game/Game.h"
#include "gusanos/network.h"
#include "game/Level.h"
#include "CGameScript.h"
#include "Utils.h"
#include "game/GameState.h"


// declare them only locally here as nobody really should use them explicitly
std::string OldLxCompatibleString(const std::string &Utf8String);


///////////////////
// Send a client a packet
void CServerNetEngine::SendPacket(CBytestream *bs)
{
	if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE)
		return;

	cl->getChannel()->AddReliablePacketToSend(*bs);
}

///////////////////
// Send all the clients a packet
void GameServer::SendGlobalPacket(CBytestream *bs)
{
	// Assume reliable
	CServerConnection *cl = cClients;
	for(int c = 0; c < MAX_CLIENTS; c++, cl++) {
		if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE) continue;
		if(cl->getNetEngine() == NULL) continue;
		cl->getNetEngine()->SendPacket(bs);
	}
}

void GameServer::SendGlobalPacket(CBytestream *bs, const Version& minVersion)
{
	// Assume reliable
	CServerConnection *cl = cClients;
	for(int c = 0; c < MAX_CLIENTS; c++, cl++) {
		if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE) continue;
		if(cl->getNetEngine() == NULL) continue;
		if(cl->getClientVersion() < minVersion) continue;
		cl->getNetEngine()->SendPacket(bs);
	}
}

// TODO: This function is designed wrong. this->cl should be the receiver
// and the parameter should be the ready client.
void CServerNetEngine::SendClientReady(CServerConnection* receiver) {
	// Let everyone know this client is ready to play
	if(!receiver) {
		for(int i = 0; i < MAX_CLIENTS; ++i) {
			if(cServer->getClients()[i].getStatus() != NET_CONNECTED) continue;
			if(cServer->getClients()[i].getNetEngine() == NULL) continue;
			SendClientReady(&cServer->getClients()[i]);
		}
		return;
	}

	if(receiver->isLocalClient()) return;
	if(receiver->getClientVersion() >= OLXBetaVersion(0,59,10)) return;

	//if(server->serverChoosesWeapons()) {
		// We don't necessarily have to send the weapons, we send them directly
		// (e.g. in cloneWeaponsToAllWorms() or in PrepareWorm()).
		
		// But the client doesn't set CWorm::bGameReady=true, so we still
		// have to send this.
	//}

	if ((receiver && receiver->getClientVersion() >= OLXBetaVersion(0,57,8)) || game.wormsOfClient(cl)->size() <= 2)  {
		CBytestream bytes;
		bytes.writeByte(S2C_CLREADY);
		bytes.writeByte(game.wormsOfClient(cl)->size());
		for_each_iterator(CWorm*, w, game.wormsOfClient(cl)) {
			// Send the weapon info here (also contains id)
			w->get()->writeWeapons(&bytes);
		}
		
		receiver->getNetEngine()->SendPacket(&bytes);
		
	} else { // old client && numworms > 2

		// Note: LX56 assumes that a client can have only 2 worms.
				
		for_each_iterator(CWorm*, w, game.wormsOfClient(cl)) {
			CBytestream bytes;
			bytes.writeByte(S2C_CLREADY);
			bytes.writeByte(1);
			w->get()->writeWeapons(&bytes);
			receiver->getNetEngine()->SendPacket(&bytes);
		}
	}
}

void CServerNetEngine::WritePrepareGame(CBytestream *bs) 
{
	bs->writeByte(S2C_PREPAREGAME);
	bs->writeBool(false);	// random map; Always false as of now
	bs->writeString("levels/" + gameSettings[FT_Map].as<LevelInfo>()->path.get());
	
	// Game info
	bs->writeInt(game.gameMode()->GeneralGameType(),1);
	bs->writeInt16(((int)gameSettings[FT_Lives] < 0) ? WRM_UNLIM : (int)gameSettings[FT_Lives]);
	bs->writeInt16((int)gameSettings[FT_KillLimit]);
	bs->writeInt16((int)(game.gameMode()->TimeLimit() / 60.0f));
	bs->writeInt16((int)gameSettings[FT_LoadingTime]);
	bs->writeBool(gameSettings[FT_Bonuses]);
	bs->writeBool(gameSettings[FT_ShowBonusName]);
	if(game.gameMode()->GeneralGameType() == GMT_TIME)
		bs->writeInt16((int)(float)gameSettings[FT_TagLimit]);
	bs->writeString(gameSettings[FT_Mod].as<ModInfo>()->path);
	
	game.weaponRestrictions()->sendList(bs, game.gameScript()->GetWeaponList());
}

void CServerNetEngine::SendPrepareGame()
{
	CBytestream bs;
	WritePrepareGame(&bs);
	SendPacket( &bs );
}

void CServerNetEngine::SendHideWorm(CWorm *worm, int forworm, bool show, bool immediate)
{
	// For old clients we move the worm out of the map and kill it

	if(!any(game.wormsOfClient(cl))(boost::bind(&CWorm::getID, _1) == forworm))
		// ignore it
		return;
	
	CBytestream bs;

	// Hide the worm
	if (!show)  {
		// 
		// Update the position
		//

		if (game.state == Game::S_Playing)  {
			bs.writeByte(S2C_UPDATEWORMS);
			bs.writeByte(1);  // Worm count
			bs.writeByte(worm->getID());
			bs.write2Int12(-20, -20);  // Position
			bs.writeInt(0, 1);  // Angle
			bs.writeByte(0);  // Flags
			bs.writeByte(0);  // Weapon

			// Velocity
			if(cl->getClientVersion() >= OLXBetaVersion(0,57,5)) {
				bs.writeInt16(0);
				bs.writeInt16(0);
			}

			// Send it reliably, this update is necessary
			SendPacket(&bs);

			//
			// Kill
			//
			SendWormDied(worm);
		}

	// Show the worm
	} else {
		SendSpawnWorm(worm, worm->getPos());
	}
}

void CServerNetEngineBeta7::WritePrepareGame(CBytestream *bs) 
{
	CServerNetEngine::WritePrepareGame(bs);

	bs->writeFloat( gameSettings[FT_GameSpeed] );

	// Set random weapons for spectating client, so it will skip weapon selection screen
	// Never do this for local client, local client must know correct state of serverChoosesWeapons!
	// TODO: it's hacky, don't have any ideas now how to make it nice
	bool spectate = !cl->isLocalClient() && game.wormsOfClient(cl)->size() > 0;
	if(spectate)
		for_each_iterator(CWorm*, w, game.wormsOfClient(cl))
			if(!w->get()->isSpectating()) {
				spectate = false;
				break;
			}

	bs->writeBool( server->serverChoosesWeapons() || spectate );
	
	// We send random weapons from server in GameServer::StartGame()
	// TODO: Where are we doing that?
}

void CServerNetEngineBeta9::WritePrepareGame(CBytestream *bs) 
{
	CServerNetEngineBeta7::WritePrepareGame(bs);

	bs->writeFloat(game.gameMode()->TimeLimit() / 60.0f);

	if(cl->getClientVersion() < OLXBetaVersion(0,59,7))
		// older clients are buggy and depend on this
		WriteFeatureSettings(bs, cl->getClientVersion());
	else
		// We have send the full feature array here. We do this now with an UpdateGameLobby packet seperately.
		// To keep compatibility, just send an empty list.
		bs->writeInt(0, 2);
	
	bs->writeString(game.gameMode()->Name());
	
	// TODO: shouldn't this be somewhere in the clear function?
	cDamageReport.clear(); // In case something left from prev game
}


///////////////////
// Send all the clients a string of text
void GameServer::SendGlobalText(const std::string& text, int type) {
	if(!cClients) {
		errors << "GS:SendGlobalText: clients not initialised" << endl;
		return;
	}

	CServerConnection *cl = cClients;
	for(short c = 0; c < MAX_CLIENTS; c++, cl++) {
		if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE)
			continue;

		cl->getNetEngine()->SendText(text, type);
	}
}


///////////////////
// Send a client a string of text
void CServerNetEngine::SendText(const std::string& text, int type) 
{
	CBytestream bs;

	std::string nohtml_text = StripHtmlTags(text);
		
	std::vector<std::string> split = splitstring(nohtml_text, 63, (game.state == Game::S_Lobby) ? 600 : 300, tLX->cFont);
	
	for (std::vector<std::string>::const_iterator it = split.begin(); it != split.end(); it++)  {
		// Send it
		bs.writeByte(S2C_TEXT);
		bs.writeInt(type, 1);
		bs.writeString(OldLxCompatibleString(*it));
	}
	
	SendPacket(&bs);
}

void CServerNetEngineBeta3::SendText(const std::string& text, int type)
{
	// For beta 3 - beta 7 (no HTML support but unlimited length support)
	CBytestream bs;

	std::string nohtml_text = StripHtmlTags(text);

	bs.writeByte(S2C_TEXT);
	bs.writeInt(type, 1);
	bs.writeString(OldLxCompatibleString(nohtml_text));

	SendPacket(&bs);
}

void CServerNetEngineBeta3::SendHideWorm(CWorm *worm, int forworm, bool show, bool immediate)
{
	CServerNetEngine::SendHideWorm(worm, show, immediate);  // Just the same as for old LX
}

void CServerNetEngineBeta8::SendText(const std::string& text, int type)
{
	// For beta 8+ (HTML support)
	CBytestream bs;

	bs.writeByte(S2C_TEXT);
	bs.writeInt(type, 1);
	bs.writeString(OldLxCompatibleString(text));

	SendPacket(&bs);
}

void CServerNetEngineBeta7::SendChatCommandCompletionSolution(const std::string& startStr, const std::string& solution) {
	CBytestream bs;

	bs.writeByte(S2C_CHATCMDCOMPLSOL);
	bs.writeString(startStr);
	bs.writeString(solution);

	SendPacket(&bs);	
}

void CServerNetEngineBeta7::SendChatCommandCompletionList(const std::string& startStr, const std::list<std::string>& solutions) {
	// the solutions are for the last parameter of the command (or the command itself if no param is given)!
	CBytestream bs;

	bs.writeByte(S2C_CHATCMDCOMPLLST);
	bs.writeString(startStr);
	bs.writeInt((uint)solutions.size(), 4);
	for(std::list<std::string>::const_iterator it = solutions.begin(); it != solutions.end(); ++it)
		bs.writeString(*it);

	SendPacket(&bs);
}

// send S2C_WORMSOUT
void CServerNetEngine::SendWormsOut(const std::list<byte>& ids) {
	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;
	if(ids.size() == 0) return; // ignore
	
	CBytestream bs;
	bs.writeByte(S2C_WORMSOUT);
	bs.writeByte(ids.size());
	
	for(std::list<byte>::const_iterator it = ids.begin(); it != ids.end(); ++it)
		bs.writeByte(*it);
	
	SendPacket(&bs);
}

// WARNING: When using this, be sure that we also drop the specific client. This is
// needed because the local worm amount of the client is different from ours in
// the meanwhile and it would screw up the network.
void GameServer::SendWormsOut(const std::list<byte>& ids) 
{
	for(int c = 0; c < MAX_CLIENTS; c++) {
		CServerConnection* cl = &cClients[c];
		if (cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE)
			continue;
		
		cl->getNetEngine()->SendWormsOut(ids);
	}
}

static float rateForClient(CServerConnection* cl, bool upscaledMax) {
	// Modem, ISDN, LAN, local
	// (Bytes per second)
	const float	Rates[4] = {2500, 7500, 10000, 50000};
	const float	UpscaledRates[4] = {2500, 7500, 100000, 1000000};

	int speed = cl->getNetSpeed();
	if(speed < 0) speed = 0;
	if(speed >= 4) speed = 2;

	if(upscaledMax) return UpscaledRates[speed];
	return Rates[speed];
}

static float rateSumForClients(bool upscaled) {
	float sum = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i) {
		CServerConnection* cl = &cServer->getClients()[i];
		if(!cl->isUsed()) continue;
		if(!cl->isConnected()) continue;
		if(cl->isLocalClient()) continue;
		sum += rateForClient(cl, upscaled);
	}
	return sum;
}

static float maxRateForClient(CServerConnection* cl) {
	float maxUploadSum = cServer->getMaxUploadBandwidth();
	bool upscaled = false;
	while(true) {
		float sum = rateSumForClients(upscaled);
		if(sum >= maxUploadSum) {
			float rate = rateForClient(cl, upscaled);
			rate *= maxUploadSum / sum; // scale down so that the sum fits
			return rate;
		}

		if(!upscaled)
			upscaled = true;
		else
			return rateForClient(cl, upscaled);
	}
	return 0.f;
}

///////////////////
// Update all the client about the playing worms
// Returns true if we sent an update
bool GameServer::SendUpdate()
{
	// Delays for different net speeds
	static const float	shootDelay[] = {0.010f, 0.005f, 0.0f, 0.0f};

	//
	// Get the update packets for each worm that needs it and save them
	//
	std::list<CWorm *> worms_to_update;
	{
		for_each_iterator(CWorm*, w, game.worms()) {
			// HINT: this can happen when a new client joins during game and has not selected weapons yet
			if (w->get()->getClient())
				if (!w->get()->getClient()->getGameReady())
					continue;

			// w is an own server-side copy of the worm-structure,
			// therefore we don't get problems by using the same checkPacketNeeded as client is also using
			if (w->get()->checkPacketNeeded())  {
				worms_to_update.push_back(w->get());
			}
		}
	}

	size_t uploadAmount = 0;

	{
		const int last = lastClientSendData;
		for (int i = 0; i < MAX_CLIENTS; i++)  {
			CServerConnection* cl = &cClients[ (i + last + 1) % MAX_CLIENTS ]; // fairly distribute the packets over the clients

			if (cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE)
				continue;

			// HINT: happens when clients join during game and haven't selected their weapons yet
			// HINT 2: this should be valid for beta 9+ though so the user can see others playing while selecting weapons
			if (cl->getClientVersion() < OLXBetaVersion(0,58,1))
				if (!cl->getGameReady())
					continue;

			// Check if we have gone over the bandwidth rating for the client
			// If we have, just don't send a packet this frame
			if( !checkBandwidth(cl) ) {
				// We have gone over the bandwidth for the client, don't send a message this frame
				//hints << "Over bandwidth for client " << i << endl;
				continue;
			}

			if(!cl->isLocalClient()) {
				// check our server bandwidth
				static Rate<100,5000> blockRate; // only for debug output
				static Rate<100,5000> blockRateAbs; // only for debug output
				blockRateAbs.addData(1);
				if(!checkUploadBandwidth(GetUpload() /* + uploadAmount */)) {
					// we have gone over our own bandwidth for non-local clients				
					blockRate.addData(1);
					static AbsTime lastMessageTime = tLX->currentTime;
					if(tLX->currentTime - lastMessageTime > 30.0) {
						notes << "we got over the max upload bandwidth" << endl;
						notes << "   current upload is " << GetUpload() << " bytes/sec (last 2 secs)" << endl;
						notes << "   current short upload is " << GetUpload(0.1f) << " bytes/sec (last 0.1 sec)" << endl;
						notes << "   upload amount of this frame is " << uploadAmount << " bytes" << endl;
						if(blockRateAbs.getRate() > 0)
							notes << "   current block/update rate is " << float(100.0f * blockRate.getRate() / blockRateAbs.getRate()) << " % (last 5 secs)" << endl;
						lastMessageTime = tLX->currentTime;
					}
					continue;
				}
			}

			if(!game.gameScript()->gusEngineUsed() && cl->getClientVersion() < OLXBetaVersion(0,59,10)) {
				CBytestream update_packets;  // Contains all the update packets except the one from this client

				byte num_worms = 0;

				// Send all the _other_ worms details
				{
					std::list<CWorm*>::const_iterator w_it = worms_to_update.begin();
					for(; w_it != worms_to_update.end(); w_it++) {
						CWorm* w = *w_it;

						// Check if this client owns the worm
						if(cl->OwnsWorm(w->getID()))
							continue;
							
						// Give the game mode a chance to override sending a packet (might reduce data sent)
						if(!game.gameMode()->NeedUpdate(cl, w))
							continue;

						++num_worms;

						CBytestream bytes;
						bytes.writeByte(w->getID());
						w->writePacket(&bytes, true, cl);

						// Send out the update
						update_packets.Append(&bytes);
					}
				}

				CBytestream *bs = cl->getUnreliable();
				size_t oldBsPos = bs->GetPos();

				if(num_worms > 0) {
					// Write the packets to the unreliable bytestream
					bs->writeByte(S2C_UPDATEWORMS);
					bs->writeByte(num_worms);
					bs->Append(&update_packets);
				}
				
				// Write out a stat packet
				{
					bool need_send = false;
					{
						for_each_iterator(CWorm*, w, game.wormsOfClient(cl))
							if (w->get()->checkStatePacketNeeded())  {
								w->get()->updateStatCheckVariables();
								need_send = true;
								break;
							}
					}

					// Only if necessary
					if (need_send)  {
						bs->writeByte( S2C_UPDATESTATS );
						bs->writeByte( game.wormsOfClient(cl)->size() );
						for_each_iterator(CWorm*, w, game.wormsOfClient(cl))
							w->get()->writeStatUpdate(bs);
					}
				}

				if(!cl->isLocalClient())
					uploadAmount += (bs->GetPos() - oldBsPos);
			}
						
			// Send the shootlist (reliable)
			CShootList *sh = cl->getShootList();
			float delay = shootDelay[cl->getNetSpeed()];

			if(tLX->currentTime - sh->getStartTime() > delay && sh->getNumShots() > 0) {
				CBytestream shootBs;

				// Send the shootlist
				if( sh->writePacket(&shootBs, cl->getClientVersion()) )
					sh->Clear();

				if(!cl->isLocalClient())
					uploadAmount += shootBs.GetLength();
					
				cl->getChannel()->AddReliablePacketToSend(shootBs);
			}
			
			// TODO: that doesn't update uploadAmount (but it also doesnt in CClient, to be fair :P)
			cl->getNetEngine()->SendReportDamage();

			if(network.getNetControl()) {
				const size_t maxBytes = (size_t)-1; // TODO ?
				if(maxBytes > 0)
					network.getNetControl()->olxSendNodeUpdates(NetConnID_conn(cl), maxBytes);
			}
			
			lastClientSendData = int(cl - cServer->getClients());
		}		
	}

	// All good
	return true;
}


void GameServer::SendGameStateUpdates() {
	static Rate<100, 1000> counter;
	static Rate<100, 1000> bandwidthHitCounter;

	const int last = lastClientSendData;
	for (int i = 0; i < MAX_CLIENTS; i++)  {
		CServerConnection* cl = &cClients[ (i + last + 1) % MAX_CLIENTS ]; // fairly distribute the packets over the clients
		if(cl->isLocalClient())
			continue;
		if(!cl->isConnected())
			continue;
		if(cl->getClientVersion() < OLXBetaVersion(0,59,10))
			continue;
		if(!checkBandwidth(cl)) {
			if(cl == firstNonlocalClientConnection()) bandwidthHitCounter.addData(1);
			continue;
		}
		if(cl->getChannel()->getBufferFull())
			continue;

		GameState& state = *cl->gameState;
		GameStateUpdates updates;
		updates.diffFromStateToCurrent(state);
		if(!updates) continue;

		{
			CBytestream bs;
			bs.writeByte(S2C_GAMEATTRUPDATE);
			updates.writeToBs(&bs, state);
			cl->getChannel()->AddReliablePacketToSend(bs);
		}

		cl->gameState->updateToCurrent();

		lastClientSendData = int(cl - cServer->getClients());
		if(cl == firstNonlocalClientConnection()) counter.addData(1);
	}

	if(firstNonlocalClientConnection()) {
		CClient::addHudDebugInfo(
					"Client cur/max rate: "
					+ ftoa(firstNonlocalClientConnection()->getChannel()->getOutgoingRate()/1024.f, 1) + " / "
					+ ftoa(maxRateForClient(firstNonlocalClientConnection())/1024.f, 1));
		CClient::addHudDebugInfo("Update FPS: " + ftoa(counter.getRate()));
		CClient::addHudDebugInfo("BandwdthHit FPS: " + ftoa(bandwidthHitCounter.getRate()));
	}
}

void CServerNetEngine::SendWeapons(CWorm* specificW)
{
	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;

	CBytestream bs;
	
	for_each_iterator(CWorm*, _w, game.worms()) {
		CWorm* w = _w->get();
		if(specificW) w = specificW;
		if(w->bWeaponsReady) {
			bs.writeByte(S2C_WORMWEAPONINFO);
			w->writeWeapons(&bs);
		}
		if(specificW) break;
	}
	
	SendPacket(&bs);
}

///////////////////
// Send weapons to the client, or, if client is NULL, send to all clients
void GameServer::SendWeapons(CServerConnection* cl, CWorm* w)
{
	if(cl)
		cl->getNetEngine()->SendWeapons(w);
	else
		for(int c = 0; c < MAX_CLIENTS; c++) {
			CServerConnection* cl = &getClients()[c];

			if(cl->isLocalClient()) continue;
			if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE) continue;
			if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) continue;

			cl->getNetEngine()->SendWeapons(w);
		}
}

///////////////////
// Tells all clients that the worm is now tagged
void GameServer::SendWormTagged(CWorm *w)
{
	// Check
	if (!w)  {
		errors << "A NULL worm passed to GameServer::SendWormTagged" << endl;
		return;
	}

	// Build the packet
	CBytestream bs;
	bs.writeByte(S2C_TAGUPDATE);
	bs.writeInt(w->getID(), 1);
	bs.writeFloat((float)w->getTagTime().seconds());

	// Send
	for(int c = 0; c < MAX_CLIENTS; c++) {
		CServerConnection* cl = &getClients()[c];

		if(cl->isLocalClient()) continue;
		if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE) continue;
		if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) continue;

		cl->getNetEngine()->SendPacket(&bs);
	}
}

///////////////////
// Check if we have gone over the clients bandwidth rating
// Returns true if we are under the bandwidth
bool GameServer::checkBandwidth(CServerConnection *cl)
{
	// Don't bother checking if the client is on the same comp as the server
	if( game.isLocalGame() )
		return true;
	if(cl->getNetSpeed() == 3) // local
		return true;

	// Are we over the clients bandwidth rate?
	if(cl->getChannel()->getOutgoingRate() > maxRateForClient(cl))
		// Don't send the packet
		return false;

	// All ok
	return true;
}

// true means we can send further data
bool GameServer::checkUploadBandwidth(float fCurUploadRate) {
	if( game.isLocalGame() )
		return true;

	float fMaxRate = getMaxUploadBandwidth();
	
	{
		static bool didShowMessageAlready = false;
		if(!didShowMessageAlready)
			notes << "using max upload rate " << (fMaxRate / 1024.0f) << " kb/sec" << endl;
		didShowMessageAlready = true;
	}
	
	return fCurUploadRate < fMaxRate;
}

void CServerNetEngineBeta9::WriteFeatureSettings(CBytestream* bs, const Version& compatVer) {
	size_t ftC = featureArrayLen();
	assert(ftC < 256*256);
	CBytestream bs1;
	int sendCount = 0;
	for_each_iterator( Feature*, f, ArrayRef(featureArray,ftC) )
	{
		if( f->get()->group < GIG_GameModeSpecific_Start ||
			f->get()->group == game.gameMode()->getGameInfoGroupInOptions() )
		{
			if( gameSettings.hostGet(f->get()) == f->get()->unsetValue ) // Do not send a feature if it has default value = LX56 behavior
				continue;

			if(compatVer < OLXBetaVersion(0,59,7))
				if(f->get()->valueType == SVT_CUSTOM) // unsupported on old clients
					continue;

			sendCount ++;
			bs1.writeString( f->get()->name );
			bs1.writeString( f->get()->humanReadableName );
			{
				ScriptVar_t value = gameSettings.hostGet(f->get());
				if(compatVer >= OLXBetaVersion(0,59,10) || value.type <= SVT_COLOR)
					bs1.writeVar( value );
				else {
					// The client does not support the type. Thus convert it to a string.
					ScriptVar_t strValue(value.toString());
					bs1.writeVar( strValue );
				}
			}
			bs1.writeBool( gameSettings.olderClientsSupportSetting(f->get()) );
		}
	}
	bs->writeInt(sendCount, 2);
	bs->Append(&bs1);
}

void CServerNetEngine::SendUpdateLobbyGame()
{
	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;
	CBytestream bs;
	WriteUpdateLobbyGame(&bs);
	SendPacket(&bs);
}

void CServerNetEngine::WriteUpdateLobbyGame(CBytestream *bs)
{
	bs->writeByte(S2C_UPDATELOBBYGAME);
	bs->writeByte(MAX(tLXOptions->iMaxPlayers,(int)game.worms()->size()));  // This fixes the player disappearing in lobby
	bs->writeString(gameSettings[FT_Map].as<LevelInfo>()->path);
	bs->writeString(gameSettings[FT_Mod].as<ModInfo>()->name);
	bs->writeString(gameSettings[FT_Mod].as<ModInfo>()->path);
	bs->writeByte(game.gameMode()->GeneralGameType());
	bs->writeInt16(((int)gameSettings[FT_Lives] < 0) ? WRM_UNLIM : (int)gameSettings[FT_Lives]);
	bs->writeInt16((int)gameSettings[FT_KillLimit]);
	bs->writeInt16((int)gameSettings[FT_LoadingTime]);
	bs->writeByte((bool)gameSettings[FT_Bonuses]);
}

void CServerNetEngineBeta7::WriteUpdateLobbyGame(CBytestream *bs)
{
	CServerNetEngine::WriteUpdateLobbyGame(bs);
	bs->writeFloat(gameSettings[FT_GameSpeed]);
	bs->writeBool(gameSettings[FT_ForceRandomWeapons]);
	bs->writeBool(gameSettings[FT_SameWeaponsAsHostWorm]);
}

void CServerNetEngineBeta9::WriteUpdateLobbyGame(CBytestream *bs)
{
	CServerNetEngineBeta7::WriteUpdateLobbyGame(bs);
	bs->writeFloat(game.gameMode()->TimeLimit() / 60.0f);
	CServerNetEngineBeta9::WriteFeatureSettings(bs, cl->getClientVersion());
	bs->writeString(game.gameMode()->Name());
}


///////////////////
// Send an update of the game details in the lobby
void GameServer::UpdateGameLobby()
{
	if(game.gameMode() == NULL) {
		errors << "Trying to play a non-existant gamemode" << endl;
		gameSettings.overwrite[FT_GameMode].as<GameModeInfo>()->mode = GameMode(GM_DEATHMATCH);
	}
	
	// Read map/mod name from map/mod file
	gameSettings.overwrite[FT_Map].as<LevelInfo>()->name = CMap::GetLevelName(gameSettings[FT_Map].as<LevelInfo>()->path);
	gameSettings.overwrite[FT_Mod].as<ModInfo>()->name = modName(gameSettings[FT_Mod].as<ModInfo>()->path);
	
	m_clientsNeedLobbyUpdate = true;
	m_clientsNeedLobbyUpdateTime = tLX->currentTime;
}

void CServerNetEngine::SendUpdateLobby(CServerConnection *target)
{	
	CServerConnection *cl = server->cClients;
	for(short c=0; c<MAX_CLIENTS; c++,cl++) 
	{
	    if( target )
	    {
    		cl = target;
			if( c != 0 )
				break;
    	}

		if(cl->isLocalClient()) continue;

        if( cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE )
            continue;

		if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) continue;

        // Set the client worms lobby ready state
		for_each_iterator(CWorm*, w, game.wormsOfClient(cl)) {
			CBytestream bytestr;
			bytestr.writeByte(S2C_UPDATELOBBY);
			bytestr.writeByte(1); // worm amount. but we just do it separately because LX56 has problems with it (with >2). it doesn't matter anyway that much
			bytestr.writeByte(w->get()->getLobbyReady());
			bytestr.writeByte(w->get()->getID());
			bytestr.writeByte(w->get()->getTeam());
			SendPacket(&bytestr);
		}		
    }
}

////////////////////////
// Hide a worm at receiver's screen
void CServerNetEngineBeta9::SendHideWorm(CWorm *worm, int forworm, bool show, bool immediate)
{
	worm->setVisibleForWorm(forworm, show);
	if(show && !immediate && cClient->isWormTargetOnAnyViewport(forworm))
		SpawnEntity(ENT_SPAWN,0,worm->getPos(),CVec(0,0),Color(),NULL); // Spawn some sparkles, looks good

	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;

	if (!worm)  {
		errors << "Invalid worm or receiver in SendHideWorm" << endl;
		return;
	}

	if(forworm < 0 || forworm >= MAX_WORMS) {
		errors << "Invalid forworm in SendHideWorm" << endl;
		return;
	}
	
	CBytestream bs;
	bs.writeByte(S2C_HIDEWORM);
	bs.writeByte(worm->getID());
	bs.writeByte(forworm);
	bs.writeBool(!show);  // True - hide, false - show
	bs.writeBool(immediate);  // True - immediate (no animation), false - animation
	SendPacket(&bs);
}

///////////////////
// Send updates for all the worm lobby states
void GameServer::SendWormLobbyUpdate(CServerConnection* receiver, CServerConnection *target)
{
	if(receiver)
		receiver->getNetEngine()->SendUpdateLobby(target);
	else
		for( int i = 0; i < MAX_CLIENTS; i++ ) {
			if(!cClients[i].isUsed()) continue;
			cClients[i].getNetEngine()->SendUpdateLobby(target);
		}
}


///////////////////
// Tell all the clients that we're disconnecting
void GameServer::SendDisconnect()
{
	CServerConnection *cl = cClients;
	if (!cl) // means we already shut down
		return;

	CBytestream bs;
	bs.writeByte(S2C_LEAVING);

	for(short c=0; c<MAX_CLIENTS; c++,cl++) {
		if(cl->getStatus() == NET_DISCONNECTED)
			continue;

		// Send it out-of-bounds 3 times to make sure all the clients received it
		for(short i=0; i<3; i++)
			cl->getChannel()->Transmit(&bs);
	}
}


///////////////////
// Update the worm name, skin, colour etc
void CServerNetEngine::SendUpdateWorm( CWorm* w )
{
	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;
	CBytestream bytestr;
	bytestr.writeByte(S2C_WORMINFO);
	bytestr.writeInt(w->getID(), 1);
	w->writeInfo(&bytestr);
	SendPacket(&bytestr);
}

// Version required to show question mark on damage popup number for older clients
void CServerNetEngineBeta9::SendUpdateWorm( CWorm* w )
{
	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;
	CBytestream bytestr;
	bytestr.writeByte(S2C_WORMINFO);
	bytestr.writeInt(w->getID(), 1);
	w->writeInfo(&bytestr);
	bytestr.writeString(w->getClient()->getClientVersion().asString());
	SendPacket(&bytestr);
}

void GameServer::UpdateWorm(CWorm* w)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		cClients[i].getNetEngine()->SendUpdateWorm(w);
}

///////////////////
// Update the worm names, skins, colours etc
void GameServer::UpdateWorms()
{
	for_each_iterator(CWorm*, w, game.worms())
		UpdateWorm(w->get());
}

#ifdef FUZZY_ERROR_TESTING
///////////////
// Used for testing network stability
void GameServer::SendRandomPacket()
{
	CBytestream bs;
	int random_length = GetRandomInt(50);
	for (int i=0; i < random_length; i++)
		bs.writeByte((uchar)GetRandomInt(255));

	CServerConnection* cl = cClients;
	for(short c=0; c<MAX_CLIENTS; c++,cl++) {
		if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE)
			continue;

		// don't send these random clients to the local client
		if(cl->isLocalClient())
			continue;
		
		cl->SendPacket(&bs);
	}
}
#endif

static const float pingCoeff = 1.5f;	// Send another packet in minPing/pingCoeff milliseconds
static const int minPingDefault = 200;

int CServerNetEngineBeta5::SendFiles()
{
	if(cl->isLocalClient()) return 0;
	if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE)
		return 0;
	int ping = 0;
	// That's a bit floody algorithm, it can be optimized I think
	if( cl->getUdpFileDownloader()->isSending() &&
		( cl->getChannel()->getBufferEmpty() ||
			( ! cl->getChannel()->getBufferFull() &&
			cl->getChannel()->getPing() != 0 &&
			tLX->currentTime - cl->getLastFileRequestPacketReceived() <= cl->getChannel()->getPing()/1000.0f / pingCoeff )) )
	{
		cl->setLastFileRequestPacketReceived( tLX->currentTime );
		CBytestream bs;
		bs.writeByte(S2C_SENDFILE);
		cl->getUdpFileDownloader()->send(&bs);
		cl->getNetEngine()->SendPacket( &bs );
		ping = minPingDefault; // Default assumed ping
		if( cl->getChannel()->getPing() != 0 )
			ping = cl->getChannel()->getPing();
	}
	return ping;
}

void GameServer::SendFiles()
{
	if(!cClients) {
		errors << "GameServer::SendFiles with no clients" << endl;
		return;
	}

	// To keep sending packets if no acknowledge received from client -
	// process will pause for a long time otherwise, 'cause we're in GUI part
	bool startTimer = false;
	int minPing = minPingDefault;

	for(int c = 0; c < MAX_CLIENTS; c++)
	{
		if(!cClients[c].getNetEngine()) continue;
		int ping = cClients[c].getNetEngine()->SendFiles();
		if( ping > 0 )
		{
			startTimer = true;
			if( minPing > ping )
				minPing = ping;
		}
	}

	if( startTimer )
		Timer( "GS::SendFiles", null, NULL, (Uint32)(minPing / pingCoeff), true ).startHeadless();
}

void GameServer::SendEmptyWeaponsOnRespawn( CWorm * Worm )
{
	for( size_t i = 0; i < Worm->tWeapons.size(); i++ )
	{
		Worm->weaponSlots.write()[i].Charge=0.f;
		Worm->weaponSlots.write()[i].Reloading=1;
	}

	if(game.gameScript()->gusEngineUsed()) return;
	
	CServerConnection * cl = Worm->getClient();
	if(cl == NULL) {
		errors << "GS::SendEmptyWeaponsOnRespawn: client of worm " << Worm->getID() << ":" << Worm->getName() << " is unset" << endl;
		DumpConnections();
		return;
	}
	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;

	int curWeapon = Worm->getCurrentWeapon();

	CBytestream bs;
	for( size_t i=0; i<Worm->tWeapons.size(); i++ )
	{
		if( i != (size_t)curWeapon )
		{
			Worm->setCurrentWeapon((int)i);
			bs.writeByte( S2C_UPDATESTATS );
			bs.writeByte( game.wormsOfClient(cl)->size() );
			for_each_iterator(CWorm*, w, game.wormsOfClient(cl) )
				w->get()->writeStatUpdate(&bs);
		}
	}
	Worm->setCurrentWeapon(curWeapon);
	bs.writeByte( S2C_UPDATESTATS );
	bs.writeByte( game.wormsOfClient(cl)->size() );
	for_each_iterator(CWorm*, w, game.wormsOfClient(cl) )
		w->get()->writeStatUpdate(&bs);
	cl->getNetEngine()->SendPacket(&bs);
}

void CServerNetEngine::SendSpawnWorm(CWorm *Worm, CVec pos)
{
	Worm->setSpawnedOnce();
	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;

	CBytestream bs;
	bs.writeByte(S2C_SPAWNWORM);
	bs.writeInt(Worm->getID(), 1);
	bs.writeInt( (int)pos.x, 2);
	bs.writeInt( (int)pos.y, 2);

	SendPacket(&bs);
}

void CServerNetEngine::SendWormDied(CWorm *Worm)
{
	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;
	if(game.gameScript()->gusEngineUsed())
		// we don't use this in Gusanos
		return;
	
	CBytestream bs;
	bs.writeByte(S2C_WORMDOWN);
	bs.writeInt(Worm->getID(), 1);

	SendPacket(&bs);
}

void CServerNetEngine::SendWormScore(CWorm *Worm)
{
	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;

	CBytestream bs;
	bs.writeByte(S2C_SCOREUPDATE);
	bs.writeInt(Worm->getID(), 1);
	bs.writeInt16(Worm->getLives());

	// Overflow hack
	if (Worm->getScore() > 255)
		bs.writeInt(255, 1);
	else
		bs.writeInt(Worm->getScore() > 0 ? Worm->getScore() : 0, 1);

	SendPacket(&bs);
}

void CServerNetEngineBeta9::SendWormScore(CWorm *Worm)
{
	// If we have some damage reports in buffer send them first so clients won't sum up updated damage score and reported damage packet sent later
	SendReportDamage(true);

	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;

	CBytestream bs;
	bs.writeByte(S2C_SCOREUPDATE);
	bs.writeInt(Worm->getID(), 1);
	bs.writeInt16(Worm->getLives());	// Still int16 to allow WRM_OUT parsing (maybe I'm wrong though)
	bs.writeInt(Worm->getScore(), 4); // Negative kills are allowed
	bs.writeFloat(Worm->getDamage());

	SendPacket(&bs);
}

void CServerNetEngineBeta9::QueueReportDamage(int victim, float damage, int offender)
{
	if(cl->isLocalClient()) return;
	// Buffer up all damage and send it once per 0.1 second for LAN nettype, or once per 0.3 seconds for modem
	std::pair< int, int > dmgPair = std::make_pair( victim, offender );
	if( cDamageReport.count( dmgPair ) == 0 )
		cDamageReport[ dmgPair ] = 0;
	cDamageReport[ dmgPair ] += damage;
	
	SendReportDamage();
}

void CServerNetEngineBeta9::SendReportDamage(bool flush)
{
	if(cl->isLocalClient()) {
		cDamageReport.clear();
		return;
	}
	if( ! flush && tLX->currentTime - fLastDamageReportSent < 0.1f * ( NST_LOCAL - cl->getNetSpeed() ) )
		return;

	CBytestream bs;

	for( std::map< std::pair< int, int >, float > :: iterator it = cDamageReport.begin(); 
			it != cDamageReport.end(); it++ )
	{
		int victim = it->first.first;
		int offender = it->first.second;
		float damage = it->second;
		bs.writeByte(S2C_REPORTDAMAGE);
		bs.writeByte(victim);
		bs.writeFloat(damage);
		bs.writeByte(offender);
	}

	SendPacket(&bs);

	cDamageReport.clear();
	fLastDamageReportSent = tLX->currentTime;
}

void CServerNetEngineBeta9::SendTeamScoreUpdate() {
	if(cl->isLocalClient()) return;
	// only do this in a team game
	if(game.gameMode()->GeneralGameType() != GMT_TEAMS) return;

	CBytestream bs;
	bs.writeByte(S2C_TEAMSCOREUPDATE);
	bs.writeByte(game.gameMode()->GameTeams());
	for(int i = 0; i < game.gameMode()->GameTeams(); ++i) {
		bs.writeInt16(game.gameMode()->TeamScores(i));
	}	
	SendPacket(&bs);
}

void GameServer::SendTeamScoreUpdate() {
	for(int c = 0; c < MAX_CLIENTS; c++) {
		if(cClients[c].getStatus() == NET_CONNECTED)
			cClients[c].getNetEngine()->SendTeamScoreUpdate();
	}
}

void CServerNetEngine::SendWormProperties(bool onlyIfNotDef) {
	if(cl->isLocalClient()) return;
	for_each_iterator(CWorm*, w, game.worms()) {
		if(onlyIfNotDef && isWormPropertyDefault(w->get())) continue;		
		SendWormProperties(w->get());
	}
}

void CServerNetEngine::SendWormProperties(CWorm* worm) {
	if(cl->isLocalClient()) return;	
	if(isWormPropertyDefault(worm)) return; // ok, don't give a warning in that case
	
	warnings << "SendWormProperties cannot be used for clients with <Beta9 (" << cl->debugName() << ")" << endl;
}

void CServerNetEngineBeta9::SendWormProperties(CWorm* worm) {
	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;

	CBytestream bs;
	bs.writeByte(S2C_SETWORMPROPS);
	bs.writeByte(worm->getID());
	bs.writeBit(worm->canUseNinja());
	bs.writeBit(worm->canAirJump());
	bs.writeFloat(worm->speedFactor());
	bs.writeFloat(worm->damageFactor());
	bs.writeFloat(worm->shieldFactor());
	SendPacket(&bs);
}

bool CServerNetEngine::isWormPropertyDefault(CWorm* worm) {
	// defaults are set in CWorm::Prepare
	return
		worm->speedFactor() == 1.0f &&
		worm->damageFactor() == 1.0f &&
		worm->shieldFactor() == 1.0f &&
		worm->canUseNinja() &&
		!worm->canAirJump();
}

void CServerNetEngine::SendSelectWeapons(CWorm* worm) {
	warnings << "SendSelectWeapons not supported for " << cl->debugName() << endl;
}

void CServerNetEngineBeta9::SendSelectWeapons(CWorm* worm) {
	if(cl->isLocalClient()) return;
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,10)) return;

	CBytestream bs;
	bs.writeByte(S2C_SELECTWEAPONS);
	bs.writeByte(worm->getID());
	SendPacket(&bs);
}

void GameServer::SetWormSpeedFactor(int wormID, float f) {
	CWorm* w = game.wormById(wormID, false);
	if(!w) {
		warnings << "SetWormSpeedFactor: worm " << wormID << " is invalid" << endl;
		return;
	}
	
	if(w->speedFactor() == f) return; // nothing need to be changed
	
	w->setSpeedFactor(f);
	
	for(int c = 0; c < MAX_CLIENTS; c++) {
		if(cClients[c].getStatus() == NET_CONNECTED)
			cClients[c].getNetEngine()->SendWormProperties(w);
	}	
}

void GameServer::SetWormCanUseNinja(int wormID, bool b) {
	CWorm* w = game.wormById(wormID, false);
	if(!w) {
		warnings << "SetWormCanUseNinja: worm " << wormID << " is invalid" << endl;
		return;
	}
	
	if(w->canUseNinja() == b) return; // nothing need to be changed
	
	w->setCanUseNinja(b);
	
	for(int c = 0; c < MAX_CLIENTS; c++) {
		if(cClients[c].getStatus() == NET_CONNECTED)
			cClients[c].getNetEngine()->SendWormProperties(w);
	}		
}

void GameServer::SetWormDamageFactor(int wormID, float f) {
	CWorm* w = game.wormById(wormID, false);
	if(!w) {
		warnings << "SetWormDamageFactor: worm " << wormID << " is invalid" << endl;
		return;
	}
	
	if(w->damageFactor() == f) return; // nothing need to be changed
	
	w->setDamageFactor(f);
	
	for(int c = 0; c < MAX_CLIENTS; c++) {
		if(cClients[c].getStatus() == NET_CONNECTED)
			cClients[c].getNetEngine()->SendWormProperties(w);
	}
}

void GameServer::SetWormShieldFactor(int wormID, float f) {
	CWorm* w = game.wormById(wormID, false);
	if(!w) {
		warnings << "SetWormDamageFactor: worm " << wormID << " is invalid" << endl;
		return;
	}
	
	if(w->shieldFactor() == f) return; // nothing need to be changed
	
	w->setShieldFactor(f);
	
	for(int c = 0; c < MAX_CLIENTS; c++) {
		if(cClients[c].getStatus() == NET_CONNECTED)
			cClients[c].getNetEngine()->SendWormProperties(w);
	}
}

void GameServer::SetWormCanAirJump(int wormID, bool b) {
	CWorm* w = game.wormById(wormID, false);
	if(!w) {
		warnings << "SetWormCanAirJump: worm " << wormID << " is invalid" << endl;
		return;
	}
	
	if(w->canAirJump() == b) return; // nothing need to be changed
	
	w->setCanAirJump(b);
	
	for(int c = 0; c < MAX_CLIENTS; c++) {
		if(cClients[c].getStatus() == NET_CONNECTED)
			cClients[c].getNetEngine()->SendWormProperties(w);
	}		
}


void CServerNetEngine::SendPlaySound(const std::string& name) {
	if(cl->getClientVersion() >= OLXBetaVersion(0,59,1)) {
		CBytestream bs;
		bs.writeByte(S2C_PLAYSOUND);
		bs.writeString(name);
		SendPacket(&bs);		
	}
}

void GameServer::SendPlaySound(const std::string& name) {
	CServerConnection* cl = cClients;
	for(short c=0; c<MAX_CLIENTS; c++,cl++) {
		if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE) continue;
		if(cl->getNetEngine() == NULL) continue;
		cl->getNetEngine()->SendPlaySound(name);
	}
}



bool GameServer::CanWormHandleClientSideRespawn(CWorm* w) {
	CServerConnection* cl = w->getClient();
	if(cl == NULL) { // shouldn't happen
		errors << "GS::CanWormHandleClientSideRespawn: client of worm " << w->getID() << ":" << w->getName() << " is unset" << endl;
		return false;
	}
	return cl->getClientVersion() >= OLXBetaVersion(0,59,10);
}

