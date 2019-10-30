#include <Windows.h>
#include "service.h"

/* The number of milliseconds in one second. */
#define UV__MILLISEC 1000

void QEngIOCPEventQueueService::update_loop_timer(double scale)
{
	LARGE_INTEGER counter;

	if (!QueryPerformanceCounter(&counter)) 
		return ;

	/* Because we have no guarantee about the order of magnitude of the
	* performance counter interval, integer math could cause this computation
	* to overflow. Therefore we resort to floating point math.
	*/
	loop_time_ = (uint64_t)((double)counter.QuadPart * hrtime_interval_ * scale);
}

void QEngIOCPEventQueueService::poll(uint32_t timeout)
{
	DWORD nIOBytes;
	ULONG_PTR pKey;
	QOverlapped* pOverlap;

	uint64_t timeout_time = loop_time_ + timeout;

	for (int repeat = 0; ; repeat++)
	{
		BOOL ret = ::GetQueuedCompletionStatus(m_hIOCP, &nIOBytes, &pKey, (LPOVERLAPPED*)&pOverlap, timeout);
		if (NULL != pOverlap)
		{
			if (GetLastError() != 0)
				printf("GetQueuedCompletionStatus error : %d\n", GetLastError());

			if (ret)
				pOverlap->m_pHandler->HandleComplete(pKey, (size_t)nIOBytes);
			else
				pOverlap->m_pHandler->HandleError(pKey, WSAGetLastError());

			pOverlap->m_pHandler->Destroy();
		}
		else if (GetLastError() != WAIT_TIMEOUT)
		{
			printf("GetQueuedCompletionStatus error : %d\n", GetLastError());
		}
		else if (timeout > 0)
		{
			update_loop_timer(UV__MILLISEC);
			if (timeout_time > loop_time_)
			{
				timeout = (DWORD)(timeout_time - loop_time_);
				timeout += repeat ? (1 << (repeat - 1)) : 0;
				continue;
			}
		}

		break;
	}
}

void QEngIOCPEventQueueService::run()
{
	for ( ;; )
	{
		update_loop_timer(UV__MILLISEC);

		process_timers();

		uint32_t timeout = INFINITE;
		if (!timer_list_.empty())
		{
			auto mini = timer_list_.begin();
			if (mini->first <= loop_time_)
				timeout = 0;
			else
				timeout = mini->first - loop_time_;
		}

		//PostRequest(0, 0, 0);

		poll(timeout);
	}
}

bool QEngIOCPEventQueueService::BeginService()
{
	WSADATA WSAData;
	int iErrorCode = WSAStartup (MAKEWORD(2, 2), &WSAData);
	if (0 != iErrorCode) 
		return false;

	m_hIOCP = CreateIoCompletionPort (INVALID_HANDLE_VALUE, NULL, NULL, 1);
	if (nullptr == m_hIOCP)
		return false;

	LARGE_INTEGER perf_frequency;
	if (QueryPerformanceFrequency(&perf_frequency))
		hrtime_interval_ = 1.0 / perf_frequency.QuadPart;
	else
		hrtime_interval_ = 0;

	update_loop_timer(UV__MILLISEC);
	return true;
}

void QEngIOCPEventQueueService::DestroyService()
{
	::PostQueuedCompletionStatus(m_hIOCP,0,NULL,NULL);

	WSACleanup();
}

bool QEngIOCPEventQueueService::PostUserEvent(IEngEventHandler*pEvtHandler)
{
	TOverlappedWrapper<UserEventHandler>* handler = new TOverlappedWrapper<UserEventHandler>();
	handler->m_pUserHandler = pEvtHandler;

#pragma warning(disable:4800)
	return PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)handler, &handler->m_overlap);
}

bool QEngIOCPEventQueueService::BindHandleToIocp( HANDLE hHandle )
{
	return (NULL != ::CreateIoCompletionPort(hHandle, m_hIOCP, 0, 1));
}

bool QEngIOCPEventQueueService::PostRequest(DWORD, void * pKey, LPOVERLAPPED ol)
{
	return PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)pKey, ol);
}

void QEngIOCPEventQueueService::add_iocp_timer(IocpTimer* pTimer, bool bRepeat, uint32_t uMS, timer_cb cb)
{
	return;
	if (timer_hash_.find(pTimer) == timer_hash_.end())
	{
		auto data = std::make_shared<IocpTimerData>();
		data->cb = cb;
		data->hash = pTimer;
		data->repeat = bRepeat;
		data->interval = uMS;
		timer_list_[loop_time_ + uMS] = data;
	}
}

void QEngIOCPEventQueueService::kill_iocp_timer(IocpTimer* hHandle)
{
	auto iter = timer_hash_.find(hHandle);
	if (iter != timer_hash_.end())
	{
		timer_hash_.erase(iter);

		for (auto itor = timer_list_.begin(); itor != timer_list_.end(); ++itor)
		{
			if (itor->second->hash == hHandle)
			{
				timer_list_.erase(itor);
				break;
			}
		}
	}
}

void QEngIOCPEventQueueService::erase_from_hash(void* hash)
{
	auto iter = timer_hash_.find(hash);
	if (iter != timer_hash_.end())
	{
		timer_hash_.erase(iter);
	}
}

void QEngIOCPEventQueueService::process_timers()
{
	std::map<int64_t, IocpTimerDataPtr>		tmp;
	auto itor = timer_list_.begin();
	while (itor != timer_list_.end())
	{
		if (itor->first <= loop_time_)
		{
			itor->second->cb(itor->second->hash);
			if (!itor->second->repeat)
			{
				erase_from_hash(itor->second->hash);
				itor = timer_list_.erase(itor);
			}
			else
			{
				tmp[loop_time_ + itor->second->interval] = itor->second;
				itor = timer_list_.erase(itor);
			}
		}
		else
			break;
	}

	for (auto iter = tmp.begin(); iter != tmp.end(); ++iter)
	{
		timer_list_[iter->first] = iter->second;
	}
}

