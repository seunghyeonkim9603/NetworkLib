#include "stdafx.h"
#include "EPacketType.h"

#include "MultiThreadChattingServer.h"


bool gExit;

MultiThreadChattingServer::MultiThreadChattingServer(WanServer* server)
	: mNetServer(server),
	mNumChatPacket(0),
	mNumLoginPacket(0),
	mNumSectorMovePacket(0),
	mNumUpdate(0)
{
	InitializeSRWLock(&mPlayersLock);

	for (int i = 0; i < SECTOR_ROW + 2; ++i)
	{
		for (int j = 0; j < SECTOR_COLUMN + 2; ++j)
		{
			InitializeSRWLock(&mSectors[i]->Lock);
		}
	}
}

MultiThreadChattingServer::~MultiThreadChattingServer()
{
}

bool MultiThreadChattingServer::TryRun(const unsigned long IP, const unsigned short port, const unsigned int numWorkerThread, const unsigned int numRunningThread, const unsigned int maxSessionCount, const bool bSupportsNagle)
{
	mhTimeoutThread = (HANDLE)_beginthreadex(nullptr, 0, &timeoutThread, this, 0, nullptr);

	if (!mNetServer->TryRun(IP, port, numWorkerThread, numRunningThread, maxSessionCount, bSupportsNagle, this))
	{
		gExit = true;

		WaitForSingleObject(mhTimeoutThread, INFINITE);
		CloseHandle(mhTimeoutThread);
		return false;
	}
	return true;
}

unsigned int MultiThreadChattingServer::GetTotalLoginPacketCount() const
{
	return mNumLoginPacket;
}

unsigned int MultiThreadChattingServer::GetTotalChattingPacketCount() const
{
	return mNumChatPacket;
}

unsigned int MultiThreadChattingServer::GetTotalSectorMovePacketCount() const
{
	return mNumSectorMovePacket;
}

unsigned int MultiThreadChattingServer::GetTotalUpdateCount() const
{
	return mNumUpdate;
}

unsigned int MultiThreadChattingServer::GetPlayerCount() const
{
	return mPlayers.size();
}

unsigned int MultiThreadChattingServer::GetPlayerPoolAllocCount() const
{
	return mPlayerPool.GetActiveCount();;
}

void MultiThreadChattingServer::OnError(const int errorCode, const wchar_t* message)
{
}

void MultiThreadChattingServer::OnRecv(const sessionID_t ID, Message* message)
{
	WORD packetType;

	*message >> packetType;

	switch (packetType)
	{
	case EPacketType::PACKET_TYPE_CS_CHAT_REQ_LOGIN:
	{
		processLoginPacket(ID, *message);
	}
	break;
	case EPacketType::PACKET_TYPE_CS_CHAT_REQ_SECTOR_MOVE:
	{
		processMoveSectorPacket(ID, *message);
	}
	break;
	case EPacketType::PACKET_TYPE_CS_CHAT_REQ_MESSAGE:
	{
		processChatPacket(ID, *message);
	}
	break;
	case EPacketType::PACKET_TYPE_CS_CHAT_REQ_HEARTBEAT:
	{
		processHeartBeatPacket(ID, *message);
	}
	break;
	default:
		mNetServer->TryDisconnect(ID);
		break;
	}
}

bool MultiThreadChattingServer::OnConnectionRequest(const unsigned long IP, const unsigned short port)
{
	return true;
}

void MultiThreadChattingServer::OnClientJoin(const sessionID_t ID, const unsigned long IP, const unsigned short port)
{
	Player* playerBeforeLogin = mPlayerPool.GetObject();
	{
		playerBeforeLogin->IP = IP;
		playerBeforeLogin->Port = port;
		playerBeforeLogin->SectorX = 0;
		playerBeforeLogin->SectorY = 0;
		playerBeforeLogin->SessionID = ID;
		playerBeforeLogin->bIsLogin = false;
		QueryPerformanceCounter(&playerBeforeLogin->LastReceivedTime);
	}

	AcquireSRWLockExclusive(&mPlayersLock);
	mPlayers.insert({ ID, playerBeforeLogin });
	ReleaseSRWLockExclusive(&mPlayersLock);
}

void MultiThreadChattingServer::OnClientLeave(const sessionID_t ID)
{
	removePlayer(ID);
}

unsigned int __stdcall MultiThreadChattingServer::timeoutThread(void* param)
{
	return 0;
}

void MultiThreadChattingServer::removePlayer(sessionID_t id)
{
	Player* leavedPlayer;

	AcquireSRWLockExclusive(&mPlayersLock);
	{
		auto iter = mPlayers.find(id);

		if (iter == mPlayers.end())
		{
			ReleaseSRWLockExclusive(&mPlayersLock);
			return;
		}
		leavedPlayer = iter->second;

		WORD x = leavedPlayer->SectorX;
		WORD y = leavedPlayer->SectorY;

		Sector* sector = &mSectors[y][x];

		AcquireSRWLockExclusive(&sector->Lock);
		sector->Players.erase(id);
		ReleaseSRWLockExclusive(&sector->Lock);

		mPlayers.erase(id);
	}
	ReleaseSRWLockExclusive(&mPlayersLock);

	mPlayerPool.ReleaseObject(leavedPlayer);
}

void MultiThreadChattingServer::processLoginPacket(sessionID_t id, Message& message)
{
	auto iter = mPlayers.find(id);

	if (iter == mPlayers.end())
	{
		return;
	}
	Player* player = iter->second;

	message >> player->AccountNo;
	message.Read((char*)player->ID, sizeof(player->ID));
	message.Read((char*)player->Nickname, sizeof(player->Nickname));

	if (player->bIsLogin)
	{
		removePlayer(id);
		mNetServer->TryDisconnect(id);
		return;
	}
	player->bIsLogin = true;

	Message* sendMessage = mNetServer->CreateMessage();
	{
		*sendMessage << EPacketType::PACKET_TYPE_SC_CHAT_RES_LOGIN;
		*sendMessage << (BYTE)1;
		*sendMessage << player->AccountNo;

		mNetServer->TrySendMessage(id, sendMessage);
	}
	mNetServer->ReleaseMessage(sendMessage);

	QueryPerformanceCounter(&player->LastReceivedTime);
	InterlockedIncrement(&mNumLoginPacket);
}

void MultiThreadChattingServer::processMoveSectorPacket(sessionID_t id, Message& message)
{
}

void MultiThreadChattingServer::processChatPacket(sessionID_t id, Message& message)
{
}

void MultiThreadChattingServer::processHeartBeatPacket(sessionID_t id, Message& message)
{
}
