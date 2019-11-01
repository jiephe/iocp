#include "acceptor.h"
#include "address.h"
#include "session.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "Mswsock.lib")

bool QAcceptor::start (QSessionMgrPtr ptr, uint16_t nPort, const char * szIP, uint32_t nMaxMsgSize)
{
	session_mgr_ = ptr;
	m_nMaxMsgSize = nMaxMsgSize;

	m_hListenSocket = WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED );
	if ( INVALID_SOCKET == m_hListenSocket ) return false;

	BOOL bDontLinger = TRUE;
	if(0!=setsockopt(m_hListenSocket,SOL_SOCKET,SO_DONTLINGER,(const char*)&bDontLinger,sizeof(BOOL)))
	{
		printf("setsockopt Error:%d\n",WSAGetLastError());
		closesocket ( m_hListenSocket );
		return false;
	}

	CAddress address ( szIP, nPort );
	int nRet = bind ( m_hListenSocket, address.Sockaddr(), sizeof ( struct sockaddr ) );
	if ( 0 != nRet )
	{
		int nError = WSAGetLastError();
		printf("Bind Error:%d\n", nError);
		closesocket ( m_hListenSocket );
		return false;
	}

	if ( !session_mgr_->get_iocp_service()->BindHandleToIocp ( ( HANDLE ) m_hListenSocket ) )
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

	AddBatchAccept();
	return true;
}

void QAcceptor::AddBatchAccept()
{
#ifndef USE_ACCEPT_EXTENTION
	for (size_t i = 0; i < MAX_FREE_SESSION; ++i)
		AddOneAcceptEx();
#else
	GUID acceptex_guid = WSAID_ACCEPTEX;
	DWORD bytes = 0;

	int nRet = WSAIoctl(m_hListenSocket,
						SIO_GET_EXTENSION_FUNCTION_POINTER,
						&acceptex_guid,
						sizeof(acceptex_guid),
						&fnAcceptEx_,
						sizeof(fnAcceptEx_),
						&bytes,
						NULL,
						NULL);
	if (nRet == SOCKET_ERROR)
	{
		printf("failed to load AcceptEx: %d\n", WSAGetLastError());
		return;
	}
	for (size_t i = 0; i < MAX_FREE_SESSION; ++i)
		AddOneAcceptExtension();
#endif
}

void QAcceptor::AddOneAccept()
{
#ifndef USE_ACCEPT_EXTENTION
	AddOneAcceptEx();
#else
	AddOneAcceptExtension();
#endif
}

bool QAcceptor::AddOneAcceptEx()
{
	DWORD dwRecvNumBytes = 0;
	auto p = std::make_shared<AcceptHandler>();
	p->m_hSocket = WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED );
	if ( INVALID_SOCKET == p->m_hSocket )
		return false;
	p->acceptor = shared_from_this();

	if (!AcceptEx (m_hListenSocket,
					p->m_hSocket,
					p->m_buffer,
					//MAX_BUFF_SIZE -(sizeof(SOCKADDR_IN)+16)*2,
					0, //如果这里不设置为0 accept之后就会有数据传上来 如果设置为0 则当WSARecv的WSABUF的buflen不为0时 数据才会传上来(GetQueuedCompletionStatus 返回)
					sizeof ( SOCKADDR_IN ) + 16,
					sizeof ( SOCKADDR_IN ) + 16,
					&dwRecvNumBytes, 
					&p->m_overlap))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			printf("failed to call AcceptEx: %d\n", WSAGetLastError());
			closesocket(p->m_hSocket);
			return false;
		}
	}

	accept_concurrent_[p] = p;
	return true;
}

bool QAcceptor::AddOneAcceptExtension()
{
	DWORD dwRecvNumBytes = 0;

	auto p = std::make_shared<AcceptHandler>();
	p->m_hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == p->m_hSocket)
		return false;
	p->acceptor = shared_from_this();

	int use = p.use_count();

	int nRet = fnAcceptEx_(m_hListenSocket, 
						p->m_hSocket,
						p->m_buffer,
						//MAX_BUFF_SIZE - (2 * (sizeof(SOCKADDR_STORAGE) + 16)),
						0,	//如果这里不设置为0 accept之后就会有数据传上来 如果设置为0 则当WSARecv的WSABUF的buflen不为0时 数据才会传上来(GetQueuedCompletionStatus 返回)
						sizeof(SOCKADDR_STORAGE) + 16, 
						sizeof(SOCKADDR_STORAGE) + 16,
						&dwRecvNumBytes,
						&p->m_overlap);
	if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()))
	{
		printf("failed to call fnAcceptEx_: %d\n", WSAGetLastError());
		closesocket(p->m_hSocket);
		return false;
	}

	accept_concurrent_[p] = p;

	use = p.use_count();

	return true;
}

void QAcceptor::OnAccpetFinish(AcceptHandlerPtr ptr)
{
#ifndef USE_ACCEPT_EXTENTION
	SOCKADDR_IN* remote = NULL;
	SOCKADDR_IN* local = NULL;

	int remoteLen	= sizeof ( SOCKADDR_IN );
	int localLen	= sizeof ( SOCKADDR_IN );

	GetAcceptExSockaddrs (ptr->m_buffer,
						0,
						sizeof(SOCKADDR_IN) + 16,
						sizeof(SOCKADDR_IN) + 16,
						(LPSOCKADDR*)&local,
						&localLen,
						(LPSOCKADDR*)&remote,
						&remoteLen);
#else
	int nRet = setsockopt(ptr->m_hSocket,
							SOL_SOCKET,
							SO_UPDATE_ACCEPT_CONTEXT,
							(char *)&m_hListenSocket,
							sizeof(m_hListenSocket));
	if (nRet == SOCKET_ERROR)
	{
		printf("setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed to update accept socket\n");
		return;
	}
#endif

	session_mgr_->OnAccepted(ptr->m_hSocket, m_nMaxMsgSize);
}

QAcceptor::QAcceptor()
{}

QAcceptor::~QAcceptor()
{}

void QAcceptor::stop()
{
	accept_concurrent_.clear();
	closesocket(m_hListenSocket);
}

void QAcceptor::remove_accepted_one(AcceptHandlerPtr ptr)
{
	auto itor = accept_concurrent_.find(ptr);
	if (itor != accept_concurrent_.end())
		accept_concurrent_.erase(itor);
}

void QAcceptor::AcceptHandler::HandleComplete ( ULONG_PTR , size_t  )
{
	acceptor->AddOneAccept();
	acceptor->OnAccpetFinish (shared_from_this());
}

void QAcceptor:: AcceptHandler::HandleError ( ULONG_PTR , size_t  )
{
	acceptor->AddOneAccept();
}

void QAcceptor::AcceptHandler::Destroy()
{
	acceptor->remove_accepted_one(shared_from_this());
}
