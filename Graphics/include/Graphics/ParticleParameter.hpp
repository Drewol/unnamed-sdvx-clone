/*
This file contains various particle parameter types and template types that can be used to specify Contants, Ranges, Curves or Distributions
these can then be set in Particle Systems for various properties
*/
#pragma once
// Prevent macro conflicts with class members
#undef min
#undef max

namespace Graphics
{
	/* Base class for particle parameters */
	template<typename T>
	class IParticleParameter
	{
	public:
		virtual ~IParticleParameter() = default;
		// Used to initialize a starting attribute
		virtual T Init(float systemTime) { return Sample(systemTime); }
		// Used to process over lifetime events
		virtual T Sample(float duration) = 0;
		virtual T GetMax() = 0;
		virtual IParticleParameter<T>* Duplicate() const = 0;
	};

	// Macro for implementing the Duplicate() function
#define IMPLEMENT_DUPLICATE(__type, __self) IParticleParameter<__type>* Duplicate() const override { return new __self(*this); }

	/* A constant value at all times */
	template<typename T>
	class PPConstant : public IParticleParameter<T>
	{
	public:
		PPConstant(const T& val) : val(val) {};
		T Sample(float in) override
		{
			return val;
		}
		T GetMax() override
		{
			return val;
		}
		IMPLEMENT_DUPLICATE(T, PPConstant);
	private:
		T val;
	};

	/* A random value between A and B */
	template<typename T>
	class PPRandomRange : public IParticleParameter<T>
	{
	public:
		PPRandomRange(const T& min, const T& max) : min(min), max(max) { delta = max - min; };
		T Init(float systemTime) override
		{
			return Sample(Random::Float());
		}
		T Sample(float in) override
		{
			return (max - min) * in + min;
		}
		T GetMax() override
		{
			return Math::Max(max, min);
		}
		IMPLEMENT_DUPLICATE(T, PPRandomRange);
	private:
		T delta;
		T min, max;
	};

	/* A value that transitions from A to B over time */
	template<typename T>
	class PPRange : public IParticleParameter<T>
	{
	public:
		PPRange(const T& min, const T& max) : min(min), max(max) { delta = max - min; };
		T Sample(float in) override
		{
			return (max - min) * in + min;
		}
		T GetMax() override
		{
			return Math::Max(max, min);
		}
		IMPLEMENT_DUPLICATE(T, PPRange);
	private:
		T delta;
		T min, max;
	};

	/* A 2 point gradient with a fade in value */
	template<typename T>
	class PPRangeFadeIn : public IParticleParameter<T>
	{
	public:
		PPRangeFadeIn(const T& min, const T& max, const float fadeIn) : fadeIn(fadeIn), min(min), max(max)
		{
			delta = max - min;
			rangeOut = 1.0f - fadeIn;
		};
		T Sample(float in) override
		{
			if(in < fadeIn)
			{
				return min * (in / fadeIn);
			}
			else
			{
				return (in - fadeIn) / rangeOut * (max - min) + min;
			}
		}
		T GetMax() override
		{
			return Math::Max(max, min);
		}
		IMPLEMENT_DUPLICATE(T, PPRangeFadeIn);
	private:
		float rangeOut;
		float fadeIn;
		T delta;
		T min, max;
	};

	/* A random sphere particle parameter */
	class PPSphere : public IParticleParameter<Vector3>
	{
	public:
		PPSphere(float radius) : radius(radius)
		{
		}
		Vector3 Sample(float in) override
		{
			return Vector3(Random::FloatRange(-1.0f, 1.0f), Random::FloatRange(-1.0f, 1.0f), Random::FloatRange(-1.0f, 1.0f)) * radius;
		}
		Vector3 GetMax() override
		{
			return Vector3(radius);
		}
		IMPLEMENT_DUPLICATE(Vector3, PPSphere);
	private:
		float radius;
	};

	/* A centered random box particle parameter with a size */
	class PPBox : public IParticleParameter<Vector3>
	{
	public:
		PPBox(Vector3 size) : size(size)
		{
		}
		Vector3 Sample(float in) override
		{
			Vector3 offset = -size * 0.5f;
			offset.x += Random::Float() * size.x;
			offset.y += Random::Float() * size.y;
			offset.z += Random::Float() * size.z;
			return offset;
		}
		Vector3 GetMax() override
		{
			return size;
		}
		IMPLEMENT_DUPLICATE(Vector3, PPBox);
	private:
		Vector3 size;
	};

	/*
	Cone directional particle parameter
	Cone angle is in degrees
	*/
	class PPCone : public IParticleParameter<Vector3>
	{
	private:
		float lengthMin, lengthMax;
		float angle;
		Transform mat;
	public:
		PPCone(Vector3 dir, float angle, float lengthMin, float lengthMax)
			: lengthMin(lengthMin), lengthMax(lengthMax), angle(angle * Math::degToRad)
		{
			Vector3 normal = dir.Normalized();
			Vector3 tangent = Vector3(normal.y, -normal.x, normal.z);
			if(normal.x == 0 && normal.y == 0)
				tangent = Vector3(normal.z, normal.y, -normal.x);
			tangent = VectorMath::Cross(tangent, normal).Normalized();
			Vector3 bitangent = VectorMath::Cross(tangent, normal);
			mat = Transform::FromAxes(bitangent, tangent, normal);
		}
		virtual Vector3 Sample(float in) override
		{
			float length = Random::FloatRange(lengthMin, lengthMax);

			float a = Random::FloatRange(-1, 1);
			float b = Random::FloatRange(-1, 1);
			float c = Random::FloatRange(-1, 1);
			a = a * a * Math::Sign(a);
			b = b * b * Math::Sign(b);
			c = c * c * Math::Sign(c);

			Vector3 v = Vector3(
				-sin(a * angle),
				sin(b * angle),
				cos(c * angle)
			);
			v = v.Normalized();

			v = mat.TransformDirection(v);
			v *= length;
			return v;
		}
		Vector3 GetMax() override
		{
			return Vector3(0, 0, lengthMax);
		}
		IMPLEMENT_DUPLICATE(Vector3, PPCone);

	};

#undef IMPLEMENT_DUPLICATE
}