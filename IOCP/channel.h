#include "itf_tcpengine.h"
#include "overlapped.h"
#include "service.h"
#include "DataBuffer.h"
#include "lock.h"
#include <list>

class QChannel;

typedef std::shared_ptr<QChannel>	QChannelPtr;

class QChannelManager:public IEngChannelManager
{
	friend class QChannel;
	std::list<QChannelPtr>			channel_list_;
	IEngTcpSink*					m_pChannelSink;
	QEngIOCPEventQueueService*		m_pChannelService;
	CDataBuffer::TDataBlockPtrList	m_oDBFreeList;
	uint64_t						m_nConnIdSeed;
	int32_t							m_nCountObj;
	IocpTimer						check_timer_;
	IocpTimer						close_timer_;

public:
	QEngIOCPEventQueueService* get_iocp_service() {return m_pChannelService; }

	static void timer_callback(IocpTimer* pTimer);
	
	void start_close_timer();
	static void close_callback(IocpTimer* pTimer);

public:
	void InitMgr ( QEngIOCPEventQueueService *iocpService, IEngTcpSink *tcpSink );

public:
	QChannelManager();
	virtual ~QChannelManager();

	CCriticalSection m_lock;
	CDataBuffer::TDataBlockPtrList* GetDBFreeList()
	{
		return &m_oDBFreeList;
	}

public:
	void HandleIdleCheck();
	void HandleConnecttoOK( SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remoteAddr, uint32_t nProtocolType, uint32_t nMaxMsgSize ,void *pAtt);
	void HandleConnecttoFail(void *pAtt);

public:
	void DealClosedChannel();
	bool OnAccepted ( SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remteAddr, uint32_t nProtocolType, uint32_t nMaxMsgSize );

protected:
	void HandleWriteData (uint32_t nConnId, char *pData, uint32_t nBytes );
	void HandleCloseChannel (uint32_t nConnId, bool bWaitingLastWriteDataFinish );

public:
	virtual bool WriteData(uint32_t nConnId,char *pData,uint32_t nBytes);
	virtual bool PostWriteDataReq (uint32_t nConnId, char *pData, uint32_t nBytes );
	virtual void PostCloseConnReq (uint32_t nConnId, bool bWaitingLastWriteDataFinish );

	virtual int  rand();

private:

#pragma region IdleCheck
	class IdleCheck: public IEngEventHandler
	{
	public:
		HANDLE hTimer;
		QChannelManager *m_pMgr;
		virtual void Fire ()
		{
			m_pMgr->HandleIdleCheck();
		}
	virtual void HandleError(){};
	virtual void Destroy(){delete this;}
	};
#pragma endregion IdleCheck

#pragma region CloseChannel
	class CloseChannel: public IIOCPHandler
	{
	public:
		QChannelManager *m_pMgr;
		virtual void HandleComplete ( ULONG_PTR pKey, size_t  );
		virtual void HandleError ( ULONG_PTR , size_t  ) {}
		virtual void Destroy()
		{
			delete this;
		};
		uint32_t m_nConnId;
		bool m_bFlags;
	};
	typedef TOverlappedWrapper<CloseChannel> CloseChannelOverLapped;
	friend class CloseChannel;
#pragma endregion CloseChannel

#pragma region AppendSend
	class AppendSend: public IIOCPHandler
	{
	public:
		virtual void HandleComplete ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void HandleError ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void Destroy();
		QChannelManager *m_pMgr;
	private:
		char *m_pBuffer;
		size_t m_nBuffLen;
		uint32_t m_nConnId;
	public:
		bool AppendBuffer ( uint32_t nConnId, char *pData, uint32_t nBytes );
		AppendSend();
		~AppendSend();
	};
	typedef TOverlappedWrapper<AppendSend> AppendSendOverLapped;
#pragma endregion AppendSend

	friend class AppendSend;
};

class QChannel
{
	int m_nLastWSAError;

	friend class QChannelManager;

	QChannelManager*m_pMgr;

#pragma region SendHandler
	class SendHandler: public IIOCPHandler
	{
	public:
		virtual void HandleComplete ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void HandleError ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void Destroy();
	public:
		
			QChannelManager *m_pMgr;
		WSABUF  m_sendBuff;
		QChannel * m_pChannel;
	};
	typedef TOverlappedWrapper<SendHandler> SendHandlerOverLapped;
#pragma endregion SendHandler

#pragma region RecvHandler
	class RecvHandler: public IIOCPHandler
	{
	public:
		virtual void HandleComplete ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void HandleError ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void Destroy();
	public:
		DWORD m_uRecvFlags;
		DWORD m_dwRecvBytes;
		WSABUF m_recvBuff;
		QChannel * m_pChannel;
		uint64_t m_nId;
			QChannelManager *m_pMgr;
	};
	typedef TOverlappedWrapper<RecvHandler> RecvHandlerOverLapped;
#pragma endregion RecvHandler

public:
	QChannel(QChannelManager *pMgr);
	~QChannel();

public:
	SOCKET m_hSocket;
	SOCKADDR_IN m_localAddress;
	SOCKADDR_IN	m_remoteAddress;

public:
	// bWaitingLastWriteDataSuccess 是否等待最后一个writedata所发送的数据完成。
	void HandleClose ( bool bWaitingLastWriteDataSuccess ) ;
	void HandleWriteData ( char *pData, uint32_t nBytes );
	void ResetChannel ( SOCKET hSocket,uint32_t nConnId, SOCKADDR_IN *local, SOCKADDR_IN *remote, uint32_t nProtocolType, uint32_t nMaxMsgSize,bool isClient );
	uint32_t CalcIdelSeconds(__time64_t tnow );

private:
	bool IsValid();
	void handleReadCompleted ( bool bSuccess, size_t dwIOBytes , WSABUF *pRecvBuff);
	void handleWriteCompleted ( bool bSuccess, size_t dwIOBytes, size_t dwWant );
	bool HandleProtocol();

	void OnIOError(bool ToWriteError);
	bool StartWrite();
	bool StartRead();
	DWORD CalcMessageSize(int nDataLen);

	void dumpChannel(std::string &s);

	bool isClosed() {
		return m_isCloseHasNotified;
	}

private:
	TChannelInfo m_info;

	// 联接id
	uint32_t m_nConnId;
	//

	/// 状态：读
	bool m_isInReading;

	/// 状态：写
	bool m_isInWritting;

	/// 状态：联接
	bool m_isConnectted;

	/// 状态：主动关闭
	bool m_isAbortRead;;

	bool m_isClient;
	bool m_hasCloseSocket;
	bool m_isCloseHasNotified;

	// 报文协议相关
	int m_nProtocolType;
	DWORD m_nMaxMsgSize;

	CDataBuffer m_recvBuff;
	CDataBuffer m_sendBuff;
};
