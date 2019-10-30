#include "channel.h"
#include <cassert>
#include "DataBuffer.h"
#include <sstream>

void QChannelManager::AppendSend::Destroy()
{
	delete this;
}

QChannelManager::AppendSend::~AppendSend()
{
	if (m_nBuffLen > 0)
		delete [] m_pBuffer;
}

QChannelManager::AppendSend::AppendSend()
	: m_nBuffLen ( 0 )
	, m_nConnId ( 0 )
	, m_pBuffer ( NULL )
{
}

bool QChannelManager::AppendSend::AppendBuffer (uint32_t nConnId, char *pData, uint32_t nBytes)
{
	m_nConnId = nConnId;
	m_pBuffer = new char[nBytes];
	memcpy (m_pBuffer, pData, nBytes);
	m_nBuffLen = nBytes;
	return true;
}

void QChannelManager::AppendSend::HandleComplete ( ULONG_PTR , size_t  )
{
	m_pMgr->HandleWriteData ( m_nConnId, m_pBuffer, m_nBuffLen );
}

void QChannelManager::AppendSend::HandleError ( ULONG_PTR , size_t  )
{
}

QChannel::QChannel ( QChannelManager *pMgr ): m_sendBuff(pMgr->GetDBFreeList())
{
	m_pMgr = pMgr;
	data_buffer_.resize(65536);
}

void QChannel::HandleWriteData(char *pData, uint32_t nBytes)
{
	if (nBytes > m_nMaxMsgSize)
	{
		printf ( "报文太大:%u\n", nBytes );
	}

	if ( !IsValid() )
	{
		return ;
	}

	m_sendBuff.AppendData (nBytes, pData);
	StartWrite();
}

bool QChannel::StartWrite()
{
	if ( !m_isConnectted )	return false;

	WSABUF *sendBuff = m_sendBuff.GetDataPtr();
	if (!sendBuff)
	{
		m_pMgr->m_pChannelSink->OnWrite(m_nConnId, 0);
		return true;
	}

	SendHandlerOverLapped *p = new SendHandlerOverLapped();
	p->m_pMgr = m_pMgr;
	p->m_pChannel = this;
	p->m_sendBuff.buf = sendBuff->buf;
	p->m_sendBuff.len = sendBuff->len;
	UINT uRet = WSASend ( m_hSocket, &p->m_sendBuff, 1, NULL, 0, &p->m_overlap, NULL);
	if (uRet == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		p->Destroy();
		m_pMgr->m_pChannelSink->OnWrite(m_nConnId, -1);
		return false;
	}

	return true;
}

bool QChannel::StartRead()
{
	RecvHandlerOverLapped *p = new RecvHandlerOverLapped();
	p->m_pChannel = this;
	p->m_uRecvFlags = 0;
	p->m_dwRecvBytes = 0;
	p->m_recvBuff.buf = &data_buffer_[0];
	p->m_recvBuff.len = 65536;
	UINT uRet = WSARecv ( m_hSocket, &p->m_recvBuff, 1, &p->m_dwRecvBytes, &p->m_uRecvFlags, &p->m_overlap, NULL);
	if ( uRet == SOCKET_ERROR )
	{
		int nErr = WSAGetLastError();
		if (nErr != WSA_IO_PENDING )
		{
			p->Destroy();
			m_pMgr->m_pChannelSink->OnRead(m_nConnId, nullptr, -1);
			return false;
		}
	}

	return true;
}

/*
 *	终于找到bug
 handleReadCompleted-->OnData-->WriteData-->OnIoError
 OnData 中客户会调用  WriteData
 WriteData 则因为socket 被关闭而进入到OnIoError
 而这时已经把m_isInReading 设置为false，从而导致删除了QChannel对象，

 handleReadCompleted 在OnData返回后，因为该对象被删除，所有的数据都变无效，从而出错


 在OnData 中会因为调用 WriteData/PostCloseConnReq，因为Post 事件，会HandleReadCompleted之外发生，故没关系
 修正：m_isInReading 其实应该全程保存，也即说，最好应该在startRead中删除对象，而不在startWrite 中删除本身
 */

void QChannel::handleReadCompleted(bool bSuccess, size_t dwIOBytes, WSABUF* pRecvBuff)
{
	UNREFERENCED_PARAMETER(pRecvBuff);

	if (!bSuccess || 0 == dwIOBytes)
	{
		m_pMgr->m_pChannelSink->OnRead(m_nConnId, pRecvBuff->buf, -1);
		return ;
	}

	m_pMgr->m_pChannelSink->OnRead(m_nConnId, pRecvBuff->buf, dwIOBytes);
	StartRead();
}

void QChannel::handleWriteCompleted (bool bSuccess, size_t dwIOBytes, size_t dwWant)
{
	if (!bSuccess || 0 == dwIOBytes || dwWant != dwIOBytes)
		m_pMgr->m_pChannelSink->OnWrite(m_nConnId, -1);
	else
	{
		m_sendBuff.PopData(dwIOBytes);
		StartWrite();
	}
}

bool QChannel::IsValid()
{
	if (m_isConnectted && ( m_hSocket != INVALID_SOCKET ))
		return true;
	else
		return false;
}

void QChannel::ResetChannel(SOCKET hSocket, uint32_t nConnId, uint32_t nMaxMsgSize)
{
	m_isConnectted	=		true;
	m_hSocket		=		hSocket;
	m_nMaxMsgSize	=		nMaxMsgSize;
	m_nConnId		=		nConnId;

	m_sendBuff.Reset();

	StartRead();
}

QChannel::~QChannel()
{
	shutdown(m_hSocket, SD_BOTH);
	closesocket(m_hSocket);
	m_hSocket = INVALID_SOCKET;
}

void QChannel::SendHandler::HandleComplete ( ULONG_PTR , size_t nIOBytes )
{
	m_pChannel->handleWriteCompleted(true, nIOBytes, (size_t)m_sendBuff.len);
}

void QChannel::SendHandler::HandleError ( ULONG_PTR , size_t nIOBytes )
{
	m_pChannel->handleWriteCompleted ( false, nIOBytes, (size_t) m_sendBuff.len );
}

void QChannel::SendHandler::Destroy()
{
	delete this;
}

void QChannel::RecvHandler::HandleComplete ( ULONG_PTR , size_t nIOBytes )
{
	m_pChannel->handleReadCompleted ( true, nIOBytes, & m_recvBuff  );
}

void QChannel::RecvHandler::HandleError ( ULONG_PTR , size_t nIOBytes )
{
	m_pChannel->handleReadCompleted ( false, nIOBytes, & m_recvBuff  );
}

void QChannel::RecvHandler::Destroy()
{
	delete this;
}

bool QChannelManager::PostWriteDataReq(uint32_t nConnId, char *pData, uint32_t nBytes)
{
	AppendSendOverLapped* p = new AppendSendOverLapped();
	p->m_pMgr = this;
	p->AppendBuffer ( nConnId, pData, nBytes );
	if (!m_pChannelService->PostRequest (nBytes, ( void* ) nConnId, &p->m_overlap))
	{
		p->Destroy();
		return false;
	}

	return true;
}

void QChannelManager::HandleWriteData ( uint32_t nConnId, char *pData, uint32_t nBytes )
{
	QChannel* pChn = (QChannel*) nConnId;
	if ( pChn )
		pChn->HandleWriteData ( pData, nBytes );
}

bool QChannelManager::OnAccepted( SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remteAddr , uint32_t nMaxMsgSize )
{
	if (!m_pChannelService->BindHandleToIocp((HANDLE)hSocket))
	{
		closesocket(hSocket);
		return false;
	}
	
	auto pChn = std::make_shared<QChannel>(this);
	channel_list_.emplace_back(pChn);
	++m_nCountObj;
	
	pChn->ResetChannel(hSocket, (uint32_t)pChn.get(), nMaxMsgSize);

	m_pChannelSink->OnAccepted ((uint32_t)pChn.get());
	return true;
}

QChannelManager::QChannelManager()
{
	m_nConnIdSeed = 0;
	m_nCountObj = 0;
}

void QChannelManager::InitMgr ( QEngIOCPEventQueueService *iocpService, IEngTcpSink *tcpSink )
{
	m_pChannelService = iocpService;
	m_pChannelSink = tcpSink;

	srand(GetCurrentThreadId() * (GetTickCount() % 1000));
}

void QChannelManager::CloseSession(uint32_t connid)
{
	QChannel* pChn = (QChannel*)connid;
	for (auto ik = channel_list_.begin(); ik != channel_list_.end(); )
	{
		if ((*ik).get() == pChn)
		{
			ik = channel_list_.erase(ik);
			break;
		}
		else
			ik++;
	}
}

QChannelManager::~QChannelManager()
{
	for (auto il = m_oDBFreeList.begin(); il != m_oDBFreeList.end(); ++il )
		delete *il;
	m_oDBFreeList.clear();
}

void QChannelManager::HandleConnecttoOK ( SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remoteAddr, uint32_t nMaxMsgSize , void *pAtt )
{	
	auto pChn = std::make_shared<QChannel>(this);
	channel_list_.emplace_back(pChn);
	++ m_nCountObj;
	pChn->ResetChannel(hSocket, (uint32_t)pChn.get(), nMaxMsgSize);
	m_pChannelSink->OnConnect(true, (uint32_t)pChn.get(), pAtt);
}

void QChannelManager::HandleConnecttoFail ( void *pAtt )
{
	m_pChannelSink->OnConnect( false, 0, pAtt);
}

bool QChannelManager::WriteData (uint32_t nConnId, char *pData, uint32_t nBytes )
{
	HandleWriteData ( nConnId, pData, nBytes );
	return true;
}

