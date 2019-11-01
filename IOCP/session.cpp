#include "session.h"
#include <cassert>

void QSessionManager::AppendSend::Destroy()
{
	m_pMgr->remove_overlapped(shared_from_this());
}

void QSessionManager::AppendSend::HandleComplete( ULONG_PTR , size_t  )
{
	m_pMgr->HandleWriteData(m_nSessionId, const_cast<char*>(data_buffer.c_str()), data_buffer.size());
}

void QSessionManager::CloseSession::HandleComplete(ULONG_PTR, size_t)
{
	m_pMgr->HandleCloseSession(m_nSessionId);
}
void QSessionManager::CloseSession::Destroy()
{
	m_pMgr->remove_overlapped(shared_from_this());
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
		auto p = std::make_shared<CloseSession>();
		p->m_nSessionId = session_id;
		p->m_pMgr = shared_from_this();
		if (get_iocp_service()->PostRequest(0, (void*)session_id, &p->m_overlap))
			overlapped_list_[p] = p;
	}
}

bool QSessionManager::PostWriteDataReq(uint32_t session_id, char* data, uint32_t size)
{
	auto p = std::make_shared<AppendSend>();
	p->m_pMgr = shared_from_this();
	p->m_nSessionId = session_id;
	p->data_buffer.append(data, size);
	if (get_iocp_service()->PostRequest(size, (void*)session_id, &p->m_overlap))
	{
		overlapped_list_[p] = p;
		return true;
	}

	return false;
}

void QSessionManager::HandleWriteData(uint32_t session_id, char* data, uint32_t size)
{
	auto itor = session_list_.find(session_id);
	if (itor != session_list_.end())
		itor->second->HandleWriteData(data, size);
}

bool QSessionManager::OnAccepted(SOCKET hSocket, uint32_t nMaxMsgSize)
{
	if (!get_iocp_service()->BindHandleToIocp((HANDLE)hSocket))
	{
		closesocket(hSocket);
		return false;
	}

	auto session = std::make_shared<QSession>(shared_from_this(), hSocket, nMaxMsgSize);
	session_list_[session->get_session_id()] = session;

	session->HandleRead();

	get_tcp_sink()->OnAccepted(session->get_session_id());

	return true;
}

QSessionManager::QSessionManager(IocpServicePtr iocpService, TcpSinkPtr tcpSink)
	:iocp_service_(iocpService), tcp_sink_(tcpSink)
{}

QSessionManager::~QSessionManager()
{}

void QSessionManager::stop()
{
	session_list_.clear();
}

void QSessionManager::remove_overlapped(BaseOverlappedPtr ptr)
{
	auto itor = overlapped_list_.find(ptr);
	if (itor != overlapped_list_.end())
		overlapped_list_.erase(itor);
}

void QSessionManager::HandleConnecttoOK(SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remoteAddr, uint32_t nMaxMsgSize, void *pAtt)
{
	auto session = std::make_shared<QSession>(shared_from_this(), hSocket, nMaxMsgSize);
	session_list_[session->get_session_id()] = session;
	session->HandleRead();
	get_tcp_sink()->OnConnect(true, session->get_session_id(), pAtt);
}

void QSessionManager::HandleConnecttoFail(void *pAtt)
{
	get_tcp_sink()->OnConnect(false, 0, pAtt);
}

bool QSessionManager::WriteData(uint32_t session_id, char* data, uint32_t size)
{
	HandleWriteData(session_id, data, size);
	return true;
}





QSession::QSession(QSessionMgrPtr pMgr, SOCKET s, uint32_t nMaxMsgSize)
	:m_pMgr(pMgr), m_hSocket(s), m_nMaxMsgSize(nMaxMsgSize), b_closing_(false)
{
	session_id_ = (uint32_t)this;
	recv_buffer_.resize(65536);
}

QSession::~QSession()
{
	shutdown(m_hSocket, SD_BOTH);
	closesocket(m_hSocket);
	m_hSocket = INVALID_SOCKET;
}

void QSession::remove_overlapped(BaseOverlappedPtr ptr)
{
	auto itor = overlapped_list_.find(ptr);
	if (itor != overlapped_list_.end())
		overlapped_list_.erase(itor);
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

bool QSession::StartWrite(char* data, uint32_t size)
{
	if (data == nullptr || size <= 0)
		return false;

	auto p = std::make_shared<SendHandler>();
	p->m_pSession = shared_from_this();

	p->send_data.append(data, size);
	p->m_sendBuff.buf = (char*)p->send_data.c_str();
	p->m_sendBuff.len = p->send_data.size();

	//每次GetQueuedCompletionStatus 时nIOBytes 至多是p->m_sendBuff.len
	UINT uRet = WSASend(m_hSocket, &p->m_sendBuff, 1, NULL, 0, &p->m_overlap, NULL);
	if (uRet == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		m_pMgr->get_tcp_sink()->OnWrite(session_id_, -1);
		return false;
	}

	overlapped_list_[p] = p;
	return true;
}
void QSession::handleWriteCompleted(bool bSuccess, size_t dwIOBytes, size_t dwWant)
{
	if (!bSuccess || 0 == dwIOBytes || dwWant != dwIOBytes)
		m_pMgr->get_tcp_sink()->OnWrite(session_id_, -1);
	else
		m_pMgr->get_tcp_sink()->OnWrite(session_id_, 0);
}
void QSession::SendHandler::HandleComplete(ULONG_PTR, size_t nIOBytes)
{
	assert(nIOBytes == send_data.size());
	//测试时 一次性发送80M 返回时都发送完成 所以应该是发送完成才回调
	if (nIOBytes == send_data.size())
		m_pSession->handleWriteCompleted(true, nIOBytes, (size_t)m_sendBuff.len);
	else
		printf("data do not send complete one time sent: %u total: %u\n", nIOBytes, send_data.size());
}
void QSession::SendHandler::HandleError(ULONG_PTR, size_t nIOBytes)
{
	m_pSession->handleWriteCompleted(false, nIOBytes, (size_t)m_sendBuff.len);
}
void QSession::SendHandler::Destroy()
{
	m_pSession->remove_overlapped(shared_from_this());
}


void QSession::HandleRead()
{
	StartRead();
}
bool QSession::StartRead()
{
	auto p = std::make_shared<RecvHandler>();
	p->m_pSession = shared_from_this();
	p->m_uRecvFlags = 0;
	p->m_dwRecvBytes = 0;
	p->m_recvBuff.buf = &recv_buffer_[0];
	p->m_recvBuff.len = 65536;

	//每次GetQueuedCompletionStatus 时nIOBytes 至多是p->m_recvBuff.len
	UINT uRet = WSARecv ( m_hSocket, &p->m_recvBuff, 1, &p->m_dwRecvBytes, &p->m_uRecvFlags, &p->m_overlap, NULL);
	if ( uRet == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		m_pMgr->get_tcp_sink()->OnRead(session_id_, nullptr, -1);
		return false;
	}

	overlapped_list_[p] = p;

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
	m_pSession->remove_overlapped(shared_from_this());
}



