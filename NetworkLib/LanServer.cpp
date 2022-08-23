#pragma comment(lib, "ws2_32")

#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <process.h>
#include <unordered_map>
#include <cassert>

#include "EIOType.h"

#include "Message.h"
#include "RingBuffer.h"
#include "ObjectPool.h"
#include "LanServer.h"
#include "INetworkEventListener.h"
#include "PacketDefine.h"

LanServer::LanServer()
	: mListenSocket(INVALID_SOCKET),
	mhCompletionPort(INVALID_HANDLE_VALUE),
	mSessionPool(DEFALT_SESSION_POOL_SIZE, false),
	mIP(0),
	mPort(0),
	mMaximumSessionCount(0),
	mCurrentSessionCount(0),
	mListener(nullptr)
{
	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);

	InitializeSRWLock(&mSessionsLock);
}

LanServer::~LanServer()
{
	WSACleanup();
}

bool LanServer::TryRun(const unsigned long IP, const unsigned short port
	, const unsigned int numWorkerThread, const unsigned int numRunningThread
	, const unsigned int maxSessionCount, const bool bSupportsNagle
	, INetworkEventListener* listener)
{
	int retval;

	mIP = IP;
	mPort = port;
	mListener = listener;
	mCurrentSessionCount = 0;
	mMaximumSessionCount = maxSessionCount;

	mSessions.reserve(maxSessionCount * 2 + 1);

	// Initialize Listen Socket
	{
		mListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (mListenSocket == INVALID_SOCKET)
		{
			return false;
		}

		SOCKADDR_IN serverAddr;
		{
			ZeroMemory(&serverAddr, sizeof(serverAddr));
			serverAddr.sin_family = AF_INET;
			serverAddr.sin_addr.s_addr = htonl(IP);
			serverAddr.sin_port = htons(port);
		}

		retval = bind(mListenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
		if (retval == SOCKET_ERROR)
		{
			return false;
		}

		LINGER lingerOptval;
		{
			lingerOptval.l_onoff = 1;
			lingerOptval.l_linger = 0;
		}
		retval = setsockopt(mListenSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOptval, sizeof(lingerOptval));
		if (retval == SOCKET_ERROR)
		{
			return false;
		}

		if (!bSupportsNagle)
		{
			bool flag = true;
			retval = setsockopt(mListenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
			if (retval == SOCKET_ERROR)
			{
				return false;
			}
		}

		retval = listen(mListenSocket, SOMAXCONN);
		if (retval == SOCKET_ERROR)
		{
			return false;
		}
	}

	mhCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, numRunningThread);
	if (mhCompletionPort == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	mhThreads.reserve(numWorkerThread + 1);
	for (unsigned int i = 0; i < numWorkerThread; ++i)
	{
		mhThreads.push_back((HANDLE)_beginthreadex(nullptr, 0, &workerThread, this, 0, nullptr));
	}
	mhThreads.push_back((HANDLE)_beginthreadex(nullptr, 0, &acceptThread, this, 0, nullptr));

	return true;
}

void LanServer::Terminate()
{
	closesocket(mListenSocket);

	size_t numThreads = mhThreads.size();

	for (auto pair : mSessions)
	{
		Session* session = pair.second;

		closesocket(session->Socket);
	}

	for (int i = 0; i < numThreads - 1; ++i)
	{
		PostQueuedCompletionStatus(mhCompletionPort, 0, 0, nullptr);
	}
	WaitForMultipleObjects(numThreads, mhThreads.data(), true, INFINITE);

	for (int i = 0; i < numThreads; ++i)
	{
		CloseHandle(mhThreads[i]);
	}
	CloseHandle(mhCompletionPort);

	mSessions.clear();
}

bool LanServer::TrySendMessage(const sessionID_t ID, const Message& message)
{
	Session* session;

	AcquireSRWLockExclusive(&mSessionsLock);
	{
		auto iter = mSessions.find(ID);
		if (iter == mSessions.end())
		{
			ReleaseSRWLockExclusive(&mSessionsLock);
			return false;
		}
		session = iter->second;
	}
	ReleaseSRWLockExclusive(&mSessionsLock);

	uint16_t length = message.GetSize();
	AcquireSRWLockExclusive(&session->Lock);
	{
		if (session->ID != ID)
		{
			ReleaseSRWLockExclusive(&session->Lock);
			return false;
		}
		if (!session->SendBuffer.TryEnqueue((char*)&length, sizeof(length)))
		{
			closesocket(session->Socket);
			ReleaseSRWLockExclusive(&session->Lock);
			return false;
		}
		if (!session->SendBuffer.TryEnqueue(message.GetBuffer(), message.GetSize()))
		{
			closesocket(session->Socket);
			ReleaseSRWLockExclusive(&session->Lock);
			return false;
		}
		sendPost(session);
	}
	ReleaseSRWLockExclusive(&session->Lock);

	return true;
}

bool LanServer::TryDisconnect(const sessionID_t ID)
{
	Session* erased;

	AcquireSRWLockExclusive(&mSessionsLock);
	{
		auto pair = mSessions.find(ID);
		if (pair == mSessions.end())
		{
			ReleaseSRWLockExclusive(&mSessionsLock);
			return false;
		}
		erased = pair->second;
	}
	ReleaseSRWLockExclusive(&mSessionsLock);

	closesocket(erased->Socket);
	return true;
}

unsigned long LanServer::GetIP() const
{
	return mIP;
}

unsigned short LanServer::GetPort() const
{
	return mPort;
}

unsigned int LanServer::GetMaximumSessionCount() const
{
	return mMaximumSessionCount;
}

unsigned int LanServer::GetCurrentSessionCount() const
{
	return mCurrentSessionCount;
}

unsigned int __stdcall LanServer::acceptThread(void* param)
{
	LanServer* server = (LanServer*)param;

	SOCKET listenSocket = server->mListenSocket;
	HANDLE hCompletionPort = server->mhCompletionPort;
	ObjectPool<Session>& sessionPool = server->mSessionPool;

	SRWLOCK sessionsLock = server->mSessionsLock;
	std::unordered_map<sessionID_t, Session*>& sessions = server->mSessions;

	INetworkEventListener& listener = *server->mListener;

	sessionID_t nextID = 0;

	while (true)
	{
		SOCKET clientSocket;
		SOCKADDR_IN clientAddr;
		int addrLen = sizeof(clientAddr);

		clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &addrLen);
		if (clientSocket == INVALID_SOCKET)
		{
			break;
		}
		if (server->mCurrentSessionCount == server->mMaximumSessionCount)
		{
			continue;
		}
		unsigned long IP = ntohl(clientAddr.sin_addr.S_un.S_addr);
		unsigned short port = ntohs(clientAddr.sin_port);

		bool bJoinable = listener.OnConnectionRequest(IP, port);

		if (!bJoinable)
		{
			continue;
		}

		sessionPool.Lock();
		Session* session = sessionPool.GetObject();
		sessionPool.Unlock();
		{
			session->ID = nextID;
			session->bIsSending = false;
			session->Socket = clientSocket;
			session->Addr = clientAddr;
			session->CurrentAsyncIOCount = 1;
		}

		AcquireSRWLockExclusive(&sessionsLock);
		sessions.insert({ session->ID, session });
		ReleaseSRWLockExclusive(&sessionsLock);

		InterlockedIncrement(&server->mCurrentSessionCount);

		CreateIoCompletionPort((HANDLE)session->Socket, hCompletionPort, (ULONG_PTR)session, 0);

		listener.OnClientJoin(session->ID, IP, port);

		server->recvPost(session);

		if (InterlockedDecrement16(&session->CurrentAsyncIOCount) == 0)
		{
			server->releaseSession(session);
		}
		++nextID;
	}
	return 0;
}

unsigned int __stdcall LanServer::workerThread(void* param)
{
	LanServer* server = (LanServer*)param;

	HANDLE hCompletionPort = server->mhCompletionPort;
	INetworkEventListener& listener = *server->mListener;

	bool bSucceded;

	while (true)
	{
		DWORD cbTransferred = 0;
		Session* session = nullptr;
		OverlappedExtension* overlapped;

		bSucceded = GetQueuedCompletionStatus(hCompletionPort, &cbTransferred
			, (PULONG_PTR)&session, (LPOVERLAPPED*)&overlapped, INFINITE);

		if (bSucceded == true && overlapped == nullptr)
		{
			break;
		}
		else
		{
			int errorCode = GetLastError();

			if (overlapped == nullptr)
			{
				// std::cout << "Failed GQCS and Overlapped is null, ErrorCode : " << errorCode << std::endl;
				break;
			}
			else if (errorCode != ERROR_NETNAME_DELETED && errorCode != 0)
			{
				// std::cout << "Failed GQCS and Overlapped is not null, ErrorCode : " << errorCode << std::endl;
			}
		}

		RingBuffer& recvBuffer = session->ReceiveBuffer;
		RingBuffer& sendBuffer = session->SendBuffer;

		if (cbTransferred != 0 && overlapped != nullptr)
		{
			switch (overlapped->Type)
			{
			case EIOType::Recv:
			{
				recvBuffer.MoveRear(cbTransferred);

				PacketHeader header;
				Message message;

				while (!recvBuffer.IsEmpty())
				{
					if (!recvBuffer.TryPeek((char*)&header, sizeof(header)))
					{
						break;
					}
					if (recvBuffer.GetSize() < header.Length + sizeof(header))
					{
						break;
					}
					message.Clear();

					recvBuffer.MoveFront(sizeof(header));
					recvBuffer.TryDequeue(message.GetRear(), header.Length);

					message.MoveWritePos(header.Length);

					listener.OnRecv(session->ID, &message);
				}
				server->recvPost(session);
			}
			break;
			case EIOType::Send:
			{
				AcquireSRWLockExclusive(&session->Lock);
				{
					sendBuffer.MoveFront(cbTransferred);
					session->bIsSending = false;

					server->sendPost(session);
				}
				ReleaseSRWLockExclusive(&session->Lock);
			}
			break;
			default:
				std::cout << "Unhandled IO Type Error" << std::endl;
				break;
			}
		}
		if (InterlockedDecrement16(&session->CurrentAsyncIOCount) == 0)
		{
			server->releaseSession(session);
		}
	}
	std::cout << "Thread is terminated" << std::endl;

	return 0;
}

void LanServer::sendPost(Session* session)
{
	int retval;

	RingBuffer& sendBuffer = session->SendBuffer;

	if (!sendBuffer.IsEmpty() && InterlockedExchange8(reinterpret_cast<char*>(&session->bIsSending), true) == false)
	{
		InterlockedIncrement16(&session->CurrentAsyncIOCount);

		ZeroMemory(&session->SendOverlapped.Overlapped, sizeof(session->SendOverlapped.Overlapped));

		session->SendOverlapped.WSABuf.buf = sendBuffer.GetFront();
		session->SendOverlapped.WSABuf.len = sendBuffer.GetDirectDequeueableSize();

		retval = WSASend(session->Socket, &session->SendOverlapped.WSABuf, 1, nullptr, 0, &session->SendOverlapped.Overlapped, nullptr);

		if (retval == SOCKET_ERROR)
		{
			int errorCode = WSAGetLastError();
			if (errorCode != ERROR_IO_PENDING)
			{
				if (errorCode != WSAECONNRESET)
				{
					// std::cout << "WSASend() Error : " << errorCode << std::endl;
				}
				if (InterlockedDecrement16(&session->CurrentAsyncIOCount) == 0)
				{
					releaseSession(session);
				}
			}
		}
	}
}

void LanServer::recvPost(Session* session)
{
	int retval;

	InterlockedIncrement16(&session->CurrentAsyncIOCount);

	RingBuffer& recvBuffer = session->ReceiveBuffer;

	ZeroMemory(&session->RecvOverlapped.Overlapped, sizeof(session->RecvOverlapped.Overlapped));

	session->RecvOverlapped.WSABuf.buf = recvBuffer.GetRear();
	session->RecvOverlapped.WSABuf.len = recvBuffer.GetDirectEnqueueableSize();
	DWORD flags = 0;

	retval = WSARecv(session->Socket, &session->RecvOverlapped.WSABuf, 1, nullptr, &flags, &session->RecvOverlapped.Overlapped, nullptr);

	if (retval == SOCKET_ERROR)
	{
		int errorCode = WSAGetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			if (errorCode != WSAECONNRESET)
			{
				// std::cout << "WSARecv() Error : " << errorCode << std::endl;
			}
			if (InterlockedDecrement16(&session->CurrentAsyncIOCount) == 0)
			{
				releaseSession(session);
			}
		}
	}
}

void LanServer::releaseSession(Session* session)
{
	sessionID_t id = session->ID;

	AcquireSRWLockExclusive(&mSessionsLock);
	{
		mSessions.erase(id);
	}
	ReleaseSRWLockExclusive(&mSessionsLock);

	AcquireSRWLockExclusive(&session->Lock);
	{
		session->ID = INVALID_SOCKET_ID;
	}
	ReleaseSRWLockExclusive(&session->Lock);

	session->SendBuffer.Clear();
	session->ReceiveBuffer.Clear();
	closesocket(session->Socket);

	mSessionPool.Lock();
	mSessionPool.ReleaseObject(session);
	mSessionPool.Unlock();

	InterlockedDecrement(&mCurrentSessionCount);

	mListener->OnClientLeave(id);
}
