#include "iocp_server.h"
#include "limit_define.h"

CIocpServerPtr CIocpServer::iocp_server_ = nullptr;
CIocpServerPtr CIocpServer::get_iocp_server()
{
	if (iocp_server_.get())
		return iocp_server_;
	else
	{
		iocp_server_.reset(new CIocpServer());
		return iocp_server_;
	}
}

CIocpServer::CIocpServer()
{}

CIocpServer::~CIocpServer()
{}

void CIocpServer::init(uint16_t port)
{
	iocp_engine_ = get_tcp_engine();
	iocp_engine_->SetListenAddr(MAX_MSG_SIZE, port, "0.0.0.0");
}

BOOL WINAPI CIocpServer::consoleHandler(DWORD signal)
{
	if (signal == CTRL_C_EVENT)
	{
		get_tcp_engine()->break_loop();
	}

	return TRUE;
}

void CIocpServer::start(accept_cb cb)
{
	SetConsoleCtrlHandler(consoleHandler, TRUE);

	accept_cb_ = cb;

	iocp_engine_->start(shared_from_this());

	iocp_engine_->loop();
}

void CIocpServer::session_read_start(uint32_t session_id, read_cb cb)
{
	map_read_cb_[session_id] = cb;
}

void CIocpServer::session_write_data(uint32_t session_id, char* data, uint32_t size, write_cb cb)
{
	map_wirte_cb_[session_id] = cb;

	iocp_engine_->WriteData(session_id, data, size);
}

void CIocpServer::session_close(uint32_t session_id, close_cb cb)
{
	map_close_cb_[session_id] = cb;
	iocp_engine_->CloseSession(session_id);
}

void CIocpServer::stop()
{
	iocp_engine_->stop();
}

bool CIocpServer::OnAccepted(uint32_t session_id)
{
	if (accept_cb_)
		accept_cb_(shared_from_this(), session_id);

	return true;
}
void CIocpServer::OnRead(uint32_t session_id, char *data, int32_t iread)
{
	auto itor = map_read_cb_.find(session_id);
	if (itor != map_read_cb_.end())
		(itor->second)(shared_from_this(), session_id, data, iread);
}
void  CIocpServer::OnWrite(uint32_t session_id, int32_t iwrite)
{
	auto itor = map_wirte_cb_.find(session_id);
	if (itor != map_wirte_cb_.end())
		(itor->second)(shared_from_this(), session_id, iwrite);
}

void CIocpServer::OnClose(uint32_t session_id)
{
	auto itor = map_close_cb_.find(session_id);
	if (itor != map_close_cb_.end())
		(itor->second)(shared_from_this(), session_id);
}

void CIocpServer::OnConnect(bool isOK, uint32_t session_id, void *pAttData)
{
	
}


