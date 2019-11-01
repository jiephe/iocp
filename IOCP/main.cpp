#include "iocp_server.h"
#include <vector>

#define ONE_M		(1024*1024*8)
#define TEN_M		(10*ONE_M)

void OnWrite(CIocpServerPtr iocp, uint32_t session_id, int32_t size)
{
	//size < 0 ���ùر� �д��󶼻��ߵ�OnRead��
	printf("session: %u write %d\n", session_id, size);
}

void OnClose(CIocpServerPtr iocp, uint32_t session_id)
{
	printf("session: %u close complete!\n", session_id);
}

void OnRead(CIocpServerPtr iocp, uint32_t session_id, char* data, int32_t size)
{
	if (size < 0)
	{
		printf("session: %u read %d\n", session_id, size);
		//Ҫ����Ϣ��GetQueuedCompletionStatus ȷ��close�����һ����Ϣ(GetQueuedCompletionStatus�����ٻص���Ϣ��) ͬʱclose֮�� �������ٷ���������Ϣ
		iocp->session_close(session_id, OnClose);
	}
	else
	{
		///for (int i = 0; i < 10; ++i)
		{
			std::vector<char> send_data(400, 'a');
			iocp->session_write_data(session_id, &send_data[0], send_data.size(), OnWrite);
		}
	}
}

void OnAccept(CIocpServerPtr iocp, uint32_t session_id)
{
	iocp->session_read_start(session_id, OnRead);
}

int __cdecl main(int argc, char* argv[])
{
	CIocpServerPtr iocp = CIocpServer::get_iocp_server();

	iocp->init(26000);

	iocp->start(OnAccept);

	iocp->stop();

	return 0;
}