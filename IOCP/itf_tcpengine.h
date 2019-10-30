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
	// 上一个读取操作时间
	__time64_t tmRead;
	__time64_t tmWrite;
	// 联接建立时间
	__time64_t connTime;


	// 当前发送缓冲中的数据长度
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
		WriteData 与PostWriteDataReq差别
		PostWriteDataReq 是线程安全的
		而WriteData 不一定是，因此WriteData 必须ChannaelManager 同一个线程,注意 Post
	*/
	virtual bool WriteData(uint32_t nConnId,char *pData,uint32_t nBytes)=0;
	virtual bool PostWriteDataReq(uint32_t nConnId,char *pData,uint32_t nBytes)=0;
};

struct ITcpEngine;
struct IEngTcpSink
{
	virtual bool OnAccepted		(uint32_t nConnId) = 0;
	virtual void OnRead			(uint32_t nConnId, char *pData, int32_t iread) = 0;
	virtual void OnWrite		(uint32_t nConnId, int32_t iwrite) = 0;
	virtual void OnConnect		(bool isOK, uint32_t nConnId,void *pAttData)=0;
};
/*
 *	
 nProtocol =0x04 4 byte 为报文头，不包括本身4字节
 nProtocol =0x02 2 byte 为报文头，不包括本身2字节
 nProtocol =0x16 6 为报文头，不包括本身6字节 前6字节为字符串需要转成atoi
 */
struct ITcpEngine
{
public:
	/*
	   按理说来 应该 把SetListenAddr 与StartEngine 合并，但这样函数参数太多，看起来不舒服，所以做成二个函数，
		因此 SetListenAddr 是必须调用的，而且只有最后一次调用有效，此外只有在StartEngine调用之前有效
	 */
	virtual bool SetListenAddr(uint32_t nMaxMsgSize,uint16_t nPort,const char * szIp=0,uint8_t nProtocol=0x04)=0;
	virtual bool StartEngine ( IEngTcpSink *tcpSink)=0;
	virtual void Loop() = 0;
	virtual void DestoryEngine() = 0;
	virtual void CloseSession(uint32_t connid) = 0;
	virtual void write_data(uint32_t session_id, char* data, uint32_t size) = 0;
	virtual bool ConnectTo ( const  char * szIp, uint16_t nPort,  void *pAttData,uint32_t nMaxMsgSize, uint8_t nProtocol = 0x04 )= 0;

	// 处理tcp 收发的线程
	virtual IEngIOCPService * GetIOCPService()=0;

	virtual IEngChannelManager * GetChannelMgr()=0;

};

ITcpEngine *		CreateTcpEngine();



