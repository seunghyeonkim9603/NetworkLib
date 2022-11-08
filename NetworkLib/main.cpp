
#include "stdafx.h"

#include "EPacketType.h"
#include "ProcessTaskManager.h"
#include "SingleThreadChattingServer.h"

#define SERVER_PORT (12201)



int main(void)
{	
	CCrashDump::Init();
	
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

	WanServer wanServer;
	SingleThreadChattingServer chatServer(&wanServer);
	ProcessTaskManager taskManager;
	WanServer::Session session;

	if (!chatServer.TryRun(INADDR_ANY, SERVER_PORT, 8, 4, 20000, true))
	{
		std::cout << "Failed" << std::endl;
	}

	while (true)
	{
		QueryPerformanceCounter(&current);
		if (freq.QuadPart <= current.QuadPart - before.QuadPart)
		{
			before.QuadPart = current.QuadPart;

			taskManager.UpdateProcessInfo();

			std::cout << "======================================================" << std::endl;
			std::cout << "Session Count : " << wanServer.GetCurrentSessionCount() << std::endl;
			std::cout << "Message Pool Alloc : " << chatServer.GetMessagePoolAllocCount() << std::endl;
			std::cout << "Message Queue Size : " << chatServer.GetMessageQueueSize() << std::endl;
			std::cout << "Player Pool Alloc : " << chatServer.GetPlayerPoolAllocCount() << std::endl;
			std::cout << "Player Count : " << chatServer.GetPlayerCount() << std::endl;
			std::cout << "Allocated Message Cunk Count : " << TLSObjectPoolMiddleware<Message>::GetAllocatedChunkCount() << std::endl;
			numTotalAccept = wanServer.GetNumAccept();
			numTotalUpdate = chatServer.GetTotalUpdateCount();
			numTotalRecv = wanServer.GetNumRecv();
			numTotalSend = wanServer.GetNumSend();

			std::cout << "Accept Total : " << numTotalAccept << std::endl;
			std::cout << "Accept TPS : " << numTotalAccept - numTotalAcceptBefore << std::endl;
			std::cout << "Update TPS : " << numTotalUpdate - numTotalUpdateBefore << std::endl;
			std::cout << "Recv TPS : " << numTotalRecv - numTotalRecvBefore << std::endl;
			std::cout << "Send TPS : " << numTotalSend - numTotalSendBefore << std::endl << std::endl;

			numTotalAcceptBefore = numTotalAccept;
			numTotalUpdateBefore = numTotalUpdate;
			numTotalRecvBefore = numTotalRecv;
			numTotalSendBefore = numTotalSend;

			std::cout << "++++++++++++++CPU Usage+++++++++++++++" << std::endl;
			std::cout << "Login Packet Recv : " << chatServer.GetTotalLoginPacketCount() << std::endl;
			std::cout << "Chat Packet Recv : " << chatServer.GetTotalChattingPacketCount() << std::endl;
			std::cout << "SectorMove Packet Recv : " << chatServer.GetTotalSectorMovePacketCount() << std::endl;
			
			std::cout << "Processor Total Usage : " << taskManager.GetProcessorTotalUsage() << std::endl;
			std::cout << "Processor User Usage : " << taskManager.GetProcessorUserUsage() << std::endl;
			std::cout << "Processor Kernel Usage : " << taskManager.GetProcessorKernelUsage() << std::endl;
			std::cout << "Process Total Usage : " << taskManager.GetProcessTotalUsage() << std::endl;
			std::cout << "Process User Usage : " << taskManager.GetProcessUserUsage() << std::endl;
			std::cout << "Process Kernel Usage : " << taskManager.GetProcessKernelUsage() << std::endl << std::endl;

			std::cout << "++++++++++++++Memory Usage+++++++++++++++" << std::endl;
			std::cout << "Total Page Fault : " << taskManager.GetTotalPageFaultCount() << std::endl;
			std::cout << "Page Fault (p/s) : " << taskManager.GetLastPageFaultCount() << std::endl;
			std::cout << "Peak Working Set (byte) : " << taskManager.GetPeakWorkingSetSize() << std::endl;
			std::cout << "Working Set (byte) : " << taskManager.GetWorkingSetSize() << std::endl;
			std::cout << "Peak Paged Pool (byte) : " << taskManager.GetQuotaPeakPagedPoolUsage() << std::endl;
			std::cout << "Paged Pool (byte) : " << taskManager.GetQuotaPagedPoolUsage() << std::endl;
			std::cout << "Peak NonPaged Pool (byte) : " << taskManager.GetQuotaPeakNonPagedPoolUsage() << std::endl;
			std::cout << "NonPaged Pool (byte) : " << taskManager.GetQuotaNonPagedPoolUsage() << std::endl;
		}
		Sleep(200);
	}

	return 0;
}


