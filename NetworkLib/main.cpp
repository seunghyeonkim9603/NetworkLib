#pragma comment(lib, "ws2_32")
#define PROFILE
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
#include "Chunk.h"
#include "TLSObjectPoolMiddleware.h"
#include "TLSObjectPool.h"
#include "LockFreeStack.h"
#include "LockFreeQueue.h"
#include "IntrusivePointer.h"
#include "LanServer.h"
#include "INetworkEventListener.h"
#include "Listener.h"
#include "Profiler.h"
#define SERVER_PORT (6000)

#define NUM_THREADS (4)
#define NUM_DATA (10000)
#define TEST_NUM (2500)
#define DATA_SIZE (1024)
#define VALID (0xFFDDCCAA)
struct Data
{
	unsigned int arr[256];
	Data()
	{
		arr[0] = VALID;
	}
};


TLSObjectPool<Data> gPool;

Data* gPoolArr[NUM_DATA];
Data* gHeapArr[NUM_DATA];

unsigned int __stdcall Test(void* param)
{
	Data* arr[TEST_NUM];
	while (true)
	{
		for (int i = 0; i < TEST_NUM; ++i)
		{
			arr[i] = gPool.GetObject();
			if (arr[i]->arr[0] != VALID)
			{
				CCrashDump::Crash();
			}
		}
		Sleep(0);
		for (int i = 0; i < TEST_NUM; ++i)
		{
			InterlockedIncrement(&arr[i]->arr[0]);
			if (arr[i]->arr[0] != VALID + 1)
			{
				CCrashDump::Crash();
			}
		}
		Sleep(0);
		for (int i = 0; i < TEST_NUM; ++i)
		{
			InterlockedDecrement(&arr[i]->arr[0]);
			if (arr[i]->arr[0] != VALID)
			{
				CCrashDump::Crash();
			}
		}

		for (int i = 0; i < TEST_NUM; ++i)
		{
			gPool.ReleaseObject(arr[i]);
		}
	}
}

unsigned int __stdcall PoolTest(void* param)
{
	Data* poolArr[TEST_NUM];

	for (int i = 0; i < 10; ++i)
	{
		PROFILE_BEGIN(L"TLSAllocMultipleTimes");
		for (int i = 0; i < TEST_NUM; ++i)
		{
			//PROFILE_BEGIN(L"TLSAllocSingleTime");
			poolArr[i] = gPool.GetObject();
			//PROFILE_END(L"TLSAllocSingleTime");
		}
		PROFILE_END(L"TLSAllocMultipleTimes");

		PROFILE_BEGIN(L"TLSReleaseMultipleTimes");
		for (int i = 0; i < TEST_NUM; ++i)
		{
			//PROFILE_BEGIN(L"TLSReleaseSingleTime");
			gPool.ReleaseObject(poolArr[i]);
			//PROFILE_END(L"TLSReleaseSingleTime");
		}
		PROFILE_END(L"TLSReleaseMultipleTimes");
	}
	return 0;
}

unsigned int __stdcall HeapTest(void* param)
{
	Data* heapArr[TEST_NUM];

	PROFILE_BEGIN(L"NewMultipleTimes1");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//	PROFILE_BEGIN(L"NewSingleTime");
		heapArr[i] = new Data();
		//	PROFILE_END(L"NewSingleTime");
	}
	PROFILE_END(L"NewMultipleTimes1");

	PROFILE_BEGIN(L"DeleteMultipleTimes1");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//PROFILE_BEGIN(L"DeleteSingleTime");
		delete heapArr[i];
		//PROFILE_END(L"DeleteSingleTime");
	}
	PROFILE_END(L"DeleteMultipleTimes1");
	PROFILE_BEGIN(L"NewMultipleTimes2");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//	PROFILE_BEGIN(L"NewSingleTime");
		heapArr[i] = new Data();
		//	PROFILE_END(L"NewSingleTime");
	}
	PROFILE_END(L"NewMultipleTimes2");

	PROFILE_BEGIN(L"DeleteMultipleTimes3");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//PROFILE_BEGIN(L"DeleteSingleTime");
		delete heapArr[i];
		//PROFILE_END(L"DeleteSingleTime");
	}
	PROFILE_END(L"DeleteMultipleTimes3");
	PROFILE_BEGIN(L"NewMultipleTimes3");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//	PROFILE_BEGIN(L"NewSingleTime");
		heapArr[i] = new Data();
		//	PROFILE_END(L"NewSingleTime");
	}
	PROFILE_END(L"NewMultipleTimes3");

	PROFILE_BEGIN(L"DeleteMultipleTimes4");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//PROFILE_BEGIN(L"DeleteSingleTime");
		delete heapArr[i];
		//PROFILE_END(L"DeleteSingleTime");
	}
	PROFILE_END(L"DeleteMultipleTimes4");

	PROFILE_BEGIN(L"NewMultipleTimes5");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//	PROFILE_BEGIN(L"NewSingleTime");
		heapArr[i] = new Data();
		//	PROFILE_END(L"NewSingleTime");
	}
	PROFILE_END(L"NewMultipleTimes5");

	PROFILE_BEGIN(L"DeleteMultipleTimes5");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//PROFILE_BEGIN(L"DeleteSingleTime");
		delete heapArr[i];
		//PROFILE_END(L"DeleteSingleTime");
	}
	PROFILE_END(L"DeleteMultipleTimes5");
	PROFILE_BEGIN(L"NewMultipleTimes6");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//	PROFILE_BEGIN(L"NewSingleTime");
		heapArr[i] = new Data();
		//	PROFILE_END(L"NewSingleTime");
	}
	PROFILE_END(L"NewMultipleTimes6");

	PROFILE_BEGIN(L"DeleteMultipleTimes6");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//PROFILE_BEGIN(L"DeleteSingleTime");
		delete heapArr[i];
		//PROFILE_END(L"DeleteSingleTime");
	}
	PROFILE_END(L"DeleteMultipleTimes6");
	PROFILE_BEGIN(L"NewMultipleTimes7");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//	PROFILE_BEGIN(L"NewSingleTime");
		heapArr[i] = new Data();
		//	PROFILE_END(L"NewSingleTime");
	}
	PROFILE_END(L"NewMultipleTimes7");

	PROFILE_BEGIN(L"DeleteMultipleTimes7");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//PROFILE_BEGIN(L"DeleteSingleTime");
		delete heapArr[i];
		//PROFILE_END(L"DeleteSingleTime");
	}
	PROFILE_END(L"DeleteMultipleTimes7");
	PROFILE_BEGIN(L"NewMultipleTimes8");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//	PROFILE_BEGIN(L"NewSingleTime");
		heapArr[i] = new Data();
		//	PROFILE_END(L"NewSingleTime");
	}
	PROFILE_END(L"NewMultipleTimes8");

	PROFILE_BEGIN(L"DeleteMultipleTimes8");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//PROFILE_BEGIN(L"DeleteSingleTime");
		delete heapArr[i];
		//PROFILE_END(L"DeleteSingleTime");
	}
	PROFILE_END(L"DeleteMultipleTimes8");
	PROFILE_BEGIN(L"NewMultipleTimes9");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//	PROFILE_BEGIN(L"NewSingleTime");
		heapArr[i] = new Data();
		//	PROFILE_END(L"NewSingleTime");
	}
	PROFILE_END(L"NewMultipleTimes9");

	PROFILE_BEGIN(L"DeleteMultipleTimes9");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//PROFILE_BEGIN(L"DeleteSingleTime");
		delete heapArr[i];
		//PROFILE_END(L"DeleteSingleTime");
	}
	PROFILE_END(L"DeleteMultipleTimes9");
	PROFILE_BEGIN(L"NewMultipleTimes10");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//	PROFILE_BEGIN(L"NewSingleTime");
		heapArr[i] = new Data();
		//	PROFILE_END(L"NewSingleTime");
	}
	PROFILE_END(L"NewMultipleTimes10");

	PROFILE_BEGIN(L"DeleteMultipleTimes10");
	for (int i = 0; i < TEST_NUM; ++i)
	{
		//PROFILE_BEGIN(L"DeleteSingleTime");
		delete heapArr[i];
		//PROFILE_END(L"DeleteSingleTime");
	}
	PROFILE_END(L"DeleteMultipleTimes10");

	return 0;
}

int main(void)
{
	//DWORD num = GetTickCount();

	CCrashDump::Init();

	for (int i = 0; i < 10; ++i)
	{
		for (int i = 0; i < NUM_DATA; ++i)
		{
			//gPoolArr[i] = gPool.GetObject();
			gHeapArr[i] = new Data();
		}
		for (int i = 0; i < NUM_DATA; ++i)
		{
			//gPool.ReleaseObject(gPoolArr[i]);
			delete gHeapArr[i];
		}
	}

	DWORD num = GetTickCount();
	HANDLE hThreads[NUM_THREADS];
	{
		// TLS Pool Test
	/*	for (int i = 0; i < NUM_THREADS; ++i)
		{
			hThreads[i] = (HANDLE)_beginthreadex(nullptr, 0, &PoolTest, nullptr, 0, nullptr);
		}

		WaitForMultipleObjects(NUM_THREADS, hThreads, true, INFINITE);

		for (int i = 0; i < NUM_THREADS; ++i)
		{
			CloseHandle(hThreads[i]);
		}*/
	}
	{
		// Heap Test
		for (int i = 0; i < NUM_THREADS; ++i)
		{
			hThreads[i] = (HANDLE)_beginthreadex(nullptr, 0, &HeapTest, nullptr, 0, nullptr);
		}

		WaitForMultipleObjects(NUM_THREADS, hThreads, true, INFINITE);

		for (int i = 0; i < NUM_THREADS; ++i)
		{
			CloseHandle(hThreads[i]);
		}
	}
	PROFILES_PRINT(L"heap test.txt");
	printf("%d\n", GetTickCount() - num);
	return 0;
	/*CCrashDump::Init();

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

	return 0;*/
}


