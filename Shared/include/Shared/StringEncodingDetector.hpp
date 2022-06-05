#pragma once

#include "Shared/StringEncoding.hpp"
#include "Shared/String.hpp"

class Buffer;
class BinaryStream;

class StringEncodingDetectorInternal;

// Detects string encoding using heuristic
// (Refer to StringEncodingHeuristic.hpp for more detail)
class StringEncodingDetector
{
public:
	StringEncodingDetector();
	~StringEncodingDetector();

	void Feed(const char* data, const size_t len);
	inline void Feed(const char* data) { Feed(data, strlen(data)); }
	inline void Feed(const String& str) { Feed(str.c_str(), str.size()); }

	inline StringEncoding GetEncoding() { if (!m_done) End(); return m_encoding; }

	inline static StringEncoding Detect(const char* data, const size_t len);
	inline static StringEncoding Detect(const char* data) { return Detect(data, strlen(data)); }
	inline static StringEncoding Detect(const String& str) { return Detect(str.c_str(), str.size()); }
	static StringEncoding Detect(BinaryStream& stream, const size_t from, const size_t len);
	static StringEncoding DetectArchive(const Buffer& buffer);

protected:
	void End();

	class StringEncodingDetectorInternal* m_internal = nullptr;
	StringEncoding m_encoding = StringEncoding::Unknown;

	bool m_done = false;

	constexpr static size_t BUFFER_SIZE = 64;
};

inline StringEncoding StringEncodingDetector::Detect(const char* data, const size_t len)
{
	StringEncodingDetector detector;
	detector.Feed(data, len);

	return detector.GetEncoding();
}
