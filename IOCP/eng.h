#pragma once
#include "itf_tcpengine.h"
#include "acceptor.h"
#include "service.h"
#include "channel.h"
#include <string>
#include <thread>
#include <memory>

class QEngine:public ITcpEngine
{
	bool m_hasListen;
	IEngTcpSink* m_pChannelSink;
	QAcceptor m_oAcceptor;
	QEngIOCPEventQueueService m_oChnMgrIocpService;
	QChannelManager m_chnMgr;

	uint32_t m_nListenMaxMsgSize;
	uint32_t m_nListenMsgProtocol;
	uint16_t m_nListenPort;
	std::string m_strListenIp;
	LPFN_CONNECTEX m_pfnConnectEx;

	IocpTimer	pulse_timer_;

public:
	QEngine();
	~QEngine();

public:
	virtual bool SetListenAddr(uint32_t nMaxMsgSize,uint16_t nPort,const char * szIp=0,uint8_t nProtocol=0x04);
	virtual bool StartEngine ( IEngTcpSink *tcpSink);
	virtual void Loop();
	virtual void DestoryEngine() ;
	virtual bool ConnectTo ( const  char * szIp, uint16_t nPort,  void *pAttData,uint32_t nMaxMsgSize, uint8_t nProtocol = 0x04 ) ;
	virtual IEngIOCPService * GetIOCPService();
	virtual IEngChannelManager * GetChannelMgr();
	virtual void CloseSession(uint32_t connid);
	virtual void write_data(uint32_t session_id, char* data, uint32_t size);

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

		QChannelManager * m_pChnMgr;

		virtual void HandleComplete(ULONG_PTR pKey,size_t nIOBytes);
		virtual void HandleError(ULONG_PTR pKey,size_t nIOBytes);
		virtual void Destroy();
	};	
	typedef  TOverlappedWrapper<Connector> ConnectorOverLapped;
#pragma endregion Connector
};