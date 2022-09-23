#pragma once

class ProfilerManager final
{
public:
	ProfilerManager();
	~ProfilerManager();
	ProfilerManager(const ProfilerManager& other) = delete;
	ProfilerManager& operator=(const ProfilerManager& other) = delete;

	void RegisterProfiler(const t_profiler* profiler);
	void PrintAll(const TCHAR* fileName);
private:
	enum { BUCKET_SIZE = 997 };

	std::unordered_map<DWORD, const t_profiler*> mProfilers;
	SRWLOCK mLock;
};