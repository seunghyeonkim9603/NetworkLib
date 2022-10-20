#pragma once

class ChattingServer : public INetworkEventListener
{
public:

	enum class EContentEvent
	{
		Join,
		Leave,
		Login,
		PacketReceived
	};

	struct ContentMessage
	{
		sessionID_t		SessionID;
		EContentEvent	Event;
		Message* Payload;
	};

	struct Player
	{
		enum
		{
			MAX_ID_LEN = 20,
			MAX_NICKNAME_LEN = 20
		};

		unsigned long	IP;
		unsigned short	Port;
		bool			bLogin;
		INT64			AccountNo;
		WCHAR			ID[MAX_ID_LEN];
		WCHAR			Nickname[MAX_NICKNAME_LEN];
		WORD			SectorX;
		WORD			SectorY;
		sessionID_t		SessionID;
		LARGE_INTEGER	LastReceivedTime;
	};


public:
	ChattingServer();
	~ChattingServer();

	bool TryRun(const unsigned long IP, const unsigned short port
		, const unsigned int numWorkerThread, const unsigned int numRunningThread
		, const unsigned int maxSessionCount, const bool bSupportsNagle);


	virtual void OnError(const int errorCode, const wchar_t* message) override;
	virtual void OnRecv(const sessionID_t ID, Message* message) override;
	virtual bool OnConnectionRequest(const unsigned long IP, const unsigned short port) override;
	virtual void OnClientJoin(const sessionID_t ID, const unsigned long IP, const unsigned short port) override;
	virtual void OnClientLeave(const sessionID_t ID) override;

private:
	static unsigned int __stdcall workerThread(void* param);
	static unsigned int __stdcall monitorThread(void* param);

	void processLoginPacket(sessionID_t id, Message& message);
	void processMoveSectorPacket(sessionID_t id, Message& message);
	void processChatPacket(sessionID_t id, Message& message);
	void processHeartBeatPacket(sessionID_t id, Message& message);

private:
	enum
	{
		LEFT = 0,
		LEFT_UP,
		UP,
		RIGHT_UP,
		RIGHT,
		RIGHT_DOWN,
		DOWN,
		LEFT_DOWN,
		NUM_DIRECTION,
		SECTOR_COLUMN = 50,
		SECTOR_ROW = 50
	};

	WanServer* mNetServer;
	HANDLE		mhWorkerThread;
	HANDLE		mhMonitorThread;
	HANDLE		mhNetworkEvent;

	ObjectPool<Player>					mPlayerPool;
	ObjectPool<ContentMessage>			mContentMessagePool;
	LockFreeQueue<ContentMessage*>		mMessageQueue;
	std::unordered_map<INT64, Player*>	mPlayers; //unordered_map들 버킷 사이즈 정할 것
	std::unordered_map<INT64, Player*>	mSectors[SECTOR_ROW][SECTOR_COLUMN];

	unsigned int mNumLoginPacket;
	unsigned int mNumChatPacket;
	unsigned int mNumSectorMovePacket;
	unsigned int mNumUpdate;
	unsigned int mNumPlayerBeforeLogin;
};