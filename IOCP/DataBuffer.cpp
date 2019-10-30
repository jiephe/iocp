#include "overlapped.h"
#include "limit_define.h"
#include "address.h"
#include "DataBuffer.h"
#include <cassert>

//////////////////////////////////////////////////////////////////////////

/*
 *
 ���Ը��ݲ�ͬ����Ҫ�����ô�С��
 һ����Զ����б��Ľṹ��Ӧ�ã����Բ��ð���80%���ĵĴ�С���ɣ�
 ��sgip/cmpp/cngp
 ������ý�壬��˴�С����,���Ե�����1M���ϣ�����ftpӦ�ÿ��Ե�����512k������
 ����httpͬ�����Ե�����256k������
 �̶�ռ�õ��ڴ�ռ�
 DB_SIZE*2*����������1�������,������Ҫռ��10k*8k=80M�ڴ�ռ䣬
 ����С����Ӧ����˵��4k�ǿ��Եģ�������Ը�Сһ�㣬����֧�ָ���Ĳ������ӡ�
 */

CDataBuffer::DataBlock::DataBlock()
{
	m_pDBBuffer = new char [DB_SIZE+2];
	assert ( m_pDBBuffer != NULL );
	reset();
}

void CDataBuffer::DataBlock::reset()
{
	memset(m_pDBBuffer, 0, DB_SIZE + 2);
	m_pDataPtr = m_pDBBuffer;
	m_pNextDB = NULL;
	m_nDBUsed = 0;
}

/*
 *
 �ӻ�����ȡ�����ݿ顣����еĻ�����������������һ����
 �����Ӧ��ok.
 */
CDataBuffer::DataBlock* CDataBuffer::newDataBlock()
{
	DataBlock *p = NULL;
	if ( m_pDBList->empty() )
	{
		p = new DataBlock ();
	}
	else
	{
		p = m_pDBList->front();
		m_pDBList->pop_front();
	}

	assert ( p != NULL );
	p->reset();

	return p;
}

void CDataBuffer::freeDataBlock ( CDataBuffer::DataBlock*p )
{
	if ( m_pDBList->size() >= MAX_FREE_DB )
	{
		delete p;
	}
	else
	{
		m_pDBList->push_back ( p );
	}
}

void CDataBuffer::AppendData ( size_t nBytes, char *pData )
{
	size_t nLeft = nBytes;
	for ( ; nLeft > 0; )
	{
		size_t t = m_tailDB->freedSize();
		if ( t >= min ( nLeft, OUTBUFF_ALIGN ) )
		{
			size_t nCopy = min ( t, nLeft );
			memcpy ( m_tailDB->m_pDataPtr + m_tailDB->m_nDBUsed, pData + nBytes - nLeft, nCopy );
			m_tailDB->m_nDBUsed += nCopy;
			nLeft -= nCopy;
		}
		if ( nLeft > 0 )
		{
			DataBlock *pp = newDataBlock();
			m_tailDB->m_pNextDB = pp;
			m_tailDB = pp;
			pp->m_pNextDB = 0;
		}
	}
}

void CDataBuffer::PopData ( size_t nBytes )
{
	//int nUsed = m_headerDB->m_nDBUsed;
	size_t nData = min ( m_headerDB->m_nDBUsed, nBytes );
	m_headerDB->m_pDataPtr += nData;
	m_headerDB->m_nDBUsed -= nData;
	if ( m_headerDB->m_nDBUsed == 0 )
	{
		if ( m_headerDB->m_pNextDB != NULL )
		{
			DataBlock *p = m_headerDB;
			m_headerDB = m_headerDB->m_pNextDB;
			freeDataBlock ( p );
		}
		else
		{
			assert ( nBytes == nData );
			m_headerDB->reset();
			//m_headerDB->m_pDataPtr = m_headerDB->m_pDBBuffer;
			return ;
		}
	}

	size_t nLeft = nBytes - nData;
	if ( nLeft > 0 )
	{
		PopData ( nLeft );
	}
}

WSABUF * CDataBuffer::GetDataPtr()
{
	assert ( m_headerDB );
	if ( !m_headerDB )
		return NULL;

	if ( m_headerDB->m_nDBUsed == 0 )
		return NULL;

	m_wsaBuffer.buf = m_headerDB->m_pDataPtr;
	m_wsaBuffer.len = (ULONG) m_headerDB->m_nDBUsed;

	return &m_wsaBuffer;
}

CDataBuffer::CDataBuffer ( TDataBlockPtrList *pList )
{
	m_pDBList = pList;
	m_headerDB = new DataBlock ();

	assert ( m_headerDB );

	m_tailDB = m_headerDB;
}

CDataBuffer::~CDataBuffer()
{
	while ( m_headerDB )
	{
		assert ( m_headerDB->m_pDBBuffer );
		DataBlock *p = m_headerDB->m_pNextDB;
		delete m_headerDB;
		m_headerDB = p;
	}
}

size_t CDataBuffer::TotalDataSize()
{
	size_t nRet = 0;
	DataBlock*p = m_headerDB;
	for ( ; p; )
	{
		nRet += p->DataSize();
		p = p->m_pNextDB;
	}

	return nRet;
}

bool CDataBuffer::NextData (DWORD dwWant, char *pBuff )
{
	size_t dwLeft = (size_t)dwWant;
	DataBlock*p = m_headerDB;
	char *pB = pBuff;
	for ( ; p != NULL; p = p->m_pNextDB )
	{
		size_t nCnt = min ( dwLeft, p->m_nDBUsed );
		memcpy ( pB, p->m_pDataPtr, nCnt );
		pB += nCnt;
		dwLeft -=  nCnt;
	}

	if ( dwLeft == 0 )
		return true;

	assert ( false );

	return false;
}

WSABUF * CDataBuffer::GetDataPtrFreeSpace()
{
	size_t nFree = m_tailDB->freedSize();
	if ( nFree < OUTBUFF_ALIGN )
	{
		m_tailDB->m_pNextDB = new DataBlock();
		m_tailDB = m_tailDB->m_pNextDB;

		assert ( m_tailDB );

		DataBlock *p = m_tailDB;
		nFree = p->freedSize();
	}

	m_wsaBuffer.buf = m_tailDB->m_pDataPtr + m_tailDB->m_nDBUsed;
	m_wsaBuffer.len = (ULONG) nFree;

	return &m_wsaBuffer;
}

void CDataBuffer::PushData ( size_t nBytes )
{
	m_tailDB->m_nDBUsed += nBytes;
}

void CDataBuffer::Reset()
{
	DataBlock *p = m_headerDB;
	for ( ; p != NULL; p = p->m_pNextDB )
	{
		DataBlock *pn = p->m_pNextDB;
		p->reset();
		p->m_pNextDB = pn;
	}
}

/// return -1 û���ҵ�,�������ҵ�λ��
int CDataBuffer::FindDataInBuff ( const  char * pData, int nLen1 )
{
	size_t nLen = (size_t)nLen1;
	DataPos dp;
	dp.m_pDataBlock = m_headerDB;
	dp.m_pPos = m_headerDB->m_pDataPtr;
	dp.m_nIndex = 0;
	size_t nSize = TotalDataSize();
	for ( size_t i = 0; i <= nSize - nLen; ++i )
	{
		DataPos p;
		memcpy ( &p, &dp, sizeof ( DataPos ) );
		if ( -1 != Compare ( &p, pData, (int)nLen ) )
		{
			return dp.m_nIndex;
		}
		if ( !dp.next() )
		{
			return -1;
		}
	}
	return -1;

}

int CDataBuffer::Compare ( DataPos*pDp, const char * pData, int nLen )
{
	if ( nLen == 1 )
	{
		if ( *pData == *pDp->m_pPos )
			return pDp->m_nIndex;
		else
			return -1;
	}
	if ( *pData == *pDp->m_pPos )
	{
		if ( !pDp->next() )
		{
			return -1;
		}
		return Compare ( pDp, pData + 1, nLen - 1 );
	}
	return -1;
}

void CDataBuffer::dump()
{
	static int nCount = 0;
	char str[100];
	sprintf ( str, "b%d.dat", ++nCount );
	FILE * fp = fopen ( str, "a+b" );
	if ( fp )
	{
		DataBlock * p = m_headerDB;
		for ( ; p; p = p->m_pNextDB )
		{
			if ( p->DataSize() > 0 )
				fwrite ( p->m_pDataPtr, 1, p->DataSize(), fp );
		}

		fclose ( fp );
	}
}

CDataBuffer::DataBlock::~DataBlock()
{
	delete [] m_pDBBuffer;
}

//////////////////////////////////////////////////////////////////////////
size_t CDataBuffer::DataBlock::freedSize()
{
	return ( DB_SIZE - ( m_pDataPtr - m_pDBBuffer ) - m_nDBUsed );
}

size_t CDataBuffer::DataBlock::DataSize()
{
	return m_nDBUsed;
}

bool CDataBuffer::DataPos::next()
{
	if ( ( m_pPos + 1 ) >= m_pDataBlock->dataEnd() )
	{
		if ( NULL == m_pDataBlock->m_pNextDB ) return false;
		DataBlock *p = m_pDataBlock->m_pNextDB;
		if ( p->m_nDBUsed == 0 ) return false;
		m_pDataBlock = p;
		m_pPos = p->m_pDataPtr;
		++m_nIndex;
		return true;
	}
	++m_pPos;
	++m_nIndex;
	return true;
}

