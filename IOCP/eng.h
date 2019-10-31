#pragma once
#include "itf_tcpengine.h"
#include "acceptor.h"
#include "service.h"
#include "session.h"
#include <string>
#include <thread>

static TcpEngingPtr tcp_engine_ = nullptr;

class QEngine : public ITcpEngine
{
	bool											m_hasListen;
	TcpSinkPtr										tcp_sink_;
	std::shared_ptr<QAcceptor>						acceptor_;
	std::shared_ptr<QEngIOCPEventQueueService>		iocp_service_;
	std::shared_ptr<QSessionManager>				session_manager_;

	uint32_t						m_nListenMaxMsgSize;
	uint16_t						m_nListenPort;
	std::string						m_strListenIp;
	LPFN_CONNECTEX					m_pfnConnectEx;

	IocpTimer						pulse_timer_;

public:
	QEngine();
	~QEngine();

public:
	virtual bool SetListenAddr(uint32_t nMaxMsgSize,uint16_t nPort,const char * szIp=0);
	virtual bool start(TcpSinkPtr tcpSink);
	virtual void loop();
	virtual void stop();
	virtual bool ConnectTo ( const  char * szIp, uint16_t nPort,  void *pAttData,uint32_t nMaxMsgSize) ;
	virtual void CloseSession(uint32_t session_id);
	virtual void WriteData(uint32_t session_id, char* data, uint32_t size);

public:
	static void timer_callback(IocpTimer* pTimer);

private:

#pragma region Connector
	class Connector:public IIOCPHandler
	{
	public:
		void *m_pAtt;
		DWORD m_dwBytes;
		SOCKADDR_IN m_remoteAddr;
		SOCKADDR_IN m_localAddr;

		SOCKET m_hSocket;
		uint32_t m_nMaxMsgSize;
		std::shared_ptr<QSessionManager> session_mgr_;

		virtual void HandleComplete(ULONG_PTR pKey,size_t nIOBytes);
		virtual void HandleError(ULONG_PTR pKey,size_t nIOBytes);
		virtual void Destroy();
	};	
	typedef  TOverlappedWrapper<Connector> ConnectorOverLapped;
#pragma endregion Connector
};