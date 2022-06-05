#include "Downloader.hpp"

#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <type_traits>

#include "cpr/cpr.h"

void Downloader::Download(const std::string& url)
{
	std::cout << "Downloading the game from \"" << url << "\"..." << std::endl;
	m_content.clear();

	cpr::Session session;
	session.SetUrl(url);
	session.SetProgressCallback(cpr::ProgressCallback([&](cpr::cpr_off_t total, cpr::cpr_off_t current, cpr::cpr_off_t, cpr::cpr_off_t, intptr_t) {
		m_progressBar.Update(m_content.size() + total, m_content.size() + current);
		return true;
		}));

	m_progressBar.Start();

	cpr::Response response = {};

	while (true)
	{
		response = session.Get();
		if (response.text.empty()) break;

		m_content += std::move(response.text);

		if (!response.error) break;
		if (response.error.code != cpr::ErrorCode::NETWORK_RECEIVE_ERROR) break;

		// It might be a fluke so continue trying...
		std::stringstream sb;
		sb << "bytes=" << m_content.size() << "-";
		session.SetHeader({ {"Range", sb.str()} });
	}

	m_progressBar.Finish();

	if (response.error)
	{
		std::stringstream sb;

		sb << "Download failed with code " << static_cast<std::underlying_type_t<cpr::ErrorCode>>(response.error.code);

		if (!response.error.message.empty())
		{
			sb << ": " << response.error.message;
		}

		throw std::runtime_error(sb.str());
	}

	if (response.status_code / 100 != 2)
	{
		std::stringstream sb;
		sb << "The server has returned HTTP status code " << response.status_code << ".";

		throw std::runtime_error(sb.str());
	}

	if (response.status_code != 200 && response.status_code != 206)
	{
		std::cerr << "Warning: downloaded succeeded with HTTP status code " << response.status_code << ".";
	}
}

void Downloader::ProgressBar::Start()
{
	Render(0, 0);
}

void Downloader::ProgressBar::Update(size_t total, size_t current)
{
	Render(total, current);
}

void Downloader::ProgressBar::Finish()
{
	std::cout << std::endl;
}

void Downloader::ProgressBar::Render(size_t total, size_t current)
{
	const size_t percentage = total == 0 ? 0 : (current * 100) / total;
	const size_t num_completes = total == 0 ? 0 : (current * m_width) / total;

	std::cout << '\r';
	std::cout << std::setfill(' ') << std::setw(3) << percentage << "%";

	std::cout << " [";

	for (size_t i = 0; i < m_width; ++i)
	{
		std::cout << (i < num_completes ? '#' : ' ');
	}

	std::cout << ']';

	size_t current_kb = current / 1000;
	size_t total_kb = total / 1000;

	// Dunno whether doing this instead of using floating points and std::setprecision is better...
	std::cout << ' ' << std::setfill(' ') << std::setw(2) << current_kb / 1000 << '.' << std::setfill('0') << std::setw(2) << (current_kb / 10) % 10;
	std::cout << " / " << std::setfill(' ') << std::setw(2) << total_kb / 1000 << '.' << std::setfill('0') << std::setw(2) << (total_kb / 10) % 10;
	std::cout << " MB";

	std::cout.flush();
}

