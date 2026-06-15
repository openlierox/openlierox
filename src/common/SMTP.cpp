/*
 *  SMTP.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 19.10.09.
 *  Code under LGPL.
 *
 */

#include "Networking.h"
#include "SMTP.h"
#include "Debug.h"
#include "Timer.h"
#include "StringUtils.h"

struct SmtpClient::Handler {
	NetworkSocket sock;
};

static bool SmtpClient_wait(NetworkSocket& sock, const std::string& expectedstart) {
	char c = 0;
	int r = 0;
	std::string line;
	while((r = sock.Read(&c, 1)) >= 0) {
		if(r == 0) {
			SDL_Delay(10);
			continue;
		}
		
		if(c == '\n') break;
		line += c;
	}
	if(r < 0) {
		errors << "SMTP: error while waiting for " << expectedstart << endl;
		return false;
	}
	
	if(!subStrEqual(line, expectedstart, expectedstart.size())) {
		errors << "SMTP: expected was " << expectedstart << " but we got '" << line << "'" << endl;
		return false;
	}
	
	return true;
}

static bool writeOnSocket(NetworkSocket& sock, std::string buf) {
	while(buf.size() > 0) {
		int ret = sock.Write(buf);
		if(ret < 0) return false;
		buf.erase(0, ret);
	}
	return true;
}

static bool SmtpClient_write(NetworkSocket& sock, const std::string& line) {
	if(!writeOnSocket(sock, line + "\n")) {
		errors << "SMTP: error while writing " << line << endl;
		return false;
	}
	return true;
}

static bool SmtpClient_writeAndWait(NetworkSocket& sock, const std::string& line, const std::string& expectedanswer) {
	if(!SmtpClient_write(sock, line)) return false;
	return SmtpClient_wait(sock, expectedanswer);
}

bool SmtpClient::connect() {
	if(intern) reset();
	intern = new Handler();
	
	NetworkAddr addr, ignored;
	AbsTime dnsRequestTime = GetTime();
	if(!GetNetAddrFromNameAsync(host)) {
		errors << "SMTP: Error while starting name resolution for " << host << endl;
		return false;
	}
	
	while(!IsNetAddrValid(addr)) {
		if((GetTime() - dnsRequestTime).seconds() >= DNS_TIMEOUT) {
			errors << "SMTP: DNS timeout while resolving " << host << endl;
			return false;
		}
		SDL_Delay(10);
		GetFromDnsCache(host, addr, ignored);
	}
	
	std::string host_addr;
	NetAddrToString(addr, host_addr);
	//notes << "SMTP: resolved " << host << " to " << host_addr << endl;
	
	if(!intern->sock.OpenReliable(0)) {
		errors << "SMTP: error while opening socket" << endl;
		return false;		
	}
	
	AbsTime connectingTime = GetTime();
	if(!intern->sock.Connect(addr)) {
		errors << "SMTP: error while trying to connect to " << host << endl;
		return false;
	}
	
	while(!intern->sock.isReady()) {
		if((GetTime() - connectingTime).seconds() >= 10) {
			errors << "SMTP: connect timeout to " << host << endl;
			return false;
		}
		SDL_Delay(10);
	}
	
	if(!SmtpClient_wait(intern->sock, "220 ")) return false;
	if(!SmtpClient_writeAndWait(intern->sock, "MAIL FROM:" + mailfrom, "250 ")) return false;
	for(std::list<std::string>::iterator i = mailrcpts.begin(); i != mailrcpts.end(); ++i)
		if(!SmtpClient_writeAndWait(intern->sock, "RCPT TO:" + *i, "250 ")) return false;
	if(!SmtpClient_writeAndWait(intern->sock, "DATA", "354 ")) return false;
	if(!SmtpClient_write(intern->sock, "From: " + mailfrom)) return false;
	for(std::list<std::string>::iterator i = mailrcpts.begin(); i != mailrcpts.end(); ++i)
		if(!SmtpClient_write(intern->sock, "To: " + *i)) return false;
	if(!SmtpClient_write(intern->sock, "Subject: " + subject)) return false;
	for(std::list<std::string>::iterator i = headers.begin(); i != headers.end(); ++i)
		if(!SmtpClient_write(intern->sock, *i)) return false;
	if(!SmtpClient_write(intern->sock, "")) return false;
	
	return true;
}

bool SmtpClient::addText(const std::string& txt) {
	if(!intern || !intern->sock.isReady()) {
		errors << "SMTP addtext: socket not ready" << endl;
		return false;
	}
	
	if(txt == "")
		return SmtpClient_write(intern->sock, "");
	
	std::string line;
	for(std::string::const_iterator i = txt.begin(); i != txt.end(); ++i) {
		if(*i == '\n') {
			if(line != "" && line[0] == '.') line = " " + line; // to not end the mail
			if(!SmtpClient_write(intern->sock, line)) return false;
			line = "";
			continue;
		}
		
		line += *i;
	}
	
	if(line != "") {
		if(line[0] == '.') line = " " + line; // to not end the mail
		if(!SmtpClient_write(intern->sock, line)) return false;
	}
	
	return true;
}

bool SmtpClient::close() {
	if(!SmtpClient_writeAndWait(intern->sock, ".", "250 ")) return false;
	if(!SmtpClient_writeAndWait(intern->sock, "QUIT", "221 ")) return false;
	intern->sock.Close();
	return true;
}

void SmtpClient::reset() {
	if(intern) {
		delete intern;
		intern = NULL;
	}
}

