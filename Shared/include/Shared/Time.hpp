#pragma once
#include <chrono>
#include <ctime>
#include <iomanip>
#include "String.hpp"

namespace Shared
{
	class Time
	{
	public:
		Time(time_t t) : m_time(t) {};
		Time(std::chrono::time_point<std::chrono::system_clock> tp)
		{
			m_time = std::chrono::system_clock::to_time_t(tp);
		};
		static Time Now()
		{
			return Time(std::time(nullptr));
		}
		String ToStringUTC(String format = "")
		{
			if (format.empty())
				format = m_format;
			m_tm = std::gmtime(&m_time);
			char buffer[128];
			std::strftime(buffer, 128, *format, m_tm);
			return String(buffer);
		}
		String ToString(String format = "")
		{
			if (format.empty())
				format = m_format;
			m_tm = std::localtime(&m_time);
			char buffer[128];
			std::strftime(buffer, 128, *format, m_tm);
			return String(buffer);
		}
		time_t Data()
		{
			return m_time;
		}

	private:
		std::tm* m_tm;
		time_t m_time;
		String m_format = "%F_%H-%M-%S";
	};
};
