#pragma once

#include "itf_tcpengine.h"
#include "overlapped.h"
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
public:
	virtual bool start() ;
	virtual void stop();

	virtual void add_iocp_timer(IocpTimer* pTimer, bool bRepeat, uint32_t uMS, timer_cb cb);
	virtual void kill_iocp_timer(IocpTimer* hHandle);

	void erase_from_hash(void* hash);

	void run();
	void poll(uint32_t timeout);

	void process_timers();

	void update_loop_timer(double scale);
public:
	bool BindHandleToIocp(HANDLE hHandle);
	BOOL PostRequest(DWORD nWantIOBytes,void * pKey,LPOVERLAPPED ol);

private:
	HANDLE									m_hIOCP;
	uint64_t								loop_time_;
	std::map<void*, bool>					timer_hash_;
	std::map<uint64_t, IocpTimerDataPtr>	timer_list_;
	double									hrtime_interval_;
};

using IocpServicePtr = std::shared_ptr<QEngIOCPEventQueueService>;



