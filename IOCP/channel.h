#include "itf_tcpengine.h"
#include "overlapped.h"
#include "service.h"
#include "DataBuffer.h"
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

public:
	QEngIOCPEventQueueService* get_iocp_service() {return m_pChannelService; }

public:
	void InitMgr ( QEngIOCPEventQueueService *iocpService, IEngTcpSink *tcpSink );

public:
	QChannelManager();
	virtual ~QChannelManager();

	CDataBuffer::TDataBlockPtrList* GetDBFreeList()
	{
		return &m_oDBFreeList;
	}

public:
	void HandleConnecttoOK( SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remoteAddr, uint32_t nMaxMsgSize ,void *pAtt);
	void HandleConnecttoFail(void *pAtt);

public:
	void CloseSession(uint32_t connid);
	bool OnAccepted ( SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remteAddr, uint32_t nMaxMsgSize );

protected:
	void HandleWriteData (uint32_t nConnId, char *pData, uint32_t nBytes );
public:
	virtual bool WriteData(uint32_t nConnId,char *pData,uint32_t nBytes);
	virtual bool PostWriteDataReq (uint32_t nConnId, char *pData, uint32_t nBytes );

private:
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
	friend class QChannelManager;

	QChannelManager*			m_pMgr;
	std::vector<char>			data_buffer_;

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
		DWORD		m_uRecvFlags;
		DWORD		m_dwRecvBytes;
		WSABUF		m_recvBuff;
		QChannel*	m_pChannel;
	};
	typedef TOverlappedWrapper<RecvHandler> RecvHandlerOverLapped;
#pragma endregion RecvHandler

public:
	QChannel(QChannelManager *pMgr);
	~QChannel();

public:
	void HandleWriteData ( char *pData, uint32_t nBytes );
	void ResetChannel (SOCKET hSocket,uint32_t nConnId, uint32_t nMaxMsgSize);

private:
	bool IsValid();
	void handleReadCompleted ( bool bSuccess, size_t dwIOBytes , WSABUF *pRecvBuff);
	void handleWriteCompleted ( bool bSuccess, size_t dwIOBytes, size_t dwWant );
	bool StartWrite();
	bool StartRead();

public:
	SOCKET					m_hSocket;

private:
	uint32_t				m_nConnId;

	bool					m_isConnectted;

	DWORD					m_nMaxMsgSize;

	CDataBuffer				m_sendBuff;
};
