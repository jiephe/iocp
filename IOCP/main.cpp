#include "itf_tcpengine.h"

class CTcp : public IEngTcpSink
{
	virtual bool OnAccepted(uint64_t nConnId, TChannelInfo *info);
	virtual void OnClose(uint64_t nConnId, TChannelInfo *info);
	virtual void OnData(uint64_t nConnId, char *pData, DWORD dwBytes, TChannelInfo *info);
	virtual bool OnIdle(uint64_t nConnId, uint32_t nIdleMS, TChannelInfo*info);
	virtual void OnConnectToResult(bool isOK, uint64_t nConnId, void *pAttData, TChannelInfo*info);

	virtual void OnPulse(uint64_t  nMsPassedAfterStart);
};

bool CTcp::OnAccepted(uint64_t nConnId, TChannelInfo *info)
{
	int a = 1;
	return true;
}
void CTcp::OnClose(uint64_t nConnId, TChannelInfo *info)
{
	int a = 1;
}
void CTcp::OnData(uint64_t nConnId, char *pData, DWORD dwBytes, TChannelInfo *info)
{
	int a = 1;
}
bool CTcp::OnIdle(uint64_t nConnId, uint32_t nIdleMS, TChannelInfo*info)
{
	int a = 1;
	return true;
}
void CTcp::OnConnectToResult(bool isOK, uint64_t nConnId, void *pAttData, TChannelInfo*info)
{
	int a = 1;
}
void CTcp::OnPulse(uint64_t  nMsPassedAfterStart)
{
	int a = 1;
	printf("onPulse %u\n", GetTickCount());
}

int __cdecl main(int argc, char* argv[])
{
	ITcpEngine* iocp = CreateTcpEngine();

	iocp->SetListenAddr(819200, 26000, "0.0.0.0", 0x02);

	CTcp* pTcp = new CTcp;

	iocp->StartEngine(pTcp, 1000);

	iocp->Loop();

	return 0;
}