#pragma once

typedef uint64_t sessionID_t;

class INetworkEventListener;

class LanServer final
{
public:
	enum { MAX_ASYNC_SENDS = 128 };
	struct OverlappedExtension
	{
		OVERLAPPED Overlapped;
		EIOType Type;
	};

	struct Session
	{
		uint64_t ID;
		SOCKET Socket;
		SOCKADDR_IN Addr;
		bool bIsSending;
		INT16 CurrentAsyncIOCount;

		RingBuffer ReceiveBuffer;

		INT16 NumSend;
		LockFreeQueue<IntrusivePointer<Message>*> SendQueue;
		IntrusivePointer<Message>* SentMessages[MAX_ASYNC_SENDS];

		OverlappedExtension RecvOverlapped;
		OverlappedExtension SendOverlapped;

		Session()
			:CurrentAsyncIOCount(0),
			SendQueue(MAX_ASYNC_SENDS)
		{
			RecvOverlapped.Type = EIOType::Recv;
			SendOverlapped.Type = EIOType::Send;
		}
	};

public:
	LanServer();
	~LanServer();
	LanServer(const LanServer& other) = delete;
	LanServer(LanServer&& other) = delete;
	LanServer& operator=(const LanServer& other) = delete;
	LanServer& operator=(LanServer&& other) noexcept = delete;

	bool TryRun(const unsigned long IP, const unsigned short port
		, const unsigned int numWorkerThread, const unsigned int numRunningThread
		, const unsigned int maxSessionCount, const bool bSupportsNagle
		, INetworkEventListener* listener);
	void Terminate();
	bool TrySendMessage(const sessionID_t ID, IntrusivePointer<Message>* messagePtr);
	bool TryDisconnect(const sessionID_t ID);

	unsigned long GetIP() const;
	unsigned short GetPort() const;

	unsigned int GetMaximumSessionCount() const;
	unsigned int GetCurrentSessionCount() const;

private:
	static unsigned int __stdcall acceptThread(void* param);
	static unsigned int __stdcall workerThread(void* param);

	void sendPost(Session* session);
	void recvPost(Session* session);
	void releaseSession(Session* session);

private:
	enum
	{
		INVALID_SESSION_ID = 0xFFFFFFFFFFFFFFFF
	};

	SOCKET mListenSocket;
	HANDLE mhCompletionPort;

	std::vector<HANDLE> mhThreads;

	Session* mSessions;

	LockFreeStack<uint64_t>* mUseableIndexesStack;

	unsigned long mIP;
	unsigned short mPort;

	unsigned int mMaximumSessionCount;
	unsigned int mCurrentSessionCount;

	INetworkEventListener* mListener;
};
