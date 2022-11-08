#pragma once

class WanServer final
{
public:
	enum { MAX_ASYNC_SENDS = 128 };

#pragma pack(push, 1)

	struct Header
	{
		uint8_t Code;
		uint16_t Length;
		uint8_t RandKey;
		uint8_t CheckSum;
	};
	static_assert(sizeof(Header) == 5, "Invalid WanHeader size");

#pragma pack(pop)

	struct OverlappedExtension
	{
		OVERLAPPED Overlapped;
		EIOType Type;
	};

	union ReleaseVerifier
	{
		struct
		{
			INT16 ReleaseFlag;
			INT16 CurrentAsyncIOCount;
		};
		INT32 Verifier;

		ReleaseVerifier()
			: Verifier(0x00010000)
		{
		}
	};
	static_assert(sizeof(ReleaseVerifier) == 4, "Invalid size");

	struct Session
	{
		sessionID_t ID;
		SOCKET Socket;
		SOCKADDR_IN Addr;
		bool bIsSending;
		ReleaseVerifier Verifier;
		RingBuffer ReceiveBuffer;

		INT16 NumSent;
		LockFreeQueue<Message*> SendQueue;
		Message* SentMessages[MAX_ASYNC_SENDS];

		OverlappedExtension RecvOverlapped;
		OverlappedExtension SendOverlapped;

		Session()
			: SendQueue(MAX_ASYNC_SENDS)
		{
			RecvOverlapped.Type = EIOType::Recv;
			SendOverlapped.Type = EIOType::Send;
		}
	};

public:
	WanServer();
	~WanServer();
	WanServer(const WanServer& other) = delete;
	WanServer(WanServer&& other) = delete;
	WanServer& operator=(const WanServer& other) = delete;
	WanServer& operator=(WanServer&& other) noexcept = delete;

	bool TryRun(const unsigned long IP, const unsigned short port
		, const unsigned int numWorkerThread, const unsigned int numRunningThread
		, const unsigned int maxSessionCount, const bool bSupportsNagle
		, INetworkEventListener* listener);
	void Terminate();
	bool TrySendMessage(const sessionID_t ID, Message* messagePtr);
	bool TryDisconnect(const sessionID_t ID);

	unsigned long GetIP() const;
	unsigned short GetPort() const;

	unsigned int GetMaximumSessionCount() const;
	unsigned int GetCurrentSessionCount() const;
	unsigned int GetNumAccept() const;
	unsigned int GetNumRecv() const;
	unsigned int GetNumSend() const;

	Message* CreateMessage();
	void ReleaseMessage(Message* message);
private:
	static unsigned int __stdcall acceptThread(void* param);
	static unsigned int __stdcall workerThread(void* param);

	static void encode(Header* header, char* data);
	static void decode(Header* header, char* data);
	static BYTE calculateCheckSum(char* data, unsigned int len);

	void sendPost(Session* session);
	void recvPost(Session* session);

	void releaseSession(Session* session);
	Session* acquireSessionOrNull(sessionID_t ID);

private:
	enum
	{
		INVALID_SESSION_ID = 0xFFFFFFFFFFFFFFFF,
		PACKET_CODE = 0x77,
		FIXED_KEY = 0x32
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

	unsigned int mNumAccept;
	unsigned int mNumRecv;
	unsigned int mNumSend;
};
