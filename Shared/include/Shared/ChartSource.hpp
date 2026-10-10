#pragma once

#include "Types.hpp"
#include "Resource.hpp"
#include "FileSystem.hpp"

/**
 * Charts come from disk or in-memory bytes. Either way as a consumer all you need
 * is a way to get the chart data, and a way to resolve assets.
 */
class ChartSource
{
public:
	virtual ~ChartSource() = default;

	/**
	 * Load the chart bytes.
	 */
	[[nodiscard]] virtual Resource LoadChart() const = 0;
	/**
	 * Resolve an asset - i.e. `.ResolvePath("music.mp3")`.
	 */
	[[nodiscard]] virtual Resource ResolvePath(const String& relativePath) const = 0;
};

// normal chart. backed by disk, no magic. no fun. no joy. no whimsy.
class DiskChartSource final : public ChartSource
{
public:
	explicit DiskChartSource(String chartPath) : m_chartPath(std::move(chartPath)) {}

	[[nodiscard]] Resource LoadChart() const override
	{
		return Resource::FromPath(m_chartPath);
	}
	[[nodiscard]] Resource ResolvePath(const String& relativePath) const override
	{
		return Resource::FromPath(Path::Normalize(Path::RemoveLast(m_chartPath, nullptr) + Path::sep + relativePath), relativePath);
	}
private:
	String m_chartPath;
};
