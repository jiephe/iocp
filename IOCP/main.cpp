#include "iocp_server.h"
#include <vector>

void OnWrite(CIocpServer* iocp, uint32_t session_id, int32_t size)
{
	if (size < 0)
		iocp->session_close(session_id);
	printf("conn: %u write %d\n", session_id, size);
}

void OnRead(CIocpServer* iocp, uint32_t session_id, char* data, int32_t size)
{
	int a = 1;
	printf("conn: %u read %d\n", session_id, size);
	if (size < 0)
		iocp->session_close(session_id);
	else
	{
		std::vector<char> send_data(10000, 'a');
		iocp->session_write_data(session_id, &send_data[0], send_data.size(), OnWrite);
	}
}

void OnAccept(CIocpServer* iocp, uint32_t session_id)
{
	iocp->session_read_start(session_id, OnRead);
}

int __cdecl main(int argc, char* argv[])
{
	CIocpServer* iocp = CIocpServer::get_iocp_server();

	iocp->init(26000);

	iocp->start(OnAccept);

	iocp->stop();

	return 0;
}