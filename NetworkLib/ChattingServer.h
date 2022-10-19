#pragma once

class ChattingServer : public INetworkEventListener
{
public:

	enum class EContentEvent
	{
		Join,
		Leave,
		PacketReceived
	};

	struct ContentMessage
	{
		sessionID_t		SessionID;
		EContentEvent	Event;
		Message*		Payload;
	};

	struct Player
	{
		unsigned long	IP;
		unsigned short	Port;
		INT64			AccountNo;
		WCHAR			ID[20];
		WCHAR			Nickname[20];
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
	 
private:
	enum
	{
		SECTOR_COLUMN = 50,
		SECTOR_ROW = 50
	};

	WanServer*	mNetServer;
	HANDLE		mhWorkerThread;
	HANDLE		mhNetworkEvent;

	ObjectPool<Player>					mPlayerPool;
	ObjectPool<ContentMessage>			mContentMessagePool;
	LockFreeQueue<ContentMessage*>		mMessageQueue;
	std::unordered_map<INT64, Player*>	mPlayers; //unordered_map들 버킷 사이즈 정할 것
	std::unordered_map<INT64, Player*>	mPlayersBeforeLogin;
	std::unordered_map<INT64, Player*>	mSectors[SECTOR_ROW][SECTOR_COLUMN];
};