#pragma once
#include "Shared/String.hpp"
#include "Shared/Map.hpp"
#include "Shared/Enum.hpp"
#include "Shared/String.hpp"
#include "Shared/Unique.hpp"

/*
	Logging utility class
	formats loggin messages with time stamps and module names
	allows message coloring on platforms that support it
*/



class Logger : Unique
{
public:
	enum Color
	{
		Red = 0,
		Green,
		Blue,
		Yellow,
		Cyan,
		Magenta,
		White,
		Gray
	};


	DefineEnum(Severity,
		Debug = 0,
		Info = 1,
		Normal = 2,
		Warning = 3,
		Error = 4
	)



public:
	Logger();
	~Logger();
	static Logger& Get();
	// Sets the foreground color of the output, if applicable
	void SetColor(Color color);
	// Log a string to the logging output, 
	void Log(const String& msg, Logger::Severity severity);

	// Write log message header, (timestamp, etc..)
	void WriteHeader(Logger::Severity severity);
	// Writes string without newline
	void Write(const String& msg);

	// Sets the log level, logs for >= level
	void SetLogLevel(Logger::Severity level);

private:
	class Logger_Impl* m_impl;
};

// Log to Logger::Get() with formatting string
template<typename... Args>
void Logf(const char* format, Logger::Severity severity, Args... args)
{
	String msg = Utility::Sprintf<Args...>(format, args...);
	Logger::Get().Log(msg, severity);
}
// Log to Logger::Get()
void Log(const String& msg, Logger::Severity severity = Logger::Severity::Normal);

#ifdef _WIN32
namespace Utility
{
	String WindowsFormatMessage(uint32 code);
}
#endif
