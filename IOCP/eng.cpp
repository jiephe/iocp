#include "eng.h"
#include "address.h"

TcpEngingPtr get_tcp_engine()
{
	if (tcp_engine_.get())
		return tcp_engine_;
	else
	{
		tcp_engine_.reset(new QEngine);
		return tcp_engine_;
	}
}

QEngine::QEngine(): m_pfnConnectEx ( nullptr ), m_hasListen ( false )
{
}

QEngine::~QEngine()
{
}

void QEngine::stop()
{
	acceptor_->stop();
	iocp_service_->kill_iocp_timer(&pulse_timer_);
	session_manager_->stop();
	iocp_service_->stop();
}

bool QEngine::start(TcpSinkPtr tcpSink)
{
	tcp_sink_ = tcpSink;

	iocp_service_.reset(new QEngIOCPEventQueueService());
	if (!iocp_service_->start())
		return false;

	session_manager_.reset(new QSessionManager(iocp_service_, tcpSink));
	
	acceptor_.reset(new QAcceptor());
	if (m_hasListen && !acceptor_->start(session_manager_, m_nListenPort, m_strListenIp.c_str(), m_nListenMaxMsgSize))
		return false;

#if 0
	{
		pulse_timer_.userdata = this;
		iocp_service_->add_iocp_timer(&pulse_timer_, true, 1000, timer_callback);
	}
#endif

	return true;
}

void QEngine::break_loop()
{
	iocp_service_->break_loop();
}

void QEngine::loop()
{
	iocp_service_->run();
}

void QEngine::timer_callback(IocpTimer* pTimer)
{
	QEngine* pEng = (QEngine*)pTimer->userdata;
	pEng->break_loop();
}

bool QEngine::SetListenAddr ( uint32_t nMaxMsgSize, uint16_t nPort, const char * szIp)
{
	m_hasListen = true;
	m_strListenIp = szIp ? szIp : "0.0.0.0";
	m_nListenPort = nPort;
	m_nListenMaxMsgSize = nMaxMsgSize;
	return true;
}

void QEngine::CloseSession(uint32_t session_id)
{
	session_manager_->PostCloseSessionReq(session_id);
}

void QEngine::WriteData(uint32_t session_id, char* data, uint32_t size)
{
	session_manager_->WriteData(session_id, data, size);
}

bool QEngine::ConnectTo(const char * szIp, uint16_t nPort, void *pAtt, uint32_t nMaxMsgSize)
{
	auto p = std::make_shared<Connector>();
	while (true)
	{
		p->m_hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_SOCKET == p->m_hSocket)
			return false;
			
		CAddress address(NULL, 0);
		int nRet = bind(p->m_hSocket, address.Sockaddr(), sizeof(struct sockaddr));
		if (0 != nRet)
		{
			closesocket(p->m_hSocket);
			return false;
		}

		int nSize = sizeof(SOCKADDR_IN);
		getsockname(p->m_hSocket, (sockaddr*)& p->m_localAddr, &nSize);
		if (ntohs( p->m_localAddr.sin_port) == nPort)
		{
			printf("Warning: Bind Port Is Same with Remote Port :%d\n", nPort);
			closesocket(p->m_hSocket);
			p->m_hSocket = NULL;
			continue;
		}
		else
			break;
	}

	CAddress addr(szIp, nPort);
	memcpy(&p->m_remoteAddr, addr.Sockaddr(), sizeof(SOCKADDR_IN));

	p->m_nMaxMsgSize	= nMaxMsgSize;
	p->m_dwBytes		= 0;
	p->session_mgr_		= session_manager_;
	p->m_pAtt			= pAtt;

	if (iocp_service_->BindHandleToIocp((HANDLE)p->m_hSocket))
	{
		if (NULL == m_pfnConnectEx)
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

		if (!m_pfnConnectEx(p->m_hSocket, (sockaddr*)&p->m_remoteAddr, sizeof(sockaddr), NULL, 0, &p->m_dwBytes, &p->m_overlap))
		{
			if (WSAGetLastError() != ERROR_IO_PENDING)
				return false;
		}

		connector_ = p;
	}

	closesocket(p->m_hSocket);
	return false;
}

void QEngine::Connector::HandleComplete(ULONG_PTR, size_t)
{
	session_mgr_->HandleConnecttoOK(m_hSocket, &m_localAddr, &m_remoteAddr, m_nMaxMsgSize, m_pAtt);
}

void QEngine::Connector::HandleError(ULONG_PTR, size_t)
{
	if (INVALID_SOCKET != m_hSocket)
	{
		closesocket(m_hSocket);
		m_hSocket = INVALID_SOCKET;
	}

	session_mgr_->HandleConnecttoFail(m_pAtt);
}



