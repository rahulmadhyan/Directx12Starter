#include "ThreadPool.h"

ThreadPool::ThreadPool()
{

}

ThreadPool::~ThreadPool()
{
	Stop();
}

void ThreadPool::Start(size_t numberOfThreads)
{	
	for (size_t i = 0; i < numberOfThreads; i++)
	{
		threads.push_back(std::thread(&ThreadPool::Worker_thread, this));
	}
}

void ThreadPool::Stop() noexcept
{
	{
		std::unique_lock<std::mutex> lock(poolMutex);
		stop = true;
	}

	poolCV.notify_all();

	for (auto &thread : threads)
		thread.join();

}

