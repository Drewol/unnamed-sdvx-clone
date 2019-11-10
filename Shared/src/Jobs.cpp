#include "stdafx.h"
#include "Jobs.hpp"
#include "List.hpp"
#include "Vector.hpp"
#include "Log.hpp"
#include "Thread.hpp"
#include <thread>
#include "Timer.hpp"

JobFlags operator|(JobFlags a, JobFlags b)
{
	return (JobFlags)((uint8)a | (uint8)b);
}
JobFlags operator&(JobFlags a, JobFlags b)
{
	return (JobFlags)((uint8)a & (uint8)b);
}

struct JobThread
{
	// Thread index
	uint32 index = 0;
	bool terminate = false;
	Thread thread;

	// Job currently being processed
	Job activeJob;

	Timer idleDuration;

	// Terminate this thread
	void Terminate()
	{
		if(terminate)
			return; // Already terminated
		terminate = true;
		if(thread.joinable())
			thread.join();
	}
	bool IsActive() const { return activeJob.IsValid(); }
};

class JobSheduler_Impl
{
public:
	// Contains tasks to be done
	List<Job> m_jobQueue;
	// Contains tasks that are done
	List<Job> m_finishedJobs;
	
	Mutex m_lock;
	Vector<JobThread*> m_threadPool;

	friend class JobBase;

	JobSheduler_Impl()
	{
		AllocateThreads();
	}
	~JobSheduler_Impl()
	{
		ClearThreads();
	}
	void ClearThreads()
	{
		for(JobThread* t : m_threadPool)
		{
			t->Terminate();
			delete t;
		}
		m_lock.lock();
		// Unregister jobs
		for(auto job : m_jobQueue)
		{
			job->m_sheduler = nullptr;
		}
		for(auto job : m_finishedJobs)
		{
			job->m_sheduler = nullptr;
		}
		m_threadPool.clear();
		m_lock.unlock();
	}
	void AllocateThreads()
	{
		assert(m_threadPool.empty());

		unsigned concurentThreadsSupported = std::thread::hardware_concurrency();
		int32 targetThreadCount = concurentThreadsSupported - 2;
		if(targetThreadCount <= 0)
			targetThreadCount = 1;

		for(int32 i = 0; i < targetThreadCount; i++)
		{
			// Create affinity mask for job threads
			// always skip the first core since it runs the main thread
			uint32 affinityMask = 1 << (i+1);

			JobThread* thread = m_threadPool.Add(new JobThread());
			thread->index = i;
			thread->thread = Thread(&JobSheduler_Impl::m_JobThread, this, thread);
			thread->thread.SetAffinityMask(affinityMask);
		}
	}

	void Update()
	{
		m_lock.lock();
		List<Job> finished = m_finishedJobs;
		m_finishedJobs.clear();
		m_lock.unlock();

		for(Job& j : finished)
		{
			j->Finalize();
			j->OnFinished.Call(j);
			j->m_finished = true;
			j->m_sheduler = nullptr;
		}
	}

	bool QueueUnchecked(Job job)
	{
		job->m_sheduler = this;

		m_lock.lock();
		m_jobQueue.AddBack(job);
		m_lock.unlock();

		return true;
	}

private:
	// Single job thread
	void m_JobThread(JobThread* myThread)
	{
		while(!myThread->terminate)
		{
			if(!m_jobQueue.empty())
			{
				m_lock.lock();
				if(!m_jobQueue.empty())
				{
					myThread->idleDuration.Restart();

					// Process a job
					Job peekJob = m_jobQueue.front();
					if((peekJob->jobFlags & JobFlags::IO) != JobFlags::IO || (myThread->index != 0)) // Only perform IO on first thread
					{
						myThread->activeJob = m_jobQueue.PopFront();
						m_lock.unlock();

						// Run
						myThread->activeJob->m_ret = myThread->activeJob->Run();
						myThread->activeJob->m_finished = true;

						// Add to finished queue
						m_lock.lock();
						m_finishedJobs.AddBack(myThread->activeJob);
						m_lock.unlock();

						// Clear the active job
						myThread->activeJob.Release();
					}
					else
					{
						m_lock.unlock();
					}
				}
				else
				{
					// Just do nothing
					m_lock.unlock();
				}
			}

			// Various idle levels
			if(myThread->idleDuration.Minutes() > 1)
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			if(myThread->idleDuration.Seconds() > 1)
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			else
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
};
JobSheduler::JobSheduler()
{
	m_impl = new JobSheduler_Impl();
}
JobSheduler::~JobSheduler()
{
	delete m_impl;
}
void JobSheduler::Update()
{
	m_impl->Update();
}
bool JobSheduler::Queue(Job job)
{
	// Can't queue jobs twice
	if(job->IsQueued())
	{
		Log("Tried to register a job twice", Logger::Warning);
		return false;
	}
	// Can't queue finished jobs
	if(job->IsFinished())
	{
		Log("Tried to register a finished job", Logger::Warning);
		return false;
	}

	return m_impl->QueueUnchecked(job);
}

bool JobBase::IsFinished() const
{
	return m_finished;
}
bool JobBase::IsSuccessfull() const
{
	return m_ret;
}
bool JobBase::IsQueued() const
{
	return m_sheduler != nullptr;
}
void JobBase::Terminate()
{
	if(!m_sheduler)
		return; // Nothing to do
	JobSheduler_Impl* sheduler = m_sheduler;

	// Try to erase from queue first
	m_sheduler->m_lock.lock();
	for(auto it = sheduler->m_jobQueue.begin(); it != sheduler->m_jobQueue.end(); it++)
	{
		if(*it == this)
		{
			sheduler->m_jobQueue.erase(it);
			m_sheduler->m_lock.unlock();
			m_sheduler = nullptr;
			return; // Ok
		}
	}
	m_sheduler->m_lock.unlock();

	// Wait for running job
	for(JobThread* t : m_sheduler->m_threadPool)
	{
		if(t->activeJob == this)
		{
			// Wait for job to complete
			while(t->activeJob == this)
			{
				std::this_thread::yield();
			}
			break;
		}
	}

	// Remove from finished jobs list
	m_sheduler->m_lock.lock();
	for(auto it = m_sheduler->m_finishedJobs.rbegin(); it != m_sheduler->m_finishedJobs.rend(); it++)
	{
		if(*it == this)
		{
			m_sheduler->m_finishedJobs.erase(--(it.base()));
			m_sheduler->m_lock.unlock();
			return;
		}
	}
	m_sheduler->m_lock.unlock();
}
void JobBase::Finalize()
{
}
