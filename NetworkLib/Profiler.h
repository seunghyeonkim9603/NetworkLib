#ifndef PROFILER_H
#define PROFILER_H

#define MAX_PROFILER (128)

typedef struct profile_sheet
{
	const TCHAR* tag_name;
	double sum_msec;
	double min_msec;
	double max_msec;
	unsigned int num_call;
} t_profile_sheet;

typedef struct profile
{
	profile_sheet sheet;
	LARGE_INTEGER start;
	BOOL is_profiling;
} t_profile;

typedef struct profiler
{
	t_profile profiles[MAX_PROFILER];
	unsigned int num_profile;
} t_profiler;

#ifdef PROFILE
#define PROFILE_BEGIN(tag_name) profile_begin(tag_name);
#define PROFILE_END(tag_name)  profile_end(tag_name)
#define PROFILES_PRINT(file_name) print_profiles(file_name)
#else
#define PROFILE_BEGIN(tag_name)
#define PROFILE_END(tag_name)
#define PROFILES_PRINT(file_name)
#endif /* PROFILE */

void profile_begin(const TCHAR* tag_name);
void profile_end(const TCHAR* tag_name);
void print_profiles(const TCHAR* file_name);

#endif /* PROFILER_H */