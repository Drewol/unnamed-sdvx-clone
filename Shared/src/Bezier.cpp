#include "stdafx.h"

/*
class TreeThingy
{
public:
	vector<vector<float>> trees;
	TreeThingy()
	{
		trees.push_back({});
		trees.push_back({});
		trees.push_back({1,1});
	}
	vector<float> GetConstants(uint32 size)
	{
		if (size < trees.size())
			return trees[size];
		auto& prev = GetConstants(size - 1);
		vector<float> v;

		v.push_back(1);
		for (uint32 i = 1; i < prev.size(); i++)
		{
			v.push_back(prev[i - 1] + prev[i]);
		}
		v.push_back(1);

		trees.push_back(v);
		return v;
	}
	static TreeThingy& Main()
	{
		static TreeThingy inst;
		return inst;
	}
};

vector2f Bezier(const vector<vector2f>& points, float time)
{
	return Bezier(points.data(), (uint32)points.size(), time);
}
vector2f Bezier(const vector2f* points, uint32 numPoints, float time)
{
	if (numPoints < 2)
		return vector2f((float)INFINITE);

	vector2f res;
	vector<float> coeff = TreeThingy::Main().GetConstants(numPoints);
	float fInvTime = (1.0f - time);
	for (uint32 i = 0; i < numPoints; i++)
	{
		float fCommon = coeff[i] *
			powf(fInvTime, (float)((numPoints - 1) - i)) *
			powf(time, (float)i);
		res.x += fCommon * points[i].x;
		res.y += fCommon * points[i].y;
	}

	return res;
}
vector<float> BezierDist(const vector<vector2f>& points, uint32 steps)
{
	return BezierDist(points.data(), (uint32)points.size(), steps);
}
vector<float> BezierDist(const vector2f* points, uint32 numPoints, uint32 steps)
{
	vector<float> res;
	vector2f last = points[0];
	float step = 1.0f / (float)(steps - 1);
	for (uint32 i = 0; i < steps; i++)
	{
		float f = (float)i * step;
		vector2f current = Bezier(points, numPoints, f);
		double dist = (double)(last - current).length();
		res.push_back((float)(dist * (float)(steps-1)));
	}
	return res;
}
float BezierLength(const vector<vector2f>& points, uint32 steps)
{
	return BezierLength(points.data(), (uint32)points.size(), steps);
}
float BezierLength(const vector2f* points, uint32 numPoints, uint32 steps)
{
	double dist = 0.0;
	vector2f last = points[0];
	float step = 1.0f / (float)(steps - 1);
	for (uint32 i = 0; i < steps; i++)
	{
		float f = (float)i * step;
		vector2f current = Bezier(points, numPoints, f);
		dist += (double)(last - current).length();
		last = current;
	}
	return (float)dist;
}

vector3f CircleFromPoints(const vector2f& point1, const vector2f& point2, const vector2f& point3)
{
	float offset = pow(point2.x, 2) + pow(point2.y, 2);
	float bc = (pow(point1.x, 2) + pow(point1.y, 2) - offset) / 2.0f;
	float cd = (offset - pow(point3.x, 2) - pow(point3.y, 2)) / 2.0f;
	float det = (point1.x - point2.x) * (point2.y - point3.y) - (point2.x - point3.x)* (point1.y - point2.y);

	float idet = 1 / det;
	float centerx = (bc * (point2.y - point3.y) - cd * (point1.y - point2.y)) * idet;
	float centery = (cd * (point1.x - point2.x) - bc * (point2.x - point3.x)) * idet;

	vector3f res;
	res.xy() = vector2f(centerx, centery);
	res.z = sqrt(pow(point2.x - centerx, 2) + pow(point2.y - centery, 2));
	return res;
}
vector2f CircularCurveAndTangent(const vector2f& start, const vector2f& ctrl, const vector2f& end, float fTime, vector2f& tangent)
{
	vector3f c = CircleFromPoints(start, ctrl, end);
	vector2f dirs[3];
	float angles[3];
	float div = 1.0f / c.z;
	dirs[0] = (start - c.xy()).normalized();
	dirs[1] = (ctrl - c.xy()).normalized();
	dirs[2] = (end - c.xy()).normalized();
	float angleStart = atan2(dirs[0].y, dirs[0].x);
	float dir = vector3f(dirs[0]).cross(vector3f(dirs[1])).z;
	float dir1 = vector3f(dirs[0]).cross(vector3f(dirs[2])).z;
	float angle = dirs[0].angle_between(dirs[2]);
	if (signbit(dir) != signbit(dir1) && angle < PI)
	{
		angle = (2.0f*PI) - angle;
	}
	if (dir < 0)
		angle = -angle;

	vector2f point;
	angle *= fTime;
	point.x = cosf(angleStart + angle);
	point.y = sinf(angleStart + angle);
	angle += PI * 0.5f;
	tangent.x = cosf(angleStart + angle);
	tangent.y = sinf(angleStart + angle);

	return point * c.z + c.xy();
}
vector2f CircularCurve(const vector2f& start, const vector2f& ctrl, const vector2f& end, float fTime)
{
	vector2f dummy;
	return CircularCurveAndTangent(start, ctrl, end, fTime, dummy);
}
float CircularCurveLength(const vector2f& start, const vector2f& ctrl, const vector2f& end)
{
	vector3f c = CircleFromPoints(start, ctrl, end);
	vector2f dirs[3];
	float angles[3];
	float div = 1.0f / c.z;
	dirs[0] = (start - c.xy()).normalized();
	dirs[1] = (ctrl - c.xy()).normalized();
	dirs[2] = (end - c.xy()).normalized();
	float angleStart = atan2(dirs[0].y, dirs[0].x);
	float dir = vector3f(dirs[0]).cross(vector3f(dirs[1])).z;
	float dir1 = vector3f(dirs[0]).cross(vector3f(dirs[2])).z;
	float angle = dirs[0].angle_between(dirs[2]);
	if (signbit(dir) != signbit(dir1) && angle < PI)
	{
		angle = (2.0f*PI) - angle;
	}
	return angle * c.z;
}
*/
