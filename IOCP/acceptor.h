#include "itf_tcpengine.h"
#include "service.h"
#include "limit_define.h"
#include <map>

class QAcceptor;
class QSessionManager;

class QAcceptor
{
#pragma region Accept
	class AcceptHandler: public IIOCPHandler
	{
	public:
		virtual void HandleComplete ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void HandleError ( ULONG_PTR pKey, size_t nIOBytes );
		virtual void Destroy();

		SOCKET m_hSocket;
		char m_buffer[MAX_BUFF_SIZE];
		QAcceptor *m_acceptor;
	};
	typedef TOverlappedWrapper<AcceptHandler> AcceptHandlerOverLapped;
#pragma endregion 

public:
	bool start (std::shared_ptr<QSessionManager>, uint16_t nPort, const char * szIP, uint32_t nMaxMsgSize);
	void stop();

public:
	QAcceptor();
	~QAcceptor();

private:
	void RemoveFromMap(AcceptHandler*p); 

	void AddOneAccept();
	void AddBatchAccept();
	bool AddOneAcceptEx();	
	bool AddOneAcceptExtension();

	void OnAccpetFinish ( AcceptHandler*pHandler );

private:
	SOCKET												m_hListenSocket;
	uint32_t											m_nMaxMsgSize;
	std::map<AcceptHandler*, AcceptHandlerOverLapped*>	m_needDelete;
	std::shared_ptr<QSessionManager>					m_pMgr;
	LPFN_ACCEPTEX										fnAcceptEx_;
};