#pragma once

class SingleThreadChattingServer : public INetworkEventListener
{
public:

	enum class EContentEvent
	{
		Join,
		Leave,
		PacketReceived,
		Timeout
	};

	struct ContentMessage
	{
		sessionID_t		SessionID;
		EContentEvent	Event;
		Message*		Payload;
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
		INT64			AccountNo;
		WCHAR			ID[MAX_ID_LEN];
		WCHAR			Nickname[MAX_NICKNAME_LEN];
		WORD			SectorX;
		WORD			SectorY;
		sessionID_t		SessionID;
		LARGE_INTEGER	LastReceivedTime;
	};


public:
	SingleThreadChattingServer(WanServer* server);
	~SingleThreadChattingServer();

	bool TryRun(const unsigned long IP, const unsigned short port
		, const unsigned int numWorkerThread, const unsigned int numRunningThread
		, const unsigned int maxSessionCount, const bool bSupportsNagle);

	unsigned int	GetTotalLoginPacketCount() const;
	unsigned int	GetTotalChattingPacketCount() const;
	unsigned int	GetTotalSectorMovePacketCount() const;
	unsigned int	GetTotalUpdateCount() const;
	unsigned int	GetPlayerCount() const;
	unsigned int	GetMessagePoolAllocCount() const;
	unsigned int	GetPlayerPoolAllocCount() const;
	long			GetMessageQueueSize() const;


	virtual void OnError(const int errorCode, const wchar_t* message) override;
	virtual void OnRecv(const sessionID_t ID, Message* message) override;
	virtual bool OnConnectionRequest(const unsigned long IP, const unsigned short port) override;
	virtual void OnClientJoin(const sessionID_t ID, const unsigned long IP, const unsigned short port) override;
	virtual void OnClientLeave(const sessionID_t ID) override;

private:
	static unsigned int __stdcall workerThread(void* param);
	static unsigned int __stdcall timeoutEventGenerator(void* param);

	void removePlayer(Player* player);

	void sendToSector(std::unordered_map<INT64, Player*>& sector, Message& message);
	void sendToSectorRange(WORD x, WORD y, Message& message);

	void processLoginPacket(sessionID_t id, Message& message);
	void processMoveSectorPacket(sessionID_t id, Message& message);
	void processChatPacket(sessionID_t id, Message& message);
	void processHeartBeatPacket(sessionID_t id, Message& message);


private:
	enum
	{
		SECTOR_COLUMN = 50,
		SECTOR_ROW = 50,
		MAX_CHAT_LENGTH = 255,
		LOGIN_PLAYER_TIMEOUT_SEC = 40,
		UNLOGIN_PLAYER_TIMEOUT_SEC = 5,
		TIMEOUT_EVENT_PERIOD_MS = 1000,
		INVALID_SESSION_ACCOUNT_NO = 0xFFFFFFFFFFFFFFFF
	};

	WanServer*	mNetServer;
	HANDLE		mhWorkerThread;
	HANDLE		mhTimeoutThread;
	HANDLE		mhNetworkEvent;

	ObjectPool<Player>							mPlayerPool;
	ObjectPool<ContentMessage>					mContentMessagePool;
	LockFreeQueue<ContentMessage*>				mMessageQueue;
	std::unordered_map<sessionID_t, Player*>	mPlayers;
	std::unordered_set<INT64>					mLoginAccountNumbers;
	std::unordered_map<INT64, Player*>			mSectors[SECTOR_ROW + 2][SECTOR_COLUMN + 2];

	unsigned int mNumLoginPacket;
	unsigned int mNumChatPacket;
	unsigned int mNumSectorMovePacket;
	unsigned int mNumUpdate;
};