#pragma once
#include <list>

class CDataBuffer
{
public:
	class DataBlock;
	typedef std::list<CDataBuffer::DataBlock*> TDataBlockPtrList;
	class DataBlock
	{
	public:
		size_t m_nDBUsed;
		char *m_pDBBuffer;
		// 数据起始点 
		char *m_pDataPtr;
		DataBlock *m_pNextDB;
	public:
		void reset();
		size_t freedSize();
		size_t DataSize();
		~DataBlock();
		DataBlock();
	public:
		char * dataEnd(){return m_pDataPtr+m_nDBUsed;}
	};

protected:
	
	/*
		*	为计算http协议用
		*/
	class DataPos
	{
	public:
		DataBlock * m_pDataBlock;
		char *m_pPos;
		inline bool next();
		int m_nIndex;
	};
	///

	DataBlock * m_headerDB;
	DataBlock * m_tailDB;
	WSABUF m_wsaBuffer;
protected:
	TDataBlockPtrList * m_pDBList;
	DataBlock* newDataBlock();
	void freeDataBlock(DataBlock*p);
public:
	CDataBuffer( TDataBlockPtrList * pList);
		
	~CDataBuffer();
public:
	void Reset();
	// 数据队列中总共的数据字节大小
	size_t TotalDataSize();
	// 取得数据链前面的有效数据块 
	WSABUF * GetDataPtr();

	//////////////////////////////////////////////////////////////////////////
	/// 数据链（队列）弹出nBytes的数据。
	void PopData(size_t nBytes);

	/// 增加数据到数据链尾部
	void AppendData(size_t nBytes,char *pData);

	/// 从尾部取得数据空闲块
	WSABUF * GetDataPtrFreeSpace();

	//////////////////////////////////////////////////////////////////////////
	// 表示为数据链的末尾增加了nbytes的数据。
	//////////////////////////////////////////////////////////////////////////
	void PushData(size_t nBytes);
		

	//////////////////////////////////////////////////////////////////////////
	/// 下面三个函数用于其他场景，一般应用于协议分析,组合数据报文
	//////////////////////////////////////////////////////////////////////////
	/// 复制缓冲中最左边的dwWant字节到 pBuff中去
	/*
		*	
		[b1]-->[b2]--[b3]
		assume:
		b1...b2...b3 
		as flat memory
		*/
	bool NextData( DWORD dwWant, char *pBuff);
		
	int FindDataInBuff(const char * pData,int nLen);
	int Compare(DataPos*pDp,const char * pData,int nLen);
	//////////////////////////////////////////////////////////////////////////

	void dump();
#ifdef _DEBUG
	static void test1();
#endif
};