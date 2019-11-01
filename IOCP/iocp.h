#pragma once

#include <stdint.h>
#include <WinSock2.h>
#include <MSWSock.h>
#include <functional>
#include <memory>

typedef struct _IocpTimer
{
	void* userdata;
}IocpTimer;

typedef std::function<void(IocpTimer* timer)> timer_cb;

struct IEngIOCPService
{
public:
	virtual bool start()=0;
	virtual void stop()=0;
	
	virtual void add_iocp_timer(IocpTimer* pTimer, bool bRepeat, uint32_t uMS, timer_cb cb) = 0;
	virtual void kill_iocp_timer(IocpTimer* hHandle) = 0;
};

struct IEngSessionManager
{
public:
	//sync
	virtual bool WriteData(uint32_t session_id, char *data, uint32_t size)=0;

	//async
	virtual bool PostWriteDataReq(uint32_t session_id, char *data, uint32_t size)=0;

	virtual void PostCloseSessionReq(uint32_t session_id) = 0;
};

struct IEngTcpSink
{
	virtual bool OnAccepted		(uint32_t session_id) = 0;
	virtual void OnRead			(uint32_t session_id, char *data, int32_t iread) = 0;
	virtual void OnWrite		(uint32_t session_id, int32_t iwrite) = 0;
	virtual void OnClose		(uint32_t session_id) = 0;
	virtual void OnConnect		(bool isOK, uint32_t session_id, void* pAttData)=0;
};

using TcpSinkPtr = std::shared_ptr<IEngTcpSink>;

struct ITcpEngine
{
public:
	virtual bool SetListenAddr(uint32_t nMaxMsgSize, uint16_t nPort, const char * szIp=0)=0;
	virtual bool start (TcpSinkPtr tcpSink)=0;
	virtual void break_loop() = 0;
	virtual void loop() = 0;
	virtual void stop() = 0;
	virtual void CloseSession(uint32_t session_id) = 0;
	virtual void WriteData(uint32_t session_id, char* data, uint32_t size) = 0;
	virtual bool ConnectTo (const char* szIp, uint16_t nPort,  void *pAttData, uint32_t nMaxMsgSize)= 0;
};

using TcpEngingPtr = std::shared_ptr<ITcpEngine>;
TcpEngingPtr	get_tcp_engine();






