#include "stdafx.h"
#include "Log.hpp"
#include "Path.hpp"
#include "File.hpp"
#include "FileStream.hpp"
#include "TextStream.hpp"
#include <ctime>
#include <map>
#include <mutex>

class Logger_Impl
{
private:
	File m_logFile;
	FileWriter m_writer;
	bool m_failedToOpen;
	std::mutex m_lock;
	Logger::Severity m_logLevel;

public:
	Logger_Impl()
	{
		// Store the name of the executable
		moduleName = Path::GetModuleName();
		m_logLevel = Logger::Severity::Debug;

#ifdef _WIN32
		// Store console output handle
		consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
#endif

		// Log to file
		String logPath = Path::Absolute(Utility::Sprintf("log_%s.txt", moduleName));
		if (!m_logFile.OpenWrite(logPath, false, true))
		{
			m_failedToOpen = true;
			return;
		}
		m_failedToOpen = false;
		m_writer = FileWriter(m_logFile);
	}

	void Lock()
	{
		m_lock.lock();
	}

	void Unlock()
	{
		m_lock.unlock();
	}

	void SetLogLevel(Logger::Severity level)
	{
		m_logLevel = level;
	}

	Logger::Severity GetLogLevel() {
		return m_logLevel;
	}

	void WriteHeader(Logger::Severity severity)
	{

		// Format a timestamp string
		char timeStr[64];
		time_t currentTime = time(0);
		tm* currentLocalTime = localtime(&currentTime);
		strftime(timeStr, sizeof(timeStr), "%T", currentLocalTime);

		// Write the formated header
		Write(Utility::Sprintf("[%s][%s] ", timeStr, Logger::Enum_Severity::ToString(severity)));
	}
	void Write(const String& msg)
	{
#ifdef _WIN32
		OutputDebugStringW(*Utility::ConvertToWString(msg));
#endif
		printf("%s", msg.c_str());
		if (!m_failedToOpen)
			TextStream::Write(m_writer, msg);
	}

#ifdef _WIN32
	HANDLE consoleHandle;
#endif
	String moduleName;
};

Logger::Logger()
{
	m_impl = new Logger_Impl;
}
Logger::~Logger()
{
#ifndef _WIN32
	// Reset terminal colors
	printf("\x1b[39m\x1b[0m");
#endif
	delete m_impl;
}
Logger& Logger::Get()
{
	static Logger logger;
	return logger;
}
void Logger::SetColor(Color color)
{
#ifdef _WIN32
	if (m_impl->consoleHandle)
	{
		static uint8 params[] =
		{
			FOREGROUND_INTENSITY | FOREGROUND_RED,
			FOREGROUND_INTENSITY | FOREGROUND_GREEN,
			FOREGROUND_INTENSITY | FOREGROUND_BLUE,
			FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_GREEN, // Yellow,
			FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_RED, // Cyan,
			FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_RED, // Magenta,
			FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, // White
			FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN, // Gray
		};
		SetConsoleTextAttribute(m_impl->consoleHandle, params[(size_t)color]);
	}
#else
	static std::map<Color, const char*> params = {
		{Color::Red,     "200;0;0"},
		{Color::Green,   "0;200;0"},
		{Color::Blue,    "0;70;200"},
		{Color::Yellow,  "200;180;0"},
		{Color::Cyan,    "0;200;200"},
		{Color::Magenta, "200;0;200"},
		{Color::Gray,    "140;140;140"}
	};
	if (color == Color::White)
		printf("\x1b[39m");
	else
		printf("\x1b[38;2;%sm", params[color]);
#endif
}
void Logger::Log(const String& msg, Logger::Severity severity)
{
	if (severity < m_impl->GetLogLevel())
		return;
	switch (severity)
	{
	case Severity::Normal:
		SetColor(White);
		break;
	case Severity::Info:
		SetColor(Gray);
		break;
	case Severity::Warning:
		SetColor(Yellow);
		break;
	case Severity::Error:
		SetColor(Red);
		break;
	case Severity::Debug:
		SetColor(Blue);
		break;
	}

	m_impl->Lock();
	m_impl->WriteHeader(severity);
	m_impl->Write(msg);
	m_impl->Write("\n");
	m_impl->Unlock();
}
void Logger::WriteHeader(Severity severity)
{
	m_impl->WriteHeader(severity);
}
void Logger::Write(const String& msg)
{
	m_impl->Write(msg);
}
void Logger::SetLogLevel(Logger::Severity level)
{
	m_impl->SetLogLevel(level);
}
void Log(const String& msg, Logger::Severity severity)
{
	Logger::Get().Log(msg, severity);
}

#ifdef _WIN32
String Utility::WindowsFormatMessage(uint32 code)
{
	if (code == 0)
	{
		return "No additional info available";
	}

	wchar_t buffer[1024] = { 0 };
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, 0, code, LANG_SYSTEM_DEFAULT, buffer, sizeof(buffer), 0);

	return Utility::ConvertToUTF8(buffer);
}
#endif
