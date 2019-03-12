#include "ThreadPool.h"

ThreadPool::ThreadPool()
{
	const unsigned int threadCount = std::thread::hardware_concurrency();
	Start(threadCount);
}

ThreadPool::~ThreadPool()
{
	Stop();
}

void ThreadPool::Start(size_t numberOfThreads)
{
	for (size_t i = 0; i < numberOfThreads; i++)
	{
		threads.emplace_back([=] {
			
			Task task;
			
			while (true)
			{
				{
					std::unique_lock<std::mutex> lock(poolMutex);
					poolCV.wait(lock, [=] { return stop || !tasks.empty(); });

					if (stop && tasks.empty())
						break;

					task = std::move(tasks.front());
					tasks.pop();
				}

				task();
			}
		});
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

//template<class T>
//auto ThreadPool::Enqueue(T task)->std::future<decltype(task())>
//{
//	auto wrapper = std::make_shared<std::packaged_task<decltype(task()) ()>>(std::move(task));
//
//	{
//		std::unique_lock<std::mutex> lock(poolMutex);
//		tasks.emplace([=] {
//			(*wrapper());
//		});
//	}
//
//	poolCV.notify_one();
//	return wrapper->get_future();
//}