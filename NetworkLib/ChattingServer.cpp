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
	// 플레이어 삭제
	mNetServer->Terminate();
	CloseHandle(mhNetworkEvent);
}

bool ChattingServer::TryRun(const unsigned long IP, const unsigned short port, const unsigned int numWorkerThread, const unsigned int numRunningThread, const unsigned int maxSessionCount, const bool bSupportsNagle)
{
	mhWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, &workerThread, this, 0, nullptr);

	if (!mNetServer->TryRun(IP, port, numWorkerThread, numRunningThread, maxSessionCount, bSupportsNagle, this))
	{
		gExit = true;
		SetEvent(mhNetworkEvent);

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

		bool bIsEmpty = !server->mMessageQueue.TryDequeue(&contentMessage);

		if (bIsEmpty)
		{
			WaitForSingleObject(server->mhNetworkEvent, INFINITE);
		}
		Message& payload = *contentMessage->Payload;

		switch (contentMessage->Event)
		{
		case EContentEvent::Join:
		{
			Player* playerBeforeLogin = server->mPlayerPool.GetObject();
			{
				payload >> playerBeforeLogin->IP;
				payload >> playerBeforeLogin->Port;
				playerBeforeLogin->SessionID = contentMessage->SessionID;
				QueryPerformanceCounter(&playerBeforeLogin->LastReceivedTime);
			}
			server->mPlayersBeforeLogin.insert({ playerBeforeLogin->SessionID, playerBeforeLogin });
		}
		break;
		case EContentEvent::Leave:
		{
			sessionID_t sessionID = contentMessage->SessionID;

			server->mPlayersBeforeLogin.erase(sessionID);

			auto iter = server->mPlayers.find(sessionID);
			if (iter != server->mPlayers.end())
			{
				Player* player = iter->second;

				server->mSectors[player->SectorY][player->SectorX].erase(sessionID);
			}
			server->mPlayersBeforeLogin.erase(sessionID);
		}
		break;
		case EContentEvent::PacketReceived:
		{

		}
		break;
		default:
			break;
		}
	}

	return 0;
}
