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
	if ( m_nBuffLen > 0 )
		delete [] m_pBuffer;
}

QChannelManager::AppendSend::AppendSend()
	: m_nBuffLen ( 0 )
	, m_nConnId ( 0 )
	, m_pBuffer ( NULL )
{
}

bool QChannelManager::AppendSend::AppendBuffer ( uint64_t nConnId, char *pData, uint32_t nBytes1 )
{
	size_t nBytes = (size_t)nBytes1;
	m_nConnId = nConnId;
	m_pBuffer = new char [ nBytes];
	assert ( m_pBuffer );

	memcpy ( m_pBuffer, pData, nBytes );
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

QChannel::QChannel ( QChannelManager *pMgr )
	: m_recvBuff ( pMgr->GetDBFreeList() )
	, m_sendBuff ( pMgr->GetDBFreeList() )
{
	m_pMgr = pMgr;
}

void QChannel::HandleWriteData ( char *pData, uint32_t nBytes )
{
	if ( nBytes > m_nMaxMsgSize )
	{
		printf ( "����̫��:%u\n", nBytes );
	}

	if ( !IsValid() )
	{
		//log it
		/*
		m_isConnectted
			&& ( !m_isAbortRead )
			&& ( m_hSocket != INVALID_SOCKET )
			*/

		/*
		printf ( " %d %d %d %I64d %d   %d HandleWriteData  bytes:%d\n",
					m_isConnectted ? 1 : 0, m_isAbortRead, m_hSocket, this->m_nConnId
					, m_isClient ? 1 : 0
					, ntohs ( m_info.remote.sin_port )
					, nBytes
				 );

		*/

		return ;
	}

	m_sendBuff.AppendData ( nBytes, pData );
	m_info.writeBuffer += nBytes;

	StartWrite();
}

bool QChannel::StartWrite()
{
	if ( m_isInWritting )	return false;
	if ( !m_isConnectted )	return false;
	WSABUF *sendBuff = m_sendBuff.GetDataPtr();
	if ( !sendBuff )
	{
		/*
		 *	û��Ҫ���͵�������
		 *	,���֮ǰҪ��رգ������ڿ�����
		 assert()m_channelMap[]nConnId==pCh;
		 */
		if ( m_isAbortRead )
		{
			OnIOError ( true );
		}
		return true;
	}

	SendHandlerOverLapped *p = new SendHandlerOverLapped();
	assert ( p );
	p->m_pMgr = this->m_pMgr;
	p->m_pChannel = this;
	p->m_sendBuff.buf = sendBuff->buf;

	ULONG uLen = sendBuff->len;

	p->m_sendBuff.len = sendBuff->len;

	UINT uRet = WSASend ( m_hSocket, &p->m_sendBuff , 1, NULL, 0, &p->m_overlap, NULL );
	int nLastError = WSAGetLastError();

	if ( uRet == SOCKET_ERROR && nLastError != WSA_IO_PENDING )
	{
		printf ( "start write error=%d port = %d \n", nLastError ,
			ntohs(this->m_info.remote.sin_port)
			);
		p->Destroy();
		OnIOError ( true );
		return false;
	}

	printf("\nWDTEST send self:%p, m_hSocket:%d, threadId: %u\n", this, m_hSocket, GetCurrentThreadId());

	++m_info.writeCall;
	m_isInWritting = true;

	return true;
}

void QChannel::OnIOError ( bool /*ToWriteError*/ )
{
	m_isAbortRead = true;
	if ( !m_hasCloseSocket )
	{
		m_hasCloseSocket = true;
		shutdown ( m_hSocket, SD_BOTH );
		m_isConnectted = false;
		closesocket ( m_hSocket );
		m_hSocket = INVALID_SOCKET;
	}

	if ( ( !m_isInWritting ) && ( !m_isInReading ) && ( !m_isCloseHasNotified ) && ( !m_isConnectted ) )
	{
		m_isCloseHasNotified = true;

		m_pMgr->OnConnClosed ( m_nConnId, this );
	}
}

bool QChannel::StartRead()
{
	// �ͻ�������closechannel��
	if ( m_isAbortRead  || ( !m_isConnectted ) )
	{
		OnIOError ( false );
		return false;
	}

	//// �����Ѿ��Ͽ�
	//if ( !m_isConnectted ) return false;

	// ���ڶ�ȡ��
	if ( m_isInReading ) return true;

	RecvHandlerOverLapped *p = new RecvHandlerOverLapped();
	assert ( p );
	p->m_pMgr = this->m_pMgr;

	/*
	 *	�����ҵ�bug�ˡ�
	 ����p�е�QChannel ָ��������Ϊ���ӱ��رն�����
	 */
	p->m_pChannel = this;
	p->m_uRecvFlags = 0;
	p->m_dwRecvBytes = 0;
	WSABUF *pBuff = m_recvBuff.GetDataPtrFreeSpace();
	p->m_recvBuff.buf = pBuff->buf;
	p->m_recvBuff.len = pBuff->len;
	p->m_nId = m_nConnId;
	UINT uRet =  WSARecv ( m_hSocket, &p->m_recvBuff, 1, &p->m_dwRecvBytes, &p->m_uRecvFlags,
								  &p->m_overlap, NULL );
	if ( uRet == SOCKET_ERROR )
	{
		int nErr = WSAGetLastError();
		if ( nErr != WSA_IO_PENDING )
		{
			p->Destroy();
			OnIOError ( false );
			return false;
		}
	}

	++m_info.readCall ;
	m_isInReading = true;

	return true;
}

/*
 *	�����ҵ�bug
 handleReadCompleted-->OnData-->WriteData-->OnIoError
 OnData �пͻ������  WriteData
 WriteData ����Ϊsocket ���رն����뵽OnIoError
 ����ʱ�Ѿ���m_isInReading ����Ϊfalse���Ӷ�����ɾ����QChannel����

 handleReadCompleted ��OnData���غ���Ϊ�ö���ɾ�������е����ݶ�����Ч���Ӷ�����


 ��OnData �л���Ϊ���� WriteData/PostCloseConnReq����ΪPost �¼�����HandleReadCompleted֮�ⷢ������û��ϵ
 ������m_isInReading ��ʵӦ��ȫ�̱��棬Ҳ��˵�����Ӧ����startRead��ɾ�����󣬶�����startWrite ��ɾ������
 */

void QChannel::handleReadCompleted(bool bSuccess, size_t dwIOBytes, WSABUF * pRecvBuff)
{
	/*if ( 41003 == ntohs ( m_info.local.sin_port ) )
		printf ( "#" );*/
	UNREFERENCED_PARAMETER(pRecvBuff);

	if ( !bSuccess || 0 == dwIOBytes   || m_isAbortRead )
	{
		m_nLastWSAError = WSAGetLastError();
		/*
		char strInfo[300];
		sprintf ( strInfo, "read error = %d dwiobytes=%d success=%d rport=%d ,localport=%d write=%d abor=%d\n", m_nLastWSAError, dwIOBytes, bSuccess ? 1 : 0
					 , ntohs ( m_info.remote.sin_port )
					 , ntohs ( m_info.local.sin_port )
					 , m_isInWritting ? 1 : 0
					 , m_isAbortRead
				  );

		write_log ( "tcp", strInfo, true );
		*/

		m_isInReading = false;
		OnIOError ( false );

		return ;
	}

	assert ( dwIOBytes <= pRecvBuff->len );
	m_info.inBytes += dwIOBytes;
	_time64 ( &m_info.tmRead );

	m_recvBuff.PushData ( dwIOBytes );
	if ( HandleProtocol() )
	{
		m_isInReading = false;
		StartRead() ;
	}
	else
	{
		m_isInReading = false;
		OnIOError ( false );
	}
}

static inline UINT32 byte4_2_d ( const void* p1 )
{
	unsigned char * p = ( unsigned char* ) p1;
	UINT32 u = p[0] << 24;
	u += p[1] << 16;
	u += p[2] << 8;
	u += p[3];

	return u;
}

static inline unsigned short byte2_2_word ( const void *p1 )
{
	unsigned char * p = ( unsigned char* ) p1;
	unsigned int v = ( p[0] << 8 ) + p[1];
	return static_cast<unsigned short> ( v );
}

DWORD QChannel::CalcMessageSize(int nDataLen)
{
	char tmp[10];
	switch ( m_nProtocolType )
	{
	case 0:
		return nDataLen;
	case 2:
		if ( nDataLen < 2 )
		{
			return 2;
		}
		else
		{
			m_recvBuff.NextData ( 2, tmp );
			return byte2_2_word ( tmp ) + 2;
		}
	case 4:
		if ( nDataLen < 4 )
		{
			return 4;
		}
		else
		{
			m_recvBuff.NextData ( 4, tmp );
			return byte4_2_d ( tmp ) + 4;
		}
	case 0x16:
		if ( nDataLen < 6 )
		{
			return 6;
		}
		else
		{
			m_recvBuff.NextData ( 6, tmp );
			tmp[6] = 0;
			int nRet = atoi ( tmp );
			nRet += 6;
			return nRet;
		}
		break;

	case 0xf2:
		if ( nDataLen < 2 )
		{
			return 2;
		}
		else
		{
			m_recvBuff.NextData ( 2, tmp );
			return byte2_2_word ( tmp );
		}

	case 0xf4:
		if ( nDataLen < 4 )
		{
			return 4;
		}
		else
		{
			m_recvBuff.NextData ( 4, tmp );
			return byte4_2_d ( tmp );
		}
	default:
		return nDataLen;
	}
}

bool QChannel::HandleProtocol()
{
	uint32_t nMinSize = m_nProtocolType & 0x0F;
	if ( 0 == nMinSize )
		nMinSize = 1;

	for ( ;; )
	{
		size_t dwSize = m_recvBuff.TotalDataSize();
		if ( dwSize < nMinSize ) break;
		DWORD dwMsgSize = CalcMessageSize((int)dwSize);
		if ( ( DWORD ) - 1 == dwMsgSize || 0 == dwMsgSize  )
		{
			return false;
		}
		WSABUF *pBuf = m_recvBuff.GetDataPtr();
		if ( !pBuf ) break;

		if ( dwMsgSize > m_nMaxMsgSize )
		{
			//int n = * ( int* ) pBuf->buf;
			//printf ( "����̫��:%d >%d:%d,%d,%d \n", dwMsgSize, m_nMaxMsgSize, dwMsgSize,
			//			n, ntohl ( n )
			//		 );

			//m_recvBuff.dump();
			return false;
		}
		if ( dwMsgSize > dwSize )
		{
			break;
		}

		if(dwMsgSize ==4)
		{
			//printf("------------- %d \n",dwSize);
		}

		if ( pBuf->len >= dwMsgSize )
		{
			m_pMgr->m_pChannelSink->OnData ( m_nConnId, pBuf->buf, dwMsgSize , &m_info );
		}
		else
		{
			char *pTmp = new char[(size_t) dwMsgSize + 2];
			assert ( pTmp );
			memset(pTmp, 0, (size_t)dwMsgSize);
			m_recvBuff.NextData ( dwMsgSize, pTmp );
			m_pMgr->m_pChannelSink->OnData ( m_nConnId, pTmp, dwMsgSize , &m_info );

			delete [] pTmp;
		}

		m_recvBuff.PopData ( (size_t)dwMsgSize );
	}

	return true;
}

void QChannel::handleWriteCompleted ( bool bSuccess, size_t dwIOBytes, size_t dwWant )
{
	m_isInWritting = false;
	m_info.writeBuffer -= dwIOBytes;
	if ( !bSuccess || 0 == dwIOBytes || dwWant != dwIOBytes || m_isAbortRead  )
	{
		/*
		char strInfo[100];

		sprintf ( strInfo, "write error = %d abort = %dd success=%d dwIobytes=%d \n", m_nLastWSAError
					 , m_isAbortRead ? 1 : 0
					 , bSuccess ? 1 : 0
					 , dwIOBytes
				  );
		*/
		//write_log("tcp",strInfo,true);

		OnIOError ( false );
	}
	else
	{
		m_info.outBytes += dwIOBytes;
		_time64 ( &m_info.tmWrite );
		m_sendBuff.PopData ( dwIOBytes );
		StartWrite();
	}
}

bool QChannel::IsValid()
{
	/*
	 *
	 *	ȷ���������еģ�����û�е���closechannel
	 */

	if (m_isConnectted && ( !m_isAbortRead ) && ( m_hSocket != INVALID_SOCKET ))
		return true;
	else
		return false;
}

void QChannel::ResetChannel ( SOCKET hSocket, uint64_t nConnId, SOCKADDR_IN *local, SOCKADDR_IN *remote, uint32_t nProtocolType, uint32_t nMaxMsgSize, bool isClient )
{
	m_isConnectted =		true;

	memset ( &m_info, 0, sizeof ( m_info ) );
	m_info.isClient = isClient;
	m_info.nConnId = nConnId;
	_time64 ( &m_info.connTime );

	m_isClient = isClient;
	m_hSocket  = hSocket;
	m_nProtocolType =		nProtocolType;
	m_nMaxMsgSize =		nMaxMsgSize;
	m_nConnId =				nConnId;

	m_isInReading =		false;
	m_isInWritting =		false;
	m_isAbortRead =		false;
	m_hasCloseSocket =	false;
	m_isCloseHasNotified		= false;

	m_recvBuff.Reset();
	m_sendBuff.Reset();

	memcpy ( &m_info.local, local, sizeof ( SOCKADDR_IN ) );
	memcpy ( &m_info.remote, remote, sizeof ( SOCKADDR_IN ) );

	memcpy ( &m_localAddress, local, sizeof ( SOCKADDR_IN ) );
	memcpy ( &m_remoteAddress, remote, sizeof ( SOCKADDR_IN ) );

	StartRead();
}

void QChannel::HandleClose ( bool bWaitingLastWriteDataSuccess )
{
	m_isAbortRead = true;

	if ( bWaitingLastWriteDataSuccess && m_isInWritting )
		return ;

	OnIOError ( false );
}

uint32_t QChannel::CalcIdelSeconds ( __time64_t tnow )
{
	return static_cast<uint32_t> ( tnow - max ( m_info.connTime, max ( m_info.tmRead, m_info.tmWrite ) ) );
}

QChannel::~QChannel()
{
}

void QChannel::dumpChannel ( std::string &s )
{
	std::ostringstream ss;
	ss << ( m_isAbortRead ? 'a' : '-' )
		<< ( m_isInReading ? 'r' : '-' )
		<< ( m_isInWritting ? 'w' : '-' )
		<< ( m_isConnectted ? 'o' : '-' )
		<< ( m_isClient ? 'c' : 's' )
		<< ( m_isCloseHasNotified ? 'n' : '-' )
		<< ( m_hasCloseSocket ? 'x' : '-' )
		<< "id=" << m_nConnId << ","
		<< "p=" << std::hex << (size_t) this
		<< ",";
		
	s = ss.str();
}

void QChannel::SendHandler::HandleComplete ( ULONG_PTR , size_t nIOBytes )
{
	//���ͳɹ�
	m_pChannel->handleWriteCompleted(true, nIOBytes, (size_t)m_sendBuff.len);
}

void QChannel::SendHandler::HandleError ( ULONG_PTR , size_t nIOBytes )
{
	// ����ʧ��
	m_pChannel->handleWriteCompleted ( false, nIOBytes,(size_t) m_sendBuff.len );
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

bool QChannelManager::PostWriteDataReq ( uint64_t nConnId, char *pData, uint32_t nBytes )
{
	AppendSendOverLapped * p = new AppendSendOverLapped();
	assert ( p );
	p->m_pMgr = this;
	p->AppendBuffer ( nConnId, pData, nBytes );
	if ( !m_pChannelService->PostRequest ( nBytes, ( void* ) nConnId, &p->m_overlap ) )
	{
		p->Destroy();
		assert ( 0 );
		return false;
	}

	return true;
}

void QChannelManager::HandleWriteData ( uint64_t nConnId, char *pData, uint32_t nBytes )
{
	QChannel *pChn = FindChannel ( nConnId );
	if ( pChn )
		pChn->HandleWriteData ( pData, nBytes );
}

bool QChannelManager::IsValidConn ( uint64_t nConnId )
{
	return ( NULL != FindChannel ( nConnId ) );
}

void QChannelManager::PostCloseConnReq ( uint64_t nConnId, bool bWaitingLastWriteDataFinish )
{
	CloseChannelOverLapped *p = new CloseChannelOverLapped();
	assert ( p );
	p->m_nConnId = nConnId;
	p->m_pMgr = this;
	p->m_bFlags = bWaitingLastWriteDataFinish;
	m_pChannelService->PostRequest ( 0, ( void* ) nConnId, &p->m_overlap );
}

void QChannelManager::HandleCloseChannel ( uint64_t nConnId, bool bWaitingLastWriteDataFinish )
{
	QChannel *pChn = FindChannel ( nConnId );
	if ( !pChn )
		return ;
	pChn->HandleClose ( bWaitingLastWriteDataFinish );
}

void QChannelManager::OnConnClosed ( uint64_t nId , QChannel*pChn )
{
	assert ( nId == pChn->m_nConnId );
	m_pChannelSink->OnClose ( nId, & pChn->m_info );
	ChannelMapT::iterator ik = m_channelMap.find ( nId );
	if ( ik != m_channelMap.end() )
		m_channelMap.erase ( ik );
	else
		assert ( 0 );

	// TODO ��map ��ɾ��
	assert ( m_nCountObj != 0 );
	delete pChn;
	--m_nCountObj;
	printf ( "-" );
}

bool QChannelManager::HandleAccepted ( SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remteAddr , uint32_t nProtocolType, uint32_t nMaxMsgSize )
{
	if ( !m_pChannelService->BindHandleToIocp ( ( HANDLE ) hSocket ) )
	{
		closesocket ( hSocket );
		return false;
	}

	uint64_t nConnId = GetNextConnId();
	QChannel *pChn = new QChannel ( this );
	++m_nCountObj;
	assert ( pChn );
	OnNewChannel ( nConnId, pChn );
	pChn->ResetChannel ( hSocket, nConnId, localAddr, remteAddr, nProtocolType, nMaxMsgSize, false );

	m_pChannelSink->OnAccepted ( nConnId , &pChn->m_info );
	printf ( "+" );

	return true;
}

void QChannelManager::PostAccepted ( SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remoteAddr, uint32_t nProtocolType, uint32_t nMaxMsgSize )
{
	AcceptOverLapped *p = new AcceptOverLapped();
	assert ( p );
	p->m_pMgr = this;
	p->m_nProtocolType = nProtocolType;
	p->m_nMaxMsgSize = nMaxMsgSize;
	p->m_hSocket = hSocket;
	memcpy ( &p->m_localAddress, localAddr, sizeof ( SOCKADDR_IN ) );
	memcpy ( &p->m_remoteAddress, remoteAddr, sizeof ( SOCKADDR_IN ) );

	m_pChannelService->PostRequest ( 0, p, &p->m_overlap );
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

	m_dwThreadId = GetCurrentThreadId();
	srand ( m_dwThreadId* ( GetTickCount() % 1000 ) );

	check_timer_.userdata = this;
	m_pChannelService->add_iocp_timer(&check_timer_, true, 1000, timer_callback);
}

void QChannelManager::timer_callback(IocpTimer* pTimer)
{
	QChannelManager* pChannel = (QChannelManager*)pTimer->userdata;
	pChannel->HandleIdleCheck();
}

QChannelManager::~QChannelManager()
{
	CDataBuffer::TDataBlockPtrList::iterator il = m_oDBFreeList.begin();
	for ( ; il != m_oDBFreeList.end(); ++il )
	{
		delete *il;
	}
	m_oDBFreeList.clear();
}

void QChannelManager::HandleIdleCheck()
{
	__time64_t tnow;
	_time64 ( &tnow );

	ChannelMapT mapClone =  m_channelMap;
	ChannelMapT::iterator ik = mapClone.begin();
	for ( ; ik != mapClone.end(); ++ik )
	{
		QChannel *pChn = ik->second;
		uint32_t tdiff = pChn->CalcIdelSeconds ( tnow );

		/*
		 *
		 bug ����
		 ��OnIdle->WriteData->OnIoError->
		 Ȼ��ͻ�ɾ��ChannelMapT�е�����
		 ����Map�ṹ����...
		 �Ľ�:
		 OnIOError ������������StartWrite �������������ر���
		 �����ȸ��ƴ�map
		 */
		if ( !m_pChannelSink->OnIdle ( ik->first,  tdiff, &pChn->m_info ) )
		{
			PostCloseConnReq ( ik->first, false );
		}
	}
}

void QChannelManager::HandleConnecttoOK ( SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remoteAddr, uint32_t nProtocolType, uint32_t nMaxMsgSize , void *pAtt )
{	
	uint64_t nConnId = GetNextConnId();

	QChannel *pChn = new QChannel ( this );
	++ m_nCountObj;
	//m_channelMap[nConnId] = pChn;
	OnNewChannel ( nConnId, pChn );
	//assert ( m_channelMap[nConnId] == pChn );

	printf("\nWDTEST connect ok socket: %d\n", hSocket);

	pChn->ResetChannel ( hSocket, nConnId, localAddr, remoteAddr, nProtocolType, nMaxMsgSize , true );
	m_pChannelSink->OnConnectToResult ( true, nConnId, pAtt, &pChn->m_info );
}

void QChannelManager::HandleConnecttoFail ( void *pAtt )
{
	m_pChannelSink->OnConnectToResult ( false, 0, pAtt , 0 );
}

uint64_t QChannelManager::GetNextConnId()
{
	uint64_t nConnId = 0;
	for ( ;; )
	{
		nConnId = ++m_nConnIdSeed;
		if (m_nConnIdSeed >=0x7FFFFFFF)
		{
			// ΪʲôҪ��ô�������ǽ�����id ���� [1,0x7FFFFFFF)֮��,��ô�������id(21��)Ӧ�����൱����ʱ���ڲ����ظ���
			m_nConnIdSeed = 0;
			continue;
		}

		if (0 == nConnId || ((uint64_t)-1 == nConnId))
		{
			continue;
		}

		if ( !FindChannel ( nConnId ) )
			break;
	}

	return nConnId;
}

__time64_t QChannelManager::now()
{
	__time64_t t;
	_time64 ( &t );

	return t;
}

int QChannelManager::rand()
{
	return rand();
}

bool QChannelManager::WriteData ( uint64_t nConnId, char *pData, uint32_t nBytes )
{
	/*
	DWORD dwThreadId = GetCurrentThreadId();
	assert ( dwThreadId == m_dwThreadId );
	*/
	HandleWriteData ( nConnId, pData, nBytes );
	return true;
}

QChannel* QChannelManager::FindChannel ( uint64_t nConnID )
{
	ChannelMapT::iterator it = m_channelMap.find ( nConnID );
	if ( it == m_channelMap.end() )
		return NULL;
	QChannel *p = it->second;
	assert ( p->m_nConnId == nConnID );
	return p;
}

void QChannelManager::OnNewChannel ( uint64_t nConnId, QChannel *pChn )
{
	m_channelMap[nConnId] = pChn;
}

void QChannelManager::CloseChannel::HandleComplete ( ULONG_PTR , size_t )
{
	//
	m_pMgr->HandleCloseChannel ( m_nConnId, m_bFlags );
}

void QChannelManager::AcceptedHandler::HandleComplete ( ULONG_PTR , size_t  )
{
	m_pMgr->HandleAccepted (
		m_hSocket, &m_localAddress, &m_remoteAddress, m_nProtocolType, m_nMaxMsgSize );
}

void QChannelManager::AcceptedHandler::HandleError ( ULONG_PTR , size_t  )
{
	closesocket ( m_hSocket );
	printf ( "$" );
}
