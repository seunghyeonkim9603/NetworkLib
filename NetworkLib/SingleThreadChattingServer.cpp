#include "stdafx.h"

#include "EPacketType.h"
#include "SingleThreadChattingServer.h"

static bool gExit;

SingleThreadChattingServer::SingleThreadChattingServer(WanServer* server)
	: mNetServer(server),
	mMessageQueue(256),
	mNumChatPacket(0),
	mNumLoginPacket(0),
	mNumSectorMovePacket(0),
	mNumUpdate(0)
{
	mhNetworkEvent = CreateEvent(nullptr, false, false, nullptr);
}

SingleThreadChattingServer::~SingleThreadChattingServer()
{
	// 플레이어 삭제, 메시지 큐 비우기
	mNetServer->Terminate();

	gExit = true;

	WaitForSingleObject(mhWorkerThread, INFINITE);
	WaitForSingleObject(mhTimeoutThread, INFINITE);
	CloseHandle(mhWorkerThread);
	CloseHandle(mhNetworkEvent);
}

bool SingleThreadChattingServer::TryRun(const unsigned long IP, const unsigned short port, const unsigned int numWorkerThread, const unsigned int numRunningThread, const unsigned int maxSessionCount, const bool bSupportsNagle)
{
	mhWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, &workerThread, this, 0, nullptr);
	mhTimeoutThread = (HANDLE)_beginthreadex(nullptr, 0, &timeoutEventGenerator, this, 0, nullptr);

	if (!mNetServer->TryRun(IP, port, numWorkerThread, numRunningThread, maxSessionCount, bSupportsNagle, this))
	{
		gExit = true;
		SetEvent(mhNetworkEvent);

		WaitForSingleObject(mhWorkerThread, INFINITE);
		WaitForSingleObject(mhTimeoutThread, INFINITE);
		CloseHandle(mhWorkerThread);
		return false;
	}
	return true;
}

unsigned int SingleThreadChattingServer::GetTotalLoginPacketCount() const
{
	return mNumLoginPacket;
}

unsigned int SingleThreadChattingServer::GetTotalChattingPacketCount() const
{
	return mNumChatPacket;
}

unsigned int SingleThreadChattingServer::GetTotalSectorMovePacketCount() const
{
	return mNumSectorMovePacket;
}

unsigned int SingleThreadChattingServer::GetTotalUpdateCount() const
{
	return mNumUpdate;
}

unsigned int SingleThreadChattingServer::GetPlayerCount() const
{
	return mPlayers.size();
}

unsigned int SingleThreadChattingServer::GetMessagePoolAllocCount() const
{
	return mContentMessagePool.GetAllCount();
}

unsigned int SingleThreadChattingServer::GetPlayerPoolAllocCount() const
{
	return mPlayerPool.GetAllCount();
}

long SingleThreadChattingServer::GetMessageQueueSize() const
{
	return mMessageQueue.GetSize();
}

void SingleThreadChattingServer::OnError(const int errorCode, const wchar_t* message)
{
}

void SingleThreadChattingServer::OnRecv(const sessionID_t ID, Message* message)
{
	message->AddReferenceCount();

	ContentMessage* contentMessage = mContentMessagePool.GetObject();
	{
		contentMessage->SessionID = ID;
		contentMessage->Event = EContentEvent::PacketReceived;
		contentMessage->Payload = message;
	}
	mMessageQueue.TryEnqueue(contentMessage);

	SetEvent(mhNetworkEvent);
}

bool SingleThreadChattingServer::OnConnectionRequest(const unsigned long IP, const unsigned short port)
{
	return true;
}

void SingleThreadChattingServer::OnClientJoin(const sessionID_t ID, const unsigned long IP, const unsigned short port)
{
	ContentMessage* contentMessage = mContentMessagePool.GetObject();
	{
		contentMessage->SessionID = ID;
		contentMessage->Event = EContentEvent::Join;
		contentMessage->Payload = mNetServer->CreateMessage();
		*contentMessage->Payload << IP << port;
	}
	mMessageQueue.TryEnqueue(contentMessage);

	SetEvent(mhNetworkEvent);
}

void SingleThreadChattingServer::OnClientLeave(const sessionID_t ID)
{
	ContentMessage* contentMessage = mContentMessagePool.GetObject();
	{
		contentMessage->SessionID = ID;
		contentMessage->Event = EContentEvent::Leave;
	}
	mMessageQueue.TryEnqueue(contentMessage);

	SetEvent(mhNetworkEvent);
}

unsigned int __stdcall SingleThreadChattingServer::workerThread(void* param)
{
	SingleThreadChattingServer* server = static_cast<SingleThreadChattingServer*>(param);

	while (!gExit)
	{
		ContentMessage* contentMessage;

		while (!server->mMessageQueue.TryDequeue(&contentMessage))
		{
			WaitForSingleObject(server->mhNetworkEvent, INFINITE);
		}
		Message& payload = *contentMessage->Payload;
		sessionID_t sessionID = contentMessage->SessionID;

		switch (contentMessage->Event)
		{
		case EContentEvent::Join:
		{
			Player* playerBeforeLogin = server->mPlayerPool.GetObject();
			{
				payload >> playerBeforeLogin->IP;
				payload >> playerBeforeLogin->Port;
				playerBeforeLogin->SectorX = 0;
				playerBeforeLogin->SectorY = 0;
				playerBeforeLogin->SessionID = sessionID;
				QueryPerformanceCounter(&playerBeforeLogin->LastReceivedTime);
			}
			server->mPlayers.insert({ sessionID, playerBeforeLogin });

			server->mContentMessagePool.ReleaseObject(contentMessage);
			server->mNetServer->ReleaseMessage(&payload);
		}
		break;
		case EContentEvent::Leave:
		{
			auto iter = server->mPlayers.find(sessionID);

			if (iter != server->mPlayers.end())
			{
				server->removePlayer(iter->second);
			}
			server->mContentMessagePool.ReleaseObject(contentMessage);
		}
		break;
		case EContentEvent::PacketReceived:
		{
			WORD packetType;

			payload >> packetType;

			switch (packetType)
			{
			case EPacketType::PACKET_TYPE_CS_CHAT_REQ_LOGIN:
			{
				server->processLoginPacket(sessionID, payload);
			}
			break;
			case EPacketType::PACKET_TYPE_CS_CHAT_REQ_SECTOR_MOVE:
			{
				server->processMoveSectorPacket(sessionID, payload);
			}
			break;
			case EPacketType::PACKET_TYPE_CS_CHAT_REQ_MESSAGE:
			{
				server->processChatPacket(sessionID, payload);
			}
			break;
			case EPacketType::PACKET_TYPE_CS_CHAT_REQ_HEARTBEAT:
			{
				server->processHeartBeatPacket(sessionID, payload);
			}
			break;
			default:
				server->mNetServer->TryDisconnect(sessionID);
				break;
			}
			server->mContentMessagePool.ReleaseObject(contentMessage);
			server->mNetServer->ReleaseMessage(&payload);
		}
		break;
		case EContentEvent::Timeout:
		{
			LARGE_INTEGER curr;
			LARGE_INTEGER freq;

			QueryPerformanceCounter(&curr);
			QueryPerformanceFrequency(&freq);

			for (auto iter : server->mPlayers)
			{
				Player* player = iter.second;

				if (server->mLoginAccountNumbers.find(player->AccountNo) != server->mLoginAccountNumbers.end())
				{
					if (LOGIN_PLAYER_TIMEOUT_SEC * freq.QuadPart < curr.QuadPart - player->LastReceivedTime.QuadPart)
					{
						server->mNetServer->TryDisconnect(player->SessionID);
					}
				}
				else
				{
					if (UNLOGIN_PLAYER_TIMEOUT_SEC * freq.QuadPart < curr.QuadPart - player->LastReceivedTime.QuadPart)
					{
						server->mNetServer->TryDisconnect(player->SessionID);
					}
				}
			}
			server->mContentMessagePool.ReleaseObject(contentMessage);
		}
		break;
		default:
			break;
		}
		++server->mNumUpdate;
	}

	return 0;
}

unsigned int __stdcall SingleThreadChattingServer::timeoutEventGenerator(void* param)
{
	SingleThreadChattingServer* server = (SingleThreadChattingServer*)param;

	while (gExit)
	{
		Sleep(TIMEOUT_EVENT_PERIOD_MS);

		ContentMessage* contentMessage = server->mContentMessagePool.GetObject();
		{
			contentMessage->Event = EContentEvent::Timeout;
		}
		server->mMessageQueue.TryEnqueue(contentMessage);

		SetEvent(server->mhNetworkEvent);
	}
	return 0;
}

void SingleThreadChattingServer::removePlayer(Player* player)
{
	mPlayers.erase(player->SessionID);
	mLoginAccountNumbers.erase(player->AccountNo);
	mSectors[player->SectorY][player->SectorX].erase(player->SessionID);

	mPlayerPool.ReleaseObject(player);
}


void SingleThreadChattingServer::sendToSector(std::unordered_map<INT64, Player*>& sector, Message& message)
{
	for (auto iter : sector)
	{
		mNetServer->TrySendMessage(iter.first, &message);
	}
}

void SingleThreadChattingServer::sendToSectorRange(WORD x, WORD y, Message& message)
{
	sendToSector(mSectors[y][x], message);
	sendToSector(mSectors[y][x - 1], message);
	sendToSector(mSectors[y - 1][x - 1], message);
	sendToSector(mSectors[y - 1][x], message);
	sendToSector(mSectors[y - 1][x + 1], message);
	sendToSector(mSectors[y][x + 1], message);
	sendToSector(mSectors[y + 1][x + 1], message);
	sendToSector(mSectors[y + 1][x], message);
	sendToSector(mSectors[y + 1][x - 1], message);
}

void SingleThreadChattingServer::processLoginPacket(sessionID_t id, Message& message)
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

	if (mLoginAccountNumbers.find(player->AccountNo) != mLoginAccountNumbers.end())
	{
		removePlayer(player);
		mNetServer->TryDisconnect(id);
		return;
	}
	mLoginAccountNumbers.insert(player->AccountNo);

	Message* sendMessage = mNetServer->CreateMessage();
	{
		*sendMessage << EPacketType::PACKET_TYPE_SC_CHAT_RES_LOGIN;
		*sendMessage << (BYTE)1;
		*sendMessage << player->AccountNo;

		mNetServer->TrySendMessage(id, sendMessage);
	}
	mNetServer->ReleaseMessage(sendMessage);

	QueryPerformanceCounter(&player->LastReceivedTime);
	++mNumLoginPacket;
}

void SingleThreadChattingServer::processMoveSectorPacket(sessionID_t id, Message& message)
{
	auto iter = mPlayers.find(id);

	if (iter == mPlayers.end())
	{
		return;
	}
	Player* player = iter->second;

	if (mLoginAccountNumbers.find(player->AccountNo) == mLoginAccountNumbers.end())
	{
		removePlayer(player);
		mNetServer->TryDisconnect(id);
		return;
	}
	INT64 accountNo;
	WORD toX;
	WORD toY;

	message >> accountNo;
	message >> toX;
	message >> toY;

	if (SECTOR_COLUMN <= toX || SECTOR_ROW <= toY || accountNo != player->AccountNo)
	{
		removePlayer(player);
		mNetServer->TryDisconnect(id);
		return;
	}
	toX += 1;
	toY += 1;
	mSectors[player->SectorY][player->SectorX].erase(id);

	player->SectorX = toX;
	player->SectorY = toY;

	mSectors[player->SectorY][player->SectorX].insert({ id, player });

	Message* sendMessage = mNetServer->CreateMessage();
	{
		*sendMessage << EPacketType::PACKET_TYPE_SC_CHAT_RES_SECTOR_MOVE;
		*sendMessage << player->AccountNo;
		*sendMessage << player->SectorX - 1;
		*sendMessage << player->SectorY - 1;

		mNetServer->TrySendMessage(id, sendMessage);
	}
	mNetServer->ReleaseMessage(sendMessage);

	QueryPerformanceCounter(&player->LastReceivedTime);
	++mNumSectorMovePacket;
}

void SingleThreadChattingServer::processChatPacket(sessionID_t id, Message& message)
{
	auto iter = mPlayers.find(id);

	if (iter == mPlayers.end())
	{
		return;
	}
	Player* player = iter->second;

	if (mLoginAccountNumbers.find(player->AccountNo) == mLoginAccountNumbers.end())
	{
		removePlayer(player);
		mNetServer->TryDisconnect(id);
		return;
	}
	INT64 accountNo;
	WORD messageLength;

	message >> accountNo;
	message >> messageLength;

	if (MAX_CHAT_LENGTH < messageLength || accountNo != player->AccountNo)
	{
		removePlayer(player);
		mNetServer->TryDisconnect(id);
		return;
	}
	WORD sectorX = player->SectorX;
	WORD sectorY = player->SectorY;

	Message* sendMessage = mNetServer->CreateMessage();
	{
		*sendMessage << EPacketType::PACKET_TYPE_SC_CHAT_RES_MESSAGE;
		*sendMessage << accountNo;
		sendMessage->Write((char*)player->ID, sizeof(player->ID));
		sendMessage->Write((char*)player->Nickname, sizeof(player->Nickname));
		*sendMessage << messageLength;
		sendMessage->Write(message.GetFront(), messageLength);

		sendToSectorRange(sectorX, sectorY, *sendMessage);
	}
	mNetServer->ReleaseMessage(sendMessage);

	QueryPerformanceCounter(&player->LastReceivedTime);
	++mNumChatPacket;
}

void SingleThreadChattingServer::processHeartBeatPacket(sessionID_t id, Message& message)
{
	auto iter = mPlayers.find(id);

	if (iter == mPlayers.end())
	{
		return;
	}
	Player* player = iter->second;

	QueryPerformanceCounter(&player->LastReceivedTime);
}
