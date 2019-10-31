#pragma once

#include "itf_tcpengine.h"
#include "session.h"
#include "limit_define.h"
#include <map>

class QAcceptor;
using QAcceptorPtr = std::shared_ptr<QAcceptor>;

class QAcceptor : public std::enable_shared_from_this<QAcceptor>
{
#pragma region Accept
	class AcceptHandler: public IIOCPHandler
	{
	public:
		virtual void HandleComplete ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void HandleError ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void Destroy();

		SOCKET			m_hSocket;
		char			m_buffer[MAX_BUFF_SIZE];
		QAcceptorPtr	acceptor;
	};
	typedef TOverlappedWrapper<AcceptHandler> AcceptHandlerOverLapped;
#pragma endregion 

public:
	bool start (QSessionMgrPtr ptr, uint16_t nPort, const char * szIP, uint32_t nMaxMsgSize);
	void stop();

public:
	QAcceptor();
	~QAcceptor();

private:
	void RemoveFromMap(AcceptHandler*p); 

	void AddOneAccept();
	void AddBatchAccept();
	bool AddOneAcceptEx();	
	bool AddOneAcceptExtension();

	void OnAccpetFinish ( AcceptHandler*pHandler );

private:
	SOCKET												m_hListenSocket;
	uint32_t											m_nMaxMsgSize;
	std::map<AcceptHandler*, AcceptHandlerOverLapped*>	m_needDelete;
	QSessionMgrPtr										session_mgr_;
	LPFN_ACCEPTEX										fnAcceptEx_;
};