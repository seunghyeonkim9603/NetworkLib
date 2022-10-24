#include "stdafx.h"

#include "ProcessTaskManager.h"

ProcessTaskManager::ProcessTaskManager(HANDLE hProcess)
	: mProcessorTotal(0),
	mProcessorUser(0),
	mProcessorKernel(0),
	mProcessTotal(0),
	mProcessUser(0),
	mProcessKernel(0),
	mWorkingSetSize(0),
	mPeakWorkingSetSize(0),
	mQuotaPagedPoolUsage(0),
	mQuotaNonPagedPoolUsage(0),
	mQuotaPeakPagedPoolUsage(0),
	mQuotaPeakNonPagedPoolUsage(0),
	mLastPageFaultCount(0)
{
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		mhProcess = GetCurrentProcess();
	}
	SYSTEM_INFO SystemInfo;

	GetSystemInfo(&SystemInfo);
	mNumberOfProcessors = SystemInfo.dwNumberOfProcessors;

	mProcessorLastKernel.QuadPart = 0;
	mProcessorLastUser.QuadPart = 0;
	mProcessorLastIdle.QuadPart = 0;

	mProcessLastUser.QuadPart = 0;
	mProcessLastKernel.QuadPart = 0;
	mProcessLastTime.QuadPart = 0;

	UpdateProcessInfo();
}

void ProcessTaskManager::UpdateProcessInfo()
{
	ULARGE_INTEGER Idle;
	ULARGE_INTEGER Kernel;
	ULARGE_INTEGER User;

	if (GetSystemTimes((PFILETIME)&Idle, (PFILETIME)&Kernel, (PFILETIME)&User) == false)
	{
		return;
	}
	ULONGLONG KernelDiff = Kernel.QuadPart - mProcessorLastKernel.QuadPart;
	ULONGLONG UserDiff = User.QuadPart - mProcessorLastUser.QuadPart;
	ULONGLONG IdleDiff = Idle.QuadPart - mProcessorLastIdle.QuadPart;

	ULONGLONG Total = KernelDiff + UserDiff;
	ULONGLONG TimeDiff;

	if (Total == 0)
	{
		mProcessorUser = 0.0f;
		mProcessorKernel = 0.0f;
		mProcessorTotal = 0.0f;
	}
	else
	{
		mProcessorTotal = (float)((double)(Total - IdleDiff) / Total * 100.0f);
		mProcessorUser = (float)((double)UserDiff / Total * 100.0f);
		mProcessorKernel = (float)((double)(KernelDiff - IdleDiff) / Total * 100.0f);
	}
	mProcessorLastKernel = Kernel;
	mProcessorLastUser = User;
	mProcessorLastIdle = Idle;

	ULARGE_INTEGER None;
	ULARGE_INTEGER NowTime;

	GetSystemTimeAsFileTime((LPFILETIME)&NowTime);

	GetProcessTimes(mhProcess, (LPFILETIME)&None, (LPFILETIME)&None, (LPFILETIME)&Kernel, (LPFILETIME)&User);

	TimeDiff = NowTime.QuadPart - mProcessLastTime.QuadPart;
	UserDiff = User.QuadPart - mProcessLastUser.QuadPart;
	KernelDiff = Kernel.QuadPart - mProcessLastKernel.QuadPart;

	Total = KernelDiff + UserDiff;

	if (Total == 0)
	{
		mProcessTotal = 0.0f;
		mProcessKernel = 0.0f;
		mProcessUser = 0.0f;
	}
	else
	{
		mProcessTotal = (float)(Total / (double)mNumberOfProcessors / (double)TimeDiff * 100.0f);
		mProcessKernel = (float)(KernelDiff / (double)mNumberOfProcessors / (double)TimeDiff * 100.0f);
		mProcessUser = (float)(UserDiff / (double)mNumberOfProcessors / (double)TimeDiff * 100.0f);
	}
	mProcessLastTime = NowTime;
	mProcessLastKernel = Kernel;
	mProcessLastUser = User;

	PROCESS_MEMORY_COUNTERS pmc;

	GetProcessMemoryInfo(mhProcess, &pmc, sizeof(pmc));
	{
		mWorkingSetSize = pmc.WorkingSetSize;
		mPeakWorkingSetSize = pmc.PeakWorkingSetSize;
		mQuotaPagedPoolUsage = pmc.QuotaPagedPoolUsage;
		mQuotaNonPagedPoolUsage = pmc.QuotaNonPagedPoolUsage;
		mQuotaPeakPagedPoolUsage = pmc.QuotaPeakPagedPoolUsage;
		mQuotaPeakNonPagedPoolUsage = pmc.QuotaPeakNonPagedPoolUsage;
		mLastPageFaultCount = pmc.PageFaultCount - mTotalPageFaultCount;
		mTotalPageFaultCount = pmc.PageFaultCount;
	}
}