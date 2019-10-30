#include "iocp_server.h"

void OnRead(CIocpServer* iocp, char* data, int32_t size)
{

}

void OnAccept(CIocpServer* iocp, uint64_t session_id)
{
	iocp->read_start(session_id, OnRead);
}

int __cdecl main(int argc, char* argv[])
{
	CIocpServer* iocp = CIocpServer::get_iocp_server();

	iocp->init(26000);

	iocp->start(OnAccept);

	iocp->stop();

	return 0;
}