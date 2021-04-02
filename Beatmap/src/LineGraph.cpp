/// From https://github.com/m4saka/ksh

#include "stdafx.h"
#include "LineGraph.hpp"

void LineGraph::Insert(MapTime mapTime, double point)
{
    auto it = m_points.find(mapTime);
    if (it == m_points.end())
    {
        m_points.emplace_hint(it, mapTime, Point{ point });
    }
    else
    {
        it->second.value.second = point;
    }
}

void LineGraph::Insert(MapTime mapTime, const LineGraph::Point& point)
{
    auto it = m_points.find(mapTime);
    if (it == m_points.end())
    {
        m_points.emplace_hint(it, mapTime, point);
    }
    else
    {
        it->second.value.second = point.value.second;
    }
}

void LineGraph::Insert(MapTime mapTime, const std::string& point)
{
    const std::size_t semicolonIdx = point.find(';');
    if (semicolonIdx == std::string::npos)
    {
        try
        {
            Insert(mapTime, std::stod(point));
        }
        catch (const std::invalid_argument&) {}
        catch (const std::out_of_range&) {}
    }
    else
    {
        Insert(mapTime, LineGraph::Point{ std::stod(point.substr(semicolonIdx + 1)), std::stod(point.substr(semicolonIdx + 1)) });
    }
}

double LineGraph::Extend(MapTime time)
{
    if (m_points.empty())
    {
        Insert(time, m_default);
        return m_default;
    }

    double value = m_default;
    auto it = m_points.lower_bound(time);
    if (it == m_points.end())
    {
        value = m_points.begin()->second.value.first;
    }
    else if(it->first < time)
    {
        value = it->second.value.second;
    }
    else
    {
        return it->second.value.first;
    }

    Insert(time, value);
    return value;
}

double LineGraph::ValueAt(MapTime mapTime) const
{
    if (m_points.empty())
    {
        return m_default;
    }

    const auto secondItr = m_points.upper_bound(mapTime);
    if (secondItr == m_points.begin())
    {
        // Before the first plot
        return secondItr->second.value.first;
    }

    const auto firstItr = std::prev(secondItr);
    const double firstValue = (*firstItr).second.value.second;

    if (secondItr == m_points.end())
    {
        // After the last plot
        return firstValue;
    }

    const double secondValue = secondItr->second.value.first;
    const MapTime firstTime = firstItr->first;
    const MapTime secondTime = secondItr->first;

    // Erratic case
    if (firstTime == secondTime)
    {
        return secondValue;
    }

    // TODO: apply bezier curve
    return Math::Lerp(firstValue, secondValue, (mapTime - firstTime) / static_cast<double>(secondTime - firstTime));
}

String LineGraph::StringValueAt(MapTime mapTime) const
{
    std::stringstream ss;
    ss << std::fixed; // Avoid scientific notation

    auto it = m_points.find(mapTime);

    if (it != m_points.end())
    {
        const Point& point = it->second;
        if (point.IsSlam())
        {
            ss << point.value.first << ';' << point.value.second;
        }
        else
        {
            ss << point.value.first;
        }
    }

    String str = ss.str();

    while (!str.empty() && *str.rbegin() == '0') str.pop_back();
    if (!str.empty() && *str.rbegin() == '.') str.pop_back();

    return str;
}