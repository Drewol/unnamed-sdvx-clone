#include "stdafx.h"
#include "Thread.hpp"

size_t Thread::SetAffinityMask(size_t affinityMask)
{
	pthread_t h = (pthread_t)native_handle();
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for (uint32 j = 0; j < 8; j++)
	{
		size_t m = 1 << j;
		if((affinityMask & m) != 0)
			CPU_SET(j, &cpuset);
	}
	pthread_setaffinity_np(h, sizeof(cpu_set_t), &cpuset);
	return 0;
}

size_t Thread::SetCurrentThreadAffinityMask(size_t affinityMask)
{
	pthread_t h = (pthread_t)pthread_self();
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for (uint32 j = 0; j < 8; j++)
	{
		size_t m = 1 << j;
		if((affinityMask & m) != 0)
			CPU_SET(j, &cpuset);
	}
	pthread_setaffinity_np(h, sizeof(cpu_set_t), &cpuset);
	return 0;
}
