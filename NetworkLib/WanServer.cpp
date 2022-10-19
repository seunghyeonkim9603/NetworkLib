#include "stdafx.h"

#define MAKE_ID(unique, index) (((unique) << 16) | (index))
#define EXTRACT_INDEX_FROM_ID(id) ((id) & 0x000000000000FFFF)

WanServer::WanServer()
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
}

WanServer::~WanServer()
{
	WSACleanup();
}

bool WanServer::TryRun(const unsigned long IP, const unsigned short port
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
	mUseableIndexesStack = new LockFreeStack<uint64_t>(maxSessionCount);
	mMessagePool = new ObjectPool<IntrusivePointer<Message>>(maxSessionCount * MAX_ASYNC_SENDS);

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
		mUseableIndexesStack->Push(index);
	}

	mhThreads.reserve(numWorkerThread + 1);
	for (unsigned int i = 0; i < numWorkerThread; ++i)
	{
		mhThreads.push_back((HANDLE)_beginthreadex(nullptr, 0, &workerThread, this, 0, nullptr));
	}
	mhThreads.push_back((HANDLE)_beginthreadex(nullptr, 0, &acceptThread, this, 0, nullptr));

	return true;
}

void WanServer::Terminate()
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

bool WanServer::TrySendMessage(const sessionID_t ID, Message* messagePtr)
{
	Session* target = acquireSessionOrNull(ID);

	if (target == nullptr)
	{
		return false;
	}
	messagePtr->AddReferenceCount();

	if (!target->SendQueue.TryEnqueue(messagePtr))
	{
		closesocket(target->Socket);
		return false;
	}
	sendPost(target);

	if (InterlockedDecrement16(&target->Verifier.CurrentAsyncIOCount) == 0)
	{
		releaseSession(target);
	}
	return true;
}

bool WanServer::TryDisconnect(const sessionID_t ID)
{
	Session* target = acquireSessionOrNull(ID);

	if (target == nullptr)
	{
		return false;
	}
	closesocket(target->Socket);

	if (InterlockedDecrement16(&target->Verifier.CurrentAsyncIOCount) == 0)
	{
		releaseSession(target);
	}
	return true;
}

unsigned long WanServer::GetIP() const
{
	return mIP;
}

unsigned short WanServer::GetPort() const
{
	return mPort;
}

unsigned int WanServer::GetMaximumSessionCount() const
{
	return mMaximumSessionCount;
}

unsigned int WanServer::GetCurrentSessionCount() const
{
	return mCurrentSessionCount;
}

Message* WanServer::CreateMessage()
{
	Message* msg = Message::Create();

	msg->MoveReadPos(sizeof(Header));
	msg->MoveWritePos(sizeof(Header));

	return msg;
}

void WanServer::ReleaseMessage(Message* message)
{
	Message::Release(message);
}

unsigned int __stdcall WanServer::acceptThread(void* param)
{
	WanServer* server = (WanServer*)param;

	SOCKET listenSocket = server->mListenSocket;
	HANDLE hCompletionPort = server->mhCompletionPort;

	LockFreeStack<uint64_t>* useableIndexesStack = server->mUseableIndexesStack;

	Session* sessions = server->mSessions;

	INetworkEventListener& listener = *server->mListener;

	uint64_t uniqueID = 0;

	SOCKET clientSocket;
	SOCKADDR_IN clientAddr;
	int addrLen = sizeof(clientAddr);

	std::srand(std::time(nullptr));

	while (true)
	{
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

		index = useableIndexesStack->Pop();

		Session* session = &sessions[index];
		{
			session->ID = MAKE_ID(uniqueID, index);
			session->bIsSending = false;
			session->Socket = clientSocket;
			session->Addr = clientAddr;
			session->NumSent = 0;
			session->Verifier.ReleaseFlag = false;
			InterlockedIncrement16(&session->Verifier.CurrentAsyncIOCount);
		}
		InterlockedIncrement(&server->mCurrentSessionCount);

		CreateIoCompletionPort((HANDLE)session->Socket, hCompletionPort, (ULONG_PTR)session, 0);

		listener.OnClientJoin(session->ID, IP, port);

		server->recvPost(session);

		if (InterlockedDecrement16(&session->Verifier.CurrentAsyncIOCount) == 0)
		{
			server->releaseSession(session);
		}
		++uniqueID;
	}
	return 0;
}

unsigned int __stdcall WanServer::workerThread(void* param)
{
	WanServer* server = (WanServer*)param;

	HANDLE hCompletionPort = server->mhCompletionPort;
	INetworkEventListener& listener = *server->mListener;

	bool bSucceded;

	std::srand(std::time(nullptr));

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
				std::cout << "Failed GQCS and Overlapped is null, ErrorCode : " << errorCode << std::endl;
				break;
			}
			else if (errorCode != ERROR_NETNAME_DELETED && errorCode != ERROR_IO_PENDING
				&& errorCode != WSAECONNRESET && errorCode != 0)
			{
				std::cout << "Failed GQCS and Overlapped is not null, ErrorCode : " << errorCode << std::endl;
			}
		}

		RingBuffer& recvBuffer = session->ReceiveBuffer;
		Message** sentMessages = session->SentMessages;

		if (cbTransferred != 0 && overlapped != nullptr)
		{
			switch (overlapped->Type)
			{
			case EIOType::Recv:
			{
				bool bIsValidSession = true;

				recvBuffer.MoveRear(cbTransferred);

				Header header;

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
					Message* message = Message::Create();

					recvBuffer.MoveFront(sizeof(header));
					recvBuffer.TryDequeue(message->GetRear(), header.Length);

					message->MoveWritePos(header.Length);

					decode(&header, message->GetFront());

					if (header.Code != PACKET_CODE || header.CheckSum != calculateCheckSum(message->GetFront(), header.Length))
					{
						bIsValidSession = false;
						break;
					}
					listener.OnRecv(session->ID, message);

					Message::Release(message);
				}

				if (bIsValidSession)
				{
					server->recvPost(session);
				}
			}
			break;
			case EIOType::Send:
			{
				int numSent = session->NumSent;

				for (int i = 0; i < numSent; ++i)
				{
					Message::Release(sentMessages[i]);
				}
				session->NumSent = 0;
				session->bIsSending = false;

				server->sendPost(session);
			}
			break;
			default:
				std::cout << "Unhandled IO Type Error" << std::endl;
				break;
			}
		}
		if (InterlockedDecrement16(&session->Verifier.CurrentAsyncIOCount) == 0)
		{
			server->releaseSession(session);
		}
	}
	std::cout << "Thread is terminated" << std::endl;

	return 0;
}



void WanServer::sendPost(Session* session)
{
	int retval;
	WSABUF buffers[MAX_ASYNC_SENDS];

	if (!session->SendQueue.IsEmpty() && InterlockedExchange8(reinterpret_cast<char*>(&session->bIsSending), true) == false)
	{
		InterlockedIncrement16(&session->Verifier.CurrentAsyncIOCount);

		ZeroMemory(&session->SendOverlapped.Overlapped, sizeof(session->SendOverlapped.Overlapped));

		Message* sendMessage;
		int numSend = 0;

		while (session->SendQueue.TryDequeue(&sendMessage))
		{
			WSABUF* buff = &buffers[numSend];
			session->SentMessages[numSend] = sendMessage;

			Header* header = reinterpret_cast<Header*>(sendMessage->GetBuffer());
			{
				header->Code = PACKET_CODE;
				header->Length = sendMessage->GetSize();
				header->RandKey = std::rand();
				header->CheckSum = calculateCheckSum(sendMessage->GetFront(), header->Length);
			}
			encode(header, sendMessage->GetFront());

			buff->buf = reinterpret_cast<char*>(header);
			buff->len = sizeof(*header) + header->Length;

			++numSend;
		}
		session->NumSent = numSend;

		retval = WSASend(session->Socket, buffers, numSend, nullptr, 0, &session->SendOverlapped.Overlapped, nullptr);

		if (retval == SOCKET_ERROR)
		{
			int errorCode = WSAGetLastError();
			if (errorCode != ERROR_IO_PENDING)
			{
				session->bIsSending = false;

				if (errorCode != WSAECONNRESET)
				{
					std::cout << "WSASend() Error : " << errorCode << std::endl;
				}
				if (InterlockedDecrement16(&session->Verifier.CurrentAsyncIOCount) == 0)
				{
					releaseSession(session);
				}
			}
		}
	}
}

void WanServer::recvPost(Session* session)
{
	int retval;
	WSABUF buffer;

	InterlockedIncrement16(&session->Verifier.CurrentAsyncIOCount);

	RingBuffer& recvBuffer = session->ReceiveBuffer;

	ZeroMemory(&session->RecvOverlapped.Overlapped, sizeof(session->RecvOverlapped.Overlapped));

	buffer.buf = recvBuffer.GetRear();
	buffer.len = recvBuffer.GetDirectEnqueueableSize();
	DWORD flags = 0;

	retval = WSARecv(session->Socket, &buffer, 1, nullptr, &flags, &session->RecvOverlapped.Overlapped, nullptr);

	if (retval == SOCKET_ERROR)
	{
		int errorCode = WSAGetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			if (errorCode != WSAECONNRESET)
			{
				std::cout << "WSARecv() Error : " << errorCode << std::endl;
			}
			if (InterlockedDecrement16(&session->Verifier.CurrentAsyncIOCount) == 0)
			{
				releaseSession(session);
			}
		}
	}
}

void WanServer::releaseSession(Session* session)
{
	ReleaseVerifier* verifier = &session->Verifier;

	if (InterlockedCompareExchange(reinterpret_cast<unsigned long long*>(&verifier->Verifier), 1, 0) != 0)
	{
		return;
	}
	sessionID_t id = session->ID;

	session->ID = INVALID_SESSION_ID;
	closesocket(session->Socket);

	session->SendQueue.Clear();
	session->ReceiveBuffer.Clear();

	mUseableIndexesStack->Push(EXTRACT_INDEX_FROM_ID(id));

	InterlockedDecrement(&mCurrentSessionCount);

	mListener->OnClientLeave(id);
}

WanServer::Session* WanServer::acquireSessionOrNull(sessionID_t ID)
{
	uint64_t index = EXTRACT_INDEX_FROM_ID(ID);

	if (mMaximumSessionCount <= index)
	{
		return nullptr;
	}
	Session* session = &mSessions[index];

	InterlockedIncrement16(&session->Verifier.CurrentAsyncIOCount);

	if (session->Verifier.ReleaseFlag == true)
	{
		if (InterlockedDecrement16(&session->Verifier.CurrentAsyncIOCount) == 0)
		{
			releaseSession(session);
		}
		return nullptr;
	}

	if (session->ID != ID)
	{
		if (InterlockedDecrement16(&session->Verifier.CurrentAsyncIOCount) == 0)
		{
			releaseSession(session);
		}
		return nullptr;
	}
	return session;
}

void WanServer::encode(Header* header, char* data)
{
	unsigned char scalar = 0;
	unsigned char encoded = 0;

	unsigned char randKey = header->RandKey;

	scalar = header->CheckSum ^ (randKey + 1);
	encoded = scalar ^ (FIXED_KEY + 1);

	header->CheckSum = encoded;

	for (uint16_t i = 0; i < header->Length; ++i)
	{
		scalar = data[i] ^ (randKey + scalar + i + 2);
		encoded = scalar ^ (FIXED_KEY + encoded + i + 2);

		data[i] = encoded;
	}
}

void WanServer::decode(Header* header, char* data)
{
	unsigned char scalar;
	unsigned char decodeKey;

	unsigned char randKey = header->RandKey;

	unsigned char decodeScalar = header->CheckSum ^ (randKey + 1);

	header->CheckSum = decodeScalar ^ (FIXED_KEY + 1);

	scalar = header->CheckSum ^ (randKey + 1);
	decodeKey = scalar ^ (FIXED_KEY + 1);

	for (uint16_t i = 0; i < header->Length; ++i)
	{
		decodeScalar = data[i] ^ (randKey + scalar + i + 2);

		data[i] = decodeScalar ^ (FIXED_KEY + decodeKey + i + 2);

		scalar = data[i] ^ (randKey + scalar + i + 2);
		decodeKey = scalar ^ (FIXED_KEY + decodeKey + i + 2);
	}
}

BYTE WanServer::calculateCheckSum(char* data, unsigned int len)
{
	BYTE checkSum = 0;

	for (unsigned int i = 0; i < len; ++i)
	{
		checkSum += data[i];
	}
	return checkSum % 256;
}
