#pragma once
#include "itf_tcpengine.h"
#include <map>

class CIocpServer;
using CIocpServerPtr = std::shared_ptr<CIocpServer>;

using accept_cb = std::function<void(CIocpServerPtr, uint32_t session_id)>;
using read_cb = std::function<void(CIocpServerPtr, uint32_t session_id, char* data, int32_t size)>;
using write_cb = std::function<void(CIocpServerPtr, uint32_t session_id, int32_t size)>;
using close_cb = std::function<void(CIocpServerPtr, uint32_t session_id)>;

class CIocpServer : public std::enable_shared_from_this<CIocpServer>, public IEngTcpSink
{
public:
	static CIocpServerPtr  get_iocp_server();
	static CIocpServerPtr  iocp_server_;

	static BOOL WINAPI consoleHandler(DWORD signal);

public:
	virtual bool OnAccepted(uint32_t session_id);
	virtual void OnRead(uint32_t session_id, char *data, int32_t iread);
	virtual void OnWrite(uint32_t session_id, int32_t iwrite);
	virtual void OnClose(uint32_t session_id);
	virtual void OnConnect(bool isOK, uint32_t session_id, void *pAttData);

public:
	CIocpServer();
	~CIocpServer();

	void init(uint16_t port);

	void start(accept_cb cb);

	void session_read_start(uint32_t session_id, read_cb cb);

	void session_write_data(uint32_t session_id, char* data, uint32_t size, write_cb cb);

	void session_close(uint32_t session_id, close_cb cb);

	void stop();

	CIocpServer(const CIocpServer&) = delete;
	CIocpServer& operator=(const CIocpServer&) = delete;

private:
	TcpEngingPtr		iocp_engine_;

	accept_cb			accept_cb_;

	std::map<uint32_t, read_cb>			map_read_cb_;
	std::map<uint32_t, write_cb>		map_wirte_cb_;
	std::map<uint32_t, close_cb>		map_close_cb_;
};