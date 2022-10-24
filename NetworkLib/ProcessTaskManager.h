#pragma once

class ProcessTaskManager final
{
public:
	ProcessTaskManager(HANDLE hProcess = INVALID_HANDLE_VALUE);

	void UpdateProcessInfo(void);

	float GetProcessorTotalUsage(void) const { return mProcessorTotal; }
	float GetProcessorUserUsage(void) const { return mProcessorUser; }
	float GetProcessorKernelUsage(void) const { return mProcessorKernel; }

	float GetProcessTotalUsage(void) const { return mProcessTotal; }
	float GetProcessUserUsage(void) const { return mProcessUser; }
	float GetProcessKernelUsage(void) const { return mProcessKernel; }

	SIZE_T GetWorkingSetSize(void) const { return mWorkingSetSize; }
	SIZE_T GetPeakWorkingSetSize(void) const { return mPeakWorkingSetSize; }
	SIZE_T GetQuotaPagedPoolUsage(void) const { return mQuotaPagedPoolUsage; }
	SIZE_T GetQuotaNonPagedPoolUsage(void) const { return mQuotaNonPagedPoolUsage; }
	SIZE_T GetQuotaPeakPagedPoolUsage(void) const { return mQuotaPeakPagedPoolUsage; }
	SIZE_T GetQuotaPeakNonPagedPoolUsage(void) const { return mQuotaPeakNonPagedPoolUsage; }
	DWORD GetLastPageFaultCount(void) const { return mLastPageFaultCount; }
	DWORD GetTotalPageFaultCount(void) const { return mTotalPageFaultCount; }

private:
	HANDLE  mhProcess;
	int     mNumberOfProcessors;

	float   mProcessorTotal;
	float   mProcessorUser;
	float   mProcessorKernel;

	float   mProcessTotal;
	float   mProcessUser;
	float   mProcessKernel;

	ULARGE_INTEGER  mProcessorLastKernel;
	ULARGE_INTEGER  mProcessorLastUser;
	ULARGE_INTEGER  mProcessorLastIdle;

	ULARGE_INTEGER  mProcessLastKernel;
	ULARGE_INTEGER  mProcessLastUser;
	ULARGE_INTEGER  mProcessLastTime;

	SIZE_T	mWorkingSetSize;
	SIZE_T	mPeakWorkingSetSize;
	SIZE_T	mQuotaPagedPoolUsage;
	SIZE_T	mQuotaNonPagedPoolUsage;
	SIZE_T	mQuotaPeakPagedPoolUsage;
	SIZE_T	mQuotaPeakNonPagedPoolUsage;
	DWORD	mLastPageFaultCount;
	DWORD	mTotalPageFaultCount;
};