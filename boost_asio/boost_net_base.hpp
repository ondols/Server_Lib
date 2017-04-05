#pragma once

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <atlcoll.h>

#include "boost_net_packet_base.hpp"
#include "boost_net_util.hpp"


// Base Handler.
class CBBaseHandler
{
public:
	CBBaseHandler() {}
	virtual ~CBBaseHandler() {}

public:
	// virtual function.
	virtual void Init() = 0;

	bool Process_Packet(class CBBaseSession* sesstion, unsigned short packetid, char* data)
	{
		pPACKET_HANDLE_FUN pFun = nullptr;
		if (false == m_mapPacketHandle.Lookup(packetid, pFun))
		{
			return false;
		}
		return pFun(sesstion, data);
	}

protected:
	CAtlMap<unsigned short, pPACKET_HANDLE_FUN> m_mapPacketHandle;
};

// Base Session.
class CBBaseSession
{
public:
	CBBaseSession(boost::asio::io_service& service, CBBaseHandler* handler)
		: m_Socket(service)
		, m_pPacketHandler(handler)
		, m_bService(false)
		, m_pReceiveBuf(nullptr)
		, m_bIsFinalize(false)
	{
		// handler.
		m_pPacketHandler->Init();

		// receivebuf.
		m_pReceiveBuf = g_MemoryPool()->AllocMemory(ePACKET_RECEIVE_BUF_SIZE);
		m_pReceiveBuf->ReferenceIncrement();		
	}
	virtual ~CBBaseSession()
	{
		if (0 <= m_pReceiveBuf->ReferenceDecrement())
		{
			g_MemoryPool()->FreeMemory(m_pReceiveBuf);
			m_pReceiveBuf = nullptr;
		}
	}

public:
	// function.
	bool Connect(char* ip, int port)
	{
		if(m_bService == true)
		{
			return false;
		}

		// blocking connect.
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip), port);
		boost::system::error_code connect_error;
		m_Socket.connect(endpoint, connect_error);
		if (connect_error)
		{
			return false;
		}

		m_bService = true;
		// session receive.
		PostReceive();

		// on function call.
		On_Connect(endpoint);
		return true;
	}
	bool AsyncConnect(char* ip, int port)
	{
		if (m_bService == true)
		{
			return false;
		}

		// async connect.
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip), port);
		m_Socket.async_connect(endpoint,
			boost::bind(&CBBaseSession::handle_connect,
				this, boost::asio::placeholders::error, endpoint)
		);
		return true;
	}
	void PostReceive()
	{
		size_t newSize = ePACKET_RECEIVE_BUF_SIZE - m_nBufOffset;
		m_Socket.async_read_some(
			boost::asio::buffer(m_pReceiveBuf->GetBufferOffset(m_nBufOffset), newSize),
			boost::bind(&CBBaseSession::handle_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
		);
	}
	bool PostSend(sPacketHeader* packetdata, int size)
	{
		if (packetdata == nullptr)
		{
			return false;
		}
		if (m_Socket.is_open() == false)
		{
			return false;
		}

		CBBuffer* pSendBuf = g_MemoryPool()->AllocMemory(size);
		if (pSendBuf == nullptr)
		{
			return false;
		}
		pSendBuf->SetData((char*)packetdata, size);
		pSendBuf->ReferenceIncrement();

		boost::asio::async_write(
			m_Socket,
			boost::asio::buffer(pSendBuf->GetBuffer(), pSendBuf->GetUseSize()),
			boost::bind(&CBBaseSession::handle_write, this, pSendBuf, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
		);
		return true;
	}
	bool PostSend(CBBuffer* pSendBuf)
	{
		if (pSendBuf == nullptr)
		{
			return false;
		}
		if (m_Socket.is_open() == false)
		{
			return false;
		}
		pSendBuf->ReferenceIncrement();

		boost::asio::async_write(
			m_Socket,
			boost::asio::buffer(pSendBuf->GetBuffer(), pSendBuf->GetUseSize()),
			boost::bind(&CBBaseSession::handle_write, this, pSendBuf, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
		);
		return true;
	}
	bool PostSend(char* buf, size_t len)
	{
		if (buf == nullptr)
		{
			return false;
		}
		if (m_Socket.is_open() == false)
		{
			return false;
		}

		CBBuffer* pSendBuf = g_MemoryPool()->AllocMemory(len);
		if (pSendBuf == nullptr)
		{
			return false;
		}
		pSendBuf->SetData(buf, len);
		pSendBuf->ReferenceIncrement();

		boost::asio::async_write(
			m_Socket,
			boost::asio::buffer(pSendBuf->GetBuffer(), pSendBuf->GetUseSize()),
			boost::bind(&CBBaseSession::handle_write, this, pSendBuf, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
		);
		return true;
	}

	// inline function.
	inline boost::asio::ip::tcp::socket& GetSocket() { return m_Socket; }
	inline void SetService(bool flag) { m_bService = flag; }
	inline bool GetService() { return m_bService; }
	inline void SetSessionNo(UINT64 no) { m_nSessionNo = no; }
	inline UINT64 GetSessionNo() { return m_nSessionNo; }

protected:
	// virtual function.
	virtual void On_Connect(boost::asio::ip::tcp::endpoint& endpoint) 
	{
		//endpoint.address().to_string().c_str();
		//endpoint.port();
	}
	virtual void On_Receive() {}
	virtual void On_Write() {}
	virtual void On_Close() = 0 {}

	// function.

private:
	// function.
	void handle_connect(const boost::system::error_code& error, boost::asio::ip::tcp::endpoint& endpoint)
	{
		if (error)
		{
			return;
		}	

		m_bService = true;
		// session receive.
		PostReceive();

		// on function call.
		On_Connect(endpoint);
	}

	void handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
	{
		if (error)
		{
			if (error == boost::asio::error::eof)
			{
				printf("Client Sesscion Close \n");
			}
			else
			{
				printf("Error No : %d, Msg : %s,  \n", error.value(), error.message().c_str());
			}
			close_session();
			return;
		}

		size_t nPacketData = m_nBufOffset + bytes_transferred;
		size_t nReadData = 0;
		char* pReceiveBuf = m_pReceiveBuf->GetBuffer();
		while (nPacketData > 0)
		{
			// packet data check.
			if (nPacketData < sizeof(sPacketHeader))
			{
				break;
			}
			sPacketHeader* pHeader = (sPacketHeader*)&pReceiveBuf[nReadData];
			// packet header check.
			if (pHeader->size > nPacketData)
			{
				break;
			}

			// packet process
			if (false == m_pPacketHandler->Process_Packet(this, pHeader->packetid, &pReceiveBuf[nReadData]))
			{
				// critical log.
			}

			// buf offset reset.
			nPacketData -= pHeader->size;
			nReadData += pHeader->size;			
		}

		// remain buf.
		if (nPacketData > 0)
		{
			// move memory.
			memmove_s(m_pReceiveBuf->GetBuffer(), ePACKET_RECEIVE_BUF_SIZE, m_pReceiveBuf->GetBufferOffset(nReadData), nPacketData);
		}
		m_nBufOffset = nPacketData;

		// session receive.
		PostReceive();

		// on function call.
		On_Receive();
	}

	void handle_write(CBBuffer* buffer, const boost::system::error_code& error, size_t bytes_transferred)
	{
		if (buffer == nullptr)
		{
			printf("write error!! buffer is null. \n");
			return;
		}

		// buffer ref dec.
		if (0 >= buffer->ReferenceDecrement())
		{
			// buffer clear.
			buffer->ClearBuffer();

			// pool free.
			g_MemoryPool()->FreeMemory(buffer);
		}

		// send by zero. socket close.
		if (bytes_transferred == 0)
		{
			// close session.
			printf("sent by zero. session no : %I64d \n", GetSessionNo());
			close_session();
			return;
		}

		// on function call.
		On_Write();
	}

	void close_session()
	{
		if (m_Socket.is_open() == false)
		{
			return;
		}

		// close session.
		printf("close session. session no : %I64d \n", GetSessionNo());

		// socket close.
		m_Socket.close();

		// on function call.
		On_Close();
	}

protected:
	// value.
	boost::asio::ip::tcp::socket m_Socket;
	CBBaseHandler* m_pPacketHandler;
	CBBuffer* m_pReceiveBuf;
	size_t m_nBufOffset;
	bool m_bService;
	bool m_bIsFinalize;
	UINT64 m_nSessionNo;
};

// WorkThread.
class CBWorkThread
{
public:
	CBWorkThread(boost::asio::io_service& service)
		: m_io_service(service)
	{
		m_vecWorkThread.reserve(100);
	}
	~CBWorkThread() {}

	void StartWorkThread(WORD threadcount)
	{
		if (threadcount < 100)	// 임시로, 100개 이상은 심하지 않나???
		{
			m_wWorkThreadCount = threadcount;
		}

		for (int i = 0; i < m_wWorkThreadCount; ++i)
		{
			boost::thread* thread = new boost::thread(boost::bind(&boost::asio::io_service::run, &m_io_service));
			m_vecWorkThread.push_back(thread);
		}
	}

	void WaitWorkThread()
	{
		for (int i = 0; i < (int)m_vecWorkThread.size(); ++i)
		{
			m_vecWorkThread[i]->join();
			printf("thread %d end. \n", i);
			delete m_vecWorkThread[i];
			m_vecWorkThread[i] = nullptr;
		}
		m_vecWorkThread.clear();
	}

	void StopWorkThread()
	{
		m_io_service.stop();
	}

private:
	WORD m_wWorkThreadCount;
	std::vector<boost::thread*> m_vecWorkThread;
	boost::asio::io_service& m_io_service;
};

// Base Acceptor.
class CBBaseAcceptor
{
public:
	CBBaseAcceptor(boost::asio::io_service& service, int port) 
		: m_Acceptor(service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
		, m_WorkThread(service)
	{
	}
	virtual ~CBBaseAcceptor() {}

public:
	// function.
	void Run(WORD workthreadcount)
	{
		m_WorkThread.StartWorkThread(workthreadcount);
	}

	void WaitDone()
	{
		m_WorkThread.WaitWorkThread();
	}

	void Stop()
	{
		m_WorkThread.StopWorkThread();
	}

protected:
	// virtual function.
	virtual void On_Acceptor(CBBaseSession* session)
	{
		session->PostReceive();
	}

	// function.
	void PostAccept(CBBaseSession* session, bool reaccept = false)
	{
		if(reaccept == true)
		{
			printf("Post Reaccept, Seq : %I64d \n", m_nSeqNumber);
		}		

		m_Acceptor.async_accept(session->GetSocket(),
			boost::bind(&CBBaseAcceptor::handle_accept, this, session, boost::asio::placeholders::error));
	}

private:
	void handle_accept(CBBaseSession* session, const boost::system::error_code& error)
	{
		if (error)
		{
			printf("Error No : %d, Msg : %s,  \n", error.value(), error.message().c_str());
			// reaccept.
			PostAccept(session, true);
			return;
		}

		++m_nSeqNumber;
		// sesstion start receive.
		session->PostReceive();		

		// On function. need PostAccept function call.
		On_Acceptor(session);
	}

protected:
	// value.
	volatile LONG64 m_nSeqNumber;
	CBWorkThread m_WorkThread;
	boost::asio::ip::tcp::acceptor m_Acceptor;	
};

// Base Connector.
class CBBaseConnector : public CBBaseSession
{
public:
	CBBaseConnector(boost::asio::io_service& service, CBBaseHandler* handler)
		: CBBaseSession(service, handler)
		, m_WorkThread(service)
	{
	}
	virtual ~CBBaseConnector() {}

public:
	// function.
	void Run(WORD workthreadcount)
	{
		m_WorkThread.StartWorkThread(workthreadcount);
	}

	void WaitDone()
	{
		m_WorkThread.WaitWorkThread();
	}

	void Stop()
	{
		m_WorkThread.StopWorkThread();
	}

protected:
	// virtual function.
	virtual bool Packet_Process() { return false; }
	virtual void On_Connect(boost::asio::ip::tcp::endpoint& endpoint)
	{
		//endpoint.address().to_string().c_str();
		//endpoint.port();
	}
	virtual void On_Receive() {}
	virtual void On_Write() {}

	// function.

protected:
	// value.
	CBWorkThread m_WorkThread;
};
