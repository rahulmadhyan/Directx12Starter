#pragma once
#include <functional>
#include <vector>
#include <thread>
#include <condition_variable>
#include <future>
#include <functional>
#include "TSQueue.h"
#include "Entity.h"

class ThreadPool
{
public:
	using Task = std::function<void()>;

	ThreadPool();
	~ThreadPool();

	/*template<class T>
	auto Enqueue(T task)->std::future<decltype(task())>
	{
		{
			auto wrapper = std::make_shared<std::packaged_task<decltype(task()) ()>>(std::move(task));

			{
				std::unique_lock<std::mutex> lock(poolMutex);
				tasks.emplace([=] {
					(*wrapper)();
				});
			}

			poolCV.notify_one();
			return wrapper->get_future();
		}
	}*/

	void Worker_thread()
	{
		while (true)
		{
			std::function<void()> task;
			if (tasks.try_pop(task))
			{
				task();
			}	
			else
			{
				std::this_thread::yield();
			}
		}
	}

	template<typename FunctionType>
	void Submit(FunctionType f)
	{
		tasks.Push(std::move(std::function<void()>(f)));
	}

	void Start(size_t numberOfThreads);

private:
	
	void Stop() noexcept;
	
	std::vector<std::thread> threads;
	std::condition_variable poolCV;
	std::mutex poolMutex;
	TSQueue<std::function<void()>> tasks;
	
	bool stop;
};

