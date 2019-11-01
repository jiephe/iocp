#pragma once

#include "iocp.h"
#include "session.h"
#include "macro.h"
#include <map>

class QAcceptor;
using QAcceptorPtr = std::shared_ptr<QAcceptor>;

class QAcceptor : public std::enable_shared_from_this<QAcceptor>
{
#pragma region Accept
	class AcceptHandler: public CBaseOverlapped, public std::enable_shared_from_this<AcceptHandler>
	{
	public:
		virtual void HandleComplete ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void HandleError ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void Destroy();

	public:
		AcceptHandler()
		{
			m_hSocket = INVALID_SOCKET;
		}
		~AcceptHandler()
		{
			int a = 1;
		}

		SOCKET			m_hSocket;
		char			m_buffer[MAX_BUFF_SIZE];
		QAcceptorPtr	acceptor;
	};
#pragma endregion 

	using AcceptHandlerPtr = std::shared_ptr<AcceptHandler>;

public:
	bool start (QSessionMgrPtr ptr, uint16_t nPort, const char * szIP, uint32_t nMaxMsgSize);
	void stop();

public:
	QAcceptor();
	~QAcceptor();

private:
	void remove_accepted_one(AcceptHandlerPtr ptr);

	void AddOneAccept();
	void AddBatchAccept();
	bool AddOneAcceptEx();	
	bool AddOneAcceptExtension();

	void OnAccpetFinish (AcceptHandlerPtr ptr);

private:
	SOCKET												m_hListenSocket;
	uint32_t											m_nMaxMsgSize;
	std::map<AcceptHandlerPtr, AcceptHandlerPtr>		accept_concurrent_;
	QSessionMgrPtr										session_mgr_;
	LPFN_ACCEPTEX										fnAcceptEx_;
};