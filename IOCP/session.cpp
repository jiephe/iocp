#include "session.h"
#include <cassert>

void QSessionManager::AppendSend::Destroy()
{
	delete this;
}

QSessionManager::AppendSend::~AppendSend()
{
	if (m_nBuffLen > 0)
		delete [] m_pBuffer;
}

QSessionManager::AppendSend::AppendSend()
	: m_nBuffLen ( 0 )
	, m_nSessionId( 0 )
	, m_pBuffer ( NULL )
{
}

bool QSessionManager::AppendSend::AppendBuffer (uint32_t session_id, char *pData, uint32_t nBytes)
{
	m_nSessionId = session_id;
	m_pBuffer = new char[nBytes];
	memcpy (m_pBuffer, pData, nBytes);
	m_nBuffLen = nBytes;
	return true;
}

void QSessionManager::AppendSend::HandleComplete ( ULONG_PTR , size_t  )
{
	m_pMgr->HandleWriteData (m_nSessionId, m_pBuffer, m_nBuffLen);
}

void QSessionManager::AppendSend::HandleError ( ULONG_PTR , size_t  )
{
}

void QSessionManager::CloseSession::HandleComplete(ULONG_PTR, size_t)
{
	m_pMgr->HandleCloseSession(m_nSessionId);
}

void QSessionManager::HandleCloseSession(uint32_t session_id)
{
	auto itor = session_list_.find(session_id);
	if (itor != session_list_.end())
		session_list_.erase(itor);

	get_tcp_sink()->OnClose(session_id);
}

void QSessionManager::PostCloseSessionReq(uint32_t session_id)
{
	auto itor = session_list_.find(session_id);
	if (itor != session_list_.end())
	{
		itor->second->set_b_closing(true);
		CloseSessionOverLapped* p = new CloseSessionOverLapped();
		p->m_nSessionId = session_id;
		p->m_pMgr = shared_from_this();
		get_iocp_service()->PostRequest(0, (void*)session_id, &p->m_overlap);
	}
}

QSession::QSession(QSessionMgrPtr pMgr, SOCKET s, uint32_t nMaxMsgSize)
	:m_pMgr(pMgr), m_hSocket(s), m_nMaxMsgSize(nMaxMsgSize), b_closing_(false)
{
	session_id_ = (uint32_t)this;
	recv_buffer_.resize(65536);
}

void QSession::HandleWriteData(char* data, uint32_t size)
{
	if (b_closing_)
	{
		printf("is cloing now\n");
		return;
	}
		
	if (size > m_nMaxMsgSize)
		printf ("报文太大:%u\n", size);

	StartWrite(data, size);
}

void QSession::handleWriteCompleted(bool bSuccess, size_t dwIOBytes, size_t dwWant)
{
	if (!bSuccess || 0 == dwIOBytes || dwWant != dwIOBytes)
		m_pMgr->get_tcp_sink()->OnWrite(session_id_, -1);
	else
		m_pMgr->get_tcp_sink()->OnWrite(session_id_, 0);
}

bool QSession::StartWriteAgain(SendHandlerOverLapped* p)
{
	if (p == nullptr)
		return false;

	assert(p->send_data.size() > 0);
	p->m_sendBuff.buf = (char*)p->send_data.c_str();
	p->m_sendBuff.len = p->send_data.size();

	//每次GetQueuedCompletionStatus 时nIOBytes 至多是p->m_sendBuff.len
	UINT uRet = WSASend(m_hSocket, &p->m_sendBuff, 1, NULL, 0, &p->m_overlap, NULL);
	if (uRet == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		m_pMgr->get_tcp_sink()->OnWrite(session_id_, -1);
		return false;
	}

	return true;
}

bool QSession::StartWrite(char* data, uint32_t size)
{
	if (data == nullptr || size <= 0)
		return false;

	SendHandlerOverLapped *p = new SendHandlerOverLapped();
	p->m_pSession = shared_from_this();
	
	p->send_data.append(data, size);
	p->m_sendBuff.buf = (char*)p->send_data.c_str();
	p->m_sendBuff.len = p->send_data.size();

	//每次GetQueuedCompletionStatus 时nIOBytes 至多是p->m_sendBuff.len
	UINT uRet = WSASend(m_hSocket, &p->m_sendBuff, 1, NULL, 0, &p->m_overlap, NULL);
	if (uRet == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		int nRet = WSAGetLastError();
		delete p;
		m_pMgr->get_tcp_sink()->OnWrite(session_id_, -1);
		return false;
	}

	return true;
}

void QSession::HandleRead()
{
	StartRead();
}

bool QSession::StartRead()
{
	RecvHandlerOverLapped *p = new RecvHandlerOverLapped();
	p->m_pSession = shared_from_this();
	p->m_uRecvFlags = 0;
	p->m_dwRecvBytes = 0;
	p->m_recvBuff.buf = &recv_buffer_[0];
	p->m_recvBuff.len = 65536;

	//每次GetQueuedCompletionStatus 时nIOBytes 至多是p->m_recvBuff.len
	UINT uRet = WSARecv ( m_hSocket, &p->m_recvBuff, 1, &p->m_dwRecvBytes, &p->m_uRecvFlags, &p->m_overlap, NULL);
	if ( uRet == SOCKET_ERROR )
	{
		int nErr = WSAGetLastError();
		if (nErr != WSA_IO_PENDING )
		{
			p->Destroy();
			m_pMgr->get_tcp_sink()->OnRead(session_id_, nullptr, -1);
			return false;
		}
	}

	return true;
}

void QSession::handleReadCompleted(bool bSuccess, size_t dwIOBytes, WSABUF* pRecvBuff)
{
	UNREFERENCED_PARAMETER(pRecvBuff);

	if (!bSuccess || 0 == dwIOBytes)
	{
		m_pMgr->get_tcp_sink()->OnRead(session_id_, pRecvBuff->buf, -1);
		return ;
	}

	m_pMgr->get_tcp_sink()->OnRead(session_id_, pRecvBuff->buf, dwIOBytes);
	StartRead();
}

QSession::~QSession()
{
	shutdown(m_hSocket, SD_BOTH);
	closesocket(m_hSocket);
	m_hSocket = INVALID_SOCKET;
}

QSession::SendHandler::SendHandler()
	:m_can_delete(false)
{}

void QSession::SendHandler::HandleComplete ( ULONG_PTR , size_t nIOBytes )
{
	assert(nIOBytes <= send_data.size());
	if (nIOBytes == send_data.size())
	{
		m_can_delete = true;
		m_pSession->handleWriteCompleted(true, nIOBytes, (size_t)m_sendBuff.len);
	}
	else
	{
		if (m_pSession->b_closing_)
			m_can_delete = true;
		else
		{
			printf("data do not send complete one time, need send again!\n");
			std::string str(send_data.substr(nIOBytes, send_data.size()));
			send_data = str;
			m_can_delete = !m_pSession->StartWriteAgain((SendHandlerOverLapped*)this);
		}
	}
}

void QSession::SendHandler::HandleError ( ULONG_PTR , size_t nIOBytes )
{
	m_pSession->handleWriteCompleted ( false, nIOBytes, (size_t) m_sendBuff.len);
}

void QSession::SendHandler::Destroy()
{
	if (m_can_delete)
		delete this;
}

void QSession::RecvHandler::HandleComplete ( ULONG_PTR , size_t nIOBytes )
{
	m_pSession->handleReadCompleted ( true, nIOBytes, & m_recvBuff  );
}

void QSession::RecvHandler::HandleError ( ULONG_PTR , size_t nIOBytes )
{
	m_pSession->handleReadCompleted ( false, nIOBytes, & m_recvBuff  );
}

void QSession::RecvHandler::Destroy()
{
	delete this;
}

bool QSessionManager::PostWriteDataReq(uint32_t session_id, char* data, uint32_t size)
{
	AppendSendOverLapped* p = new AppendSendOverLapped();
	p->m_pMgr = shared_from_this();
	p->AppendBuffer (session_id, data, size);
	if (!get_iocp_service()->PostRequest (size, (void*)session_id, &p->m_overlap))
	{
		p->Destroy();
		return false;
	}

	return true;
}

void QSessionManager::HandleWriteData (uint32_t session_id, char* data, uint32_t size)
{
	auto itor = session_list_.find(session_id);
	if (itor != session_list_.end())
		itor->second->HandleWriteData(data, size);	
}

bool QSessionManager::OnAccepted( SOCKET hSocket, uint32_t nMaxMsgSize )
{
	if (!get_iocp_service()->BindHandleToIocp((HANDLE)hSocket))
	{
		closesocket(hSocket);
		return false;
	}
	
	auto session = std::make_shared<QSession>(shared_from_this(), hSocket, nMaxMsgSize);
	session_list_[session->get_session_id()] = session;
	
	session->HandleRead();

	get_tcp_sink()->OnAccepted (session->get_session_id());
	
	return true;
}

QSessionManager::QSessionManager(IocpServicePtr iocpService, TcpSinkPtr tcpSink)
	:iocp_service_(iocpService), tcp_sink_(tcpSink)
{
}

QSessionManager::~QSessionManager()
{
}

void QSessionManager::HandleConnecttoOK (SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remoteAddr, uint32_t nMaxMsgSize , void *pAtt )
{	
	auto session = std::make_shared<QSession>(shared_from_this(), hSocket, nMaxMsgSize);
	session_list_[session->get_session_id()] = session;
	session->HandleRead();
	get_tcp_sink()->OnConnect(true, session->get_session_id(), pAtt);
}

void QSessionManager::HandleConnecttoFail ( void *pAtt )
{
	get_tcp_sink()->OnConnect( false, 0, pAtt);
}

bool QSessionManager::WriteData (uint32_t session_id, char* data, uint32_t size)
{
	HandleWriteData (session_id, data, size);
	return true;
}

