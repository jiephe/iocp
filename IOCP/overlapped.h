#pragma once

#include <Windows.h>

struct IIOCPHandler
{
	virtual void HandleComplete(ULONG_PTR pKey,size_t nIOBytes)=0;
	virtual void HandleError(ULONG_PTR pKey,size_t nIOBytes)=0;
	virtual void Destroy()=0;
	virtual ~IIOCPHandler(){}
};

struct QOverlapped : public OVERLAPPED
{
	IIOCPHandler* m_pHandler;
};

template<class T>
struct TOverlappedWrapper : T
{
	QOverlapped m_overlap;

	TOverlappedWrapper()
	{
		ZeroMemory ( &m_overlap, sizeof ( m_overlap ) );
		m_overlap.m_pHandler = this;
	}

	operator OVERLAPPED*()
	{
		return &m_overlap;
	}
};