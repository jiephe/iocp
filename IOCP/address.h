#pragma once

#include <string>
#include <WinSock2.h>

class CAddress
{
	sockaddr_in  m_address;
	std::string m_string;
public:
	bool operator==(const CAddress &r)
	{
		return 0 == memcmp(&m_address, &r.m_address, sizeof(sockaddr_in));
	}
	CAddress(sockaddr_in &a) :m_address(a) {}
	CAddress() { m_address.sin_family = AF_INET; }
	CAddress(const char *szIp, unsigned short nPort);
	sockaddr_in & Address();
	sockaddr * Sockaddr();
	const CAddress & operator=(sockaddr_in &r);

	std::string & ToString();
};
