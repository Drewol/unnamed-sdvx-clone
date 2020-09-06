#include "stdafx.h"
#include "StringEncodingDetector.hpp"
#include "StringEncodingHeuristic.hpp"

#include "Buffer.hpp"
#include "BinaryStream.hpp"
#include "Log.hpp"

#include "archive.h"
#include "archive_entry.h"

class StringEncodingDetectorInternal final : public TieredStringEncodingHeuristic
<
	// UTF-8 always take top priority
	StringEncodingHeuristicCollection<UTF8Heuristic>,
	// Japanese encodings take priority over others
	StringEncodingHeuristicCollection<CP932Heuristic, CP954Heuristic>
	// Disabled because they are not as frequent as ShiftJIS or EUC-JP
	// StringEncodingHeuristicCollection<CP949Heuristic, CP850Heuristic, CP923Heuristic>
>
{};

StringEncodingDetector::StringEncodingDetector()
{
	m_internal = new StringEncodingDetectorInternal();
}

StringEncodingDetector::~StringEncodingDetector()
{
	delete m_internal;
}

void StringEncodingDetector::Feed(const char* data, const size_t len)
{
	assert(!m_done);

	for (size_t i = 0; i < len; ++i)
	{
		m_internal->Consume(data[i]);
	}
}

void StringEncodingDetector::End()
{
	assert(!m_done);

	m_internal->Finalize();
	
	const StringEncodingHeuristic& best = m_internal->GetBestHeuristic();
	if (best.IsValid()) m_encoding = best.GetEncoding();
	else m_encoding = StringEncoding::Unknown;

	// m_internal->DebugPrint();

	m_done = true;
}

StringEncoding StringEncodingDetector::Detect(BinaryStream& stream, const size_t from, const size_t len)
{
	assert(stream.IsReading());

	if (len == 0 || from >= stream.GetSize()) return StringEncoding::Unknown;

	const size_t to = from + len;
	if (to > stream.GetSize()) return Detect(stream, from, stream.GetSize() - from);

	StringEncodingDetector detector;

	size_t pos = stream.Tell();
	stream.Seek(from);

	size_t curr_pos = from;
	char buffer[BUFFER_SIZE];

	while (curr_pos < to)
	{
		const size_t curr_size = curr_pos + BUFFER_SIZE <= to ? BUFFER_SIZE : to - curr_pos;
		const size_t read_size = stream.Serialize(buffer, curr_size);

		detector.Feed(buffer, read_size);

		if (read_size < curr_size)
		{
			Log("StringEncodingDetector::Detect couldn't read BinaryStream properly", Logger::Severity::Error);
			break;
		}

		curr_pos += BUFFER_SIZE;
	}

	stream.Seek(pos);
	return detector.GetEncoding();
}

StringEncoding StringEncodingDetector::DetectArchive(const Buffer& buffer)
{
	struct archive* a = archive_read_new();
	if (a == nullptr)
	{
		return StringEncoding::Unknown;
	}

	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	if (archive_read_open_memory(a, buffer.data(), buffer.size()) != ARCHIVE_OK)
	{
		archive_read_free(a);
		return StringEncoding::Unknown;
	}

	StringEncodingDetector detector;

	struct archive_entry* entry = nullptr;

	while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
	{
		if (const char* entryName = archive_entry_pathname(entry))
		{
			detector.Feed(entryName);
		}
		else
		{
			archive_read_free(a);
			return detector.GetEncoding();
		}

		archive_read_data_skip(a);
	}

	archive_read_free(a);
	return detector.GetEncoding();
}
