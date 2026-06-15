#include "svr_udp.h"

bool AreNetAddrEqual( sockaddr_in a1, sockaddr_in a2 )
{
	return a1.sin_addr.s_addr == a2.sin_addr.s_addr && a1.sin_port == a2.sin_port;
}

bool AreNetAddrEqual( const sockaddr_in6 & a1, const sockaddr_in6 & a2 )
{
	return memcmp(&a1.sin6_addr, &a2.sin6_addr, sizeof(a1.sin6_addr)) == 0 && a1.sin6_port == a2.sin6_port;
}

void printStr(const std::string & s)
{
	for(size_t f=0; f<s.size(); f++)
		printf("%c", s[f] >= 32 ? s[f] : '?' );
	printf("\n");
}
