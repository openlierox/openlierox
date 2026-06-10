/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie, Albert Zeyer and Martin Griffin
//
//
/////////////////////////////////////////


// Server class - Parsing
// Created 1/7/02
// Jason Boettcher



#include "LieroX.h"
#include "FindFile.h"
#include "CServer.h"
#include "ProfileSystem.h"
#include "DeprecatedGUI/Menu.h"
#include "CServerConnection.h"
#include "CServerNetEngine.h"
#include "CChannel.h"
#include "StringUtils.h"
#include "CWorm.h"
#include "Protocol.h"
#include "ConfigHandler.h"
#include "ChatCommand.h"
#include "DedicatedControl.h"
#include "AuxLib.h"
#include "Version.h"
#include "Timer.h"
#include "NotifyUser.h"
#include "XMLutils.h"
#include "CClientNetEngine.h"
#include "IpToCountryDB.h"
#include "Debug.h"
#include "CGameMode.h"
#include "FlagInfo.h"
#include "WeaponDesc.h"
#include "Autocompletion.h"
#include "Command.h"
#include "TaskManager.h"


#ifdef _MSC_VER
#undef min
#undef max
#endif


// TODO: move this out here after we have moved all the Send/Parse stuff
// declare them only locally here as nobody really should use them explicitly
std::string OldLxCompatibleString(const std::string &Utf8String);
std::string Utf8String(const std::string &OldLxString);


int CServerNetEngine::getConnectionArrayIndex() {
	if(!cl) {
		errors << "CServerNetEngine::getConnectionArrayIndex: connection not set" << endl;
		return -1;
	}
	
	if(cl->getNetEngine() != this) {
		errors << "CServerNetEngine::getConnectionArrayIndex: net engine not consistent" << endl;
		return -1;
	}
	
	return cl->getConnectionArrayIndex();
}


/*
=======================================
		Connected Packets
=======================================
*/

///////////////////
// Parse a packet
void CServerNetEngine::ParsePacket(CBytestream *bs) {

	// TODO: That's hack, all processing should be done in CChannel_056b
	// Maybe server will work without that code at all, should check against 0.56b
	CChannel *chan = cl->getChannel();
	if( chan != NULL )
	{
		chan->recheckSeqs();
	}

	cl->setLastReceived(tLX->currentTime);


	// Do not process empty packets
	if (bs->isPosAtEnd())
		return;

#if defined(DEBUG) || !defined(FUZZY_ERROR_TESTING_C2S)
	typedef std::list<CBytestream> BSList;
	BSList bss;
#endif
	
	uchar cmd;

	while (!bs->isPosAtEnd()) {
#ifdef DEBUG
		size_t startPos = bs->GetPos();
#endif
		cmd = bs->readInt(1);

		switch (cmd) {

			// Client is ready
		case C2S_IMREADY:
			ParseImReady(bs);
			break;

			// Update packet
		case C2S_UPDATE:
			ParseUpdate(bs);
			break;

			// Death
		case C2S_DEATH:
			ParseDeathPacket(bs);
			break;

			// Chat text
		case C2S_CHATTEXT:
			ParseChatText(bs);
			break;

		case C2S_CHATCMDCOMPLREQ:
			ParseChatCommandCompletionRequest(bs);	
			break;

		case C2S_AFK:
			ParseAFK(bs);
			break;

			// Update lobby
		case C2S_UPDATELOBBY:
			ParseUpdateLobby(bs);
			break;

			// Disconnect
		case C2S_DISCONNECT:
			ParseDisconnect();
			break;

			// Bonus grabbed
		case C2S_GRABBONUS:
			ParseGrabBonus(bs);
			break;

		case C2S_SENDFILE:
			ParseSendFile(bs);
			break;

		case C2S_REPORTDAMAGE:
			ParseReportDamage(bs);
			break;

		default:
			// HACK, HACK: old olx/lxp clients send the ping twice, once normally once per channel
			// which leads to warnings here - we simply parse it here and avoid warnings

			/*
			It's a bug in LieroX Pro that sent the ping
			packet twice: once per CChannel - it looked like this:
			(8 bytes of CChannel's sequence)(Connectionless header)(Packet itself)
			and then the same packet normally (via CBytestream::Send)
			(Connectionless header) (Packet itself)

			The hack solved the first case which generated warning of unknown packet.
			*/

			// Avoid "reading from stream behind end" warning if this is really a bad packet
			// and print the bad command instead
			if (cmd == 0xff && bs->GetRestLen() > 3) {
				if (bs->readInt(3) == 0xffffff) {
					std::string address;
					NetAddrToString(cl->getChannel()->getAddress(), address);
					server->ParseConnectionlessPacket(cl->getChannel()->getSocket(), bs, address);
					break;
				}
				bs->revertByte(); bs->revertByte(); bs->revertByte();
			}

			// Really a bad packet
#if !defined(FUZZY_ERROR_TESTING_C2S)
			warnings << "sv: Bad command in packet (" << itoa(cmd) << ") from " << cl->debugName() << endl;
			if(cl->isLocalClient()) {
				notes << "Bad package from local client" << endl;
				for(BSList::iterator i = bss.begin(); i != bss.end(); ++i) {
					notes << "already parsed packet:" << endl;
					i->Dump();
				}
				notes << "full bytestream:" << endl;
				bs->revertByte(); // to have the cmd-byte the marked byte in the dump 
				bs->Dump();
				bs->readByte(); // skip the bad cmd-byte
			}
#endif
			
			// The stream is screwed up and we should ignore the rest.
			// In most cases, we would get further bad command errors but
			// sometimes we would parse wrongly some invalid stuff.
			bs->SkipAll();
		}
		
#ifdef DEBUG
		if(!bs->isPosAtEnd())
			bss.push_back( CBytestream( bs->getRawData(startPos, bs->GetPos()) ) );
#endif
	}
}


///////////////////
// Parse a 'im ready' packet
void CServerNetEngine::ParseImReady(CBytestream *bs) {
	if ( server->iState == SVS_LOBBY )
	{
		notes << "GameServer::ParseImReady: Not waiting for ready, packet is being ignored." << endl;

		// Skip to get the correct position in the stream
		int num = bs->readByte();
		for (int i = 0;i < num;i++)  {
			bs->Skip(1);
			CWorm::skipWeapons(bs);
		}

		return;
	}

	int i, j;
	// Note: This isn't a lobby ready

	// Read the worms weapons
	int num = bs->readByte();
	if(server->serverChoosesWeapons() && num > 0) {
		// It's only a note because <Beta9 clients will do that wrong anyway.
		notes << "ParseImReady: " << cl->debugName() << " wants to set own weapons but we have serverChoosesWeapons" << endl;		
	}
	for (i = 0; i < num; i++) {
		if(bs->isPosAtEnd()) {
			warnings << "ParseImReady: packaged screwed" << endl;
			break;
		}
		int id = bs->readByte();
		if (id >= 0 && id < MAX_WORMS)  {
			if(!server->cWorms[id].isUsed()) {
				warnings << "ParseImReady: got unused worm-ID!" << endl;
				CWorm::skipWeapons(bs);
				continue;
			}
			if(!cl->OwnsWorm(id)) {
				warnings << "ParseImReady: " << cl->debugName() << " wants to update the weapons of worm " << id << endl;
				CWorm::skipWeapons(bs);
				continue;
			}
			if(server->serverChoosesWeapons()) {
				// we already made a warning about that
				CWorm::skipWeapons(bs);
				continue;
			}
			notes << "Server:ParseImReady: ";
			server->cWorms[id].readWeapons(bs);
			for (j = 0; j < 5; j++) {
				if(server->cWorms[id].getWeapon(j)->Weapon)
					server->cWorms[id].getWeapon(j)->Enabled =
						server->cWeaponRestrictions.isEnabled(server->cWorms[id].getWeapon(j)->Weapon->Name) ||
						server->cWeaponRestrictions.isBonus(server->cWorms[id].getWeapon(j)->Weapon->Name);
			}
			
		} else { // wrong id -> Skip to get the right position
			warnings << "ParseImReady: got wrong worm-ID!" << endl;
			CWorm::skipWeapons(bs);
		}
	}


	// Set this client to 'ready'
	notes << "Server: client " << cl->debugName() << " got ready" << endl;
	cl->setGameReady(true);

	SendClientReady(NULL);
	
	if(server->iState == SVS_PLAYING)
		server->BeginMatch(cl);
	else if(tLXOptions->tGameInfo.features[FT_ImmediateStart])
		server->BeginMatch(cl);
	else
		// Check if all the clients are ready
		server->CheckReadyClient();
}


///////////////////
// Parse an update packet
void CServerNetEngine::ParseUpdate(CBytestream *bs) {
	for (short i = 0; i < cl->getNumWorms(); i++) {
		CWorm *w = cl->getWorm(i);

		bool wasShootingBefore = w->getWormState()->bShoot;
		const weapon_t* oldWeapon = (w->getCurWeapon() && w->getCurWeapon()->Enabled) ? w->getCurWeapon()->Weapon : NULL;
		w->readPacket(bs, server->cWorms);

		if(server->iState == SVS_PLAYING) {
			// If the worm is shooting, handle it
			if (w->getWormState()->bShoot && w->getAlive())
				server->WormShoot(w); // handle shot and add to shootlist to send it later to the clients
			
			// handle FinalProj for weapon
			if(oldWeapon && ((wasShootingBefore && !w->getWormState()->bShoot) || (wasShootingBefore && oldWeapon != w->getCurWeapon()->Weapon)))
				server->WormShootEnd(w, oldWeapon);
		}
	}
}


///////////////////
// Parse a death packet
void CServerNetEngine::ParseDeathPacket(CBytestream *bs) {
	// No kills in lobby
	if (server->iState != SVS_PLAYING)  {
		notes << "GameServer::ParseDeathPacket: Not playing, ignoring the packet." << endl;

		// Skip to get the correct position in the stream
		bs->Skip(2);

		return;
	}

	int victim = bs->readInt(1);
	int killer = bs->readInt(1);

	// Bad packet
	if (bs->GetPos() > bs->GetLength())  {
		warnings << "GameServer::ParseDeathPacket: Reading beyond the end of stream." << endl;
		return;
	}

	// If the game is already over, ignore this
	if (server->bGameOver)  {
		notes("GameServer::killWorm: Game is over, ignoring.\n");
		return;
	}
	// Safety check
	if (victim < 0 || victim >= MAX_WORMS)  {
		warnings("GameServer::killWorm: victim ID out of bounds.\n");
		return;
	}
	if (killer < 0 || killer >= MAX_WORMS)  {
		warnings("GameServer::killWorm: killer ID out of bounds.\n");
		return;
	}

	if (tLXOptions->tGameInfo.bServerSideHealth)  {
		// Cheat prevention check (God Mode etc), make sure killer is the host or the packet is sent by the client owning the worm
		if (!cl->isLocalClient())  {
			if (cl->OwnsWorm(victim))  {  // He wants to die, let's fulfill his dream ;)
				CWorm *w = cClient->getRemoteWorms() + victim;
				if (!w->getAlreadyKilled())  // Prevents killing the worm twice (once by server and once by the client itself)
					cClient->getNetEngine()->SendDeath(victim, killer);
			} else {
				warnings << "GameServer::ParseDeathPacket: victim " << victim << " is not one of the clients (" << cl->debugName(true) << ") worms, killer was " << killer << endl;
				server->netError = "ParseDeathPacket with invalid victim + SSH";
			}

			// The client on this machine will send the death again, then we'll parse it
			return;
		}
	} else {
		// Cheat prevention check: make sure the victim is one of the client's worms
		if (!cl->OwnsWorm(victim))  {
			warnings << "GameServer::ParseDeathPacket: victim " << victim << " is not one of the clients (" << cl->debugName(true) << "), killer was " << killer << endl;
			server->netError = "ParseDeathPacket with invalid victim";
			return;
		}
	}

	// A small hack: multiple-suiciding can lag down the game, check if
	// the packet contains another death (with same killer and victim), if so, just
	// increase the suicide variable and proceed
	if (killer == victim)  {
		server->iSuicidesInPacket++;
		if (!bs->isPosAtEnd())  {
			if (bs->peekByte() == C2S_DEATH)  {
				std::string s = bs->peekData(3);
				if (s.size() == 3)  {
					s.erase(s.begin()); // The C2S_DEATH byte
					if (((uchar)s[0] == (uchar)victim) && ((uchar)s[1] == (uchar)killer))
						return;
				}
			}
		}
	}

	server->killWorm(victim, killer, server->iSuicidesInPacket);
	
	server->iSuicidesInPacket = 0; // Reset counter, so next /suicide command will work correctly
}


///////////////////
// Parse a chat text packet
void CServerNetEngine::ParseChatText(CBytestream *bs) {
	std::string buf = Utf8String(bs->readString());

	if(cl->getNumWorms() == 0 && !cl->isLocalClient()) {
		warnings << cl->debugName() << " with no worms sends message '" << buf << "'" << endl;
		return;
	}

	if (buf.empty()) { // Ignore empty messages
		warnings << cl->debugName() << " sends empty message" << endl;
		return;
	}
	
	//Check message length.
	//OLX seems to allow sending arbitrary long chat messages.
	//These messages can be a problem as they can cause excessive lag.
	//TODO: Limit message length client side too?
	if ((int)buf.size() > tLXOptions->iMaxChatMessageLength) {
		buf.resize(tLXOptions->iMaxChatMessageLength);
	}
	
	
	// TODO: is it correct that we check here only for worm 0 ?
	// TODO: should we perhaps also check, if the beginning of buf is really the correct name?

	std::string command_buf = buf;
	{
		std::string startStr = (cl->getWorm(0) ? cl->getWorm(0)->getName() : "") + ": ";
		if( strStartsWith(buf, startStr) )
			command_buf = buf.substr(startStr.size());  // Special buffer used for parsing special commands (begin with /)
	}
		
	// Check for special commands
	if (command_buf.size() > 2)
		if (command_buf[0] == '/' && command_buf[1] != '/')  {  // When two slashes at the beginning, parse as a normal message
			ParseChatCommand(command_buf);
			return;
		}

	//Previously, OLX logged all chat to the main log when hosting a server.
	//It clutters the log so now log only if configured to do so.
	//NOTE: This has nothing to do with the "Log conversations" option - it goes to different log!
	if (tLXOptions->bLogServerChatToMainlog) {
		if(!strStartsWith(command_buf, "!login") && !strStartsWith(command_buf, "//login"))	// people could try wrong chat command
			notes << "CHAT: " << buf << endl;
	}
	
	// Check for Clx (a cheating version of lx)
	if(buf[0] == 0x04) {
		server->SendGlobalText(cl->debugName() + " seems to have CLX or some other hack", TXT_NORMAL);
	}

	// Don't send text from muted players
	if (cl->getMuted()) {
		notes << "ignored message from muted " << cl->debugName() << endl;
		return;
	}

	if( !cl->isLocalClient() ) {
		// Check if player tries to fake other player
		bool nameMatch = false;
		for( int i=0; i<cl->getNumWorms(); i++ )
			if( buf.find(cl->getWorm(i)->getName() + ": ") == 0 )
				nameMatch = true;
		if( !nameMatch )
		{
			notes << "Client " << cl->debugName() << " probably tries to fake other player, or an old client uses /me cmd" << endl;
			return;
		}
	}
	
	server->SendGlobalText(buf, TXT_CHAT);

	if( DedicatedControl::Get() && buf.size() > cl->getWorm(0)->getName().size() + 2 )
		DedicatedControl::Get()->ChatMessage_Signal(cl->getWorm(0),buf.substr(cl->getWorm(0)->getName().size() + 2));
}

void CServerNetEngineBeta7::ParseChatCommandCompletionRequest(CBytestream *bs) {
	std::list<std::string> possibilities;
	
	std::string startStr = bs->readString();
	TrimSpaces(startStr);
	std::vector<std::string> cmdStart = ParseCommandMessage("/" + startStr, false);
	
	if(cmdStart.size() > 1) {
		ChatCommand* cmd = GetCommand(cmdStart[0]);
		if(!cmd) {
			SendText("Chat auto completion: unknown command", TXT_NETWORK);
			return;
		}
		
		// TODO: Move it out here and make it more general (add it to ChatCommand structure).
		if(cmd->tProcFunc == &ProcessSetVar && cmdStart.size() == 2) {
			for(CScriptableVars::const_iterator it = CScriptableVars::lower_bound(cmdStart[1]); it != CScriptableVars::Vars().end(); ++it) {
				// ignore callbacks
				if(it->second.var.type == SVT_CALLBACK) continue;
				
				if( subStrCaseEqual(cmdStart[1], it->first, cmdStart[1].size()) ) {
					std::string nextComplete = it->first.substr(0, cmdStart[1].size());
					for(size_t f = cmdStart[1].size();; ++f) {
						if(f >= it->first.size()) { nextComplete += ' '; break; }
						nextComplete += it->first[f];
						if(it->first[f] == '.') break;
					}
					
					if(possibilities.size() == 0 || *possibilities.rbegin() != nextComplete) {
						possibilities.push_back(nextComplete);
					}
				}
				else
					break;
			}

			if(possibilities.size() == 0) {
				SendText("Chat auto completion: unknown variable", TXT_NETWORK);
				return;
			}

			if(possibilities.size() == 1) {
				cl->getNetEngine()->SendChatCommandCompletionSolution(startStr, cmd->sName + " " + possibilities.front());
				return;
			}

			size_t l = maxStartingEqualStr(possibilities);
			if(l > cmdStart[1].size()) {
				// we can complete to some longer sequence
				cl->getNetEngine()->SendChatCommandCompletionSolution(startStr, cmd->sName + " " + possibilities.front().substr(0, l));
				return;
			}
			
			// send list of all possibilities
			size_t startSugPos = 0;
			for(size_t p = 0; p < cmdStart[1].size(); ++p)
				if(cmdStart[1][p] == '.') {
					startSugPos = p + 1;
				}
			std::list<std::string> possNew;
			while(possibilities.size() > 0) {
				possNew.push_back(possibilities.front().substr(startSugPos));
				possibilities.pop_front();
			}
			cl->getNetEngine()->SendChatCommandCompletionList(startStr, possNew);
			return;
		} // end autocomplete for setvar
		
		// some hacks to autocomplete also some more commands
		if(cmd->tProcFunc == &ProcessLevel) {
			if(!cl->getRights()->ChooseLevel) {
				SendText("You don't have the right to change the map", TXT_NETWORK);
				return;
			}
			cmd = GetCommand("ded");
			cmdStart.insert(cmdStart.begin(), "ded");
			cmdStart[1] = "map";
		}

		if(cmd->tProcFunc == &ProcessMod) {
			if(!cl->getRights()->ChooseMod) {
				SendText("You don't have the right to change the mod", TXT_NETWORK);
				return;
			}
			cmd = GetCommand("ded");
			cmdStart.insert(cmdStart.begin(), "ded");
			cmdStart[1] = "mod";
		}
		
		// autocomplete for dedicated
		if(cmd->tProcFunc == &ProcessDedicated) {
			std::string cmdToBeCompleted;
			if(cmdStart.size() >= 2) cmdToBeCompleted = cmdStart[1]; // first is 'ded', second is ded-cmd
			for(size_t i = 2; i < cmdStart.size(); ++i) {
				cmdToBeCompleted += " \"" + cmdStart[i];
				if(i != cmdStart.size() - 1) cmdToBeCompleted += "\"";
			}
			
			struct AutoCompleter : Task {
				AutocompletionInfo info;
				std::string oldChatCmd;
				std::string cmdToBeCompleted;
				struct Conn {
					int connectionIndex; CServerNetEngine* cl;
					Conn(int i, CServerNetEngine* c) : connectionIndex(i), cl(c) {}
				} conn;
				
				struct CLI : CmdLineIntf {
					Conn conn;
					CLI(const Conn& c) : conn(c) {}
					
					virtual void pushReturnArg(const std::string& str) {}
					virtual void finalizeReturn() {}
					virtual void writeMsg(const std::string& msg, CmdLineMsgType type) {
						// we need to do that from the gameloopthread
						mainQueue->push(new MsgSender(conn.connectionIndex, conn.cl, msg));
					}

				} cli;
				
				AutoCompleter(const std::string& old, const std::string& cmd, int i, CServerNetEngine* c)
				: oldChatCmd(old), cmdToBeCompleted(cmd), conn(Conn(i,c)), cli(Conn(i,c)) {
					name = "Chat command autocompleter";
				}
				
				struct AutocompleteSender : Action {
					int connectionIndex;
					CServerNetEngine* cl;
					AutocompleteSender(int i, CServerNetEngine* c) : connectionIndex(i), cl(c) {}
					bool checkValid() {
						if(!cServer || !cServer->isServerRunning() || !cServer->getClients()) return false;
						if(!cServer->getClients()[connectionIndex].getNetEngine()) return false;
						if(cl != cServer->getClients()[connectionIndex].getNetEngine()) return false;
						if(cServer->getClients()[connectionIndex].getStatus() == NET_DISCONNECTED) return false;
						if(cServer->getClients()[connectionIndex].getStatus() == NET_ZOMBIE) return false;
						return true;
					}
				};
				
				struct SuggestionSender : AutocompleteSender {
					std::string request;
					std::string solution;
					SuggestionSender(int i, CServerNetEngine* c, const std::string& req, const std::string& sol)
					: AutocompleteSender(i,c), request(req), solution(sol) {}
					int handle() {
						if(!checkValid()) return -1;
						cl->SendChatCommandCompletionSolution(request, solution);
						return 0;
					}
				};

				struct MsgSender : AutocompleteSender {
					std::string msg;
					MsgSender(int i, CServerNetEngine* c, const std::string& m) : AutocompleteSender(i,c), msg(m) {}
					int handle() {
						if(!checkValid()) return -1;
						cl->SendText(msg, TXT_NOTICE);
						return 0;
					}
				};
								
				int handle() {
					AutoComplete(cmdToBeCompleted, cmdToBeCompleted.size(), cli, info);
					bool fail = false;
					AutocompletionInfo::InputState replace;
					info.popWait( AutocompletionInfo::InputState(cmdToBeCompleted), replace, fail );
					std::string replaceStr = replace.text;
					// another hack to have shorter completion
					if(!strStartsWith(replaceStr, "map ") && !strStartsWith(replaceStr, "mod "))
						replaceStr = "ded " + replaceStr;
					if(replaceStr != "" && replaceStr[replaceStr.size()-1] == '\"')
						replaceStr.erase(replaceStr.size()-1);
					if(!fail)
						// we need to do that from the gameloopthread
						mainQueue->push(new SuggestionSender(conn.connectionIndex, conn.cl, oldChatCmd, replaceStr));						
					return 0;
				}
			};
			
			int connectionIndex = getConnectionArrayIndex();
			if(connectionIndex < 0) return;
			
			taskManager->start(new AutoCompleter(startStr, cmdToBeCompleted, connectionIndex, this), true);
			return;
		} // end autocomplete for ded
		   
		return;
	}
	std::string match;
	if(cmdStart.size() >= 1)
		match = cmdStart[0];
	stringlwr(match);
	
	for (uint i=0; tKnownCommands[i].tProcFunc != NULL; ++i) {
		if(subStrEqual(match, tKnownCommands[i].sName, match.size()))
			possibilities.push_back(tKnownCommands[i].sName);
		else if(subStrEqual(match, tKnownCommands[i].sAlias, match.size()))
			possibilities.push_back(tKnownCommands[i].sAlias);
	}
	
	if(possibilities.size() == 0) {
		SendText("Chat auto completion: unknown command", TXT_NETWORK);
		return;
	}
	
	if(possibilities.size() == 1) {
		// we have exactly one solution
		ChatCommand* cmd = GetCommand(startStr);
		if(cmd && startStr.size() <= possibilities.front().size()) {
			SendText("Chat auto completion: " + cmd->sName + ": " +
					 itoa(cmd->iMinParamCount) + "-" + itoa(cmd->iMaxParamCount) + " params",
					 TXT_NETWORK);		   
		}
		else
			cl->getNetEngine()->SendChatCommandCompletionSolution(startStr, possibilities.front() + " ");
		return;
	}
	
	size_t l = maxStartingEqualStr(possibilities);
	if(l > startStr.size()) {
		// we can complete to some longer sequence
		cl->getNetEngine()->SendChatCommandCompletionSolution(startStr, possibilities.front().substr(0, l));
		return;
	}
	
	// send list of all possibilities
	cl->getNetEngine()->SendChatCommandCompletionList(startStr, possibilities);
}

void CServerNetEngineBeta7::ParseAFK(CBytestream *bs) {

	int wormid = bs->readByte();
	int afkType = bs->readByte();
	std::string message = bs->readString(128);
	if (wormid < 0 || wormid >= MAX_WORMS)
		return;
	
	CWorm *w = &server->cWorms[wormid];
	if( ! w->isUsed() || w->getClient() != cl )
		return;
	
	w->setAFK( (AFK_TYPE)afkType, message );

	CBytestream bs1;
	bs1.writeByte( S2C_AFK );
	bs1.writeByte( wormid );
	bs1.writeByte( afkType );
	bs1.writeString( message );
	
	CServerConnection *cl1;
	int i;
	for( i=0, cl1=server->cClients; i < MAX_CLIENTS; i++, cl1++ )
		if( cl1->getStatus() == NET_CONNECTED && cl1->getClientVersion() >= OLXBetaVersion(7) )
			cl1->getNetEngine()->SendPacket( &bs1 );
}


///////////////////
// Parse a 'update lobby' packet
void CServerNetEngine::ParseUpdateLobby(CBytestream *bs) {
	// Must be in lobby
	if ( server->iState != SVS_LOBBY )  {
		notes << "GameServer::ParseUpdateLobby: Not in lobby." << endl;

		// Skip to get the right position
		bs->Skip(1);

		return;
	}

	bool ready = bs->readBool();
	int i;

	// Set the client worms lobby ready state
	for (i = 0; i < cl->getNumWorms(); i++) {
		cl->getWorm(i)->setLobbyReady( ready );
	}
	
	// Let all the worms know about the new lobby state
	for( int i=0; i<MAX_CLIENTS; i++ )
		server->cClients[i].getNetEngine()->SendUpdateLobby();
}


///////////////////
// Parse a disconnect packet
void CServerNetEngine::ParseDisconnect() {
	// Check if the client hasn't already left
	if (cl->getStatus() == NET_DISCONNECTED)  {
		notes << "GameServer::ParseDisconnect: Client has already disconnected." << endl;
		return;
	}

	// Host cannot leave...
	if (cl->isLocalClient())  {
		if(cClient->getStatus() != NET_DISCONNECTED) {
			warnings << "we got disconnect-package from local client but local client is not disconnected" << endl;
			return; // ignore
		}
		
		/* There are several cases (e.g. in ParsePrepareGame) where we would
		 * just disconnect, even as the local client. It's hard to catch all
		 * these possible cases and in most cases, we have a screwed up
		 * network stream with the client.
		 * For that reason, we just try to reconnect it now.
		 */
		warnings << "host-client disconnected, reconnecting now ..." << endl;
		cClient->Reconnect();
		
		return;
	}

	server->DropClient(cl, CLL_QUIT, "client disconnected");
}



///////////////////
// Parse a 'grab bonus' packet
void CServerNetEngine::ParseGrabBonus(CBytestream *bs) {
	int id = bs->readByte();
	int wormid = bs->readByte();
	int curwpn = bs->readByte();

	// Check
	if (server->iState != SVS_PLAYING)  {
		notes << "GameServer::ParseGrabBonus: Not playing." << endl;
		return;
	}


	// Worm ID ok?
	if (wormid >= 0 && wormid < MAX_WORMS) {
		CWorm *w = &server->cWorms[wormid];

		// Bonus id ok?
		if (id >= 0 && id < MAX_BONUSES) {
			CBonus *b = &server->cBonuses[id];

			if (b->getUsed()) {

				// If it's a weapon, change the worm's current weapon
				if (b->getType() == BNS_WEAPON) {

					if (curwpn >= 0 && curwpn < w->getNumWeaponSlots()) {
						wpnslot_t *wpn = w->getWeapon(curwpn);
						const weapon_t* oldWeapon = wpn->Weapon;
						if(b->getWeapon() >= 0 && b->getWeapon() < server->cGameScript->GetNumWeapons()) {
							wpn->Weapon = server->cGameScript.get()->GetWeapons() + b->getWeapon();
							wpn->Enabled = true;
							wpn->Charge = 1;
							wpn->Reloading = false;
						}
						else {
							notes << "worm " << w->getID() << " selected invalid bonus weapon" << endl;
							wpn->Weapon = NULL;
							wpn->Enabled = false;
						}
						
						// handle worm shoot end if needed
						if(oldWeapon && wpn->Weapon != oldWeapon && w->getWormState()->bShoot)
							server->WormShootEnd(w, oldWeapon);
						if (wpn->Weapon != oldWeapon) {
							for ( int i = 0; i < MAX_CLIENTS; i++ ) {
								if ( !server->getClients()[i].isConnected() || w->getClient() == &(server->getClients()[i]) )
									continue;
								server->SendWeapons(&(server->getClients()[i])); // Tell other clients about weapons of this worm, otherwise laser will not be drawn
							}
						}
					}
				}

				// Tell all the players that the bonus is now gone
				CBytestream bs;
				bs.writeByte(S2C_DESTROYBONUS);
				bs.writeByte(id);
				server->SendGlobalPacket(&bs);
				
				if( b->getType() == BNS_HEALTH && server->getClient(w->getID())->getClientVersion() < OLXBetaVersion(0,58,1) )
					for( int i=0; i < MAX_CLIENTS; i++ )
						if( server->cClients[i].isConnected() )
							server->cClients[i].getNetEngine()->QueueReportDamage( w->getID(), -30, w->getID() ); // It's random between 10-50 actually, we're doing approximation here
			} else {
				notes << "GameServer::ParseGrabBonus: Bonus already destroyed." << endl;
			}
		} else {
			notes << "GameServer::ParseGrabBonus: Invalid bonus ID" << endl;
		}
	} else {
		notes << "GameServer::ParseGrabBonus: invalid worm ID" << endl;
	}
}

void CServerNetEngine::ParseSendFile(CBytestream *bs)
{
	cl->setLastFileRequestPacketReceived( AbsTime() ); // Set time in the past to force sending next packet
	if( cl->getUdpFileDownloader()->receive(bs) )
	{
		if( cl->getUdpFileDownloader()->isFinished() &&
			( cl->getUdpFileDownloader()->getFilename() == "GET:" || cl->getUdpFileDownloader()->getFilename() == "STAT:" ) )
		{
			cl->getUdpFileDownloader()->abortDownload();	// We can't provide that file or statistics on it
		}
	}
}

/////////////////
// Parse a command from chat
bool CServerNetEngine::ParseChatCommand(const std::string& message)
{
	// Parse the message
	const std::vector<std::string>& parsed = ParseCommandMessage(message, false);

	// Invalid
	if (parsed.size() == 0)
		return false;
	
	// Get the command
	ChatCommand *cmd = GetCommand(parsed[0]);
	if (!cmd)  {
		SendText("The command is not supported.", TXT_NETWORK);
		return false;
	}

	// Check the params
	size_t num_params = parsed.size() - 1;
	if (num_params < cmd->iMinParamCount || num_params > cmd->iMaxParamCount)  {
		SendText("Invalid parameter count.", TXT_NETWORK);
		return false;
	}
	
	
	//Previously, OLX logged all private messages and team chat to the main log when hosting a server.
	//It clutters the log and can be considered a privacy violation.
	//So now log only if configured to do so.
	//NOTE: This has nothing to do with the "Log conversations" option - it goes to a different log!
	//Other commands except login are logged as previously.
	if ((cmd->tProcFunc != &ProcessLogin) && ((cmd->tProcFunc != &ProcessPrivate) ||
		(tLXOptions->bLogServerChatToMainlog)) && ((cmd->tProcFunc != &ProcessTeamChat) || (tLXOptions->bLogServerChatToMainlog)))
		notes << "ChatCommand from " << cl->debugName(true) << ": " << message << endl;
	
	// Get the parameters
	std::vector<std::string> parameters = std::vector<std::string>(parsed.begin() + 1, parsed.end());

	// Process the command
	std::string error = cmd->tProcFunc(parameters, (cl->getNumWorms() > 0) ? cl->getWorm(0)->getID() : -1);
	if (error.size() != 0)  {
		SendText(error, TXT_NETWORK);
		notes << "ChatCommand " << cmd->sName << " returned error: " << error << endl;
		return false;
	}

	return true;
}

void CServerNetEngineBeta9::ParseReportDamage(CBytestream *bs)
{
	int id = bs->readByte();
	float damage = bs->readFloat();
	int offenderId = bs->readByte();

	if( server->iState != SVS_PLAYING )
		return;

	if( id < 0 || id >= MAX_WORMS || offenderId < 0 || offenderId >= MAX_WORMS )
		return;

	CWorm *w = & server->getWorms()[id];
	CWorm *offender = & server->getWorms()[offenderId];
	
	if( ! w->isUsed() || ! offender->isUsed() )
		return;
	
	if( ! cl->OwnsWorm(id) && ! cl->isLocalClient() )	// Allow local client to send damage for pre-Beta9 clients
		return;
	
	offender->addDamage( damage, w, tLXOptions->tGameInfo );

	//notes << "CServerNetEngineBeta9::ParseReportDamage() offender " << offender->getID() << " dmg " << damage << " victim " << id << endl;
	// Re-send the packet to all clients, except the sender
	for( int i=0; i < MAX_CLIENTS; i++ )
		if( server->cClients[i].getStatus() == NET_CONNECTED && (&server->cClients[i]) != cl )
			server->cClients[i].getNetEngine()->QueueReportDamage( w->getID(), damage, offender->getID() );
}

/*
===========================

  Connectionless packets

===========================
*/


///////////////////
// Parses connectionless packets
void GameServer::ParseConnectionlessPacket(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs, const std::string& ip) {
	std::string cmd = bs->readString(128);

	if (cmd == "lx::getchallenge")
		ParseGetChallenge(tSocket, bs);
	else if (cmd == "lx::connect")
		ParseConnect(tSocket, bs);
	else if (cmd == "lx::ping")
		ParsePing(tSocket);
	else if (cmd == "lx::time") // request for cServer->fServertime
		ParseTime(tSocket);
	else if (cmd == "lx::query")
		ParseQuery(tSocket, bs, ip);
	else if (cmd == "lx::getinfo")
		ParseGetInfo(tSocket);
	else if (cmd == "lx::wantsjoin")
		ParseWantsJoin(tSocket, bs, ip);
	else if (cmd == "lx::traverse")
		ParseTraverse(tSocket, bs, ip);
	else if (cmd == "lx::registered")
		ParseServerRegistered(tSocket, bs);
	else  {
		warnings << "GameServer::ParseConnectionlessPacket: unknown packet \"" << cmd << "\"" << endl;
		bs->SkipAll(); // Safety: ignore any data behind this unknown packet
	}
}


///////////////////
// Handle a "getchallenge" msg
void GameServer::ParseGetChallenge(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs_in) {
	int			i;
	NetworkAddr	adrFrom;
	AbsTime		OldestTime = AbsTime::Max();
	int			ChallengeToSet = -1;
	CBytestream	bs;

	//hints << "Got GetChallenge packet" << endl;

	adrFrom = tSocket->remoteAddress();

	std::string client_version;
	if( ! bs_in->isPosAtEnd() )
		client_version = bs_in->readString(128);

	if( Version(client_version).isBanned() ) {
		// TODO: move this out here
		bs.writeInt(-1, 4);
		bs.writeString("lx::badconnect");
		bs.writeString("Your " + client_version + " support was dropped, please download a new version at http://openlierox.net/");
		bs.Send(tSocket);
		notes << "GameServer::ParseGetChallenge: client has version " + client_version + " which is not supported." << endl;
		return;
	}

	if( tLXOptions->bForceCompatibleConnect ) {
		std::string incompReason;
		if(!isVersionCompatible(Version(client_version), &incompReason)) {
			// TODO: move this out here
			bs.writeInt(-1, 4);
			bs.writeString("lx::badconnect");
			bs.writeString("Your " + client_version + " is incompatible with the server, please download a new version at http://openlierox.net/.\n"
						   "Incompatibility reason: " + incompReason);
			bs.Send(tSocket);
			notes << "GameServer::ParseGetChallenge: client has incompatible version " << client_version << ": " << incompReason << endl;
			return;			
		}
	}
	
	// If were in the game, deny challenges
	if ( iState != SVS_LOBBY && !serverAllowsConnectDuringGame() ) {
		// TODO: move this out here
		bs.writeInt(-1, 4);
		bs.writeString("lx::badconnect");
		bs.writeString(OldLxCompatibleString(networkTexts->sGameInProgress));
		bs.Send(tSocket.get());
		notes << "GameServer::ParseGetChallenge: Client from " << tSocket->debugString() << " cannot join, the game is in progress." << endl;
		return;
	}
	
	// see if we already have a challenge for this ip
	for (i = 0;i < MAX_CHALLENGES;i++) {

		if (IsNetAddrValid(tChallenges[i].Address)) {
			if (AreNetAddrEqual(adrFrom, tChallenges[i].Address))
				continue;
			if (ChallengeToSet < 0 || tChallenges[i].fTime < OldestTime) {
				OldestTime = tChallenges[i].fTime;
				ChallengeToSet = i;
			}
		} else {
			ChallengeToSet = i;
			break;
		}
	}

	if (ChallengeToSet >= 0) {

		// overwrite the oldest
		tChallenges[ChallengeToSet].iNum = (rand() << 16) ^ rand();
		tChallenges[ChallengeToSet].Address = adrFrom;
		tChallenges[ChallengeToSet].fTime = tLX->currentTime;
		tChallenges[ChallengeToSet].sClientVersion = client_version;

		i = ChallengeToSet;
	}

	// Send the challenge details back to the client
	tSocket->setRemoteAddress(adrFrom);


	// TODO: move this out here
	bs.writeInt(-1, 4);
	bs.writeString("lx::challenge");
	bs.writeInt(tChallenges[i].iNum, 4);
	if( client_version != "" )
		bs.writeString(GetFullGameName());
	bs.Send(tSocket.get());
}


///////////////////
// Handle a 'connect' message
void GameServer::ParseConnect(const SmartPointer<NetworkSocket>& net_socket, CBytestream *bs) {
	NetworkAddr		adrFrom;
	int				p;
	int				numplayers;
	CServerConnection	*newcl = NULL;

	//hints << "Got Connect packet" << endl;

	// Connection details
	int		ProtocolVersion;
	//int		Port = LX_PORT;
	int		ChallId;
	int		iNetSpeed = 0;


	// Ignore if we are playing (the challenge should have denied the client with a msg)
	if ( !serverAllowsConnectDuringGame() && iState != SVS_LOBBY )  {
//	if (iState == SVS_PLAYING) {
		notes << "GameServer::ParseConnect: In game, ignoring." << endl;
		return;
	}
	
	// User Info to get
	adrFrom = net_socket->remoteAddress();
	std::string addrFromStr;
	NetAddrToString(adrFrom, addrFromStr);

	// Check if this ip isn't already connected
	// HINT: this works in every case, even if there are two people behind one router
	// because the address ports are checked as well (router assigns a different port for each person)
	CServerConnection* reconnectFrom = NULL;
	p = 0;
	for(CServerConnection* cl = cClients;p<MAX_CLIENTS;p++,cl++) {
		
		if(cl->getStatus() == NET_DISCONNECTED)
			continue;
		
		if(AreNetAddrEqual(adrFrom, cl->getChannel()->getAddress())) {
			reconnectFrom = cl;
			break;
		}
	}			
	
	
	// Read packet
	ProtocolVersion = bs->readInt(1);
	if (ProtocolVersion != PROTOCOL_VERSION) {
		hints << "Wrong protocol version " << ProtocolVersion << ", server protocol version is " << int(PROTOCOL_VERSION) << endl;

		// Get the string to send
		std::string buf;
		if (networkTexts->sWrongProtocol != "<none>")  {
			replacemax(networkTexts->sWrongProtocol, "<version>", itoa(PROTOCOL_VERSION), buf, 1);
		} else
			buf = " ";

		// Wrong protocol version, don't connect client
		CBytestream bytestr;
		bytestr.writeInt(-1, 4);
		bytestr.writeString("lx::badconnect");
		bytestr.writeString(OldLxCompatibleString(buf));
		bytestr.Send(net_socket.get());
		hints << "GameServer::ParseConnect: Wrong protocol version" << endl;
		return;
	}

	std::string szAddress;
	NetAddrToString(adrFrom, szAddress);

	// Is this IP banned?
	if (getBanList()->isBanned(szAddress))  {
		hints << "Banned client " << szAddress << " was trying to connect" << endl;
		CBytestream bytestr;
		bytestr.writeInt(-1, 4);
		bytestr.writeString("lx::badconnect");
		bytestr.writeString(OldLxCompatibleString(networkTexts->sYouAreBanned));
		bytestr.Send(net_socket.get());
		return;
	}

	//Port = pack->ReadShort();
	ChallId = bs->readInt(4);
	iNetSpeed = bs->readInt(1);

	// Make sure the net speed is within bounds, because it's used for indexing
	iNetSpeed = CLAMP(iNetSpeed, 0, 3);

	// Get user info
	int numworms = bs->readInt(1);
	
	//Block attempts to join without any "worms" - this fixes the "empty name join glitch"
	//NOTE: The local client should we allowed to connect without worms in dedicated mode??
	//NOTE: Is this the correct/best way to do it?
	if (numworms<=0 && !(addrFromStr.find("127.0.0.1")==0)) {
		CBytestream sKickmsg;
		sKickmsg.writeInt(-1, 4);
		sKickmsg.writeString("lx::badconnect");
		sKickmsg.writeString(OldLxCompatibleString("Connection failed - you must have a name"));
		sKickmsg.Send(net_socket.get());
		return;
	}
	
	
	numworms = CLAMP(numworms, 0, (int)MAX_PLAYERS);
	
	Version clientVersion;
	
	// If we ignored this challenge verification, there could be double connections

	// See if the challenge is valid
	{
		bool valid_challenge = false;
		int i;
		for (i = 0; i < MAX_CHALLENGES; i++) {
			if (IsNetAddrValid(tChallenges[i].Address) && AreNetAddrEqual(adrFrom, tChallenges[i].Address)) {

				if (ChallId == tChallenges[i].iNum)  { // good
					SetNetAddrValid(tChallenges[i].Address, false); // Invalidate it here to avoid duplicate connections
					tChallenges[i].iNum = 0;
					valid_challenge = true;
					break;
				} else { // bad
					valid_challenge = false;

					// HINT: we could receive another connect packet which will contain this challenge
					// and therefore get the worm connected twice. To avoid it, we clear the challenge here
					SetNetAddrValid(tChallenges[i].Address, false);
					tChallenges[i].iNum = 0;
					if(!reconnectFrom) {
						hints << "HINT: deleting a doubled challenge" << endl;
					}
					
					// There can be more challanges from one client, if this one doesn't match,
					// perhaps some other does
				}
			}
		}

		// Ran out of challenges
		if (!reconnectFrom && i == MAX_CHALLENGES ) {
			notes << "No connection verification for client found" << endl;
			CBytestream bytestr;
			bytestr.writeInt(-1, 4);
			bytestr.writeString("lx::badconnect");
			bytestr.writeString(OldLxCompatibleString(networkTexts->sNoIpVerification));
			bytestr.Send(net_socket.get());
			return;
		}

		if (!reconnectFrom && !valid_challenge)  {
			notes << "Bad connection verification of client" << endl;
			CBytestream bytestr;
			bytestr.writeInt(-1, 4);
			bytestr.writeString("lx::badconnect");
			bytestr.writeString(OldLxCompatibleString(networkTexts->sBadVerification));
			bytestr.Send(net_socket.get());
			return;
		}
				
		if(reconnectFrom)
			clientVersion = reconnectFrom->getClientVersion();
		else
			clientVersion = tChallenges[i].sClientVersion;
	}


	newcl = NULL;
	if(reconnectFrom) {
		// HINT: just let the client connect again
		newcl = reconnectFrom;
		notes << "Reconnecting ";
		if(reconnectFrom->isLocalClient()) {
			bLocalClientConnected = false; // because we are reconnecting the client
			notes << "local ";
		}
		notes << "client " << reconnectFrom->debugName() << endl;
		
		if(reconnectFrom->isLocalClient() && addrFromStr.find("127.0.0.1") != 0) {
			errors << "client cannot be the local client because it has address " << addrFromStr << endl;
			bLocalClientConnected = true; // we just assume that
			reconnectFrom->setLocalClient(false);
		}
				
		/*
		 // Must not have got the connection good packet
		 if(cl->getStatus() == NET_CONNECTED) {
		 hints << "Duplicate connection" << endl;
		 
		 // Resend the 'good connection' packet
		 bytestr.Clear();
		 bytestr.writeInt(-1,4);
		 bytestr.writeString("lx::goodconnection");
		 // Send the worm ids
		 for( int i=0; i<cl->getNumWorms(); i++ )
		 bytestr.writeInt(cl->getWorm(i)->getID(), 1);
		 bytestr.Send(net_socket);
		 return;
		 }
		 
		 // TODO: does this make sense? the already connected client tries to connect again while playing?
		 // Trying to connect while playing? Drop the client
		 if(cl->getStatus() == NET_PLAYING) {
		 //conprintf("Client tried to reconnect\n");
		 //DropClient(&players[p]);
		 return;
		 }
		 */		
	}

	// Find a spot for the client
	p=0;
	if(newcl == NULL)
		for (CServerConnection* cl = cClients; p < MAX_CLIENTS; p++, cl++) {
			if (cl->getStatus() == NET_DISCONNECTED) {
				newcl = cl;
				break;
			}
		}

	
	// Calculate number of current players on this server
	numplayers = 0;
	for (p = 0; p < MAX_WORMS; p++) {
		if (cWorms[p].isUsed())
			numplayers++;
	}
	if(numplayers != iNumPlayers)
		warnings << "WARNING: stored player count " << iNumPlayers << " is different from recalculated player count " << numplayers << endl;

	// Ran out of slots
	if (!newcl) {
		warnings << "I have no more open slots for the new client" << endl;
		notes << GetDateTime() << " - Server Error report" << endl;
		notes << "currentTime is " << (tLX->currentTime - AbsTime()).seconds() << " Numplayers is " << numplayers << endl;
		std::string msg;

		CBytestream bytestr;
		bytestr.writeInt(-1, 4);
		bytestr.writeString("lx::badconnect");
		bytestr.writeString(OldLxCompatibleString(networkTexts->sNoEmptySlots));
		bytestr.Send(net_socket.get());
		return;
	}

	// Server full (maxed already, or the number of extra worms wanting to join will go over the max)
	int max_players = (tLX->iGameType == GME_HOST ? tLXOptions->tGameInfo.iMaxPlayers : MAX_WORMS); // No limits (almost) for local play
	if (!newcl->isLocalClient() && numplayers + numworms > max_players) {
		notes << "I am full, so the new client cannot join" << endl;
		CBytestream bytestr;
		bytestr.writeInt(-1, 4);
		bytestr.writeString("lx::badconnect");
		bytestr.writeString(OldLxCompatibleString(networkTexts->sServerFull));
		bytestr.Send(net_socket.get());
		return;
	}


	// Connect
	if (!newcl)
		return;

	// TODO: this is a bad hack, fix it
	// If this is the first client connected, it is our local client
	if (!bLocalClientConnected)  {
		if (addrFromStr.find("127.0.0.1") == 0)  { // Safety: check the IP
			newcl->setLocalClient(true);
			bLocalClientConnected = true;
			if(!reconnectFrom) // don't spam too much
				notes << "GameServer: our local client has connected" << endl;
		}
	} else {
		newcl->setLocalClient(false);
	}

	if( newcl->getChannel() ) {
		// Note: do it also for reconnecting clients as reconnecting detection could be wrong
		// TODO: It seems that this happens very often. Why?
		//warnings << "ParseConnect: new client has old channel set" << endl;
		newcl->resetChannel();
	}

	if( newcl->getChannel() == NULL) { 
		if(! newcl->createChannel( std::min(clientVersion, GetGameVersion() ) ) )
		{	// This should not happen - just in case
			errors << "Cannot create CChannel for client - invalid client version " << clientVersion.asString() << endl;
			CBytestream bytestr;
			bytestr.writeInt(-1, 4);
			bytestr.writeString("lx::badconnect");
			bytestr.writeString(OldLxCompatibleString("Your client is incompatible to this server"));
			bytestr.Send(net_socket.get());
			return;
		}

		newcl->getChannel()->Create(adrFrom, net_socket);
	}
	
	newcl->setLastReceived(tLX->currentTime);
	newcl->setNetSpeed(iNetSpeed);

	newcl->setClientVersion( clientVersion );

	newcl->setStatus(NET_CONNECTED);

	if(newcl->getNetEngine()) {
		// Note: do it also for reconnecting clients as reconnecting detection could be wrong
		// TODO: It seems that this happens very often. Why?
		//warnings << "ParseConnect: old net engine was still set" << endl;
		newcl->resetNetEngine();
	}
	if(!newcl->getNetEngine())
		newcl->setNetEngineFromClientVersion(); // Deletes CServerNetEngine instance
	// WARNING/HINT/TODO: If we'll ever move ParseConnect() to CServerNetEngine this func should be called last, 'cause *this is deleted

	if(!reconnectFrom)
		newcl->getRights()->Nothing();  // Reset the rights here
	
	// check version compatibility already earlier to not add/kick bots if not needed
	if( iState != SVS_LOBBY ) {
		std::string msg;
		if(!checkVersionCompatibility(newcl, false, false, &msg)) {
			notes << "ParseConnect: " << newcl->debugName(false) << " is too old: " << msg << endl;
			CBytestream bytestr;
			bytestr.writeInt(-1, 4);
			bytestr.writeString("lx::badconnect");
			bytestr.writeString(OldLxCompatibleString("Your OpenLieroX version is too old, please update.\n" + msg));
			bytestr.Send(net_socket.get());
			RemoveClient(newcl, "version too old (while connecting)");
			return;
		}
	}
	
	
	
	// prepare Welcome message
	std::string strWelcomeMessage = tLXOptions->sWelcomeMessage;
	if(strWelcomeMessage == "<none>") strWelcomeMessage = "";
	if(strWelcomeMessage != "")  {
		
		// Server name3
		replacemax(strWelcomeMessage, "<server>", tLXOptions->sServerName, strWelcomeMessage, 1);
		
		// Host name
		replacemax(strWelcomeMessage, "<me>", cWorms[0].getName(), strWelcomeMessage, 1);
		
		// Version
		replacemax(strWelcomeMessage, "<version>", clientVersion.asHumanString(), strWelcomeMessage, 1);
		
		// Country
		bool hasDist = strWelcomeMessage.find("<distance>") != std::string::npos;
		if (strWelcomeMessage.find("<country>") != std::string::npos || 
				strWelcomeMessage.find("<city>") != std::string::npos ||
				strWelcomeMessage.find("<continent>") != std::string::npos ||
				hasDist)  {

			IpInfo info, localInfo;
			float dist = 0;
			std::string str_addr;
			NetAddrToString(newcl->getChannel()->getAddress(), str_addr);
			if (str_addr != "")  {
				info = tIpToCountryDB->GetInfoAboutIP(str_addr);
				if (hasDist && sExternalIP.size())  {
					localInfo = tIpToCountryDB->GetInfoAboutIP(sExternalIP);
					dist = tIpToCountryDB->GetDistance(localInfo, info);
					replace(strWelcomeMessage, "<distance>", ftoa(dist, 0), strWelcomeMessage);
				}
					
				replace(strWelcomeMessage, "<country>", info.countryName, strWelcomeMessage);
				replace(strWelcomeMessage, "<continent>", info.continent, strWelcomeMessage);
				replace(strWelcomeMessage, "<city>", info.city, strWelcomeMessage);
			}
		}
		
		// Address
		std::string str_addr;
		NetAddrToString(newcl->getChannel()->getAddress(), str_addr);
		// Remove port
		size_t pos = str_addr.rfind(':');
		if (pos != std::string::npos)
			str_addr.erase(pos);
		replacemax(strWelcomeMessage, "<ip>", str_addr, strWelcomeMessage, 1);
	}
	
	
	
	// Find spots in our list for the worms
	int ids[MAX_PLAYERS];
	for(int i = 0; i < MAX_PLAYERS; ++i) ids[i] = -1;
	
	std::vector<WormJoinInfo> newWorms;
	newWorms.resize(numworms);
	for (int i = 0; i < numworms; i++) {
		newWorms[i].readInfo(bs);

		// If bots aren't allowed, disconnect the client
		if (newWorms[i].m_type == PRF_COMPUTER && !tLXOptions->bAllowRemoteBots && !strincludes(szAddress, "127.0.0.1"))  {
			hints << "Bot was trying to connect from " << newcl->debugName() << endl;
			CBytestream bytestr;
			bytestr.writeInt(-1, 4);
			bytestr.writeString("lx::badconnect");
			bytestr.writeString(OldLxCompatibleString(networkTexts->sBotsNotAllowed));
			bytestr.Send(net_socket.get());
			
			RemoveClient(newcl, "bot tried to connect");
			return;
		}
		
		//Check name length - by using tricks it's possible (at least in 0.58) to create excessively long names
		//which cause lots of annoyance. This server-side check prevents that and truncates oversized nicks.
		//Because the player creation menu allows max 20 characters, we can check it very easily.
		//Player name does not allow Unicode characters for backward compatibility
		if (newWorms[i].sName.size() > MAX_WORM_NAME_LENGTH)
			newWorms[i].sName.resize(MAX_WORM_NAME_LENGTH);
	}

	std::set<CWorm*> removeWormList;
	
	if(reconnectFrom) {
		for(int i = 0; i < reconnectFrom->getNumWorms(); ++i) {
			CWorm* w = reconnectFrom->getWorm(i);
			if(!w) {
				warnings << "ParseConnect with reconnecting client: worm nr " << i << " from client is unset" << endl;
				continue;
			}
			if(!w->isUsed()) {
				warnings << "ParseConnect with reconnecting client: worm nr " << i << " from client is not used" << endl;
				continue;
			}
			
			bool found = false;
			for(int j = 0; j < numworms; ++j) {
				// compare by name, we have no possibility to do it more exact but it's also not that important
				if(ids[j] < 0 && newWorms[j].sName == w->getName()) {
					// found one
					found = true;
					ids[j] = w->getID();
					// HINT: Don't apply the other information from WormJoinInfo,
					// the worm should be up-to-date and we could screw it up (e.g. skin or team).
					break;
				}
			}
			
			if(!found) {
				notes << "reconnecting client " << newcl->debugName(false) << " doesn't have worm ";
				notes << w->getID() << ":" << w->getName() << " anymore, removing that worm globally" << endl;
				removeWormList.insert(w);
			}
		}
	}
	
	if(removeWormList.size() > 0) {
		RemoveClientWorms(newcl, removeWormList, "reconnected " + newcl->debugName(false) + " and worm does not exist anymore");
	}

	std::set<CWorm*> newJoinedWorms;
	
	// search slots for new worms
	for (int i = 0; i < numworms; ++i) {
		if(ids[i] >= 0) continue; // this worm is already associated
		
		CWorm* w = AddWorm(newWorms[i]);
		if(w == NULL) {
			warnings << "Server:Connect: cannot add " << i << "th worm for " << newcl->debugName(false) << endl;
			break;
		}
		
		w->setClient(newcl);
		ids[i] = w->getID();
		newJoinedWorms.insert(w);

		hints << "Worm joined: " << w->getName();
		hints << " (id " << w->getID() << ",";
		hints << " from " << newcl->debugName(false) << ")" << endl;
		
		// "Has connected" message
		if (networkTexts->sHasConnected != "<none>" && networkTexts->sHasConnected != "")  {
			SendGlobalText(Replace(networkTexts->sHasConnected, "<player>", w->getName()), TXT_NETWORK);
		}
		
		// Send the welcome message
		if(strWelcomeMessage != "")
			SendGlobalText(Replace(strWelcomeMessage, "<player>", w->getName()), TXT_NETWORK);		
	}

	
	// Set the worm info
	newcl->setNumWorms(numworms);
	for (int i = 0;i < numworms;i++) {
		if(ids[i] < 0) {
			// Very strange, we should have catched this case earlier. This means that there were not enough open slots.
			errors << "Server::ParseConnect: We didn't found a slot for " << newWorms[i].sName << " for " << newcl->debugName(false) << endl;
			newcl->setWorm(i, NULL);
			continue;
		}
		newcl->setWorm(i, &cWorms[ids[i]]);
	}
	
	// remove bots if not wanted anymore
	CheckForFillWithBots();
	numworms = newcl->getNumWorms(); // it was earlier when also local client could reconnect - should have no effect anymore
	net_socket->setRemoteAddress(adrFrom); // it could have been changed

	{
		// Let em know they connected good
		CBytestream bytestr;
		bytestr.writeInt(-1, 4);
		bytestr.writeString("lx::goodconnection");

		// Tell the client the id's of the worms
		for (int i = 0;i < newcl->getNumWorms(); i++)
			bytestr.writeInt(newcl->getWorm(i)->getID(), 1);

		bytestr.Send(net_socket.get());
	}

	// If we now the client version already, we can avoid this for newer clients.
	if(newcl->getClientVersion() <= OLXBetaVersion(4)) {
		// Let them know our version
		CBytestream bytestr;
		bytestr.writeInt(-1, 4);
		// sadly we have to send this because it was not thought about any forward-compatibility when it was implemented in Beta3
		// Server version is also added to challenge packet so client will receive it for sure (or won't connect at all).
		bytestr.writeString("lx::openbeta3");
		// sadly we have to send this for Beta4
		// we are sending the version string already in the challenge
		bytestr.writeInt(-1, 4);
		bytestr.writeString("lx::version");
		bytestr.writeString(GetFullGameName());
		bytestr.Send(net_socket.get());
	}
	
	// In older clients, it was possible to disallow that (which doesn't really make sense).
	// In >=Beta9, we just always can use it. So we have to tell old clients that it's OK.
	if(newcl->getClientVersion() <= OLXVersion(0,57)) {
		CBytestream bytestr;
		bytestr.writeInt(-1, 4);
		bytestr.writeString("lx:mouseAllowed");
		bytestr.Send(net_socket.get());
	}

	// TODO: why is this still done here and not via feature array or similar?
	// we do this since rev1897, i.e. since 0.57beta5
	if (tLXOptions->tGameInfo.bAllowStrafing) {
		CBytestream bytestr;
		bytestr.writeInt(-1, 4);
		bytestr.writeString("lx:strafingAllowed");
		bytestr.Send(net_socket.get());
	}
	
	NotifyUserOnEvent(); // new player connected; if user is away, notify him	
	
	{
		// Send info about new worms to other clients.
		for(std::set<CWorm*>::iterator w = newJoinedWorms.begin(); w != newJoinedWorms.end(); ++w) {
			for(int j = 0; j < MAX_CLIENTS; ++j) {
				CServerConnection* cl = &cClients[j];
				if(cl == newcl) continue;
				if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE) continue;
				if(cl->getNetEngine() == NULL) continue;
				cl->getNetEngine()->SendUpdateWorm( *w );
			}			
		}
	}
	
	// Just inform everybody in case the client is not compatible.
	// If ingame, we kicked the player already earlier if not compatible.
	checkVersionCompatibility(newcl, false);

	
	if( iState == SVS_LOBBY )
		// Tell new client about game lobby details like mod/map and all game settings.
		// In game, we would send all these info in SendPrepareGame().
		newcl->getNetEngine()->SendUpdateLobbyGame();
		
	// Update players listbox
	DeprecatedGUI::bHost_Update = true;

	// Make host authorised
	if(newcl->isLocalClient())
		newcl->getRights()->Everything();
		
	newcl->setConnectTime(tLX->currentTime);
	
	
	// handling for connect during game
	if( iState != SVS_LOBBY ) {
		// we already check for compatibility earlier
		
		if(!reconnectFrom) {
			newcl->getShootList()->Clear();
			newcl->getUdpFileDownloader()->allowFileRequest(false);
		}
		newcl->setGameReady(false);

		for(std::set<CWorm*>::iterator w = newJoinedWorms.begin(); w != newJoinedWorms.end(); ++w) {
			(*w)->Prepare(true);
		}
		
		if(newcl->getClientVersion() <= OLXVersion(0,57))
			// HINT: this is necessary because of beta8 which doesn't update all its state variables from preparegame
			newcl->getNetEngine()->SendUpdateLobbyGame();
		newcl->getNetEngine()->SendPrepareGame();
	}	

	if (tLX->iGameType != GME_LOCAL) {
		if( iState == SVS_LOBBY )
			SendWormLobbyUpdate(); // to everbody
	}
	
	// Tell all the connected clients the info about these worm(s)
	// Send all information about all worms to new client.
	UpdateWorms();
	

	if( iState != SVS_LOBBY ) {
		newcl->getNetEngine()->SendTeamScoreUpdate();
		
		// TODO: what is the information of this hint? and does it apply here anyway?
		// Cannot send anything after S2C_PREPAREGAME because of bug in old 0.56 clients - Beta5+ does not have this bug

		// In ParseImReady, we will inform other clients that this new client got ready.
		// Here, we inform the new client about other ready clients.
		// This sends also the weapons.
		CServerConnection *cl = cClients;
		for(int c = 0; c < MAX_CLIENTS; c++, cl++) {
			if(cl->getStatus() != NET_CONNECTED) continue;
			if(cl->getNumWorms() == 0) continue;
			if(!cl->getGameReady()) continue;
			cl->getNetEngine()->SendClientReady(newcl);
		}
		
		/*
		 // TODO: This should only be needed if gamemode or other stuff changed it to some unexpected value.
		 // Should we perhaps change it? Or let gamemode (or other code) just take care about this anyway?
		 // Set some info about the new worms
		 for(std::set<CWorm*>::iterator w = newJoinedWorms.begin(); w != newJoinedWorms.end(); ++w) {
		 for(int ii = 0; ii < MAX_CLIENTS; ii++)
		 cClients[ii].getNetEngine()->SendWormScore( (*w) );
		 }*/
		
		m_flagInfo->sendCurrentState(newcl);
				
		for(std::set<CWorm*>::iterator w = newJoinedWorms.begin(); w != newJoinedWorms.end(); ++w) {
			PrepareWorm(*w);

			// send other clients non-default worm properties of new worms 

			if(CServerNetEngine::isWormPropertyDefault(*w)) continue;
			
			for( int i = 0; i < MAX_CLIENTS; i++ ) {
				if( newcl == &cClients[i] ) continue;
				if( cClients[i].getStatus() != NET_CONNECTED ) continue;
				cClients[i].getNetEngine()->SendWormProperties(*w); // if we have changed them in prepare or so
			}
		}

		newcl->getNetEngine()->SendWormProperties(true); // send new client other non-default worm properties
	}
	
	
	
	// Share reliable channel bandwidth equally between all clients - 
	// the amount of reliable data that should be transmitted is more-less the same for each client
	// But we finetuning the delay between reliable packets
	int clientsAmount = 0;
	for(int c = 0; c < MAX_CLIENTS; c++) 
	{
		if(cClients[c].getStatus() != NET_CONNECTED) 
			continue;
		if( !cClients[c].isLocalClient() )
			clientsAmount ++;
	}
	for(int c = 0; c < MAX_CLIENTS; c++) 
	{
		if(cClients[c].getStatus() != NET_CONNECTED) 
			continue;
		// TODO: finetune this coefficients
		if( cClients[c].isLocalClient() )
			cClients[c].getChannel()->LimitReliableStreamBandwidth( -1.0f ); // No limit
		else
			cClients[c].getChannel()->LimitReliableStreamBandwidth( getMaxUploadBandwidth() / clientsAmount * 0.5f, 
																4.0f + cClients[c].getNetSpeed() );
	}
}


///////////////////
// Parse a ping packet
void GameServer::ParsePing(const SmartPointer<NetworkSocket>& net_socket) 
{
	// Ignore pings in local
	// HINT: this can happen when you quit your server and go play local immediatelly - some
	// people have not updated their serverlist yet and try to ping and query the server
	if (tLX->iGameType != GME_HOST)
		return;

	NetworkAddr		adrFrom = net_socket->remoteAddress();
	net_socket->reapplyRemoteAddress();
	// Send the challenge details back to the client

	CBytestream bs;

	bs.Clear();
	bs.writeInt(-1, 4);
	bs.writeString("lx::pong");

	bs.Send(net_socket);
}


///////////////////
// Parse a fservertime request packet
void GameServer::ParseTime(const SmartPointer<NetworkSocket>& tSocket)
{
	// Ignore pings in local
	// HINT: this can happen when you quit your server and go play local immediatelly - some
	// people have not updated their serverlist yet and try to ping and query the server
	if (tLX->iGameType != GME_HOST)
		return;


	// Send the challenge details back to the client
	tSocket->reapplyRemoteAddress();
	CBytestream bs;

	bs.Clear();
	bs.writeInt(-1, 4);
	bs.writeString("lx::timeis");
	bs.writeFloat( (float)fServertime.seconds() );

	bs.Send(tSocket);
}


///////////////////
// Parse a "wants to join" packet
void GameServer::ParseWantsJoin(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs, const std::string& ip) {

	std::string Nick = bs->readString();
	xmlEntityText(Nick);

	// Ignore wants to join in local
	// HINT: this can happen when you quit your server and go play local immediatelly - some
	// people have not updated their serverlist yet and try to ping and query the server
	if (tLX->iGameType != GME_HOST)
		return;

	// Allowed?
	if (!tLXOptions->bAllowWantsJoinMsg)
		return;

	// Accept these messages from banned clients?
	if (!tLXOptions->bWantsJoinBanned && cBanList.isBanned(ip))
		return;
	
	//Truncate nicks longer than "legitimately" possible
	if (Nick.size() > MAX_WORM_NAME_LENGTH)
		Nick.resize(MAX_WORM_NAME_LENGTH);
	
	// Notify about the wants to join
	if (networkTexts->sWantsJoin != "<none>")  {
		std::string buf;
		replacemax(networkTexts->sWantsJoin, "<player>", Nick, buf, 1);
		SendGlobalText(buf, TXT_NORMAL);
	}
}


///////////////////
// Parse a query packet
void GameServer::ParseQuery(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs, const std::string& ip) 
{
	CBytestream bytestr;

	int num = bs->readByte();

	// Ignore queries in local
	// HINT: this can happen when you quit your server and go play local immediatelly - some
	// people have not updated their serverlist yet and try to ping and query the server
	if (tLX->iGameType != GME_HOST)
		return;

	bytestr.writeInt(-1, 4);
	bytestr.writeString("lx::queryreturn");

	// Get Port
	size_t pos = ip.rfind(':');
	if (pos != std::string::npos)
		ip.substr(pos);
	
	//if(ip == "23401")
	//	bytestr.writeString(OldLxCompatibleString(sName+" (private)")); // Not used anyway
	//else
	bytestr.writeString(OldLxCompatibleString(tLXOptions->sServerName));
	bytestr.writeByte(iNumPlayers);
	bytestr.writeByte(tLXOptions->tGameInfo.iMaxPlayers);
	bytestr.writeByte(iState);
	bytestr.writeByte(num);
	// Beta8+ info - old clients will just skip it
	bytestr.writeString( GetGameVersion().asString() );
	bytestr.writeByte( serverAllowsConnectDuringGame() );

	bytestr.Send(tSocket);
}


///////////////////
// Parse a get_info packet
void GameServer::ParseGetInfo(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bsHeader) 
{
	// Ignore queries in local
	// HINT: this can happen when you quit your server and go play local immediatelly - some
	// people have not updated their serverlist yet and try to ping and query the server
	if (tLX->iGameType != GME_HOST)
		return;

	CBytestream     bs;

	if(bsHeader)
		bs.Append(bsHeader);
	else
		bs.writeInt(-1, 4);
	bs.writeString("lx::serverinfo");

	bs.writeString(OldLxCompatibleString(tLXOptions->sServerName));
	bs.writeByte(tLXOptions->tGameInfo.iMaxPlayers);
	bs.writeByte(iState);

	// TODO: check if we should append "levels/" string here, it was like this in old code
	bs.writeString( iState == SVS_PLAYING ? "levels/" + tLXOptions->tGameInfo.sMapFile : tLXOptions->tGameInfo.sMapFile );
	bs.writeString(tLXOptions->tGameInfo.sModName);
	bs.writeByte(getGameMode()->GeneralGameType());
	bs.writeInt16((tLXOptions->tGameInfo.iLives < 0) ? WRM_UNLIM : tLXOptions->tGameInfo.iLives);
	bs.writeInt16(tLXOptions->tGameInfo.iKillLimit);
	bs.writeInt16(tLXOptions->tGameInfo.iLoadingTime);
	bs.writeBool(tLXOptions->tGameInfo.bBonusesOn);


	// Players
	int     numplayers = 0;
	int     p;
	CWorm *w = cWorms;
	for (p = 0;p < MAX_WORMS;p++, w++) {
		if (w->isUsed())
			numplayers++;
	}

	bs.writeByte(numplayers);
	w = cWorms;
	for (p = 0;p < MAX_WORMS;p++, w++) {
		if (w->isUsed()) {
			bs.writeString(RemoveSpecialChars(w->getName()));
			bs.writeInt(w->getScore(), 2);
		}
	}

	w = cWorms;
	// Write out lives
	for (p = 0;p < MAX_WORMS;p++, w++) {
		if (w->isUsed())
			bs.writeInt(w->getLives(), 2);
	}

	// Write out IPs
	w = cWorms;
	for (p = 0; p < MAX_WORMS; p++, w++)  {
		if (w->isUsed())  {
			std::string addr = "0.0.0.0";
			// Disabled for privacy reasons
			/*
			if (NetAddrToString(w->getClient()->getChannel()->getAddress(), addr))  {
				size_t pos = addr.rfind(':');
				if (pos != std::string::npos)
					addr.erase(pos, std::string::npos);
			} else {
				errors << "Cannot convert address for worm " << w->getName() << endl;
			}

			if (addr.size() == 0)
				addr = "0.0.0.0";
			*/
			bs.writeString(addr);
		}
	}

	// Write out my version (we do this since Beta5)
	bs.writeString(GetFullGameName());

	// since Beta7
	bs.writeFloat(tLXOptions->tGameInfo.features[FT_GameSpeed]);
	
	// since Beta9
	CServerNetEngineBeta9::WriteFeatureSettings(&bs);

	// Game mode name
	bs.writeString(getGameMode()->Name());

	bs.Send(tSocket);
}



// Parse NAT traverse packet - can be received only with CServer::tSocket, send responce to one of tNatTraverseSockets[]
void GameServer::ParseTraverse(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs, const std::string& ip)
{
	NetworkAddr		adrFrom, adrClient;

	// Get the server and the connecting client addresses
	adrFrom = tSocket->remoteAddress();
	std::string adrClientStr = bs->readString();
	if (!StringToNetAddr( adrClientStr, adrClient ))  {
		errors << "GameServer: the address specified in ParseTraverse is invalid: " << adrClientStr << endl;
		return;
	}

	// Send lx::traverse to udp server 
	CBytestream bs1;

	bs1.Clear();
	bs1.writeInt(-1, 4);
	bs1.writeString("lx::traverse");
	bs1.writeString(adrClientStr);

	if( !bs->isPosAtEnd() )
	{
		// Some request sent over UDP masterserver
		std::string cmd = bs->readString();
		if (cmd == "lx::getinfo")
			ParseGetInfo(tSocket, &bs1); // TODO: it's pretty huge to be sent through our masterserver
		else if (cmd == "lx::wantsjoin")
			ParseWantsJoin(tSocket, bs, ip);
		return;
	}
	notes << "GameServer: Got a traverse from client " << adrClientStr << endl;

	// Open a new connection for the client
	SmartPointer<NatConnection> newcl = new NatConnection();
	newcl->tAddress = adrClient;
	newcl->tTraverseSocket->OpenUnreliable(0);
	newcl->tConnectHereSocket->OpenUnreliable(0);
	if (!newcl->tTraverseSocket->isOpen())  {
		errors << "Could not open a new socket for a client connecting via NAT traversal: " << GetSocketErrorStr(GetSocketErrorNr()) << endl;
		return;
	}

	if (!newcl->tTraverseSocket->Listen())  {
		errors << "Could not start listening on a NAT socket: " << GetSocketErrorStr(GetSocketErrorNr()) << endl;
		return;
	}

	
	/*bool conn_here =*/ newcl->tConnectHereSocket->Listen();

	// Update the last used time
	newcl->fLastUsed = tLX->currentTime;

	// Send traverse to server, to traverse socket
	newcl->tTraverseSocket->setRemoteAddress(adrFrom);
	bs1.Send(newcl->tTraverseSocket);

	// Send ping to client to open NAT port
	bs1.Clear();
	bs1.writeInt(-1, 4);
	bs1.writeString("lx::pong");

	//SetNetAddrPort(adrClient, (ushort)(port + i));
	newcl->tTraverseSocket->setRemoteAddress(adrClient);
	newcl->tConnectHereSocket->setRemoteAddress(adrClient);

	// Send 3 times - first packet may be ignored by remote NAT
	bs1.Send(newcl->tTraverseSocket);
	bs1.Send(newcl->tTraverseSocket);
	bs1.Send(newcl->tTraverseSocket);

	bs1.Clear();
	bs1.writeInt(-1, 4);
	bs1.writeString("lx::connect_here");
	bs1.Send(newcl->tConnectHereSocket);

	tNatClients.push_back(newcl);  // Add the client
}

// Server sent us "lx::registered", that means it's alive - record that
void GameServer::ParseServerRegistered(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs)
{
	if( tUdpMasterServers.size() == 0 )
		return;

	notes << "Registered on UDP masterserver " << NetAddrToString(tSocket->remoteAddress()) << endl;

	NetworkAddr addr, addr6;
	std::string domain = tUdpMasterServers[0].substr( 0, tUdpMasterServers[0].rfind(':') );
	int port = atoi(tUdpMasterServers[0].substr( tUdpMasterServers[0].rfind(':') + 1 ));
	if( !GetFromDnsCache(domain, addr, addr6) )
		return;
	SetNetAddrPort( addr, port );
	SetNetAddrPort( addr6, port );
		
	if( tSocket->remoteAddress() == addr || tSocket->remoteAddress() == addr6 )
		iFirstUdpMasterServerNotRespondingCount = 0;

	if( bs->isPosAtEnd() )
		return;
	std::string myAddr = bs->readString();

	if( myAddr == "" )
		return;
	if (IsNetAddrV6(myAddr) == 0) {
		sServerAddressV6 = myAddr;
	} else {
		sServerAddressV4 = myAddr;
	}
}

