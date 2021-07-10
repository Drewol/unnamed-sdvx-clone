#pragma once
#include "Shared/String.hpp"
#include "Shared/Color.hpp"

class IConfigEntry
{
public:
	enum class EntryType
	{
		Integer,
		Float,
		Boolean,
		String,
		Color,
		Enum,
		Blob
	};

	virtual ~IConfigEntry() = default;
	// Converts entry to string value
	virtual String ToString() const = 0;
	// Sets the current entry from a string value
	virtual void FromString(const String& str) = 0;

	virtual EntryType GetType() = 0;

	template<typename T> T* As() { return dynamic_cast<T*>(this); }
};

class IntConfigEntry : public IConfigEntry
{
public:
	int32 data;
public:
	virtual String ToString() const override;
	virtual void FromString(const String& str) override;
	EntryType GetType() override { return EntryType::Integer; };
};

class BoolConfigEntry : public IConfigEntry
{
public:
	bool data;
public:
	virtual String ToString() const override;
	virtual void FromString(const String& str) override;
	EntryType GetType() override { return EntryType::Boolean; };
};

class FloatConfigEntry : public IConfigEntry
{
public:
	float data;
public:
	virtual String ToString() const override;
	virtual void FromString(const String& str) override;
	EntryType GetType() override { return EntryType::Float; };
};

class StringConfigEntry : public IConfigEntry
{
public:
	String data;
public:
	virtual String ToString() const override;
	virtual void FromString(const String& str) override;
	EntryType GetType() override { return EntryType::String; };
};

class ColorConfigEntry : public IConfigEntry
{
public:
	Color data;
	virtual String ToString() const override;
	virtual void FromString(const String& str) override;
	EntryType GetType() override { return EntryType::Color; };
};

template<typename EnumClass>
class EnumConfigEntry : public IConfigEntry
{
public:
	typename EnumClass::EnumType data;
public:
	virtual String ToString() const override
	{
		return EnumClass::ToString(data);
	}
	virtual void FromString(const String& str) override
	{
		data = EnumClass::FromString(str);
	}
	EntryType GetType() override { return EntryType::Enum; };
};

template<size_t N>
class BlobConfigEntry : public IConfigEntry
{
	//TODO: Base64?
public:
	std::array<uint8, N> data;
public:
	virtual String ToString() const override
	{
		String ret = "";

		for (size_t i = 0; i < N; i++)
		{
			ret += Utility::Sprintf("%02X", data[i]);
		}

		return ret;
	}
	virtual void FromString(const String& str) override
	{
		if (str.length() != N * 2) {
			return;
		}

		for (size_t i = 0; i < N; i++)
		{
			int value;
			sscanf(str.substr(i * 2, 2).c_str(), "%02X", &value);
			data.at(i) = value;
		}
	}
	EntryType GetType() override { return EntryType::Blob; };
};