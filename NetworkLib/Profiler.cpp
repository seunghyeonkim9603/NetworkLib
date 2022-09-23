#pragma warning(disable: 4996)

#include <assert.h>
#include <tchar.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <unordered_map>

#include "profiler.h"
#include "ProfilerManager.h"

#define MAX_PROFILER (128)
#define MIN(num0, num1) ((num0) < (num1) ? (num0) : (num1))
#define MAX(num0, num1) ((num0) < (num1) ? (num1) : (num0))

static ProfilerManager g_manager;
static LARGE_INTEGER g_freq;

thread_local t_profiler* g_profiler;


static t_profile* get_profile_or_null(const TCHAR* tag_name)
{
	unsigned int num_profile = g_profiler->num_profile;

	for (unsigned int i = 0; i < num_profile; ++i)
	{
		if (_tcscmp(g_profiler->profiles[i].sheet.tag_name, tag_name) == 0)
		{
			return &g_profiler->profiles[i];
		}
	}
	return NULL;
}

void profile_begin(const TCHAR* tag_name)
{
	if (g_profiler == nullptr)
	{
		g_profiler = new t_profiler();
		{
			g_profiler->num_profile = 0;
		}
		g_manager.RegisterProfiler(g_profiler);
		QueryPerformanceFrequency(&g_freq);
	}
	t_profile* p_profile = get_profile_or_null(tag_name);

	if (p_profile == NULL)
	{
		p_profile = &g_profiler->profiles[g_profiler->num_profile];

		t_profile_sheet* p_sheet = &p_profile->sheet;
		{
			p_sheet->tag_name = tag_name;
			p_sheet->num_call = 0;
			p_sheet->sum_msec = 0;
			p_sheet->max_msec = DBL_MIN;
			p_sheet->min_msec = DBL_MAX;
		}
		++(g_profiler->num_profile);
	}
	p_profile->is_profiling = TRUE;

	QueryPerformanceCounter(&p_profile->start);
}

void profile_end(const TCHAR* tag_name)
{
	LARGE_INTEGER end;

	QueryPerformanceCounter(&end);

	t_profile* p_profile = get_profile_or_null(tag_name);

	if (p_profile == NULL || !p_profile->is_profiling)
	{
		return;
	}
	double elapsed_time = (end.QuadPart - p_profile->start.QuadPart) / (double)g_freq.QuadPart * 1000000;

	t_profile_sheet* p_sheet = &p_profile->sheet;
	{
		++p_sheet->num_call;
		p_sheet->sum_msec += elapsed_time;
		p_sheet->min_msec = MIN(elapsed_time, p_sheet->min_msec);
		p_sheet->max_msec = MAX(elapsed_time, p_sheet->max_msec);
	}
	p_profile->is_profiling = false;
}

void print_profiles(const TCHAR* file_name)
{
	g_manager.PrintAll(file_name);
}
