#include "stdafx.h"
#include "StringEncodingConverter.hpp"

#include <errno.h>

#include "Shared/Log.hpp"

#include "iconv.h"
#include "archive_entry.h"

String StringEncodingConverter::ToUTF8(StringEncoding encoding, const char* str, const size_t str_len)
{
	if (!NeedsConversion(encoding)) return String(str);

	iconv_t conv_d = iconv_open("UTF-8", GetIConvArg(encoding));
	if (conv_d == (iconv_t)-1)
	{
		Logf("Error in StringEncodingConverter::ToUTF8: iconv_open returned -1 for encoding %s", Logger::Severity::Error, GetDisplayString(encoding));
		return String(str);
	}

	String result;
	char out_buf_arr[BUFFER_SIZE];
	out_buf_arr[BUFFER_SIZE - 1] = '\0';

	const char* in_buf = str;
	const char* in_buf_prev = in_buf;
	size_t in_buf_left = str_len;

	char* out_buf = out_buf_arr;
	size_t out_buf_left = BUFFER_SIZE - 1;

	while (iconv(conv_d, const_cast<char**>(&in_buf), &in_buf_left, &out_buf, &out_buf_left) == -1)
	{
		// errno doesn't seem to be realible on Windows (set to 0 or 9 instead of E2BIG)
		const int err = errno;

		if (in_buf != in_buf_prev)
		{
			in_buf_prev = in_buf;

			*out_buf = '\0';
			result += out_buf_arr;
			out_buf = out_buf_arr;
			out_buf_left = BUFFER_SIZE - 1;

			continue;
		}

		Logf("Error in StringEncodingConverter::ToUTF8: iconv failed with %d for encoding %s", Logger::Severity::Error, err, GetDisplayString(encoding));
		iconv_close(conv_d);
		return String(str);
	}

	iconv_close(conv_d);

	*out_buf = '\0';
	result.append(out_buf_arr);

	return result;
}

String StringEncodingConverter::PathnameToUTF8(StringEncoding encoding, struct archive_entry* entry)
{
	if (encoding != StringEncoding::Unknown)
	{
		if (const char* pathname = archive_entry_pathname(entry))
		{
			return StringEncodingConverter::ToUTF8(encoding, pathname);
		}
	}

	if (const wchar_t* pathname_w = archive_entry_pathname_w(entry))
	{
		return Utility::ConvertToUTF8(pathname_w);
	}

	Log("Error in StringEncodingConverter::PathnameToUTF8: pathname couldn't be read", Logger::Severity::Error);

	return String();
}
