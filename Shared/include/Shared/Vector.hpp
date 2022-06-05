#pragma once
#include <vector>
#include <algorithm>

/* Vector class, extends std::vector */
template<typename I>
class Vector : public std::vector<I>
{
public:
	using std::vector<I>::vector;

	// These are for allowing function to be called on the base class when compiling on GCC
	using std::vector<I>::back;
	using std::vector<I>::front;
	using std::vector<I>::begin;
	using std::vector<I>::end;
	using std::vector<I>::erase;
	using std::vector<I>::push_back;

	// Adds a new element and returns it
	I& Add(const I& obj = I()) { push_back(std::move(obj)); return back(); };
	I& AddZeroed() { push_back(I()); memset(&back(), 0, sizeof(I)); return back(); };
	void AddUnique(const I& obj)
	{
		if (!Contains(obj))
			Add(obj);
	}

	// Removes the first or all entries of <obj>
	void Remove(const I& obj, bool all = true)
	{
		for (auto it = begin(); it != end();)
		{
			if (*it == obj)
			{
				it = erase(it);
				if (!all)
					break;
			}
			else
				it++;
		}
	}

	// O(N) lookup to check if object is in array
	bool Contains(const I& obj) const
	{
		for (auto& i : *this)
		{
			if (obj == i)
				return true;
		}
		return false;
	}

	// Find first item that matches Predicate
	template<typename Predicate>
	I* Find(Predicate&& pred)
	{
		for (auto& i : *this)
		{
			if (pred(i))
			{
				return &i;
			}
		}
		return nullptr;
	}
	// Find first item that matches Predicate
	template<typename Predicate>
	const I* Find(Predicate&& pred) const
	{
		for (auto& i : *this)
		{
			if (pred(i))
			{
				return &i;
			}
		}
		return nullptr;
	}

	// Find all items that match Predicate
	template<typename Predicate>
	Vector<I*> FindAll(Predicate&& pred)
	{
		Vector<I*> r;
		for (auto& i : *this)
		{
			if (pred(i))
			{
				r.push_back(i);
			}
		}
		return r;
	}
	template<typename Predicate>
	Vector<const I*> FindAll(Predicate&& pred) const
	{
		Vector<I*> r;
		for (auto& i : *this)
		{
			if (pred(i))
			{
				r.push_back(i);
			}
		}
		return r;
	}

	template<typename Predicate>
	void Sort(Predicate&& pred)
	{
		std::sort(begin(), end(), pred);
	}
};
