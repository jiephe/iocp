#include "eng.h"
#include "address.h"

ITcpEngine * CreateTcpEngine()
{
	return new QEngine();
}

QEngine::QEngine()
	: m_pfnConnectEx ( NULL )
	, m_hasListen ( false )
{
}

QEngine::~QEngine()
{
}

void QEngine::DestoryEngine()
{
	m_oAcceptor.DestroyAcceptor();
	
	m_oChnMgrIocpService.kill_iocp_timer(&pulse_timer_);
	m_oChnMgrIocpService.DestroyService();
}

bool QEngine::StartEngine (IEngTcpSink *tcpSink)
{
	m_pChannelSink = tcpSink;
	if (!m_oChnMgrIocpService.BeginService()) 
		return false;
	
	m_chnMgr.InitMgr ( &m_oChnMgrIocpService, tcpSink );
	if ( m_hasListen && !m_oAcceptor.StartAcceptor ( &m_chnMgr, m_nListenPort, m_strListenIp.c_str(), m_nListenMsgProtocol, m_nListenMaxMsgSize ) )
		return false;

#if 0
	{
		pulse_timer_.userdata = this;
		m_oChnMgrIocpService.add_iocp_timer(&pulse_timer_, true, 1000, timer_callback);
	}
#endif

	return true;
}

void QEngine::Loop()
{
	m_oChnMgrIocpService.run();
}

void QEngine::timer_callback(IocpTimer* pTimer)
{
	QEngine* pEng = (QEngine*)pTimer->userdata;
	//
}

bool QEngine::SetListenAddr ( uint32_t nMaxMsgSize, uint16_t nPort, const char * szIp, uint8_t nProtocol )
{
	m_hasListen = true;
	m_strListenIp = szIp ? szIp : "0.0.0.0";
	m_nListenPort = nPort;
	m_nListenMsgProtocol = nProtocol;
	m_nListenMaxMsgSize = nMaxMsgSize;
	return true;
}

IEngIOCPService * QEngine::GetIOCPService()
{
	return &m_oChnMgrIocpService;
}

IEngChannelManager * QEngine::GetChannelMgr()
{
	return &m_chnMgr;
}

void QEngine::CloseSession(uint32_t connid)
{
	m_chnMgr.CloseSession(connid);
}

void QEngine::write_data(uint32_t session_id, char* data, uint32_t size)
{
	m_chnMgr.PostWriteDataReq(session_id, data, size);
}

bool QEngine::ConnectTo ( const char * szIp, uint16_t nPort,  void *pAtt, uint32_t nMaxMsgSize, uint8_t nProtocol /*= 0x04 */ )
{
	ConnectorOverLapped *p = new ConnectorOverLapped;
	for ( ;; )
	{
		p->m_hSocket =  WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED );
		if (INVALID_SOCKET == p->m_hSocket)
		{
			delete p;
			return false;
		}
			
		CAddress address ( NULL, 0 );
		int nRet = bind ( p->m_hSocket, address.Sockaddr(), sizeof ( struct sockaddr ) );
		if ( 0 != nRet )
		{
			closesocket ( p->m_hSocket );
			// 本地联接数可能满了
			delete p;
			return false;
		}

		int nSize = sizeof ( SOCKADDR_IN );
		getsockname ( p->m_hSocket, ( sockaddr* ) & p->m_localAddr, &nSize );
		if (ntohs( p->m_localAddr.sin_port) == nPort )
		{
			printf ( "Warning: Bind Port Is Same with Remote Port :%d\n" ,nPort);
			closesocket ( p->m_hSocket );
			p->m_hSocket = NULL;
			continue;
		}
		else
			break;
	}

	CAddress addr ( szIp, nPort );
	memcpy ( &p->m_remoteAddr, addr.Sockaddr(), sizeof ( SOCKADDR_IN ) );

	p->m_nMaxMsgSize	= nMaxMsgSize;
	p->m_dwBytes		= 0;
	p->m_pChnMgr		= &m_chnMgr;
	p->m_pAtt			= pAtt;

	if ( m_oChnMgrIocpService.BindHandleToIocp ( ( HANDLE ) p->m_hSocket ) )
	{
		if ( NULL == m_pfnConnectEx )
		{
			GUID guid = WSAID_CONNECTEX;
			DWORD dwBytes = 0;
			WSAIoctl ( p->m_hSocket,
						  SIO_GET_EXTENSION_FUNCTION_POINTER,
						  &guid,
						  sizeof ( guid ),
						  &m_pfnConnectEx,
						  sizeof ( m_pfnConnectEx ),
						  &dwBytes,
						  NULL,
						  NULL );
		}

		if ( !m_pfnConnectEx ( p->m_hSocket , ( sockaddr* ) &p->m_remoteAddr,  sizeof ( sockaddr  ), NULL, 0, &p->m_dwBytes, &p->m_overlap ) )
		{
			DWORD dwErr = WSAGetLastError ();
			if ( dwErr == ERROR_IO_PENDING )
				return true;
		}
	}

	closesocket ( p->m_hSocket );
	p->Destroy();
	return false;
}

void QEngine::Connector::HandleComplete ( ULONG_PTR , size_t  )
{
	m_pChnMgr->HandleConnecttoOK ( m_hSocket, &m_localAddr, &m_remoteAddr, m_nMaxMsgSize, m_pAtt );
}

void QEngine::Connector::HandleError ( ULONG_PTR , size_t  )
{
	if ( INVALID_SOCKET != m_hSocket )
	{
		closesocket ( m_hSocket );
		m_hSocket = INVALID_SOCKET;
	}

	m_pChnMgr->HandleConnecttoFail ( m_pAtt );
}

void QEngine::Connector::Destroy()
{
	delete this;
}


