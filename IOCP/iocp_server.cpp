#include "iocp_server.h"

CIocpServer* CIocpServer::iocp_server_ = nullptr;

CIocpServer* CIocpServer::get_iocp_server()
{
	if (iocp_server_)
		return iocp_server_;
	else
	{
		iocp_server_ = new CIocpServer();
		return iocp_server_;
	}
}

CIocpServer::CIocpServer()
{}

CIocpServer::~CIocpServer()
{}

void CIocpServer::init(uint16_t port)
{
	iocp_engine_ = CreateTcpEngine();
	iocp_engine_->SetListenAddr(819200, port, "0.0.0.0", 0x02);
}

void CIocpServer::start(accept_cb cb)
{
	accept_cb_ = cb;

	iocp_engine_->StartEngine(this);

	iocp_engine_->Loop();
}

void CIocpServer::session_read_start(uint32_t session_id, read_cb cb)
{
	map_read_cb_[session_id] = cb;
}

void CIocpServer::session_write_data(uint32_t session_id, char* data, uint32_t size, write_cb cb)
{
	map_wirte_cb_[session_id] = cb;

	iocp_engine_->write_data(session_id, data, size);
}

void CIocpServer::session_close(uint32_t session_id)
{
	iocp_engine_->CloseSession(session_id);
}

void CIocpServer::stop()
{
	iocp_engine_->DestoryEngine();
}

bool CIocpServer::OnAccepted(uint32_t nConnId)
{
	if (accept_cb_)
		accept_cb_(this, nConnId);

	return true;
}
void CIocpServer::OnRead(uint32_t nConnId, char *pData, int32_t iread)
{
	auto itor = map_read_cb_.find(nConnId);
	if (itor != map_read_cb_.end())
		(itor->second)(this, nConnId, pData, iread);
}
void  CIocpServer::OnWrite(uint32_t nConnId, int32_t iwrite)
{
	auto itor = map_wirte_cb_.find(nConnId);
	if (itor != map_wirte_cb_.end())
		(itor->second)(this, nConnId, iwrite);
}
void CIocpServer::OnConnect(bool isOK, uint32_t nConnId, void *pAttData)
{
	int a = 1;
}
