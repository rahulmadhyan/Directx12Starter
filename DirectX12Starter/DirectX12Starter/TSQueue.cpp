#include "TSQueue.h"

template<typename T>
TSQueue<T>::TSQueue()
{
}

template<typename T>
TSQueue<T>::~TSQueue()
{
}

template<typename T>
void TSQueue<T>::Push(T value)
{
	lock_guard<mutex> lock(mutex);
	queue.push(value);
	cv.notify_one();
}

template<typename T>
void TSQueue<T>::waitAndPop(T &value)
{
	unique_lock<mutex> lock(mutex);
	cv.wait(lock, [this] { return !queue.empty });
	value = move(queue.front());
	queue.pop();
}

template<typename T>
std::shared_ptr<T> TSQueue<T>::waitAndPop()
{
	unique_lock<mutex> lock(mutex);
	cv.wait(lock, [this] { !queue.empty });
	shared_ptr<T> result(make_shared<T>(std::move(queue.front())));
	queue.pop();
	return result;
}

template<typename T>
bool TSQueue<T>::try_pop(T &value)
{
	lock_guard<mutex> lock(mutex);
	if (queue.empty())
		return false;
	
	value = move(queue.front());
	queue.pop();
	return true;
}

template<typename T>
shared_ptr<T> TSQueue<T>::try_pop()
{
	lock_guard<mutex> lock(mutex);
	if (queue.empty())
		return shared_ptr<T>();

	shared_ptr<T> result(make_shared<T>(move(queue.front())));
	queue.pop();
	return result;
}

template<typename T>
bool TSQueue<T>::empty() const
{
	lock_guard<mutex> lock(mutex);
	return queue.empty();
}
