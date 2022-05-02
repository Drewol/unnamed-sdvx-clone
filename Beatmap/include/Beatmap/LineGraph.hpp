#pragma once

/// From https://github.com/m4saka/ksh
/// but modified to USC's taste and somewhat compatible to KSON

#include <utility>
#include <Shared/Map.hpp>

#include "BeatmapObjects.hpp"

class LineGraph {
public:
    LineGraph(double defaultValue = 0.0) : m_default(defaultValue) {}

    struct Point     {
        explicit Point(double val) : value(val, val) {}
        explicit Point(double start, double end) : value(start, end) {}

        inline bool IsSlam() const { return value.first != value.second; }

        std::pair<double, double> value;
        std::pair<double, double> curve = {};
    };

private:
    using Points = Map<MapTime, Point>;
    Points m_points;
    const double m_default = 0.0;

public:
    using PointsIterator = Points::const_iterator;

    void Insert(MapTime time, double point);
    void Insert(MapTime time, const Point& point);
    void Insert(MapTime time, const std::string& point);

    void RangeSet(MapTime begin, MapTime end, double value);
    void RangeAdd(MapTime begin, MapTime end, double delta);

    /// Returns the value being extended.
    double Extend(MapTime time);

    double Integrate(MapTime begin, MapTime end) const;

    /// When you know for certain that curr->first &lt;= begin &lt;= end &lt;= std::next(curr)-&gt;first
    double Integrate(PointsIterator curr, MapTime begin, MapTime end) const;
    double Integrate(PointsIterator curr) const;

    inline PointsIterator lower_bound(MapTime time) const
    {
        return m_points.lower_bound(time);
    }

    inline PointsIterator upper_bound(MapTime time) const
    {
        return m_points.upper_bound(time);
    }

    inline std::size_t erase(MapTime time)
    {
        return m_points.erase(time);
    }

    inline const Point& at(MapTime time) const
    {
        return m_points.at(time);
    }

    inline Map<MapTime, Point>::iterator begin()
    {
        return m_points.begin();
    }

    inline PointsIterator begin() const
    {
        return m_points.begin();
    }

    inline PointsIterator cbegin() const
    {
        return m_points.cbegin();
    }

    inline Map<MapTime, Point>::iterator end()
    {
        return m_points.end();
    }

    inline PointsIterator end() const
    {
        return m_points.end();
    }

    inline PointsIterator cend() const
    {
        return m_points.cend();
    }

    inline std::size_t size() const
    {
        return m_points.size();
    }

    inline bool empty() const
    {
        return m_points.empty();
    }

    inline std::size_t count(MapTime mapTime) const
    {
        return m_points.count(mapTime);
    }

    double ValueAt(MapTime mapTime) const;
};