#pragma once
#include <map>

/* Map class, extends std::map */
template<typename K, typename V>
class Map : public std::map<K, V>
{
public:
	using std::map<K, V>::map;

	// These are for allowing function to be called on the base class when compiling on GCC
	using std::map<K, V>::begin;
	using std::map<K, V>::end;
	using std::map<K, V>::erase;
	using std::map<K, V>::insert;
	using std::map<K, V>::find;

	bool Contains(const K& key) const
	{
		return find(key) != end();
	}
	V& FindOrAdd(const K& key, const V& defaultValue = V())
	{
		auto it = find(key);
		if (it == end())
		{
			it = insert(std::make_pair(key, defaultValue)).first;
		}
		return it->second;
	}
	V& Add(const K& k, const V& v = V())
	{
		return FindOrAdd(k) = v;
	}

	// Finds the value in the map or returns null
	V* Find(const K& k)
	{
		auto it = find(k);
		if (it == end())
			return nullptr;
		return &it->second;
	}
	const V* Find(const K& k) const
	{
		auto it = find(k);
		if (it == end())
			return nullptr;
		return &it->second;
	}

	// Finds the value that lies either on of before 'key'
	// O(N)
	K* FindBefore(const K& key)
	{
		K* res = nullptr;
		for (auto it = begin(); it != end(); it++)
		{
			if (it->first > key)
				break;
			res = &it->second;
		}
		return res;
	}
	// Finds the first value that lies after 'key'
	// O(N)
	K* FindAfter(const K& key)
	{
		for (auto it = begin(); it != end(); it++)
		{
			if (it->first > key)
			{
				return &it->second;
			}
		}
		return nullptr;
	}
};

/* Multimap class, extends std::map */
template<typename K, typename V>
class Multimap : public std::multimap<K, V>
{
public:
	using std::multimap<K, V>::multimap;

	// These are for allowing function to be called on the base class when compiling on GCC
	using std::multimap<K, V>::begin;
	using std::multimap<K, V>::end;
	using std::multimap<K, V>::erase;
	using std::multimap<K, V>::insert;
	using std::multimap<K, V>::find;

	bool Contains(const K& key) const
	{
		return find(key) != end();
	}
	V& Add(const K& k, const V& v = V())
	{
		auto it = insert(std::make_pair(k, v));
		return it->second;
	}
};
