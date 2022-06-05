#include "Extractor.hpp"

#include <iostream>
#include <stdexcept>

#include "archive.h"
#include "archive_entry.h"

void Extractor::Extract(const std::string_view data)
{
	struct archive* src = CreateRead(data);
	struct archive* dst = CreateDiskWrite();

	try
	{
		CopyArchive(src, dst);
	}
	catch (...)
	{
		archive_read_free(src);
		archive_write_free(dst);

		throw;
	}

	archive_read_free(src);
	archive_write_free(dst);
}

archive* Extractor::CreateRead(const std::string_view data)
{
	struct archive* a = archive_read_new();
	archive_read_support_format_all(a);
	archive_read_support_compression_all(a);

	if (int r = archive_read_open_memory(a, data.data(), data.size()))
	{
		archive_read_free(a);
		throw std::runtime_error("Failed to open the archive.");
	}

	return a;
}

archive* Extractor::CreateDiskWrite()
{
	int flags = 0;

	flags |= ARCHIVE_EXTRACT_TIME;
	flags |= ARCHIVE_EXTRACT_ACL;
	flags |= ARCHIVE_EXTRACT_FFLAGS;

	flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;
	flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;

	struct archive* a = archive_write_disk_new();
	archive_write_disk_set_options(a, flags);
	archive_write_disk_set_standard_lookup(a);

	return a;
}

// https://github.com/libarchive/libarchive/wiki/Examples#a-complete-extractor

static void WarnOrThrow(int code, archive* a, int throw_level)
{
	if (code < throw_level) throw std::runtime_error(archive_error_string(a));
	if (code < ARCHIVE_OK)
	{
		std::cerr << "- Warning: " << archive_error_string(a) << std::endl;
	}
}

void Extractor::CopyArchive(archive* src, archive* dst)
{
	struct archive_entry* entry = nullptr;

	for (;;)
	{
		int r = archive_read_next_header(src, &entry);

		if (r == ARCHIVE_EOF) break;
		if (r < ARCHIVE_WARN) throw std::runtime_error(archive_error_string(src));

		std::cout << "Extracting \"" << archive_entry_pathname(entry) << "\"..." << std::endl;

		if (r < ARCHIVE_OK)
		{
			std::cerr << "- Warning: " << archive_error_string(src) << std::endl;
		}

		r = archive_write_header(dst, entry);
		if (r < ARCHIVE_OK)
		{
			std::cerr << "- Warning: " << archive_error_string(dst) << std::endl;
		}
		else if (archive_entry_size(entry) > 0)
		{
			CopyArchiveData(src, dst);
		}
	}
}

void Extractor::CopyArchiveData(archive* src, archive* dst)
{
	int r;
	const void* buff;
	size_t size;
	la_int64_t offset;

	for (;;) {
		r = archive_read_data_block(src, &buff, &size, &offset);

		if (r == ARCHIVE_EOF) break;
		WarnOrThrow(r, src, ARCHIVE_OK);

		r = archive_write_data_block(dst, buff, size, offset);
		WarnOrThrow(r, dst, ARCHIVE_OK);
	}

	r = archive_write_finish_entry(dst);
	WarnOrThrow(r, dst, ARCHIVE_WARN);
}
