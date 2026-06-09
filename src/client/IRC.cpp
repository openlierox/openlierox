/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// IRC client class
// Sergyi Pylypenko


#include "IRC.h"
#include "IRC_ReplyCodes.h"
#include "LieroX.h"
#include "FindFile.h"
#include "MathLib.h"
#include "Options.h"
#include "Version.h"
#include "StringBuf.h"
#include "StringUtils.h"
#include "Event.h"


/////////////////////////
// Initialize the IRC networking things (private)
bool IRCClient::initNet()
{
	// Check if already inited
	if(m_socketOpened)
		return true;
	
	// Open the socket
	if (!m_chatSocket.OpenReliable(0))  {
		errors("Could not open a socket for IRC networking: " + GetSocketErrorStr(GetSocketErrorNr()) + "\n");
		return false;
	}

	m_socketOpened = true;

	// Get the address
	ResetNetAddr(m_chatServerAddr);
	NetworkAddr ignored; // irc.quakenet.org has no IPv6 address, and I don't want to use irc.ipv6.quakenet.org

	if(!GetFromDnsCache(m_chatServerAddrStr, m_chatServerAddr, ignored)) {
		GetNetAddrFromNameAsync(m_chatServerAddrStr);
	}

	return true;
}


///////////////////////////
// Add a chat message and call the "on update" event
void IRCClient::addChatMessage(const std::string &msg, IRCTextType type)
{
	// Add the text
	m_chatText.push_back(IRCChatLine(msg, type));

	// Get rid of old messages
	while (m_chatText.size() > 1024)
		m_chatText.pop_front();

	// Run the event functions
	for( std::set<IRCNewMessageCB>::const_iterator it = m_newMsgCallback.begin(); it != m_newMsgCallback.end(); it++ )
		(*it)(msg, (int)type);
}

///////////////////////
// Connecting process, returns true if finished
bool IRCClient::processConnecting()
{
	// If connected, just quit
	if (m_authorizedState != AUTH_NONE)
		return true;

	// Check for DNS resolution
	if (!IsNetAddrValid(m_chatServerAddr))
		return false;

	// Convert the address to string
	std::string addrStr;
	NetAddrToString(m_chatServerAddr, addrStr);
	
	// Add IRC port
	SetNetAddrPort(m_chatServerAddr, IRC_PORT );

	// Connect
	if (!m_socketConnected)  {
		if (!m_chatSocket.Connect(m_chatServerAddr))  {
			errors("IRC error: could not connect to the server " + addrStr);
			disconnect();
			return true;
		}

		// Connected
		m_socketConnected = true;
		m_socketIsReady = false;
		m_netBuffer.clear();
		m_authorizedState = AUTH_NONE;
	}

	// Check for socket readiness
	if (!m_chatSocket.isReady())
		return false;
	else
		m_socketIsReady = true;

	return true;
}


//////////////////////////
// Escapes special characters in the nick
void IRCClient::makeNickIRCFriendly()
{
	// Escape some symbols, make nick IRC-compatible
	replace(m_myNick, " ", "_" ); 
	#define S_LETTER_UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	#define S_LETTER_LOWER "abcdefghijklmnopqrstuvwxyz"
	#define S_LETTER S_LETTER_UPPER S_LETTER_LOWER
	#define S_NUMBER "0123456789"
	#define S_SYMBOL "-[]\\`^{}_"

	// Length overflow
	if(m_myNick.size() > IRC_NICK_MAX_LEN)
		m_myNick.resize(IRC_NICK_MAX_LEN);

	// Empty nick
	if(m_myNick.empty())
		m_myNick = "z";

	// Nick starting with a number
	if(m_myNick.find_first_of(S_NUMBER) == 0 )
		m_myNick[0] = 'z';

	// Replace the dangerous characters
	while (m_myNick.find_first_not_of(S_LETTER S_NUMBER S_SYMBOL) != std::string::npos)
		m_myNick[m_myNick.find_first_not_of(S_LETTER S_NUMBER S_SYMBOL)] = '-';

	if (m_myNick.size() > 0)
		if (m_myNick[0] == '_' || m_myNick[0] == '-' )
			m_myNick[0] = 'z';

	m_nickUniqueNumber = -1;
	m_updatingUserList = false;
}


/////////////////////////////
// Connect to server#channel
bool IRCClient::connect(const std::string &server, const std::string &channel, const std::string &nick)
{
	// Disconnect first
	if (m_socketOpened)
		disconnect();

	m_connecting = true;

	// Set the info
	m_chatServerAddrStr = server;
	m_chatServerChannel = channel;
	m_myNick = nick;

	// Initialize the network
	if (!initNet())
		return false;

	processConnecting();
	return true;
}

////////////////////////
// Process the IRC client (handle incoming messages etc.)
void IRCClient::process()
{
	// Re-connect once per 5 seconds
	if (!m_socketConnected && !m_connecting)  {
		if(tLX->currentTime - m_connectionClosedTime >= m_reconnectWaitTime)  {
			m_connectionClosedTime = tLX->currentTime;
			connect(m_chatServerAddrStr, m_chatServerChannel, m_myNick);
		}
		return;
	}

	// Connecting process
	if (!processConnecting())
		return;

	// Initiate server response
	if (m_authorizedState == AUTH_NONE)  {
		// Adjust the nickname
		if (m_myNick.empty())
			m_myNick = "OpenLieroXor";
		makeNickIRCFriendly();

		// Send the nick command
		sendNick();
		m_authorizedState = AUTH_NICK_SENT;
	}

	// Read data from the socket
	readData();
}

/////////////////////////
// Read data from the socket and process it
void IRCClient::readData()
{
	// Get data from the socket
	char buf[1024];

	int read = 1;
	while (read > 0)  {
		int read = m_chatSocket.Read(buf, sizeof(buf)); // HINT: non-blocking

		// Nothing yet
		if (read == 0)
			break;

		// Error
		if(read < 0)  {
			if (!IsMessageEndSocketErrorNr(GetSocketErrorNr()))  {
				errors("IRC: network error - " + GetSocketErrorStr(GetSocketErrorNr()) + "\n");
				disconnect();
			}
			break;
		}

		// Add the data to the buffer
		m_netBuffer.append(buf, read);
	}

	if (m_netBuffer.empty())
		return;

	size_t pos = m_netBuffer.find("\r\n");
	while(pos != std::string::npos)  {
		std::string line = m_netBuffer.substr(0, pos);

		//printf("IRC: %s\n", line.c_str());

		// Check if the sender is specified
		m_netBuffer.erase(0, pos + 2);
		IRCCommand cmd;
		if(line.size() && line[0] == ':')  {
			size_t pos2 = line.find(' ');
			cmd.sender = line.substr(1, pos2);

			// Check for exclamation mark in the nick as a special character
			size_t excl_pos = cmd.sender.find('!');
			if (excl_pos != std::string::npos)  {
				cmd.sender.erase(excl_pos); // Erase anything (usually IP + ISP info) from the nick
			}

			line.erase(0, pos2 + 1);
		}

		// Get the command and parameters
		cmd.cmd = line.substr(0, line.find(' '));
		while (line.find(' ') != std::string::npos )  {
			line = line.substr(line.find(' ') + 1);
			if (line.size() == 0)
				break;
			if (line[0] == ':')  {
				cmd.params.push_back(line.substr(1));
				break;
			}

			cmd.params.push_back(line.substr(0, line.find(' ')));
		}

		// Parse the command
		parseCommand(cmd);

		pos = m_netBuffer.find("\r\n");
	}
}

///////////////////////
// Disconnect from the server
void IRCClient::disconnect()
{
	// Disconnect only when connected
	if (!m_socketOpened)
		return;

	// Call the disconnect callback
	for( std::set<IRCDisconnectCB>::const_iterator it = m_disconnectCallback.begin(); it != m_disconnectCallback.end(); it++ )
		(*it)();

	// Close socket
	if (m_chatSocket.isReady())
		m_chatSocket.Write("QUIT\r\n");
	if (m_chatSocket.isOpen())
		m_chatSocket.Close();

	// Clear the variables
	m_socketConnected = false;
	m_socketIsReady = false;
	m_socketOpened = false;
	m_connecting = false;
	m_connectionClosedTime = tLX->currentTime;
	m_chatSocket.Clear();
	SetNetAddrValid(m_chatServerAddr, false);
	m_netBuffer.clear();
	m_authorizedState = AUTH_NONE;
	m_nickUniqueNumber = -1;
	m_updatingUserList = false;
	m_chatUsers.clear();

	// Call the update user list callback
	for( std::set<IRCUpdateUserListCB>::const_iterator it = m_updateUsersCallback.begin(); it != m_updateUsersCallback.end(); it++ )
		(*it)(m_chatUsers);

	notes("IRC: disconnected\n");
}

//////////////////////////
// Converts IRC formatting to HTML (OLX uses HTML for rich-text formatting)
std::string IRCClient::ircFormattingToHtml(const std::string &irctext)
{
	std::list<std::string> open_tags;

	// Indexed IRC colors
	static const char * irc_colors[] = { "#FFFFFF", "#000000", "#000080", "#00FF00", "#FF0000", "#804040",
		"#8000FF", "#808000", "#FFFF00", "#00FF00", "#008080", "#00FFFF", "#0000FF", "#FF00FF", "#808080",
		"#C0C0C0" };

	std::string result;
	for (std::string::const_iterator it = irctext.begin(); it != irctext.end(); )  {
		// Check for special characters
		if ((unsigned char)*it < 32)  {
			switch (*it)  {
			case 2:  {  // Bold
				// If the B tag is already open, consider this as an end of the bold text
				bool bold = false;
				for (std::list<std::string>::reverse_iterator rit = open_tags.rbegin(); rit != open_tags.rend(); rit++)  {
					if (*rit == "b")  {
						result += "</b>";
						std::list<std::string>::iterator it = rit.base();
						it--;
						open_tags.erase(it);
						bold = true;
						break;
					}
				}

				if (!bold)  {
					result += "<b>";
					open_tags.push_back("b");
				}

				it++;
				continue; // Skip the character
			} break;
			case 3:  { // Color
				it++; // Skip the control character
				// Read the color index
				std::string index;
				while (it != irctext.end())  {
					if (!isdigit((uchar)*it))
						break;
					index += *it;
					it++;
				}

				// Don't add the format, if there's no text to format
				if (it == irctext.end() || index.empty())
					continue;

				int col_index = CLAMP(atoi(index), 0, (int)(sizeof(irc_colors)/sizeof(char *) - 1)); // Convert the index

				result += "<font color=\"" + std::string(irc_colors[col_index]) + "\">";
				open_tags.push_back("font");
			} break;

			case 31:  {  // Underline
				// If the U tag is already open, consider this as an end of the underlined text
				bool u = false;
				for (std::list<std::string>::reverse_iterator rit = open_tags.rbegin(); rit != open_tags.rend(); rit++)  {
					if (*rit == "u")  {
						result += "</u>";
						std::list<std::string>::iterator it = rit.base();
						it--;
						open_tags.erase(it);
						u = true;
						break;
					}
				}

				if (!u)  {
					result += "<u>";
					open_tags.push_back("u");
				}

				it++;
				continue; // Skip the character
			} break;

			case '\t': // Tab
				break;

			default:
				it++;
				continue; // Ignore the non-printable character
			}
		}

		// Normal character
		result += GetUtf8FromUnicode(GetNextUnicodeFromUtf8(it, irctext.end()));
	}

	// Close any open tags
	while (open_tags.size())  {
		std::string& tag = *open_tags.rbegin();
		result += "</" + tag + ">";
		open_tags.pop_back();
	}

	return result;
}

/*
 *
 * Sending part
 *
 */

///////////////
// Join a channel
void IRCClient::sendJoin()
{
	m_chatSocket.Write("JOIN " + m_chatServerChannel + "\r\n");
	m_authorizedState = AUTH_JOINED_CHANNEL;
	if( m_AwayMessage != "" )
		m_chatSocket.Write("AWAY :" + m_AwayMessage + "\r\n");
}


///////////////////
// Tell the server our nickname
void IRCClient::sendNick()
{
	if (m_nickUniqueNumber < 0)
		m_chatSocket.Write("NICK " + m_myNick + "\r\n");
	else
		m_chatSocket.Write("NICK " + m_myNick + itoa(m_nickUniqueNumber) + "\r\n");
}

///////////////////////
// Send a ping reply
void IRCClient::sendPong(const std::string &param)
{
	m_chatSocket.Write("PONG :" + param + "\r\n");
}

///////////////////////
// Request nick list from the server
void IRCClient::sendRequestNames()
{
	m_chatSocket.Write("NAMES " + m_chatServerChannel + "\r\n");
}

///////////////////////
// Sends user authentication to the server
void IRCClient::sendUserAuth()
{
	std::string nick = m_myNick;
	if (m_nickUniqueNumber >= 0)
		nick += itoa(m_nickUniqueNumber);
	// quakenet server doesn't like "--" or "__" or "-_" or anything like that in username
	replace(nick, "_", "-");
	replace(nick, "--", "-");
	m_chatSocket.Write("USER " + nick + " ? ? :" + GetFullGameName() + "\r\n");
	m_authorizedState = AUTH_USER_SENT;
}

/////////////////////////
// Send a message to the IRC channel, returns true if the chat has been sent
bool IRCClient::sendChat(const std::string &text1)
{
	// Make sure we are connected
	if (!m_socketConnected || !m_socketIsReady || m_authorizedState != AUTH_JOINED_CHANNEL || text1.empty())
		return false;

	std::string text(text1);
	// Some useful chat commands
	if( (text.find("/pm ") == 0 && text.find(" ", 4) != std::string::npos) ||
		(text.find("/msg ") == 0 && text.find(" ", 5) != std::string::npos) )
	{
		// PM specified user
		std::string::size_type space1 = text.find(" ") + 1;
		std::string user = text.substr( space1, space1 - text.find(" ", space1));
		text = text.substr(text.find(" ", space1));
		m_chatSocket.Write("PRIVMSG " + user + " :" + text + "\r\n");
	}
	else if( text.find("/nick ") == 0 || text.find("/name ") == 0 || text.find("/setmyname ") == 0 )
	{
		m_myNick = text.substr( text.find(" ") + 1 );
		TrimSpaces(m_myNick);
		m_nickUniqueNumber = -1;
		makeNickIRCFriendly();
		m_chatSocket.Write("NICK " + m_myNick + "\r\n");
		text = "You changed your name to " + m_myNick;
	}
	else
	{
		// Send the text
		m_chatSocket.Write("PRIVMSG " + m_chatServerChannel + " :" + text + "\r\n");
	}

	// Route the same message back to our parser func, there's no echo in IRC
	IRCCommand cmd;
	cmd.sender = m_myNick;
	cmd.cmd = "PRIVMSG";
	cmd.params.push_back(m_chatServerChannel);
	cmd.params.push_back(text);
	parseCommand(cmd);

	return true;	
}

/////////////////////////
// Set Away message (we put OLX version here)
void IRCClient::setAwayMessage(const std::string & msg)
{
	m_AwayMessage = msg;

	if (!m_socketConnected || !m_socketIsReady || m_authorizedState != AUTH_JOINED_CHANNEL)
		return;

	if( m_AwayMessage != "" )
		m_chatSocket.Write("AWAY" "\r\n");	// Clean AWAY msg or server won't reset it
	m_chatSocket.Write("AWAY :" + m_AwayMessage + "\r\n");
}

/////////////////////////
// Send Whois command on user
void IRCClient::sendWhois(const std::string & userName)
{
	m_chatSocket.Write("WHOIS " + userName + "\r\n");
}

/////////////////////////
// Send Whois command on user, and get back info
std::string IRCClient::getWhois(const std::string & user)
{
	if( m_whoisUserName != user )
	{
		m_whoisUserName = user;
		m_whoisUserInfo = "";
		m_whoisUserAwayMsg = "";
		sendWhois( user );
	}

	std::string ret;
	if( m_whoisUserInfo != "" )
		ret = m_whoisUserInfo;
	if( m_whoisUserAwayMsg != "" )
		ret += "\n" + m_whoisUserAwayMsg;
	return ret;
}


/*
 *
 * Parsing part
 *
 */

/////////////////////
// End of connection
void IRCClient::parseDropped(const IRCClient::IRCCommand &cmd)
{
	// Get our real nick (add the unique number) for comparison
	std::string real_nick = m_myNick;
	if (m_nickUniqueNumber >= 0)
		real_nick += itoa(m_nickUniqueNumber);

	// Make sure the quit is directed to us (if not, someone else has been dropped/has left)
	if (cmd.sender == real_nick)
		disconnect();
	else {
		// Remove the person that has left from the list
		for (std::list<std::string>::iterator it = m_chatUsers.begin(); it != m_chatUsers.end(); it++)  {
			if (*it == cmd.sender)  {
				m_chatUsers.erase(it);
				for( std::set<IRCUpdateUserListCB>::const_iterator it = m_updateUsersCallback.begin(); it != m_updateUsersCallback.end(); it++ )
					(*it)(m_chatUsers);
				break;
			}
		}
	}
}

///////////////////
// Kicked out of the server
void IRCClient::parseKicked(const IRCClient::IRCCommand &cmd)
{
	// Get our real nick (add the unique number) for comparison
	std::string real_nick = m_myNick;
	if (m_nickUniqueNumber >= 0)
		real_nick += itoa(m_nickUniqueNumber);

	// Invalid command
	if (cmd.params.size() < 2)
		return;

	// Check that it was us who gets kicked
	if (cmd.params[1] != m_myNick)
		return;

	// Get the kick reason
	std::string reason = "No reason was given";
	if (cmd.params.size() >= 3)
		reason = cmd.params[2];

	// Inform the user
	addChatMessage("You have been kicked: " + reason, IRC_TEXT_NOTICE);

	// Disconnect
	disconnect();
}

///////////////////
// End of name list
void IRCClient::parseEndOfNames(const IRCClient::IRCCommand &cmd)
{
	m_updatingUserList = false;
	m_chatUsers.sort();
}

////////////////////
// Join successful
void IRCClient::parseJoin(const IRCClient::IRCCommand& cmd)
{
	m_connecting = false;

	// Re-request the nick names
	sendRequestNames();

	// Callback
	for( std::set<IRCConnectCB>::const_iterator it = m_connectCallback.begin(); it != m_connectCallback.end(); it++ )
		(*it)();

	notes("IRC connected to " + m_chatServerChannel + "@" + m_chatServerAddrStr + "\n");
}

/////////////////////
// Parse a mode packet
void IRCClient::parseMode(const IRCClient::IRCCommand &cmd)
{
	if (m_authorizedState == AUTH_USER_SENT)  {
		sendJoin();
	}
}

///////////////////////
// List of the nicknames in the channel
void IRCClient::parseNameReply(const IRCClient::IRCCommand &cmd)
{
	// Check
	if(cmd.params.size() < 4 )
		return;

	// If this is the first reply packet, clear the user list
	if(!m_updatingUserList )
		m_chatUsers.clear();

	m_updatingUserList = true;
	
	// Get the nick names
	StringBuf line(cmd.params[3]);
	std::vector<std::string> nicks = line.splitBy(' ');
	
	for (std::vector<std::string>::iterator it = nicks.begin(); it != nicks.end(); it++)  {
		// Check for validity
		std::string& user = *it;
		if (user.size() == 0)
			continue;

		if (user[0] == '@') // Channel Operator
			user.erase(0, 1);

		m_chatUsers.push_back(user); // Add the user to the list
	}

	// Callback
	for( std::set<IRCUpdateUserListCB>::const_iterator it = m_updateUsersCallback.begin(); it != m_updateUsersCallback.end(); it++ )
		(*it)(m_chatUsers);
}

////////////////////
// Parse the NICK command
void IRCClient::parseNick(const IRCClient::IRCCommand &cmd)
{
	sendRequestNames(); // Re-request the nicknames
}

//////////////////////
// The nickname is in use
void IRCClient::parseNickInUse(const IRCClient::IRCCommand &cmd)
{
	++m_nickUniqueNumber;

	// Make sure we don't overflow the maximum allowed nick length
	if(m_myNick.size() + itoa(m_nickUniqueNumber).size() > IRC_NICK_MAX_LEN )
		m_myNick.resize(IRC_NICK_MAX_LEN - itoa(m_nickUniqueNumber).size());

	sendNick();
}

///////////////////////
// Ping
void IRCClient::parsePing(const IRCClient::IRCCommand &cmd)
{
	// Check
	if (cmd.params.size() == 0)
		return;

	// Send reply
	sendPong(cmd.params[0]);

	// Progress with authorisation if not yet connected
	if (m_authorizedState == AUTH_NICK_SENT)
		sendUserAuth();
}

///////////////////////
// Private message
void IRCClient::parsePrivmsg(const IRCClient::IRCCommand &cmd)
{
	// Check
	if (cmd.params.size() < 2 )
		return;

	// Add the message
	std::string nick = cmd.sender.substr(0, cmd.sender.find('!'));
	std::string text;

	std::string my_nick = m_myNick;
	if (m_nickUniqueNumber >= 0)
		my_nick += itoa(m_nickUniqueNumber);

	IRCTextType type = IRC_TEXT_CHAT;
	if (cmd.params[1].size() && cmd.params[1][0] == '\1' && *cmd.params[1].rbegin() == '\1')  { // Action message
		text = cmd.params[1].substr(1, cmd.params[1].size() - 2);
		replace(text, "ACTION", nick);
		type = IRC_TEXT_ACTION;
	} else if (cmd.params[0] == my_nick) {
		text = nick + ": " + cmd.params[1];
		type = IRC_TEXT_PRIVATE;
	} else
		text = nick + ": " + cmd.params[1];
	addChatMessage(ircFormattingToHtml( text ), type);
}

///////////////////////
// Notice
void IRCClient::parseNotice(const IRCClient::IRCCommand &cmd)
{
	// Notice is actually a chat message

	// Check
	if (cmd.params.size() < 2 )
		return;

	// Ignore any notices before joining the channel (they are more or less some server spam)
	if (m_authorizedState != AUTH_JOINED_CHANNEL)
		return;

	// Get the nick
	std::string nick = cmd.sender.substr(0, cmd.sender.find('!'));

	// Add the message
	std::string text;
	if (nick.size())
		text = nick + ": " + cmd.params[1];
	else
		text = cmd.params[1];
	addChatMessage(text, IRC_TEXT_NOTICE);
}


///////////////////////////
// Fatal error from the server
void IRCClient::parseError(const IRCClient::IRCCommand &cmd)
{
	if (cmd.params.size() > 0)  {
		std::string errMsg = cmd.params[0];
		warnings("IRC server error: " + errMsg + "\n");
		addChatMessage("Server error: " + errMsg, IRC_TEXT_NOTICE);
		disconnect();
		if(errMsg.find("connect too fast") != std::string::npos)
			m_reconnectWaitTime = 60.0f; // wait at least a minute
	}
}


///////////////////////////
// Away message (contains the server on which user is playing)
void IRCClient::parseAway(const IRCCommand& cmd)
{
	if (cmd.params.size() < 3 )
		return;
	if( cmd.params[1] != m_whoisUserName )
		return;
	m_whoisUserAwayMsg = cmd.params[2];
	TrimSpaces(m_whoisUserAwayMsg);
};

///////////////////////////
// Whois message (contains the OLX version of user)
void IRCClient::parseWhois(const IRCCommand& cmd)
{
	if (cmd.params.size() < 6 )
		return;
	if( cmd.params[1] != m_whoisUserName )
		return;
	m_whoisUserInfo = cmd.params[5];
	TrimSpaces(m_whoisUserInfo);
};

///////////////////////////
// End of Whois message (pretty useless for us)
void IRCClient::parseEndOfWhois(const IRCCommand& cmd)
{
};

void IRCClient::parseTopic(const IRCCommand& cmd)
{
	if( cmd.params.size() < 3 )
		return;
	addChatMessage("Topic: " + cmd.params[2], IRC_TEXT_NOTICE);
};

//////////////////////////////
// Parse an IRC command (private)
void IRCClient::parseCommand(const IRCClient::IRCCommand &cmd)
{
	
	/*printf("IRC: sender '%s' cmd '%s'", cmd.sender.c_str(), cmd.cmd.c_str() );
	for( size_t i=0; i<cmd.params.size(); i++ )
		printf(" param %i '%s'", i, cmd.params[i].c_str());
	printf("\n");*/
	

	// Process depending on the command

	bool fail = false;
	int num_command = from_string<int>(cmd.cmd, fail);

	// String commands
	if (fail)  {
		if (cmd.cmd == "PING")
			parsePing(cmd);

		else if (cmd.cmd == "MODE")
			parseMode(cmd);

		else if (cmd.cmd == "PRIVMSG")
			parsePrivmsg(cmd);

		else if (cmd.cmd == "KICK")
			parseKicked(cmd);

		else if (cmd.cmd == "PART" || cmd.cmd == "QUIT")
			parseDropped(cmd);

		else if (cmd.cmd == "JOIN")
			parseJoin(cmd);

		else if (cmd.cmd == "NICK")
			parseNick(cmd);

		else if (cmd.cmd == "NOTICE")
			parseNotice(cmd);

		else if (cmd.cmd == "ERROR")
			parseError(cmd);

		else
			warnings("IRC: unknown command " + cmd.cmd + "\n");

	// Numeric commands
	} else {
		switch (num_command)  {

		// Nick in use
		case LIBIRC_RFC_ERR_NICKNAMEINUSE:
			parseNickInUse(cmd);
			break;

		// List of names
		case LIBIRC_RFC_RPL_NAMREPLY:
			parseNameReply(cmd);
			break;

		// End of name list
		case LIBIRC_RFC_RPL_ENDOFNAMES:
			parseEndOfNames(cmd);
			break;
			
			
		case LIBIRC_RFC_RPL_AWAY:
			parseAway(cmd);
			break;

		case LIBIRC_RFC_RPL_WHOISUSER:
			parseWhois(cmd);
			break;

		case LIBIRC_RFC_RPL_ENDOFWHOIS:
			parseEndOfWhois(cmd);
			break;
			
		case LIBIRC_RFC_RPL_TOPIC:
			parseTopic(cmd);
			break;

		// Message of the day
		case LIBIRC_RFC_RPL_MOTDSTART:
		case LIBIRC_RFC_RPL_MOTD:
		case LIBIRC_RFC_RPL_ENDOFMOTD:
			// Message of the day (ignored currently)
			break;

		// Messages sent upon a successful join
		case LIBIRC_RFC_RPL_WELCOME:
		case LIBIRC_RFC_RPL_YOURHOST:
		case LIBIRC_RFC_RPL_CREATED:
		case LIBIRC_RFC_RPL_MYINFO:
			// Quite useless stuff...
			break;

		default: {}
			// Just ignore, there are many "pro" commands that we don't need
		}
	}
}


/*
 *
 * Global stuff
 *
 */

IRCClient *globalIRC = NULL;

/////////////////////////
// Initializes the IRC client and connects to the server (specified in options)
bool InitializeIRC()
{
	// Already initialized?
	if (globalIRC)
		return true;

	if (!tLXOptions->bEnableChat)
		return false;

	// Allocate the IRC client
	try  {
		globalIRC = new IRCClient();
	} catch (...)  {
		return false;
	}

	// Get the server
	FILE *fp = OpenGameFile("cfg/chatserver.txt", "r");
	if (fp)  {
		std::string addr = ReadUntil(fp, '/');
		std::string chann = ReadUntil(fp, '\n');
		fclose(fp);
		return globalIRC->connect(addr, chann, tLXOptions->sLastSelectedPlayer);
	} else { // Defaults
		return globalIRC->connect("irc.quakenet.org", "#LieroX", tLXOptions->sLastSelectedPlayer);
	}
}

/////////////////////////
// Disconnects the IRC client and does all the cleanup
void ShutdownIRC()
{
	if (globalIRC)
		delete globalIRC;
	globalIRC = NULL;
}

////////////////////////
// Returns an instance of the global IRC client
IRCClient *GetGlobalIRC()
{
	return globalIRC;
}

/////////////////////////
// Handles processing of the global IRC
void ProcessIRC()
{
	if (globalIRC)
		globalIRC->process();
}
