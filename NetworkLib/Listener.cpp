#pragma comment(lib, "ws2_32")

#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <process.h>
#include <unordered_set>

#include "EIOType.h"

#include "Message.h"
#include "RingBuffer.h"
#include "ObjectPool.h"
#include "LockFreeStack.h"
#include "LockFreeQueue.h"
#include "IntrusivePointer.h"
#include "LanServer.h"
#include "INetworkEventListener.h"
#include "Listener.h"

Listener::Listener(LanServer* server)
	:	mServer(server)
{
}

bool Listener::OnConnectionRequest(const unsigned long IP, const unsigned short port)
{
	return true;
}

void Listener::OnClientJoin(const sessionID_t ID, const unsigned long IP, const unsigned short port)
{
}

void Listener::OnClientLeave(const sessionID_t ID)
{
	// std::cout << "leave : " << ID << std::endl;
}

void Listener::OnRecv(const sessionID_t ID, const Message* message)
{
	IntrusivePointer<Message> sendMessage(new Message());

	sendMessage->Write(message->GetFront(), message->GetSize());

	mServer->TrySendMessage(ID, &sendMessage);
	sendMessage.Release();
}

void Listener::OnError(const int errorCode, const wchar_t* message)
{
}
