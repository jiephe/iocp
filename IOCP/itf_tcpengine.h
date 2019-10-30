#pragma once

#include <stdint.h>
#include <WinSock2.h>
#include <MSWSock.h>
#include <functional>

typedef struct _IocpTimer
{
	void* userdata;
}IocpTimer;

typedef std::function<void(IocpTimer* timer)> timer_cb;

struct TChannelInfo
{
	uint64_t nConnId;
	SOCKADDR_IN local;
	SOCKADDR_IN remote;
	uint64_t inBytes;
	uint64_t outBytes;
	uint64_t readCall;
	uint64_t writeCall;
	bool		isClient;
	// ��һ����ȡ����ʱ��
	__time64_t tmRead;
	__time64_t tmWrite;
	// ���ӽ���ʱ��
	__time64_t connTime;


	// ��ǰ���ͻ����е����ݳ���
	uint64_t writeBuffer;
	void * user_ptr;

	TChannelInfo::TChannelInfo()
	{
		user_ptr = NULL;
	}
};

struct IEngEventHandler
{
public:
	virtual void Fire()=0;
	virtual void HandleError()=0;
	virtual void Destroy()=0;
};

struct IEngIOCPService
{
public:
	virtual bool BeginService()=0;
	virtual void DestroyService()=0;
	
	virtual void add_iocp_timer(IocpTimer* pTimer, bool bRepeat, uint32_t uMS, timer_cb cb) = 0;
	virtual void kill_iocp_timer(IocpTimer* hHandle) = 0;

	virtual bool PostUserEvent(IEngEventHandler*pEvtHandler)=0;
};

struct IEngChannelManager
{
public:
	/* 
		WriteData ��PostWriteDataReq���
		PostWriteDataReq ���̰߳�ȫ��
		��WriteData ��һ���ǣ����WriteData ����ChannaelManager ͬһ���߳�,ע�� Post
	*/
	virtual bool WriteData(uint64_t nConnId,char *pData,uint32_t nBytes)=0;
	virtual bool PostWriteDataReq(uint64_t nConnId,char *pData,uint32_t nBytes)=0;
	virtual bool IsValidConn(uint64_t nConnId)=0;
	virtual void PostCloseConnReq(uint64_t nConnId,bool bWaitingLastWriteDataFinish)=0;

	virtual __time64_t now()=0;
	virtual int  rand()=0;
};

struct ITcpEngine;
struct IEngTcpSink
{
	virtual bool OnAccepted		( uint64_t nConnId,TChannelInfo *info) = 0;
	virtual void OnClose			( uint64_t nConnId,TChannelInfo *info) = 0;
	virtual void OnData			( uint64_t nConnId, char *pData, DWORD dwBytes ,TChannelInfo *info) = 0;
	virtual bool OnIdle			( uint64_t nConnId, uint32_t nIdleMS,TChannelInfo*info ) = 0;
	virtual void OnConnectToResult( bool isOK,uint64_t nConnId,void *pAttData,TChannelInfo*info)=0;

	virtual void OnPulse			( uint64_t  nMsPassedAfterStart ) = 0;
};
/*
 *	
 nProtocol =0x04 4 byte Ϊ����ͷ������������4�ֽ�
 nProtocol =0x02 2 byte Ϊ����ͷ������������2�ֽ�
 nProtocol =0x16 6 Ϊ����ͷ������������6�ֽ� ǰ6�ֽ�Ϊ�ַ�����Ҫת��atoi
 */
struct ITcpEngine
{
public:
	/*
	   ����˵�� Ӧ�� ��SetListenAddr ��StartEngine �ϲ�����������������̫�࣬��������������������ɶ���������
		��� SetListenAddr �Ǳ�����õģ�����ֻ�����һ�ε�����Ч������ֻ����StartEngine����֮ǰ��Ч
	 */
	virtual bool SetListenAddr(uint32_t nMaxMsgSize,uint16_t nPort,const char * szIp=0,uint8_t nProtocol=0x04)=0;
	virtual bool StartEngine ( IEngTcpSink *tcpSink,uint32_t nPulseMS=0)=0;
	virtual void Loop() = 0;
	virtual void DestoryEngine() = 0;
	virtual bool ConnectTo ( const  char * szIp, uint16_t nPort,  void *pAttData,uint32_t nMaxMsgSize, uint8_t nProtocol = 0x04 )= 0;

	// ����tcp �շ����߳�
	virtual IEngIOCPService * GetIOCPService()=0;

	virtual IEngChannelManager * GetChannelMgr()=0;

};

ITcpEngine *		CreateTcpEngine();
IEngIOCPService *	CreateIOCPService();


