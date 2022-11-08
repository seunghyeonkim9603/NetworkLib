#pragma once

class MultiThreadChattingServer : public INetworkEventListener
{
public:
	struct Player
	{
		enum
		{
			MAX_ID_LEN = 20,
			MAX_NICKNAME_LEN = 20
		};

		unsigned long	IP;
		unsigned short	Port;
		bool			bIsLogin;
		INT64			AccountNo;
		WORD			SectorX;
		WORD			SectorY;
		sessionID_t		SessionID;
		LARGE_INTEGER	LastReceivedTime;
		WCHAR			ID[MAX_ID_LEN];
		WCHAR			Nickname[MAX_NICKNAME_LEN];
	};

	struct Sector
	{
		SRWLOCK										Lock;
		std::unordered_map<sessionID_t, Player*>	Players;
	};

public:
	MultiThreadChattingServer(WanServer* server);
	~MultiThreadChattingServer();

	bool TryRun(const unsigned long IP, const unsigned short port
		, const unsigned int numWorkerThread, const unsigned int numRunningThread
		, const unsigned int maxSessionCount, const bool bSupportsNagle);

	unsigned int	GetTotalLoginPacketCount() const;
	unsigned int	GetTotalChattingPacketCount() const;
	unsigned int	GetTotalSectorMovePacketCount() const;
	unsigned int	GetTotalUpdateCount() const;
	unsigned int	GetPlayerCount() const;
	unsigned int	GetPlayerPoolAllocCount() const;

	virtual void OnError(const int errorCode, const wchar_t* message) override;
	virtual void OnRecv(const sessionID_t ID, Message* message) override;
	virtual bool OnConnectionRequest(const unsigned long IP, const unsigned short port) override;
	virtual void OnClientJoin(const sessionID_t ID, const unsigned long IP, const unsigned short port) override;
	virtual void OnClientLeave(const sessionID_t ID) override;
private:
	static unsigned int __stdcall timeoutThread(void* param);

	void removePlayer(sessionID_t id);
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
	HANDLE		mhTimeoutThread;

	SRWLOCK										mPlayersLock;
	ObjectPool<Player>							mPlayerPool;
	std::unordered_map<sessionID_t, Player*>	mPlayers;
	Sector										mSectors[SECTOR_ROW + 2][SECTOR_COLUMN + 2];

	unsigned int mNumLoginPacket;
	unsigned int mNumChatPacket;
	unsigned int mNumSectorMovePacket;
	unsigned int mNumUpdate;
};