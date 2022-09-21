#pragma comment(lib, "ws2_32")

#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <process.h>
#include <unordered_set>
#include <memory>

#include "EIOType.h"
#include "CCrashDump.h"
#include "Message.h"
#include "IntrusivePointer.h"
#include "RingBuffer.h"
#include "ObjectPool.h"
#include "LockFreeStack.h"
#include "LockFreeQueue.h"
#include "IntrusivePointer.h"
#include "LanServer.h"
#include "INetworkEventListener.h"
#include "Listener.h"

#define SERVER_PORT (6000)

int main(void)
{
	CCrashDump::Init();

	LanServer server;
	Listener listener(&server);
	
	LARGE_INTEGER frep;
	LARGE_INTEGER before;
	LARGE_INTEGER cur;

	QueryPerformanceFrequency(&frep);

	server.TryRun(INADDR_ANY, SERVER_PORT, 16, 4, 3000, true, &listener);

	QueryPerformanceCounter(&before);

	while (true)
	{
		QueryPerformanceCounter(&cur);
		if (frep.QuadPart <= cur.QuadPart - before.QuadPart)
		{
			std::cout << "current session count : " << server.GetCurrentSessionCount() << std::endl;
			before = cur;
		}
		Sleep(100);
	}
	server.Terminate();

	return 0;
}


