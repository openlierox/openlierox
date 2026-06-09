#pragma once

#include <string>
#include <vector>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock.h>
#include <time.h>

typedef int socklen_t;

#else

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#endif


#define DEFAULT_PORT 23450

extern int main6(int argc, char ** argv);

struct HostInfo
{
	HostInfo( std::string _addr, time_t _lastping, std::string _name, int _maxworms, int _numplayers, int _state,
				std::string _version = "OpenLieroX/0.57_beta5", bool _allowsJoinDuringGame = false, std::string _v4address = "" ):
		addr(_addr), lastping(_lastping), name(_name), maxworms(_maxworms), numplayers(_numplayers), state(_state),
		version(_version), allowsJoinDuringGame(_allowsJoinDuringGame), v4address(_v4address) {};

	HostInfo(): lastping(0), maxworms(0), numplayers(0), state(0),
				version("OpenLieroX/0.57_beta5"), allowsJoinDuringGame(false) {};

	std::string addr;
	time_t lastping;
	std::string name;
	unsigned maxworms;
	unsigned numplayers;
	unsigned state;
	std::string version;
	bool allowsJoinDuringGame;
	std::string v4address;
};

bool AreNetAddrEqual( sockaddr_in a1, sockaddr_in a2 );

bool AreNetAddrEqual( const sockaddr_in6 & a1, const sockaddr_in6 & a2 );

struct RawPacketRequest
{
	RawPacketRequest( sockaddr_in _src, sockaddr_in _dst, time_t _lastping ):
		src( _src ), dst( _dst ), lastping( _lastping ) {};

	struct sockaddr_in src;
	struct sockaddr_in dst;
	time_t lastping;
};

struct RawPacketRequest6
{
	RawPacketRequest6( sockaddr_in6 _src, sockaddr_in6 _dst, time_t _lastping ):
		src( _src ), dst( _dst ), lastping( _lastping ) {};

	struct sockaddr_in6 src;
	struct sockaddr_in6 dst;
	time_t lastping;
};

void printStr(const std::string & s);
