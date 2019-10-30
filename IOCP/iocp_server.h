#pragma once
#include "itf_tcpengine.h"
#include <functional>
#include <map>

class CIocpServer;

using accept_cb = std::function<void(CIocpServer*, uint32_t)>;
using read_cb = std::function<void(CIocpServer*, char* data, int32_t size)>;

class CIocpServer : public IEngTcpSink
{
public:
	static CIocpServer* get_iocp_server();
	static CIocpServer* iocp_server_;

public:
	virtual bool OnAccepted(uint32_t nConnId);
	virtual void OnClose(uint32_t nConnId);
	virtual void OnData(uint32_t nConnId, char *pData, DWORD dwBytes);
	virtual bool OnIdle(uint32_t nConnId, uint32_t nIdleMS);

	virtual void OnConnect(bool isOK, uint32_t nConnId, void *pAttData);

	virtual void OnPulse(uint64_t  nMsPassedAfterStart);

public:
	CIocpServer();
	~CIocpServer();

	void init(uint16_t port);

	void start(accept_cb cb);

	void read_start(uint64_t session_id, read_cb cb);

	void stop();

	CIocpServer(const CIocpServer&) = delete;
	CIocpServer& operator=(const CIocpServer&) = delete;

private:
	ITcpEngine*		iocp_engine_;

	accept_cb		accept_cb_;

	std::map<uint64_t, read_cb>		map_read_cb_;
};