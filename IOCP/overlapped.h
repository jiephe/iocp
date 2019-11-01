#pragma once

#include <Windows.h>
#include <memory.h>

class CBaseOverlapped 
{
public:
	CBaseOverlapped()
	{
		memset(&m_overlap, 0x0, sizeof(OVERLAPPED));
	}

public:
	virtual void HandleComplete(ULONG_PTR pKey, size_t nIOBytes) {}
	virtual void HandleError(ULONG_PTR pKey, size_t nIOBytes) {}
	virtual void Destroy() {}

	OVERLAPPED m_overlap;
};
using BaseOverlappedPtr = std::shared_ptr<CBaseOverlapped>;