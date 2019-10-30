#pragma once
#include "itf_tcpengine.h"
#include <functional>
#include <map>

class CIocpServer;

using accept_cb = std::function<void(CIocpServer*, uint32_t)>;
using read_cb = std::function<void(CIocpServer*, uint32_t, char* data, int32_t size)>;
using write_cb = std::function<void(CIocpServer*, uint32_t, int32_t size)>;

class CIocpServer : public IEngTcpSink
{
public:
	static CIocpServer* get_iocp_server();
	static CIocpServer* iocp_server_;

public:
	virtual bool OnAccepted(uint32_t nConnId);
	virtual void OnRead(uint32_t nConnId, char *pData, int32_t iread);
	virtual void OnWrite(uint32_t nConnId, int32_t iwrite);

	virtual void OnConnect(bool isOK, uint32_t nConnId, void *pAttData);

public:
	CIocpServer();
	~CIocpServer();

	void init(uint16_t port);

	void start(accept_cb cb);

	void session_read_start(uint32_t session_id, read_cb cb);

	void session_write_data(uint32_t session_id, char* data, uint32_t size, write_cb cb);

	void session_close(uint32_t session_id);

	void stop();

	CIocpServer(const CIocpServer&) = delete;
	CIocpServer& operator=(const CIocpServer&) = delete;

private:
	ITcpEngine*		iocp_engine_;

	accept_cb		accept_cb_;

	std::map<uint32_t, read_cb>			map_read_cb_;
	std::map<uint32_t, write_cb>		map_wirte_cb_;
};