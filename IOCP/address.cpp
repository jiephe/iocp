#include "address.h"

CAddress::CAddress( const char *szIp,unsigned short nPort )
{
	m_address.sin_family=AF_INET;
	m_address.sin_port = htons(nPort);

	if(szIp==NULL || szIp[0]==NULL)
		m_address.sin_addr.s_addr=INADDR_ANY;
	else
		m_address.sin_addr.s_addr=inet_addr(szIp);
}

sockaddr_in & CAddress::Address()
{
	return m_address;
}

sockaddr * CAddress::Sockaddr()
{
	return (sockaddr*)  & m_address;
}

const CAddress & CAddress::operator=( sockaddr_in &r )
{
	memcpy(&m_address,&r,sizeof(sockaddr_in));

	return *this;
}

std::string & CAddress::ToString()
{
	char tmp[100];
	sprintf(tmp,"(%s:%d)",inet_ntoa(m_address.sin_addr),ntohs(m_address.sin_port));
	m_string=tmp;

	return m_string;
}


