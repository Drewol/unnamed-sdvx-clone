///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                           //
//  Copyright (c) 2012-2015, Jan de Graaf (jan@jlib.nl)                                                      //
//                                                                                                           //
//  Permission to use, copy, modify, and/or distribute this software for any purpose with or                 //
//  without fee is hereby granted, provided that the above copyright notice and this permission              //
//  notice appear in all copies.                                                                             //
//                                                                                                           //
//  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS             //
//  SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL              //
//  THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES          //
//  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE     //
//  OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  //
//                                                                                                           //
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Modified version of 
//	(http://jlib.nl/index.php?file=./code/include/jlib/transform) and
//	(http://jlib.nl/index.php?file=./code/include/jlib/matrix)

#pragma once
#include "Shared/Utility.hpp"
#include "Shared/Math.hpp"
#include "Shared/VectorMath.hpp"
#include <initializer_list>

/*
	4x4 Transformation matrix class
*/
class Transform
{
public:
	Transform();
	Transform(const Transform& other);
	Transform(std::initializer_list<float> values);
	Transform& operator=(const Transform& right);

	const float& operator[](size_t idx) const;
	float& operator[](size_t idx);

	Transform operator*(const Transform& other) const;
	Transform& operator*=(const Transform& other);

	void ScaleTransform(const Vector3& scale);

	void SetIdentity();

	static Transform Translation(const Vector3& pos);
	static Transform Rotation(const Vector3& euler);
	static Transform Scale(const Vector3& scale);
	static Transform LookAt(const Vector3& position, const Vector3& target, const Vector3& up = Vector3(0, 1, 0));
	static Transform FromAxes(Vector3 bitangent, Vector3 tangent, Vector3 normal);

	Vector3 GetPosition() const;
	Vector3 GetScale() const;
	Vector3 GetEuler() const;

	Vector3 GetForward() const;
	Vector3 GetUp() const;
	Vector3 GetRight() const;

	Vector3 TransformPoint(const Vector3& position) const;
	Vector3 TransformDirection(const Vector3& direction) const;

	float mat[16] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1,
	};
};

namespace ProjectionMatrix
{
	Transform CreatePerspective(float field_of_view, float aspect_ratio, float z_near, float z_far);
	Transform CreateOrthographic(float left, float right, float bottom, float top, float z_near, float z_far);
}

namespace CameraMatrix
{
	// Inverts a matrix to create a camera matrix or the inverse camera matrix
	Transform BillboardMatrix(const Transform& matrix);
}
