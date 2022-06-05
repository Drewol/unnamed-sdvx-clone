#pragma once
#include <type_traits>

namespace Graphics
{
	/*
		Description of a single element in a vertex type
	*/
	struct VertexFormatDesc
	{
		uint32 components;
		uint32 componentSize;
		bool isFloat;
		bool isSigned;
	};

	typedef Vector<VertexFormatDesc> VertexFormatList;

	/*
		Used By VertexFormat base class
		see below
	*/
	struct VertexFormats
	{
		// Vector formats
		template<typename VT, size_t N>
		static void AddFormat(VertexFormatList& dsc, VectorMath::VectorBase<VT, N> dummy)
		{
			VertexFormatDesc d;
			d.components = N;
			d.componentSize = sizeof(VT);
			d.isFloat = std::is_floating_point<VT>::value;
			d.isSigned = std::is_signed<VT>::value;
			dsc.push_back(d);
		}
		// Integer and float formats
		template<typename K>
		static void AddFormat(VertexFormatList& dsc, K dummy)
		{
			VertexFormatDesc d;
			d.components = 1;
			d.componentSize = sizeof(K);
			d.isFloat = std::is_floating_point<K>::value;
			d.isSigned = std::is_signed<K>::value;
			dsc.push_back(d);
		}

		template<size_t left, typename TFirst, typename... TRest>
		static void AddFormats(VertexFormatList& dsc)
		{
			AddFormat(dsc, TFirst());
			AddFormats<left - 1, TRest...>(dsc);
		}
	};

	template<> void VertexFormats::AddFormats<0, void>(VertexFormatList& dsc);

	/*
		Structure that generates a vertex format layout based on a variadic template argument list
		use this as a base class for a vertex structure and pass the data members of your structure as template arguments in order

		Example:
		struct Vert : public VertexFormat<Vector3, float, Vector4>
		{
			Vector3 pos;
			float scale;
			Vector4 color;
		};

		this adds a fuction Verts::GetDescriptors() which returns an array of VertexFormatDesc
	*/
	template<typename... T>
	struct VertexFormat
	{
		static Vector<VertexFormatDesc> GetDescriptors()
		{
			Vector<VertexFormatDesc> res;
			VertexFormats::AddFormats<sizeof...(T), T..., void>(res);
			return res;
		}
	private:

	};

}
