// That's the same as svr_udp.php but needs no MySQL or PHP :)

#include "svr_udp.h"
#include <ctime>

static bool quit = false;	// Signal here on Ctrl-C

static int port = DEFAULT_PORT;
static int sock = -1;

static void signal_handler_impl(int signum)
{
	printf("Caught signal %i, quitting\n", signum);
	quit=true;
	struct sockaddr_in6 addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(port);
	inet_pton(AF_INET6, "::1", &addr.sin6_addr);
	sendto( sock, "lx::ping", 9, 0, (struct sockaddr *)&addr, sizeof(addr) );
};

#ifdef WIN32
static BOOL signal_handler( DWORD signum )
{
	signal_handler_impl(signum);
	return TRUE;
};
#else
static void signal_handler(int signum)
{
	signal_handler_impl(signum);
};
#endif

int main6(int argc, char ** argv)
{
	#ifdef WIN32
	WSADATA dummy;
	WSAStartup(MAKEWORD(2,0), &dummy );
	SetConsoleCtrlHandler( &signal_handler, TRUE );
	#else
	signal( SIGINT, &signal_handler );
	#endif

	if( argc > 1 )
		port = atoi( argv[1] );

	sock = socket( AF_INET6, SOCK_DGRAM, IPPROTO_UDP );
	if( sock == -1 )
	{
		printf("Error opening UDP6 socket\n");
		return 1;
	};

	int v6only = 1;
	setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&v6only, sizeof(v6only));

	struct sockaddr_in6 addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(port);
	
	if( bind( sock, (struct sockaddr *)&addr, sizeof(addr) ) != 0 )
	{
		printf("Error binding UDP6 socket at port %i\n", port);
		return 1;
	};
	
	printf("UDP6 masterserver started at port %i\n", port);
	
	std::list< HostInfo > hosts;
	std::list< RawPacketRequest6 > askedRawPackets;
	
	struct sockaddr_in6 source;
	unsigned sourcePort;
	socklen_t sourceLen;
	char buf[1500];
	std::string data;
	
	while( ! quit )
	{
		sourceLen = sizeof(source);
		int size = recvfrom( sock, buf, sizeof(buf)-1, 0, (struct sockaddr *)&source, &sourceLen );
		if( size == -1 )
			continue;

		time_t lastping = time(NULL);
		
		data.assign( buf, size );
		
		sourcePort = ntohs(source.sin6_port);
		
		char sourceAddrBuf[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &source.sin6_addr, sourceAddrBuf, sizeof(sourceAddrBuf));

		std::string srcAddr = "[";
		srcAddr += sourceAddrBuf;
		srcAddr += "]";
		sprintf(sourceAddrBuf, "%i", sourcePort );
		srcAddr += ":";
		srcAddr += sourceAddrBuf;
		
		//printf("Got msg from %s: %s\n", srcAddr.c_str(), data.c_str() );

		if( data.find( "\xff\xff\xff\xfflx::getserverlist" ) == 0 )
		{
			bool v058rc5 = data.find( "\xff\xff\xff\xfflx::getserverlist3" ) == 0;
			bool beta8 = data.find( "\xff\xff\xff\xfflx::getserverlist2" ) == 0 || v058rc5;
			std::string response = std::string("\xff\xff\xff\xfflx::serverlist") + '\0';
			if( v058rc5 )
				response = std::string("\xff\xff\xff\xfflx::serverlist3") + '\0';
			else if( beta8 )
				response = std::string("\xff\xff\xff\xfflx::serverlist2") + '\0';
			std::string send;
			unsigned amount = 0;
			for( std::list<HostInfo> :: iterator it = hosts.begin(); it != hosts.end(); it++, amount++ )
			{
				if( send.size() >= 255 || amount >= 255 )
				{
					send = response + char((unsigned char)amount) + send;
					sendto( sock, send.c_str(), send.size(), 0, (struct sockaddr *)&source, sizeof(source) );
					amount = 0;
					send = "";
				};
				send += it->addr + '\0' + it->name + '\0' + 
						char((unsigned char)it->numplayers) +
						char((unsigned char)it->maxworms) +
						char((unsigned char)it->state);
				if( beta8 )
					send += it->version + '\0' + char((unsigned char)it->allowsJoinDuringGame);
				if( v058rc5 )
					send += it->v4address + '\0';
			};
			// Send serverlist even with 0 entries so client will know we're alive
			send = response + char((unsigned char)amount) + send;
			//printf( "Sending lx::serverlist\n" );
			//printStr( send );
			sendto( sock, send.c_str(), send.size(), 0, (struct sockaddr *)&source, sizeof(source) );
		}
		
		else if( data.find( "\xff\xff\xff\xfflx::traverse" ) == 0 )
		{
			struct sockaddr_in6 dest;
			memset(&dest, 0, sizeof(dest));
			dest.sin6_family = AF_INET6;
			unsigned destPort;
			int f = data.find( '\0' );
			if( f == std::string::npos )
				continue;
			f++;
			if( f >= data.size() || data[f] != '[' )
				continue;
			f++;
			if( f >= data.size() || data.find(']', f) == std::string::npos )
				continue;
			inet_pton(AF_INET6, data.substr( f, data.find(']', f) - f ).c_str(), &dest.sin6_addr);
			f = data.find(']', f);
			f++;
			if( f >= data.size() || data[f] != ':' )
				continue;
			f++;
			if( f >= data.size() )
				continue;
			destPort = atoi( data.c_str()+f );
			dest.sin6_port = htons(destPort);
			std::string send = "\xff\xff\xff\xfflx::traverse";
			send += '\0';
			send += srcAddr;
			send += '\0';

			f = data.find( '\0', f );
			if( f != std::string::npos )	// Additional data, for future OLX versions - just copy it into dest packet
			{
				f++;
				send += data.substr( f );
			};

			//printf("Sending lx::traverse %s to %s:%i\n", send.c_str() + send.find('\0')+1, inet_ntoa( dest.sin_addr ), destPort );
			sendto( sock, send.c_str(), send.size(), 0, (struct sockaddr *)&dest, sizeof(dest) );
		}

		else if( data.find( "\xff\xff\xff\xfflx::register" ) == 0 )
		{
			int f = data.find( '\0' );
			if( f == std::string::npos )
				continue;
			f++;
			if( data.find( '\0', f ) == std::string::npos )
				continue;
			std::string name = data.substr( f, data.find( '\0', f ) - f );
			f = data.find( '\0', f );
			if( f == std::string::npos )
				continue;
			f++;
			if( f + 3 > data.size() )
				continue;
			unsigned numplayers = (unsigned char)(data[f]);
			unsigned maxworms = (unsigned char)(data[f+1]);
			unsigned state = (unsigned char)(data[f+2]);
	
			std::list<HostInfo> :: iterator it;
			for( it = hosts.begin(); it != hosts.end(); it++ )
			{
				if( it->addr == srcAddr )
				{
					*it = HostInfo( srcAddr, lastping, name, maxworms, numplayers, state );
					//printf("Host db updated: updated: %s %s %u/%u %u\n", srcAddr.c_str(), name.c_str(), numplayers, maxworms, state );
					break;
				};
			};
			if( it == hosts.end() )
			{
				hosts.push_back( HostInfo( srcAddr, lastping, name, maxworms, numplayers, state ) );
				it == hosts.end();
				it --; // End of list
				//printf("Host db updated: added: %s %s %u/%u %u\n", srcAddr.c_str(), name.c_str(), numplayers, maxworms, state );
			};
			// Send back confirmation so host will know we're alive
			std::string send = std::string("\xff\xff\xff\xfflx::registered") + '\0' + srcAddr + '\0';
			sendto( sock, send.c_str(), send.size(), 0, (struct sockaddr *)&source, sizeof(source) );
			
			// Beta8+
			f += 3;
			if( f >= data.size() || data.find( '\0', f ) == std::string::npos )
				continue;
			std::string version = data.substr( f, data.find( '\0', f ) - f );
			f = data.find( '\0', f );
			f++;
			if( f >= data.size() )
				continue;
			bool allowsJoinDuringGame = (unsigned char)(data[f]);
			*it = HostInfo( srcAddr, lastping, name, maxworms, numplayers, state, version, allowsJoinDuringGame );
			f += 1;
			if( f < data.size() && data.find( '\0', f ) != std::string::npos )
			{
				std::string v4address = data.substr( f, data.find( '\0', f ) - f );
				f = data.find( '\0', f ) + 1;
				*it = HostInfo( srcAddr, lastping, name, maxworms, numplayers, state, version, allowsJoinDuringGame, v4address );
			}
		}

		else if( data.find( "\xff\xff\xff\xfflx::deregister" ) == 0 )
		{
			std::list<HostInfo> :: iterator it;
			for( it = hosts.begin(); it != hosts.end(); it++ )
			{
				if( it->addr == srcAddr )
				{
					//printf("Host db updated: deleted: %s %s\n", it->addr.c_str(), it->name.c_str() );
					hosts.erase(it);
					break;
				};
			};
			// No need in confirmation here
		}

		else if( data.find( "\xff\xff\xff\xfflx::ask" ) == 0 )
		{
			// Directly send given packet to server, and return back an answer
			struct sockaddr_in6 dest;
			memset(&dest, 0, sizeof(dest));
			dest.sin6_family = AF_INET6;
			unsigned destPort;
			int f = data.find( '\0' );
			if( f == std::string::npos )
				continue;
			f++;
			if( f >= data.size() || data[f] != '[' )
				continue;
			f++;
			if( f >= data.size() || data.find(']', f) == std::string::npos )
				continue;
			inet_pton(AF_INET6, data.substr( f, data.find(']', f) - f ).c_str(), &dest.sin6_addr);
			f = data.find(']', f);
			f++;
			if( f >= data.size() || data[f] != ':' )
				continue;
			f++;
			if( f >= data.size() )
				continue;
			destPort = atoi( data.c_str()+f );
			dest.sin6_port = htons(destPort);

			f = data.find( '\0', f );
			if( f != std::string::npos )	// Raw packet data to send to remote host
			{
				f++;
				std::string send = data.substr( f );
				//printf("Sending raw packet to %s:%i\n", inet_ntoa( dest.sin_addr ), destPort );
				sendto( sock, send.c_str(), send.size(), 0, (struct sockaddr *)&dest, sizeof(dest) );
				askedRawPackets.push_back( RawPacketRequest6( source, dest, lastping ) );
			};
		}
		
		else if( data.find( "\xff\xff\xff\xfflx::" ) == 0 )
		{
			// We got response for lx::ask packet
			for( std::list< RawPacketRequest6 > :: iterator it = askedRawPackets.begin(); it != askedRawPackets.end(); it++ )
			{
				if( AreNetAddrEqual( it->dst, source ) )
				{
					std::string send = "\xff\xff\xff\xfflx::answer";
					send += '\0';
					send += srcAddr;
					send += '\0';
					send += data;
				
					//printf("Sending raw packet answer %s:%i\n", inet_ntoa( it->src.sin_addr ), ntohs(it->src.sin_port) );
					sendto( sock, send.c_str(), send.size(), 0, (struct sockaddr *)&it->src, sizeof(it->src) );

					break;
				}
			}
		}

		// Clean up outdated entries
		for( std::list<HostInfo> :: iterator it = hosts.begin(); it != hosts.end(); it++ )
		{
			if( lastping - it->lastping > 2*60 )
			{
				//printf("Host db updated: deleted: %s %s\n", it->addr.c_str(), it->name.c_str() );
				hosts.erase(it);
				it = hosts.begin();
			};
		};

		for( std::list< RawPacketRequest6 > :: iterator it = askedRawPackets.begin(); it != askedRawPackets.end(); it++ )
		{
			if( lastping - it->lastping > 10 )
			{
				askedRawPackets.erase(it);
				it = askedRawPackets.begin();
			}
		}

	};

	#ifdef WIN32
	closesocket(sock);
	WSACleanup();
	#else
	close(sock);
	#endif
	return 0;
};
