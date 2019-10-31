#pragma once

#include "itf_tcpengine.h"
#include "overlapped.h"
#include "service.h"
#include <map>

class QSession;
using QSessionPtr = std::shared_ptr<QSession>;

class QSessionManager;
using QSessionMgrPtr = std::shared_ptr<QSessionManager>;

class QSessionManager: public IEngSessionManager, public std::enable_shared_from_this<QSessionManager>
{
	std::map<uint32_t, QSessionPtr>						session_list_;
	TcpSinkPtr											tcp_sink_;
	IocpServicePtr										iocp_service_;

public:
	IocpServicePtr get_iocp_service() {return iocp_service_; }
	TcpSinkPtr	get_tcp_sink() { return tcp_sink_; }

public:
	QSessionManager(IocpServicePtr iocpService, TcpSinkPtr tcpSink);
	virtual ~QSessionManager();

public:
	void HandleConnecttoOK( SOCKET hSocket, SOCKADDR_IN *localAddr, SOCKADDR_IN *remoteAddr, uint32_t nMaxMsgSize ,void *pAtt);
	void HandleConnecttoFail(void *pAtt);

public:
	bool OnAccepted ( SOCKET hSocket, uint32_t nMaxMsgSize );
	
protected:
	void HandleWriteData (uint32_t session_id, char* data, uint32_t size);
	void HandleCloseSession(uint32_t session_id);
public:
	virtual bool WriteData(uint32_t session_id, char* data, uint32_t size);
	virtual bool PostWriteDataReq (uint32_t session_id, char* data, uint32_t size);
	virtual void PostCloseSessionReq(uint32_t session_id);

private:
#pragma region CloseSession
	class CloseSession : public IIOCPHandler
	{
	public:
		virtual void HandleComplete(ULONG_PTR pKey, size_t);
		virtual void HandleError(ULONG_PTR, size_t) {}
		virtual void Destroy() {delete this;}
		QSessionMgrPtr		m_pMgr;
		uint32_t			m_nSessionId;
	};
	typedef TOverlappedWrapper<CloseSession> CloseSessionOverLapped;
#pragma endregion CloseSession
	friend class CloseSession;

#pragma region AppendSend
	class AppendSend: public IIOCPHandler
	{
	public:
		virtual void HandleComplete ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void HandleError ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void Destroy();
		QSessionMgrPtr m_pMgr;
	private:
		char*		m_pBuffer;
		size_t		m_nBuffLen;
		uint32_t	m_nSessionId;
	public:
		bool AppendBuffer ( uint32_t session_id, char *pData, uint32_t nBytes );
		AppendSend();
		~AppendSend();
	};
	typedef TOverlappedWrapper<AppendSend> AppendSendOverLapped;
#pragma endregion AppendSend

	friend class AppendSend;
};

class QSession : public std::enable_shared_from_this<QSession>
{
#pragma region SendHandler
	class SendHandler: public IIOCPHandler
	{
	public:
		SendHandler();
	public:
		virtual void HandleComplete ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void HandleError ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void Destroy();
	public:
		std::string			send_data;
		WSABUF				m_sendBuff;
		QSessionPtr			m_pSession;
		bool				m_can_delete;
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
		QSessionPtr	m_pSession;
	};
	typedef TOverlappedWrapper<RecvHandler> RecvHandlerOverLapped;
#pragma endregion RecvHandler

public:
	QSession(QSessionMgrPtr ptr, SOCKET s, uint32_t nMaxMsgSize);
	~QSession();

public:
	uint32_t get_session_id() { return session_id_; }

	void set_b_closing(bool b) { b_closing_ = b; }

public:
	void HandleWriteData (char* data, uint32_t size);
	void HandleRead();

private:
	void handleReadCompleted ( bool bSuccess, size_t dwIOBytes , WSABUF *pRecvBuff);
	void handleWriteCompleted ( bool bSuccess, size_t dwIOBytes, size_t dwWant);
	bool StartWrite(char* data, uint32_t size);
	bool StartWriteAgain(SendHandlerOverLapped* p);
	bool StartRead();

public:
	SOCKET					m_hSocket;

private:
	uint32_t				session_id_;

	bool					b_closing_;

	DWORD					m_nMaxMsgSize;

	QSessionMgrPtr			m_pMgr;

	std::vector<char>		recv_buffer_;
};
