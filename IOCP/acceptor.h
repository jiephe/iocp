#include "itf_tcpengine.h"
#include "service.h"
class QAcceptor;
class QChannelManager;
#include <map>

class QAcceptor
{
#pragma region Accept
	class AcceptHandler: public IIOCPHandler
	{
	public:
		virtual void HandleComplete ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void HandleError ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void Destroy();

		SOCKET m_hSocket;
#define BUFF_LEN (sizeof ( SOCKADDR_IN )+16)*2+16
		// 用来存放地址
		char m_buffer[BUFF_LEN ];

		QAcceptor *m_acceptor;
		// 没有用
		DWORD m_dwBytesRead;
	};
	typedef TOverlappedWrapper<AcceptHandler> AcceptHandlerOverLapped;
#pragma endregion 

public:
	bool StartAcceptor (QChannelManager*pMgr, uint16_t nPort, const char * szIP ,uint32_t nProtocolType,uint32_t nMaxMsgSize);
	void DestroyAcceptor();

public:
	QAcceptor();
	~QAcceptor();

private:
	void RemoveFromMap(AcceptHandler*p); 
	bool AddNewAcceptEx();
	void OnAccpetFinish ( AcceptHandler*pHandler );

private:
	SOCKET  m_hListenSocket;
	uint32_t m_nMaxMsgSize;
	std::map<AcceptHandler*, AcceptHandlerOverLapped*> m_needDelete;
	QChannelManager* m_pMgr;
};