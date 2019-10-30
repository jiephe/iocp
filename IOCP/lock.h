#pragma once

class CCriticalSection
{
	CRITICAL_SECTION m_lock;

public:
	bool TryLock()
	{
		TryEnterCriticalSection ( &m_lock );
	}
	void Lock()
	{
		EnterCriticalSection ( &m_lock );
	}
	void UnLock()
	{
		LeaveCriticalSection ( &m_lock );
	}

	CCriticalSection()
	{
		InitializeCriticalSection ( &m_lock );
	}
	~CCriticalSection()
	{
		DeleteCriticalSection ( &m_lock );
	}
};

class CAutoLock
{
	CCriticalSection &m_cs;
	CAutoLock();
	const CAutoLock& operator= ( const CAutoLock& );

public:
	explicit CAutoLock ( CCriticalSection &lock ) : m_cs ( lock )
	{
		lock.Lock();
	}

	~CAutoLock()
	{
		m_cs.UnLock();
	}
};