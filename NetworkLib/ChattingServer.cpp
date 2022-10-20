#include "stdafx.h"

#include "EPacketType.h"
#include "ChattingServer.h"

static bool gExit;

ChattingServer::ChattingServer()
	: mNetServer(new WanServer()),
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
	WaitForSingleObject(mhMonitorThread, INFINITE);
	CloseHandle(mhWorkerThread);
	CloseHandle(mhMonitorThread);
	CloseHandle(mhNetworkEvent);
}

bool ChattingServer::TryRun(const unsigned long IP, const unsigned short port, const unsigned int numWorkerThread, const unsigned int numRunningThread, const unsigned int maxSessionCount, const bool bSupportsNagle)
{
	mhWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, &workerThread, this, 0, nullptr);
	mhMonitorThread = (HANDLE)_beginthreadex(nullptr, 0, &monitorThread, this, 0, nullptr);

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

	while (gExit)
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

unsigned int __stdcall ChattingServer::monitorThread(void* param)
{
	ChattingServer* server = (ChattingServer*)param;

	LARGE_INTEGER freq;
	LARGE_INTEGER current;
	LARGE_INTEGER before;

	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&before);

	unsigned int numTotalAccept = 0;
	unsigned int numTotalAcceptBefore = 0;
	unsigned int numTotalUpdate = 0;
	unsigned int numTotalUpdateBefore = 0;
	unsigned int numTotalRecv = 0;
	unsigned int numTotalRecvBefore = 0;
	unsigned int numTotalSend = 0;
	unsigned int numTotalSendBefore = 0;

	while (gExit)
	{
		QueryPerformanceCounter(&current);
		if (freq.QuadPart <= current.QuadPart - before.QuadPart)
		{
			before.QuadPart = current.QuadPart;

			std::cout << "Session Count : " << server->mNetServer->GetCurrentSessionCount() << std::endl;
			std::cout << "Message Pool Alloc : " << server->mContentMessagePool.GetAllCount() << std::endl;
			std::cout << "Message Queue Size : " << server->mMessageQueue.GetSize() << std::endl;
			std::cout << "Player Pool Alloc : " << server->mPlayerPool.GetAllCount() << std::endl;
			std::cout << "Player Count : " << server->mPlayers.size() << std::endl;

			numTotalAccept = server->mNetServer->GetNumAccept();
			numTotalUpdate = server->mNumUpdate;
			numTotalRecv = server->mNetServer->GetNumRecv();
			numTotalSend = server->mNetServer->GetNumSend();

			std::cout << "Accept Total : " << numTotalAccept << std::endl;
			std::cout << "Accept TPS : " << numTotalAccept - numTotalAcceptBefore << std::endl;
			std::cout << "Update TPS : " << numTotalUpdate - numTotalUpdateBefore << std::endl;
			std::cout << "Recv TPS : " << numTotalRecv - numTotalRecvBefore << std::endl;
			std::cout << "Send TPS : " << numTotalSend - numTotalSendBefore << std::endl;

			numTotalAcceptBefore = numTotalAccept;
			numTotalUpdateBefore = numTotalUpdate;
			numTotalRecvBefore = numTotalRecv;
			numTotalSendBefore = numTotalSend;
		}
		Sleep(200);
	}
	return 0;
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
	message.Read((char*)player->ID, Player::MAX_ID_LEN * 2);
	message.Read((char*)player->Nickname, Player::MAX_NICKNAME_LEN * 2);

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

	if (!player->bLogin)
	{
		mNetServer->TryDisconnect(id);
		return;
	}
	mSectors[player->SectorY][player->SectorX].erase(id);

	message >> player->AccountNo;
	message >> player->SectorX;
	message >> player->SectorY;

	mSectors[player->SectorY][player->SectorX].insert({ id, player });

	Message* sendMessage = mNetServer->CreateMessage();
	{
		*sendMessage << EPacketType::PACKET_TYPE_SC_CHAT_RES_SECTOR_MOVE;
		*sendMessage << player->AccountNo;
		*sendMessage << player->SectorX;
		*sendMessage << player->SectorY;

		mNetServer->TrySendMessage(id, sendMessage);
	}
	mNetServer->ReleaseMessage(sendMessage);

	QueryPerformanceCounter(&player->LastReceivedTime);
	++mNumSectorMovePacket;
}

void ChattingServer::processChatPacket(sessionID_t id, Message& message)
{
	static int directionOffsets[NUM_DIRECTION][2] = { {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, -1} };
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
		sendMessage->Write((char*)player->ID, Player::MAX_ID_LEN * 2);
		sendMessage->Write((char*)player->Nickname, Player::MAX_NICKNAME_LEN * 2);
		*sendMessage << messageLength;
		sendMessage->Write(message.GetFront(), messageLength);

		for (auto iter : mSectors[sectorY][sectorX])
		{
			mNetServer->TrySendMessage(iter.first, sendMessage);
		}
		for (int dir = 0; dir < NUM_DIRECTION; ++dir)
		{
			int y = sectorY + directionOffsets[dir][0];
			int x = sectorX + directionOffsets[dir][1];

			if (0 <= y && y < SECTOR_ROW && 0 <= x && x < SECTOR_COLUMN)
			{
				for (auto iter : mSectors[y][x])
				{
					mNetServer->TrySendMessage(iter.first, sendMessage);
				}
			}
		}
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
