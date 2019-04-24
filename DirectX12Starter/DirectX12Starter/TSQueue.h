#pragma once
#include <queue>
#include <memory>
#include <thread>
#include <condition_variable>

using namespace std;

template<typename T>
class TSQueue
{
public:
	TSQueue() {}

	void Push(T value)
	{
		lock_guard<mutex> lock(mx);
		queue.push(value);
		cv.notify_one();
	}

	void waitAndPop(T &value)
	{
		unique_lock<mutex> lock(mx);
		cv.wait(lock, [this] { return !queue.empty(); });
		value = move(queue.front());
		queue.pop();
	}

	std::shared_ptr<T> waitAndPop()
	{
		unique_lock<mutex> lock(mx);
		cv.wait(lock, [this] { !queue.empty(); });
		shared_ptr<T> result(make_shared<T>(std::move(queue.front())));
		queue.pop();
		return result;
	}

	bool try_pop(T &value)
	{
		lock_guard<mutex> lock(mx);
		if (queue.empty())
			return false;

		value = move(queue.front());
		queue.pop();
		return true;
	}

	shared_ptr<T> try_pop()
	{
		lock_guard<mutex> lock(mx);
		if (queue.empty())
			return shared_ptr<T>();

		shared_ptr<T> result(make_shared<T>(move(queue.front())));
		queue.pop();
		return result;
	}

	bool empty() const
	{
		lock_guard<mutex> lock(mx);
		return queue.empty();
	}

private:
	mutable mutex mx;
	queue<T> queue;
	condition_variable cv;
};

