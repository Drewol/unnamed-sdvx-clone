#pragma once
#include "BeatmapObjects.hpp"
#include "LineGraph.hpp"

/// Loosely following https://github.com/m4saka/ksh

/// Contains camera and other miscellaneous effects for lane / notes
class EffectTimeline
{
public:
	enum class GraphType
	{
		ZOOM_BOTTOM,
		ZOOM_TOP,
		SHIFT_X,
		ROTATION_Z,
		SCROLL_SPEED,
	};

	inline LineGraph& GetGraph(GraphType type)
	{
		switch (type)
		{
		case GraphType::ZOOM_BOTTOM: return m_zoomBottom;
		case GraphType::ZOOM_TOP: return m_zoomTop;
		case GraphType::SHIFT_X: return m_shiftX;
		case GraphType::ROTATION_Z: return m_rotationZ;
		case GraphType::SCROLL_SPEED: return m_scrollSpeed;
		}
	}

	inline const LineGraph& GetGraph(GraphType type) const
	{
		switch (type)
		{
		case GraphType::ZOOM_BOTTOM: return m_zoomBottom;
		case GraphType::ZOOM_TOP: return m_zoomTop;
		case GraphType::SHIFT_X: return m_shiftX;
		case GraphType::ROTATION_Z: return m_rotationZ;
		case GraphType::SCROLL_SPEED: return m_scrollSpeed;
		}
	}

	inline void InsertGraphValue(GraphType type, MapTime mapTime, double value)
	{
		GetGraph(type).Insert(mapTime, value);
	}

	inline Vector<ChartStop>& GetChartStops() { return m_chartStops; }
	inline const Vector<ChartStop>& GetChartStops() const { return m_chartStops; }

private:
	MapTime m_offset;

	Vector<ChartStop> m_chartStops;

	LineGraph m_zoomBottom;
	LineGraph m_zoomTop;
	LineGraph m_shiftX; /// former zoom_side

	LineGraph m_rotationZ; /// former manual tilt

	LineGraph m_scrollSpeed = LineGraph{ 1.0 };
};