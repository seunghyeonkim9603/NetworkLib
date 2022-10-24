#pragma warning(disable: 4996)

#include "stdafx.h"

ProfilerManager::ProfilerManager()
	: mProfilers(BUCKET_SIZE)
{
	InitializeSRWLock(&mLock);
}

ProfilerManager::~ProfilerManager()
{
}

void ProfilerManager::RegisterProfiler(const t_profiler* profiler)
{
	AcquireSRWLockExclusive(&mLock);
	{
		DWORD threadID = GetCurrentThreadId();

		mProfilers.insert({ threadID, profiler });
	}
	ReleaseSRWLockExclusive(&mLock);
}

void ProfilerManager::PrintAll(const TCHAR* fileName)
{
	AcquireSRWLockShared(&mLock);
	{
		FILE* fp = _tfopen(fileName, _T("w"));
		{
			for (auto pair : mProfilers)
			{
				DWORD threadID = pair.first;
				const t_profiler* profiler = pair.second;

				unsigned int numProfile = profiler->num_profile;

				_ftprintf(fp, L"Thread ID : %d =========================================================================\n", threadID);
				_ftprintf(fp, L"%17s |%17s |%17s |%17s |%15s\n", _T("Name"), _T("Average"), _T("Min"), _T("Max"), _T("Call"));
				for (unsigned int i = 0; i < numProfile; ++i)
				{
					const t_profile_sheet* sheet = &profiler->profiles[i].sheet;
					{
						const TCHAR* tag_name = sheet->tag_name;
						double min = sheet->min_msec;
						double max = sheet->max_msec;
						double sum = sheet->sum_msec;
						unsigned int num_call = sheet->num_call;
						double average = (sum - max - min) / (num_call - 2);

						_ftprintf(fp, L"%17s |%15.4fus |%15.4fus |%15.4fus |%15u\n", tag_name, average, min, max, num_call);
					}
				}
			}
		}
		fclose(fp);
	}
	ReleaseSRWLockShared(&mLock);
}
