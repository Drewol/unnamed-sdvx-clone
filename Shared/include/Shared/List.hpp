#pragma once
#include <list>

/*
	Doubly linked list
	wrapper around std::list
*/
template<typename I>
class List : public std::list<I>
{
public:
	using std::list<I>::list;
	using std::list<I>::list::insert;
	using std::list<I>::list::begin;
	using std::list<I>::list::end;
	using std::list<I>::list::sort;
	using std::list<I>::list::front;
	using std::list<I>::list::back;
	using std::list<I>::list::pop_front;
	using std::list<I>::list::pop_back;

	I& AddBack(const I& item = I())
	{
		auto it = insert(end(), item);
		return *it;
	}
	I& AddFront(const I& item = I())
	{
		auto it = insert(begin(), item);
		return *it;
	}

	// Pop and return
	I PopBack()
	{
		I r = back();
		pop_back();
		return std::move(r);
	}
	// Pop and return
	I PopFront()
	{
		I r = front();
		pop_front();
		return std::move(r);
	}

	// Sort function with uppercase for consistency
	template<typename Predicate>
	void Sort(Predicate& pred)
	{
		sort(pred);
	}
};
