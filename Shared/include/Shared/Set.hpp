#pragma once
#include <set>

/* Set class, extends std::set */
template<typename K>
class Set : public std::set<K>
{
public:
	using std::set<K>::set;
	using std::set<K>::set::find;
	using std::set<K>::set::end;
	using std::set<K>::set::insert;

	bool Contains(const K& key) const
	{
		return find(key) != end();
	}
	const K& Add(const K& key)
	{
		return *insert(key).first;
	}
};
