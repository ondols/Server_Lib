#pragma once

// interlocked slist.
class CBSList
{
public:
	CBSList() { InitializeSListHead(&m_ListHead); }
	~CBSList() {}

public:
	void Push(SLIST_ENTRY* StackNode) { InterlockedPushEntrySList(&m_ListHead, StackNode); }
	SLIST_ENTRY* Pop() { return InterlockedPopEntrySList(&m_ListHead); }
	SLIST_ENTRY* Eraser() { return InterlockedFlushSList(&m_ListHead); }
	int GetSize() { return QueryDepthSList(&m_ListHead); }

private:
	SLIST_HEADER m_ListHead;
};

// ref count.
class CBRefCount
{
public:
	CBRefCount() : m_nRefCount(0) {}
	virtual ~CBRefCount() {}

public:
	// inc ref.
	long ReferenceIncrement() { return InterlockedIncrement(&m_nRefCount); }
	// dec ref.
	long ReferenceDecrement() { return InterlockedDecrement(&m_nRefCount); }
	_inline volatile long	GetReferenceCount() { return m_nRefCount; }

	CBRefCount& operator=(CBRefCount* p)
	{
		p->ReferenceIncrement();
		return *p;
	}

private:
	volatile long m_nRefCount;
};

// net buffer.
class CBBuffer
	: public SLIST_ENTRY
	, public CBRefCount
{
public:
	CBBuffer(size_t len) : m_pBuffer(nullptr), m_nLen(len), m_nUseLen(0)
	{
		m_pBuffer = new char[len];
	}
	~CBBuffer()
	{
		if (m_pBuffer)
		{
			delete[] m_pBuffer;
			m_pBuffer = nullptr;
		}
	}

	char* GetBuffer() { return m_pBuffer; }
	char* GetBufferOffset(size_t offset) { if (offset > m_nLen) return nullptr; return &m_pBuffer[offset]; }
	size_t GetSize() { return m_nLen; }
	size_t GetUseSize() { return m_nUseLen; }
	void ClearBuffer() { ZeroMemory(m_pBuffer, sizeof(char) * m_nLen); m_nUseLen = 0; }
	void SetData(char* data, size_t len)
	{
		if (m_pBuffer == nullptr)
		{
			return;
		}
		if (len > m_nLen)
		{
			return;
		}
		m_nUseLen = len;
		CopyMemory(m_pBuffer, data, len);
	}

private:
	char* m_pBuffer;
	size_t m_nLen;
	size_t m_nUseLen;
};

enum eMEMORY_INFO
{
	eMEMORY_POOL_ARRAY_MAX = 20,
	eMEMORY_POOL_DEFAULT_COUNT = 1000,
	eMEMORY_DEFAULT_SIZE = 1024,
};

// memory pool. use slist.
class CBMemoryPool
{
public:
	CBMemoryPool() { Init(); }
	~CBMemoryPool() {}

	static CBMemoryPool* GetInstance()
	{
		static CBMemoryPool* pInstance = new CBMemoryPool;
		return pInstance;
	}

	void Init()
	{
		// 1, 2, 4, 6, 8, 10, .... 20Kbyte Data Create.
		for (int i = 0, z = 2; i < eMEMORY_POOL_ARRAY_MAX; i++, z += 2)
		{
			for (int j = 0; j < eMEMORY_POOL_DEFAULT_COUNT; j++)
			{
				CBBuffer* pNewMemory = new CBBuffer(eMEMORY_DEFAULT_SIZE * z);
				m_Pool[i].Push(pNewMemory);
			}
		}
	}

	void Destroy()
	{
		for (int i = 0; i < eMEMORY_POOL_ARRAY_MAX; i++)
		{
			int nListSize = m_Pool[i].GetSize();
			for (int j = 0; j < nListSize; j++)
			{
				CBBuffer* pMemory = (CBBuffer*)m_Pool[i].Pop();
				if (pMemory == nullptr)
				{
					continue;
				}
				delete pMemory;
				pMemory = nullptr;
			}
			m_Pool[i].Eraser();
		}
	}

	CBBuffer* AllocMemory(size_t datasize)
	{
		size_t nPoolIndex = 0;
		CBBuffer* pMemory = nullptr;

		nPoolIndex = (datasize - 1) >> 11;
		pMemory = (CBBuffer*)m_Pool[nPoolIndex].Pop();

		if (pMemory == nullptr)
		{
			size_t nNewMemorySize = (nPoolIndex + 1) << 11;
			pMemory = new CBBuffer(nNewMemorySize);
		}
		return pMemory;
	}

	void FreeMemory(CBBuffer* pBuffer)
	{
		size_t nPoolIndex = (pBuffer->GetSize() - 1) >> 11;
		pBuffer->ClearBuffer();

		m_Pool[nPoolIndex].Push(pBuffer);
	}

private:
	CBSList m_Pool[eMEMORY_POOL_ARRAY_MAX];
};

#define g_MemoryPool() CBMemoryPool::GetInstance()

class CBLock
{
public:
	CBLock() { InitializeCriticalSection(&m_CS); }
	~CBLock() { DeleteCriticalSection(&m_CS); }

public:
	void Lock() { EnterCriticalSection(&m_CS); }	// CriticalSection Lock.
	void UnLock() { LeaveCriticalSection(&m_CS); }	// CriticalSection UnLock.

	CRITICAL_SECTION* GetCritical() { return &m_CS; } // CriticalSection Data return.

private:
	CRITICAL_SECTION m_CS;		// CriticalSection Data.
};

class CBAutoLock
{
public:
	CBAutoLock(CBLock* lock) : m_pLock(lock) { m_pLock->Lock(); }
	~CBAutoLock() { m_pLock->UnLock(); }

private:
	CBLock* m_pLock;	// BaseCriticalSection Data.
};