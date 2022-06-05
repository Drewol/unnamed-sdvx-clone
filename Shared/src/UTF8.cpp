/*
Copyright (c) 2009 SegFault aka "ErV" (altctrlbackspace.blogspot.com)

Redistribution and use of this source code, with or without modification, is
permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "stdafx.h"
#include "String.hpp"

void utf8toWStr(WString& dest, const String& src)
{
	dest.clear();
	uint32_t w = 0;
	int bytes = 0;
	wchar_t err = { 0xFFFD };
	for (size_t i = 0; i < src.size(); i++)
	{
		uint8_t c = static_cast<uint8_t>(src[i]);
		if (c <= 0x7f)
		{
			// First byte
			if (bytes)
			{
				dest.push_back(err);
				bytes = 0;
			}
			dest.push_back(static_cast<wchar_t>(c));
		}
		else if (c <= 0xbf)
		{
			// Second/third/etc byte
			if (bytes)
			{
				w = ((w << 6) | (c & 0x3f));
				bytes--;
				if (bytes == 0)
				{
					// Single wchar
					if (w < 0x10000)
					{
						dest.push_back(w);
					}
					// Convert w to a surrogate pair
					else
					{
						w -= 0x10000;
						dest.push_back(static_cast<wchar_t>(0xD800 + static_cast<uint16_t>(w >> 10)));
						dest.push_back(static_cast<wchar_t>(0xDC00 + static_cast<uint16_t>(w & 0b11'1111'1111)));
					}
				}
			}
			else
				dest.push_back(err);
		}
		else if (c <= 0xdf)
		{
			// 2 byte sequence start
			bytes = 1;
			w = c & 0x1f;
		}
		else if (c <= 0xef)
		{
			// 3 byte sequence start
			bytes = 2;
			w = c & 0x0f;
		}
		else if (c <= 0xf7)
		{
			// 4 byte sequence start
			bytes = 3;
			w = c & 0x07;
		}
		else
		{
			dest.push_back(err);
			bytes = 0;
		}
	}
	if (bytes)
		dest.push_back(err);
}

static void pushBMPCharToUtf8(String& dest, uint16_t ch)
{
	if (ch <= 0x7f)
	{
		dest.push_back((char)ch);
	}
	else if (ch <= 0x7ff)
	{
		dest.push_back(0xc0 | ((ch >> 6) & 0x1f));
		dest.push_back(0x80 | (ch & 0x3f));
	}
	else
	{
		dest.push_back(0xe0 | ((ch >> 12) & 0x0f));
		dest.push_back(0x80 | ((ch >> 6) & 0x3f));
		dest.push_back(0x80 | (ch & 0x3f));
	}
}

void wstrToUtf8(String& dest, const WString& src)
{
	dest.clear();
	for (size_t i = 0; i < src.size(); ++i)
	{
		const uint16_t ch = static_cast<uint16_t>(src[i]);

		// Characters represented by single-wchar
		if (ch < 0xD800 || 0xDC00 <= ch)
		{
			pushBMPCharToUtf8(dest, ch);
			continue;
		}

		// The current character is encoded with a surrogate pair.
		// Check whether this high surrogate can be matched by a low surrogate.
		// If it can't be matched, then let's just encode the pair in UTF-8 ~_~
		if (i + 1 >= src.size())
		{
			pushBMPCharToUtf8(dest, ch);
			continue;
		}

		const uint16_t cl = static_cast<uint16_t>(src[++i]);
		if (cl < 0xDC00 || 0xE000 <= cl)
		{
			pushBMPCharToUtf8(dest, ch);
			pushBMPCharToUtf8(dest, cl);
			continue;
		}

		const uint32_t w = (static_cast<uint32_t>(ch - 0xD800) << 10 | static_cast<uint32_t>(cl - 0xDC00)) + 0x10000;
		dest.push_back(0xf0 | ((w >> 18) & 0x07));
		dest.push_back(0x80 | ((w >> 12) & 0x3f));
		dest.push_back(0x80 | ((w >> 6) & 0x3f));
		dest.push_back(0x80 | (w & 0x3f));
	}
}
