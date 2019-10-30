#pragma once
#include "itf_tcpengine.h"
#include "overlapped.h"
#include <memory>
#include <vector>
#include <thread>
#include <map>

struct IocpTimerData
{
	IocpTimer* hash;
	timer_cb cb;
	uint32_t repeat;
	uint32_t interval;		//ms
};
typedef std::shared_ptr<IocpTimerData>	IocpTimerDataPtr;

class QEngIOCPEventQueueService: public IEngIOCPService
{
	struct UserEventHandler:public IIOCPHandler
	{
		IEngEventHandler *m_pUserHandler;
		virtual void HandleComplete(ULONG_PTR ,size_t )
		{
			m_pUserHandler->Fire();
		}

		virtual void HandleError(ULONG_PTR ,size_t )
		{
			m_pUserHandler->HandleError();
		}

		virtual void Destroy()
		{
			m_pUserHandler->Destroy();
			delete this;
		}
	};

public:
	virtual bool BeginService() ;
	virtual void DestroyService();

	virtual bool PostUserEvent(IEngEventHandler*pEvtHandler);

	virtual void add_iocp_timer(IocpTimer* pTimer, bool bRepeat, uint32_t uMS, timer_cb cb);
	virtual void kill_iocp_timer(IocpTimer* hHandle);

	void erase_from_hash(void* hash);

	void run();
	void poll(uint32_t timeout);

	void process_timers();

	void update_loop_timer(double scale);
public:
	bool BindHandleToIocp(HANDLE hHandle);
	bool PostRequest(DWORD nWantIOBytes,void * pKey,LPOVERLAPPED ol);

private:
	HANDLE									m_hIOCP;
	uint64_t								loop_time_;
	std::map<void*, bool>					timer_hash_;
	std::map<uint64_t, IocpTimerDataPtr>	timer_list_;
	double									hrtime_interval_;
};



