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
		// ������ʼ�� 
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
		*	Ϊ����httpЭ����
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
	// ���ݶ������ܹ��������ֽڴ�С
	size_t TotalDataSize();
	// ȡ��������ǰ�����Ч���ݿ� 
	WSABUF * GetDataPtr();

	//////////////////////////////////////////////////////////////////////////
	/// �����������У�����nBytes�����ݡ�
	void PopData(size_t nBytes);

	/// �������ݵ�������β��
	void AppendData(size_t nBytes,char *pData);

	/// ��β��ȡ�����ݿ��п�
	WSABUF * GetDataPtrFreeSpace();

	//////////////////////////////////////////////////////////////////////////
	// ��ʾΪ��������ĩβ������nbytes�����ݡ�
	//////////////////////////////////////////////////////////////////////////
	void PushData(size_t nBytes);
		

	//////////////////////////////////////////////////////////////////////////
	/// ��������������������������һ��Ӧ����Э�����,������ݱ���
	//////////////////////////////////////////////////////////////////////////
	/// ���ƻ���������ߵ�dwWant�ֽڵ� pBuff��ȥ
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