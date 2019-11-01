#pragma once

#include "iocp.h"
#include "acceptor.h"
#include "service.h"
#include "session.h"
#include <string>
#include <thread>

static TcpEngingPtr tcp_engine_ = nullptr;

class QEngine : public ITcpEngine
{
public:
	QEngine();
	~QEngine();

public:
	virtual bool SetListenAddr(uint32_t nMaxMsgSize,uint16_t nPort,const char * szIp=0);
	virtual bool start(TcpSinkPtr tcpSink);
	virtual void break_loop();
	virtual void loop();
	virtual void stop();
	virtual bool ConnectTo ( const  char * szIp, uint16_t nPort,  void *pAttData,uint32_t nMaxMsgSize) ;
	virtual void CloseSession(uint32_t session_id);
	virtual void WriteData(uint32_t session_id, char* data, uint32_t size);

public:
	static void timer_callback(IocpTimer* pTimer);

private:

#pragma region Connector
	class Connector:public CBaseOverlapped, public std::enable_shared_from_this<Connector>
	{
	public:
		void*			m_pAtt;
		DWORD			m_dwBytes;
		SOCKADDR_IN		m_remoteAddr;
		SOCKADDR_IN		m_localAddr;

		SOCKET			m_hSocket;
		uint32_t		m_nMaxMsgSize;
		QSessionMgrPtr	session_mgr_;

		virtual void HandleComplete(ULONG_PTR pKey,size_t nIOBytes);
		virtual void HandleError(ULONG_PTR pKey,size_t nIOBytes);
		virtual void Destroy() {}
	};	
	using ConnectorPtr = std::shared_ptr<Connector>;
#pragma endregion Connector

	bool											m_hasListen;
	TcpSinkPtr										tcp_sink_;
	QAcceptorPtr									acceptor_;
	IocpServicePtr									iocp_service_;
	QSessionMgrPtr									session_manager_;

	uint32_t										m_nListenMaxMsgSize;
	uint16_t										m_nListenPort;
	std::string										m_strListenIp;
	LPFN_CONNECTEX									m_pfnConnectEx;

	IocpTimer										pulse_timer_;
	ConnectorPtr									connector_;
};