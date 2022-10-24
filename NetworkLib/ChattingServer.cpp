#include "stdafx.h"

#include "EPacketType.h"
#include "ChattingServer.h"

static bool gExit;

ChattingServer::ChattingServer(WanServer* server)
	: mNetServer(server),
	mMessageQueue(256)
{
	mhNetworkEvent = CreateEvent(nullptr, false, false, nullptr);
}

ChattingServer::~ChattingServer()
{
	// 플레이어 삭제, 메시지 큐 비우기
	mNetServer->Terminate();

	gExit = true;

	WaitForSingleObject(mhWorkerThread, INFINITE);
	CloseHandle(mhWorkerThread);
	CloseHandle(mhNetworkEvent);
}

bool ChattingServer::TryRun(const unsigned long IP, const unsigned short port, const unsigned int numWorkerThread, const unsigned int numRunningThread, const unsigned int maxSessionCount, const bool bSupportsNagle)
{
	mhWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, &workerThread, this, 0, nullptr);

	if (mhWorkerThread == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	if (!mNetServer->TryRun(IP, port, numWorkerThread, numRunningThread, maxSessionCount, bSupportsNagle, this))
	{
		gExit = true;
		SetEvent(mhNetworkEvent);

		WaitForSingleObject(mhWorkerThread, INFINITE);
		CloseHandle(mhWorkerThread);
		return false;
	}
	return true;
}

unsigned int ChattingServer::GetTotalLoginPacketCount() const
{
	return mNumLoginPacket;
}

unsigned int ChattingServer::GetTotalChattingPacketCount() const
{
	return mNumChatPacket;
}

unsigned int ChattingServer::GetTotalSectorMovePacketCount() const
{
	return mNumSectorMovePacket;
}

unsigned int ChattingServer::GetTotalUpdateCount() const
{
	return mNumUpdate;
}

unsigned int ChattingServer::GetPlayerCount() const
{
	return mPlayers.size();
}

unsigned int ChattingServer::GetPlayerBeforeLoginCount() const
{
	return mNumPlayerBeforeLogin;
}

unsigned int ChattingServer::GetMessagePoolAllocCount() const
{
	return mContentMessagePool.GetAllCount();
}

unsigned int ChattingServer::GetPlayerPoolAllocCount() const
{
	return mPlayerPool.GetAllCount();
}

long ChattingServer::GetMessageQueueSize() const
{
	return mMessageQueue.GetSize();
}

void ChattingServer::OnError(const int errorCode, const wchar_t* message)
{
}

void ChattingServer::OnRecv(const sessionID_t ID, Message* message)
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

bool ChattingServer::OnConnectionRequest(const unsigned long IP, const unsigned short port)
{
	return true;
}

void ChattingServer::OnClientJoin(const sessionID_t ID, const unsigned long IP, const unsigned short port)
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

void ChattingServer::OnClientLeave(const sessionID_t ID)
{
	ContentMessage* contentMessage = mContentMessagePool.GetObject();
	{
		contentMessage->SessionID = ID;
		contentMessage->Event = EContentEvent::Leave;
	}
	mMessageQueue.TryEnqueue(contentMessage);

	SetEvent(mhNetworkEvent);
}

unsigned int __stdcall ChattingServer::workerThread(void* param)
{
	ChattingServer* server = static_cast<ChattingServer*>(param);

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
				playerBeforeLogin->bLogin = false;
				playerBeforeLogin->SessionID = sessionID;
				QueryPerformanceCounter(&playerBeforeLogin->LastReceivedTime);
			}
			server->mPlayers.insert({ sessionID, playerBeforeLogin });

			++server->mNumPlayerBeforeLogin;

			server->mNetServer->ReleaseMessage(contentMessage->Payload);
			server->mContentMessagePool.ReleaseObject(contentMessage);
		}
		break;
		case EContentEvent::Leave:
		{
			auto iter = server->mPlayers.find(sessionID);

			Player* leftPlayer = iter->second;

			if (!leftPlayer->bLogin)
			{
				--server->mNumPlayerBeforeLogin;
			}
			server->mPlayers.erase(sessionID);
			server->mSectors[leftPlayer->SectorY][leftPlayer->SectorX].erase(sessionID);

			server->mPlayerPool.ReleaseObject(leftPlayer);
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
				// Unhandled Packet type
				break;
			}
			server->mNetServer->ReleaseMessage(contentMessage->Payload);
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


void ChattingServer::sendToSector(std::unordered_map<INT64, Player*>& sector, Message& message)
{
	for (auto iter : sector)
	{
		mNetServer->TrySendMessage(iter.first, &message);
	}
}

void ChattingServer::sendToSectorRange(WORD x, WORD y, Message& message)
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

void ChattingServer::processLoginPacket(sessionID_t id, Message& message)
{
	auto iter = mPlayers.find(id);

	Player* player = iter->second;

	if (player->bLogin)
	{
		mNetServer->TryDisconnect(id);
		return;
	}
	message >> player->AccountNo;
	message.Read((char*)player->ID, sizeof(player->ID));
	message.Read((char*)player->Nickname, sizeof(player->Nickname));

	player->bLogin = true;

	Message* sendMessage = mNetServer->CreateMessage();
	{
		*sendMessage << EPacketType::PACKET_TYPE_SC_CHAT_RES_LOGIN;
		*sendMessage << (BYTE)1;
		*sendMessage << player->AccountNo;

		mNetServer->TrySendMessage(id, sendMessage);
	}
	mNetServer->ReleaseMessage(sendMessage);

	QueryPerformanceCounter(&player->LastReceivedTime);
	--mNumPlayerBeforeLogin;
	++mNumLoginPacket;
}

void ChattingServer::processMoveSectorPacket(sessionID_t id, Message& message)
{
	auto iter = mPlayers.find(id);

	Player* player = iter->second;

	INT64 accountNo;
	WORD toX;
	WORD toY;

	message >> accountNo;
	message >> toX;
	message >> toY;

	if (!player->bLogin || SECTOR_COLUMN <= toX || SECTOR_ROW <= toY)
	{
		mNetServer->TryDisconnect(id);
		return;
	}
	toX += 1;
	toY += 1;
	mSectors[player->SectorY][player->SectorX].erase(id);

	player->AccountNo = accountNo;
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

void ChattingServer::processChatPacket(sessionID_t id, Message& message)
{
	auto iter = mPlayers.find(id);

	Player* player = iter->second;

	if (!player->bLogin)
	{
		mNetServer->TryDisconnect(id);
		return;
	}
	INT64 accountNo;
	WORD messageLength;

	message >> accountNo;
	message >> messageLength;

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

void ChattingServer::processHeartBeatPacket(sessionID_t id, Message& message)
{
	auto iter = mPlayers.find(id);

	Player* player = iter->second;

	QueryPerformanceCounter(&player->LastReceivedTime);
}
