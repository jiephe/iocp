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

	iocp_engine_->StartEngine(this, 1000);

	iocp_engine_->Loop();
}

void CIocpServer::read_start(uint64_t session_id, read_cb cb)
{
	map_read_cb_[session_id] = cb;
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
void CIocpServer::OnClose(uint32_t nConnId)
{
	int a = 1;
}
void CIocpServer::OnData(uint32_t nConnId, char *pData, DWORD dwBytes)
{
	auto itor = map_read_cb_.find(nConnId);
	if (itor != map_read_cb_.end())
		(itor->second)(this, pData, dwBytes);
}
bool CIocpServer::OnIdle(uint32_t nConnId, uint32_t nIdleMS)
{
	int a = 1;
	return true;
}
void CIocpServer::OnConnect(bool isOK, uint32_t nConnId, void *pAttData)
{
	int a = 1;
}
void CIocpServer::OnPulse(uint64_t  nMsPassedAfterStart)
{
	int a = 1;
	printf("onPulse %u\n", GetTickCount());
}