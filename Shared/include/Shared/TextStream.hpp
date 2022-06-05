#pragma once
#include "Shared/BinaryStream.hpp"

/*
	Static helper functions for reading/writing text from BinaryStream objects instead of binary data
*/
class TextStream
{
public:
	static bool ReadLine(BinaryStream& stream, String& out, const String& lineEnding = "\n");
	static void Write(BinaryStream& stream, const String& out);
	static void WriteLine(BinaryStream& stream, const String& out, const String& lineEnding = "\n");
};
