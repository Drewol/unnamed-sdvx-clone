#pragma once
#include "Shared/ConfigEntry.hpp"
#include "Shared/Map.hpp"
#include "Shared/Enum.hpp"
#include "Shared/Unique.hpp"
#include "Shared/BinaryStream.hpp"
#include <unordered_set>


/*
	Base class used for config files
	Used to save and load data to a text file format
	The config file uses an enum key to index the config file members
*/
class ConfigBase : public Unique
{
public:
	typedef std::unordered_set<uint32> KeyList;
	virtual ~ConfigBase();

	// Load from text file
	bool Load(BinaryStream& stream, bool reload = true);
	bool Load(const String& path, bool reload = true);
	// Save to text file
	void Save(BinaryStream& stream, KeyList* ignore = nullptr, KeyList* only = nullptr);
	bool Save(const String& path, KeyList* ignore = nullptr, KeyList* only = nullptr);

	bool IsDirty() const;

	// Resets config back to default state
	void Clear();

	// Update this config with values from a second
	void Update(ConfigBase& other, KeyList* ignore = nullptr, KeyList* only = nullptr);

	// Only really useful for profiles
	const KeyList& GetKeysInFile() { return m_entriesInFile;  }

protected:
	ConfigBase();
	virtual void InitDefaults() = 0;

	Map<String, uint32> m_keys;
	Map<uint32, String> m_reverseKeys;
	Map<uint32, IConfigEntry*> m_entries;
	KeyList m_entriesInFile;
	
	bool m_dirty = false;
};

/*
	Template config class, specify the type Enum_<enum name> as the key enum for config entries
*/
template<typename EnumClass>
class Config : public ConfigBase
{
public:
	typedef typename EnumClass::EnumType KeyType;
	Config()
	{
		for(auto it : EnumClass::GetMap())
		{
			m_keys.Add(it.second, (uint32)it.first);
			m_reverseKeys.Add((uint32)it.first, it.second);
		}
	}
	bool IsSet(KeyType key) const
	{
		auto it = m_entries.find((uint32)key);
		return it != m_entries.end();
	}

	int32 GetInt(KeyType key) const
	{
		return GetEnsure<IntConfigEntry>(key)->data;
	}
	float GetFloat(KeyType key) const
	{
		return GetEnsure<FloatConfigEntry>(key)->data;
	}
	String GetString(KeyType key) const
	{
		return GetEnsure<StringConfigEntry>(key)->data;
	}
	bool GetBool(KeyType key) const
	{
		return GetEnsure<BoolConfigEntry>(key)->data;
	}
	template<size_t N>
	std::array<uint8, N> GetBlob(KeyType key) const {
		return GetEnsure<BlobConfigEntry<N>>(key)->data;
	}

	template<typename EnumClass1>
	typename EnumClass1::EnumType GetEnum(KeyType key) const
	{
		return GetEnsure<EnumConfigEntry<EnumClass1>>(key)->data;
	}

	void Set(KeyType key, const int32& value)
	{
		int32& dst = SetEnsure<IntConfigEntry>(key)->data;
		if(dst != value)
		{
			dst = value;
			m_dirty = true;
		}
	}
	void Set(KeyType key, const float& value)
	{
		float& dst = SetEnsure<FloatConfigEntry>(key)->data;
		if(dst != value)
		{
			dst = value;
			m_dirty = true;
		}
	}
	void Set(KeyType key, const bool& value)
	{
		bool& dst = SetEnsure<BoolConfigEntry>(key)->data;
		if(dst != value)
		{
			dst = value;
			m_dirty = true;
		}
	}
	void Set(KeyType key, const String& value)
	{
		String& dst = SetEnsure<StringConfigEntry>(key)->data;
		if(dst != value)
		{
			dst = value;
			m_dirty = true;
		}
	}
	void Set(KeyType key, const char* value)
	{
		String& dst = SetEnsure<StringConfigEntry>(key)->data;
		if(dst != value)
		{
			dst = value;
			m_dirty = true;
		}
	}

	template<size_t N>
	void SetBlob(KeyType key, std::array<uint8, N> value)
	{
		std::array<uint8, N>& dst = SetEnsure<BlobConfigEntry<N>>(key)->data;
		if (memcmp(dst.data(), value.data(), N) != 0)
		{
			memcpy(dst.data(), value.data(), N);
			m_dirty = true;
		}
	}

	template<typename EnumClass1>
	void SetEnum(KeyType key, typename EnumClass1::EnumType value)
	{
		typename EnumClass1::EnumType& dst = SetEnsure<EnumConfigEntry<EnumClass1>>(key)->data;
		if(dst != value)
		{
			dst = value;
			m_dirty = true;
		}
	}



private:
	// Create or returns with type checking
	template<typename T> T* SetEnsure(KeyType key)
	{
		IConfigEntry** found = m_entries.Find((uint32)key);
		if(found)
		{
			T* targetType = found[0]->As<T>();
			assert(targetType); // Make sure type matches
			return targetType;
		}
		else
		{
			// Add new entry
			T* ret = new T();
			m_entries.Add((uint32)key, ret);
			return ret;
		}
	}
	// Gets the wanted type with type checking and seeing if it exists
	template<typename T> const T* GetEnsure(KeyType key) const
	{
		IConfigEntry*const* found = m_entries.Find((uint32)key);
		assert(found);
		const T* targetType = found[0]->As<T>();
		assert(targetType); // Make sure type matches
		return targetType;
	}
};

