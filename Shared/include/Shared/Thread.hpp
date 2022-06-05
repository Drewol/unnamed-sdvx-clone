#pragma once
#include <thread>
#include <mutex>

/*
	std::thread extension that allows affinity setting
*/
class Thread : public std::thread
{
public:
	using std::thread::thread;
	size_t SetAffinityMask(size_t affinityMask);
	static size_t SetCurrentThreadAffinityMask(size_t affinityMask);
};

/*
	Mutex class to fit program naming convention
*/
class Mutex : public std::mutex
{
};
