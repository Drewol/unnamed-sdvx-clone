#include "stdafx.h"
#include "Transform.hpp"
#include <cmath>

Transform ProjectionMatrix::CreatePerspective(float field_of_view, float aspect_ratio, float z_near, float z_far)
{
	assert(z_near > 0.0f);
	Transform result;
	float height, width;
	height = z_near * tanf((field_of_view * Math::degToRad) * 0.5f);
	width = height * aspect_ratio;

	const float f1 = z_near * 2;
	const float f2 = width * 2;
	const float f3 = height * 2;
	const float f4 = z_far - z_near;
	result[0] = f1 / f2;
	result[5] = f1 / f3;
	result[10] = (-z_far - z_near) / f4;
	result[11] = -1;
	result[14] = (-f1 * z_far) / f4;
	result[15] = 0;
	return result;
}
Transform ProjectionMatrix::CreateOrthographic(float left, float right, float bottom, float top, float z_near, float z_far)
{
	Transform result;
	result[0] = 2 / (right - left);
	result[5] = 2 / (top - bottom);
	result[10] = -2 / (z_far - z_near);
	result[12] = -(right + left) / (right - left);
	result[13] = -(top + bottom) / (top - bottom);
	result[14] = -(z_far + z_near) / (z_far - z_near);
	result[15] = 1;
	return result;
}

Transform& Transform::operator=(const Transform& right)
{
	memcpy(this, &right, sizeof(mat));
	return *this;
}
Transform::Transform(std::initializer_list<float> values)
{
	auto it = values.begin();
	for (size_t i = 0; i < Math::Max<size_t>(values.size(), 16); i++)
	{
		mat[i] = *it++;
	}
}
Transform::Transform(const Transform& other)
{
	memcpy(this, &other, sizeof(mat));
}
Transform::Transform()
{

}
float& Transform::operator[](size_t idx)
{
	return mat[idx];
}
const float& Transform::operator[](size_t idx) const
{
	return mat[idx];
}
Transform& Transform::operator*=(const Transform& other)
{
	Transform result;

	size_t index = 0;
	for (size_t x = 0; x < 4; x++)
	{
		for (size_t y = 0; y < 4; y++)
		{
			result[index] = 0;
			size_t left = y;
			size_t top = x * 4;
			for (size_t i = 0; i < 4; i++)
			{
				result[index] += mat[left] * other.mat[top];
				left += 4, top++;
			}
			index++;
		}
	}

	return *this = result;
}
Transform Transform::operator*(const Transform& other) const
{
	Transform result;

	size_t index = 0;
	for (size_t x = 0; x < 4; x++)
	{
		for (size_t y = 0; y < 4; y++)
		{
			result[index] = 0;
			size_t left = y;
			size_t top = x * 4;
			for (size_t i = 0; i < 4; i++)
			{
				result[index] += mat[left] * other.mat[top];
				left += 4, top++;
			}
			index++;
		}
	}

	return result;
}
void Transform::ScaleTransform(const Vector3& scale)
{
	Transform factor;
	factor[0] = scale.x;
	factor[5] = scale.y;
	factor[10] = scale.z;
	*this *= factor;
}
void Transform::SetIdentity()
{
	Utility::MemsetZero(mat);
	mat[0] = 1.0f;
	mat[5] = 1.0f;
	mat[10] = 1.0f;
	mat[15] = 1.0f;
}
Transform Transform::Translation(const Vector3& pos)
{
	Transform ret;
	ret.mat[12] = pos.x;
	ret.mat[13] = pos.y;
	ret.mat[14] = pos.z;
	return ret;
}
Transform Transform::Rotation(const Vector3& euler)
{
	Transform ret;
	float pitch = -(euler.x * Math::degToRad);
	float yaw = -(euler.y * Math::degToRad);
	float roll = -(euler.z * Math::degToRad);
	const float a = cos(pitch);
	const float b = sin(pitch);
	const float c = cos(yaw);
	const float d = sin(yaw);
	const float e = cos(roll);
	const float f = sin(roll);
	const float ad = a * d;
	const float bd = b * d;

	ret.mat[0] = c * e;
	ret.mat[1] = -c * f;
	ret.mat[2] = d;
	ret.mat[3] = 0;
	ret.mat[4] = bd * e + a * f;
	ret.mat[5] = -bd * f + a * e;
	ret.mat[6] = -b * c;
	ret.mat[7] = 0;
	ret.mat[8] = -ad * e + b * f;
	ret.mat[9] = ad * f + b * e;
	ret.mat[10] = a * c;
	ret.mat[11] = 0;
	ret.mat[12] = 0;
	ret.mat[13] = 0;
	ret.mat[14] = 0;
	ret.mat[15] = 1;
	return ret;
}
Transform Transform::Scale(const Vector3& scale)
{
	Transform ret;
	ret.mat[0] = scale.x;
	ret.mat[5] = scale.y;
	ret.mat[10] = scale.z;
	return ret;
}
Vector3 Transform::GetPosition() const
{
	return Vector3(this->mat[12], this->mat[13], this->mat[14]);
}
Vector3 Transform::GetScale() const
{
	return Vector3(
		sqrt(this->mat[0] * this->mat[0] + this->mat[1] * this->mat[1] + this->mat[2] * this->mat[2]),
		sqrt(this->mat[4] * this->mat[4] + this->mat[5] * this->mat[5] + this->mat[6] * this->mat[6]),
		sqrt(this->mat[8] * this->mat[8] + this->mat[9] * this->mat[9] + this->mat[10] * this->mat[10])
	);
}
Vector3 Transform::GetEuler() const
{
	Transform copy = *this;
	const Vector3 currentScale = copy.GetScale();
	copy.ScaleTransform({ 1 / currentScale.x, 1 / currentScale.y, 1 / currentScale.z });

	Vector3 euler;

	euler.y = asin(copy[2]);
	const float cosine = cos(euler.y);

	float n1, n2;
	if (fabs(cosine) > float(0.0005))
	{
		n1 = copy[10] / cosine;
		n2 = -copy[6] / cosine;
		euler.x = atan2(n2, n1);
		n1 = copy[0] / cosine;
		n2 = -copy[1] / cosine;
		euler.z = atan2(n2, n1);
	}
	else
	{
		n1 = copy[5];
		n2 = copy[4];
		euler.x = 0;
		euler.z = atan2(n2, n1);
	}

	euler.x = (euler.x * Math::radToDeg);
	euler.y = (euler.y * Math::radToDeg);
	euler.z = (euler.z * Math::radToDeg);

	return euler;
}
Vector3 Transform::GetForward() const
{
	return Vector3(this->mat[8], this->mat[9], this->mat[10]).Normalized();
}
Vector3 Transform::GetUp() const
{
	return Vector3(this->mat[4], this->mat[5], this->mat[6]).Normalized();
}
Vector3 Transform::GetRight() const
{
	return Vector3(this->mat[0], this->mat[1], this->mat[2]).Normalized();
}
Vector3 Transform::TransformPoint(const Vector3& position) const
{
	const float w = this->mat[3] * position.x + this->mat[7] * position.y + this->mat[11] * position.z + this->mat[15];
	return Vector3(
		this->mat[0] * position.x + this->mat[4] * position.y + this->mat[8] * position.z + this->mat[12],
		this->mat[1] * position.x + this->mat[5] * position.y + this->mat[9] * position.z + this->mat[13],
		this->mat[2] * position.x + this->mat[6] * position.y + this->mat[10] * position.z + this->mat[14]
	) / w;
}
Vector3 Transform::TransformDirection(const Vector3& direction) const
{
	return Vector3(
		this->mat[0] * direction.x + this->mat[4] * direction.y + this->mat[8] * direction.z,
		this->mat[1] * direction.x + this->mat[5] * direction.y + this->mat[9] * direction.z,
		this->mat[2] * direction.x + this->mat[6] * direction.y + this->mat[10] * direction.z
	);
}

Transform Transform::FromAxes(Vector3 bitangent, Vector3 tangent, Vector3 normal)
{
	Transform ret;

	ret[0] = bitangent.x;
	ret[4] = bitangent.y;
	ret[8] = bitangent.z;

	ret[1] = tangent.x;
	ret[5] = tangent.y;
	ret[9] = tangent.z;

	ret[2] = normal.x;
	ret[6] = normal.y;
	ret[10] = normal.z;

	return ret;
}

Transform Transform::LookAt(const Vector3& position, const Vector3& target, const Vector3& up /*= Vector3(0, 1, 0)*/)
{
	Transform ret;

	const Vector3 z_axis = (target - position).Normalized();
	const Vector3 x_axis = VectorMath::Cross(up, z_axis).Normalized();
	const Vector3 y_axis = VectorMath::Cross(z_axis, x_axis);

	ret.mat[0] = x_axis.x;
	ret.mat[1] = x_axis.y;
	ret.mat[2] = x_axis.z;
	ret.mat[4] = y_axis.x;
	ret.mat[5] = y_axis.y;
	ret.mat[6] = y_axis.z;
	ret.mat[8] = z_axis.x;
	ret.mat[9] = z_axis.y;
	ret.mat[10] = z_axis.z;
	ret.mat[12] = position.x;
	ret.mat[13] = position.y;
	ret.mat[14] = position.z;
	return ret;
}

Transform CameraMatrix::BillboardMatrix(const Transform& matrix)
{
	Vector3 rot = matrix.GetEuler();
	Transform ret;
	ret *= Transform::Rotation(rot);
	return ret;
}
