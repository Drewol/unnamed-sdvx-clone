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

    auto it = m_points.upper_bound(time);

    if (it == m_points.begin())
    {
        return it->second.value.first;
    }

    it = std::prev(it);

    if (it->first == time)
    {
        return it->second.value.first;
    }

    const double value = it->second.value.second;
    Insert(time, value);

    return value;
}

double LineGraph::Integrate(MapTime begin, MapTime end) const
{
    if (begin == end)
    {
        return 0.0;
    }

    if (end < begin)
    {
        std::swap(begin, end);
    }

    // Integration range is after the last point
    auto beginIt = m_points.upper_bound(begin);
    if (beginIt == m_points.end())
    {
        return m_points.rbegin()->second.value.second * (end - begin);
    }

    if (end <= beginIt->first)
    {
        if (beginIt == m_points.begin())
        {
            // Integration range is before the first point
            return beginIt->second.value.first * (end - begin);
        }
        else
        {
            // Integration range contained in a single segment
            return Integrate(std::prev(beginIt), begin, end);
        }
    }

    double result = 0.0;

    // Ensure that the beginning of the integration range is beginIt
    if (beginIt == m_points.begin())
    {
        result = beginIt->second.value.first * (beginIt->first - begin);
    }
    else if(begin != beginIt->first)
    {
        auto beginPrev = std::prev(beginIt);
        result = Integrate(beginPrev, begin, beginIt->first);
    }

    auto endIt = m_points.lower_bound(end);
    if (endIt == m_points.begin())
    {
        // This means that end <= m_points.begin()->first
        // But then, since begin < end, begin < m_points.begin()->first so beginIt == m_points.begin()
        // Therefore end <= beginIt->first and this case is already handled
        // But let's check for this case just to be sure
        assert(false);
        return endIt->second.value.first * (end - begin);
    }

    // Ensure that the end of the integration range is endIt
    if (endIt->first != end)
    {
        endIt = std::prev(endIt);
        result += Integrate(endIt, endIt->first, end);
    }

    // Integrate the remaining part
    for (; beginIt != endIt; ++beginIt)
    {
        result += Integrate(beginIt);
    }

    return result;
}

double LineGraph::Integrate(PointsIterator curr, MapTime begin, MapTime end) const
{
    if (begin == end)
    {
        return 0.0;
    }

    if (end < begin)
    {
        std::swap(begin, end);
    }

    if (m_points.empty())
    {
        return (end - begin) * m_default;
    }

    if (curr == m_points.end())
    {
        return m_points.rbegin()->second.value.second * (end - begin);
    }

    assert(curr->first <= begin);

    auto next = std::next(curr);
    if (next == m_points.end())
    {
        return curr->second.value.second * (end - begin);
    }

    assert(end <= next->first);

    // TODO: support integration of bezier curves
    double value = Integrate(curr);

    if (curr->first != begin)
    {
        const double x = static_cast<double>(begin - curr->first) / (next->first - curr->first);
        value -= (begin - curr->first) * Math::Lerp(curr->second.value.second, next->second.value.first, x * 0.5);
    }

    if (end != next->first)
    {
        const double x = static_cast<double>(next->first - end) / (next->first - curr->first);
        value -= (next->first - end) * Math::Lerp(curr->second.value.second, next->second.value.first, 1 - x * 0.5);
    }

    return value;
}

double LineGraph::Integrate(PointsIterator curr) const
{
    if (m_points.empty() || curr == m_points.end())
    {
        return 0.0;
    }

    auto next = std::next(curr);
    if (next == m_points.end())
    {
        assert(false);
        return 0.0;
    }

    return static_cast<double>(next->first - curr->first) * (next->second.value.first + curr->second.value.second) * 0.5;
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