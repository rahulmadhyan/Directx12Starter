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
	TSQueue();
	~TSQueue();

	void Push(T value);
	void waitAndPop(T &value);
	std::shared_ptr<T> waitAndPop();
	bool try_pop(T &value);
	shared_ptr<T> try_pop();
	bool empty() const;

private:
	mutable mutex mutex;
	queue<T> queue;
	condition_variable cv;
};

