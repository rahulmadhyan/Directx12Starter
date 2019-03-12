#pragma once
#include <functional>
#include <vector>
#include <thread>
#include <condition_variable>
#include <future>
#include <functional>
#include "TSQueue.h"

class ThreadPool
{
public:
	using Task = std::function<void()>;

	ThreadPool();
	~ThreadPool();

	template<class T>
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
	}

private:
	void Start(size_t numberOfThreads);
	void Stop() noexcept;
	
	std::vector<std::thread> threads;
	std::condition_variable poolCV;
	std::mutex poolMutex;
	std::queue<Task> tasks;

	bool stop;

};

