#pragma comment(lib, "ws2_32")

#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <process.h>
#include <unordered_set>
#include <cassert>

#include "EIOType.h"

#include "Message.h"
#include "RingBuffer.h"
#include "ObjectPool.h"
#include "Stack.h"
#include "LanServer.h"
#include "IntrusivePointer.h"
#include "INetworkEventListener.h"
#include "PacketDefine.h"

#define SEND_WSABUF_COUNT (256)
#define MAKE_ID(unique, index) (((unique) << 16) | (index))
#define EXTRACT_INDEX_FROM_ID(id) ((id) & 0x000000000000FFFF)

LanServer::LanServer()
	: mListenSocket(INVALID_SOCKET),
	mhCompletionPort(INVALID_HANDLE_VALUE),
	mIP(0),
	mPort(0),
	mMaximumSessionCount(0),
	mCurrentSessionCount(0),
	mListener(nullptr)
{
	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);

	InitializeSRWLock(&mIndexesStackLock);
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

	mSessions = new Session[maxSessionCount];
	mUseableIndexesStack = new Stack<uint64_t>(maxSessionCount);

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

	for (uint64_t index = 0; index < maxSessionCount; ++index)
	{
		mUseableIndexesStack->TryPush(index);
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

	for (unsigned int i = 0; i < mMaximumSessionCount; ++i)
	{
		if (mSessions[i].ID != INVALID_SESSION_ID)
		{
			closesocket(mSessions[i].Socket);
		}
	}
	for (size_t i = 0; i < numThreads - 1; ++i)
	{
		PostQueuedCompletionStatus(mhCompletionPort, 0, 0, nullptr);
	}
	WaitForMultipleObjects((DWORD)numThreads, mhThreads.data(), true, INFINITE);

	for (size_t i = 0; i < numThreads; ++i)
	{
		CloseHandle(mhThreads[i]);
	}
	CloseHandle(mhCompletionPort);

	delete mSessions;
	delete mUseableIndexesStack;
}

bool LanServer::TrySendMessage(const sessionID_t ID, IntrusivePointer<Message>& messagePtr)
{
	uint64_t index = EXTRACT_INDEX_FROM_ID(ID);

	if (mMaximumSessionCount <= index)
	{
		return false;
	}
	Session* target = &mSessions[index];

	AcquireSRWLockExclusive(&target->Lock);
	{
		if (target->ID != ID)
		{
			ReleaseSRWLockExclusive(&target->Lock);
			return false;
		}
		messagePtr.AddRefCount();

		if (!target->SendBuffer.TryEnqueue((char*)&messagePtr, sizeof(void*)))
		{
			closesocket(target->Socket);
			ReleaseSRWLockExclusive(&target->Lock);
			return false;
		}
		sendPost(target);
	}
	ReleaseSRWLockExclusive(&target->Lock);

	return true;
}

bool LanServer::TryDisconnect(const sessionID_t ID)
{
	uint64_t index = EXTRACT_INDEX_FROM_ID(ID);
	
	if (mMaximumSessionCount <= index)
	{
		return false;
	}
	Session* target = &mSessions[index];

	AcquireSRWLockExclusive(&target->Lock);
	{
		if (target->ID != ID)
		{
			ReleaseSRWLockExclusive(&target->Lock);
			return false;
		}
		closesocket(target->Socket);
	}
	ReleaseSRWLockExclusive(&target->Lock);

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

	Stack<uint64_t>* useableIndexesStack = server->mUseableIndexesStack;

	Session* sessions = server->mSessions;

	INetworkEventListener& listener = *server->mListener;

	uint64_t uniqueID = 0;

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
			closesocket(clientSocket);
			continue;
		}
		unsigned long IP = ntohl(clientAddr.sin_addr.S_un.S_addr);
		unsigned short port = ntohs(clientAddr.sin_port);

		bool bJoinable = listener.OnConnectionRequest(IP, port);

		if (!bJoinable)
		{
			continue;
		}

		uint64_t index;
		AcquireSRWLockExclusive(&server->mIndexesStackLock);
		{
			index = useableIndexesStack->Pop();
		}
		ReleaseSRWLockExclusive(&server->mIndexesStackLock);

		Session* session = &sessions[index];
		{
			session->ID = MAKE_ID(uniqueID, index);
			session->bIsSending = false;
			session->Socket = clientSocket;
			session->Addr = clientAddr;
			session->CurrentAsyncIOCount = 1;
		}
		InterlockedIncrement(&server->mCurrentSessionCount);

		CreateIoCompletionPort((HANDLE)session->Socket, hCompletionPort, (ULONG_PTR)session, 0);

		listener.OnClientJoin(session->ID, IP, port);

		server->recvPost(session);

		if (InterlockedDecrement16(&session->CurrentAsyncIOCount) == 0)
		{
			server->releaseSession(session);
		}
		++uniqueID;
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
	WSABUF buffers[SEND_WSABUF_COUNT];

	RingBuffer& sendBuffer = session->SendBuffer;

	if (!sendBuffer.IsEmpty() && InterlockedExchange8(reinterpret_cast<char*>(&session->bIsSending), true) == false)
	{
		if (InterlockedIncrement16(&session->CurrentAsyncIOCount) == 1)
		{
			return;
		}
		ZeroMemory(&session->SendOverlapped.Overlapped, sizeof(session->SendOverlapped.Overlapped));

		int numMessage = sendBuffer.GetSize() % sizeof(void*);
		for (int i = 0; i < numMessage; ++i)
		{
			IntrusivePointer<Message>* messagePtr;
			sendBuffer.TryDequeue((char*)&messagePtr, sizeof(void*));

			buffers[i].buf = (*messagePtr)->CreateMessage((unsigned int*)&buffers[i].len); asdf; asdfasf; lajsdf;l
		}
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

	AcquireSRWLockExclusive(&session->Lock);
	{
		session->ID = INVALID_SESSION_ID;
		closesocket(session->Socket);
	}
	ReleaseSRWLockExclusive(&session->Lock);

	session->SendBuffer.Clear();
	session->ReceiveBuffer.Clear();

	AcquireSRWLockExclusive(&mIndexesStackLock);
	{
		mUseableIndexesStack->TryPush(EXTRACT_INDEX_FROM_ID(id));
	}
	ReleaseSRWLockExclusive(&mIndexesStackLock);

	InterlockedDecrement(&mCurrentSessionCount);

	mListener->OnClientLeave(id);
}
