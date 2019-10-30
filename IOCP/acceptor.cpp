#include "acceptor.h"
#include "address.h"
#include "channel.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "Mswsock.lib")

bool QAcceptor::StartAcceptor ( QChannelManager*pMgr, uint16_t nPort, const char * szIP , uint32_t nProtocolType, uint32_t nMaxMsgSize )
{
	m_pMgr=pMgr;
	m_nMaxMsgSize		= nMaxMsgSize;
	m_nProtocolType	= nProtocolType;

#if 0
	m_pAcceptEventQueueService = new QEngIOCPEventQueueService();
	if (!m_pAcceptEventQueueService->BeginService ())
		return false;
#endif

	m_pAcceptEventQueueService = pMgr->get_iocp_service();

	m_hListenSocket = WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED );
	if ( INVALID_SOCKET == m_hListenSocket ) return false;

	BOOL bDontLinger = TRUE;
	if(0!=setsockopt(m_hListenSocket,SOL_SOCKET,SO_DONTLINGER,(const char*)&bDontLinger,sizeof(BOOL)))
	{
		printf("setsockopt Error:%d\n",WSAGetLastError());
		closesocket ( m_hListenSocket );
		return false;
	}

	/*
	BOOL bFlag = TRUE;
	if(0!=setsockopt(m_hListenSocket,SOL_SOCKET,SO_REUSEADDR,(const char*)&bFlag,sizeof(bFlag)))
	{
		printf("setsockopt  SO_REUSEADDR Error:%d\n",WSAGetLastError());
		closesocket ( m_hListenSocket );
		return false;
	}
	bFlag = FALSE;
	if(0!=setsockopt(m_hListenSocket,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char*)&bFlag,sizeof(bFlag)))
	{
		printf("setsockopt SO_EXCLUSIVEADDRUSE  Error:%d\n",WSAGetLastError());
		closesocket ( m_hListenSocket );
		return false;
	}
	*/

	CAddress address ( szIP, nPort );
	int nRet = bind ( m_hListenSocket, address.Sockaddr(), sizeof ( struct sockaddr ) );
	if ( 0 != nRet )
	{
		int nError = WSAGetLastError();
		printf("Bind Error:%d\n", nError);
		closesocket ( m_hListenSocket );
		return false;
	}

	if ( !m_pAcceptEventQueueService->BindHandleToIocp ( ( HANDLE ) m_hListenSocket ) )
	{
		closesocket ( m_hListenSocket );
		return false;
	}

	nRet =::listen ( m_hListenSocket, 10 );
	if ( 0 != nRet )
	{
		int nError = WSAGetLastError();
		printf("Listen Error:%d\n", nError);
		closesocket ( m_hListenSocket );
		return false;
	}

	for ( size_t i = 0; i < 1000; ++i )
		AddNewAcceptEx();

	return true;
}

bool QAcceptor::AddNewAcceptEx()
{
	AcceptHandlerOverLapped *p = new AcceptHandlerOverLapped;
	p->m_hSocket = WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED );
	if ( INVALID_SOCKET == p->m_hSocket )
	{
		delete p;
		return false;
	}
	p->m_acceptor = this;

	if ( !AcceptEx ( m_hListenSocket,
						  p->m_hSocket,
						  p->m_buffer,
						  0,//512-(sizeof(SOCKADDR_IN)+16)*2,
						  sizeof ( SOCKADDR_IN ) + 16,
						  sizeof ( SOCKADDR_IN ) + 16,
						  &p->m_dwBytesRead, &p->m_overlap )
		)
	{
		int nLastErr =WSAGetLastError();
		if ( WSA_IO_PENDING != nLastErr )
		{
#ifdef _DEBUG
			printf("AcceptEx Error\n");
#endif
			closesocket ( p->m_hSocket );
			delete p;
			return false;
		}
	}

	m_needDelete[p] = p;

	return true;
}

void QAcceptor::OnAccpetFinish ( AcceptHandler*pHandler )
{
	SOCKADDR_IN* remote = NULL;
	SOCKADDR_IN* local = NULL;

	int remoteLen	= sizeof ( SOCKADDR_IN );
	int localLen	= sizeof ( SOCKADDR_IN );

	GetAcceptExSockaddrs (
		pHandler->m_buffer,
		0,
		sizeof ( SOCKADDR_IN ) + 16,
		sizeof ( SOCKADDR_IN ) + 16,
		( LPSOCKADDR* ) &local,
		&localLen,
		( LPSOCKADDR* ) &remote,
		&remoteLen
	);

	m_pMgr->PostAccepted ( pHandler->m_hSocket, local, remote, m_nProtocolType, m_nMaxMsgSize );
}

QAcceptor::QAcceptor()
{
	m_pAcceptEventQueueService = NULL;
}

void QAcceptor::DestroyAcceptor()
{
	if (m_pAcceptEventQueueService)
		m_pAcceptEventQueueService->DestroyService();
	closesocket ( m_hListenSocket );
}

QAcceptor::~QAcceptor()
{
	std::map<AcceptHandler*, AcceptHandlerOverLapped*>::iterator ik = m_needDelete.begin();
	for ( ; ik != m_needDelete.end(); ++ik )
	{
		AcceptHandler *p = ik->first;
		if ( INVALID_SOCKET != p->m_hSocket )
		{
			closesocket ( p->m_hSocket );
		}
		ik->second->Destroy();
	}
}

void QAcceptor::RemoveFromMap ( AcceptHandler*p )
{
	std::map<AcceptHandler*, AcceptHandlerOverLapped*>::iterator ik = m_needDelete.find ( p );
	if ( ik != m_needDelete.end() )
	{
		m_needDelete.erase ( ik );
	}
}

void QAcceptor::AcceptHandler::HandleComplete ( ULONG_PTR , size_t  )
{
	m_acceptor->RemoveFromMap ( this );
	m_acceptor->AddNewAcceptEx();

	m_acceptor->OnAccpetFinish ( this );
}

void QAcceptor:: AcceptHandler::HandleError ( ULONG_PTR , size_t  )
{
	m_acceptor->RemoveFromMap ( this );

	closesocket ( m_hSocket );
	m_hSocket = INVALID_SOCKET;

	m_acceptor->AddNewAcceptEx();
}

void QAcceptor::AcceptHandler::Destroy()
{
	delete this;
}
