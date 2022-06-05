#pragma once
#include <memory>


template<typename T>
using Ref = std::shared_ptr<T>;
// class Ref : public std::shared_ptr<T> {
// public:
//     using std::shared_ptr<T>::get;
//     using std::shared_ptr<T>::reset;
//     using std::shared_ptr<T>::use_count;

//     inline bool IsValid() const {return get() != nullptr;} 

//     inline void Destroy()
// 	{
// 		assert(IsValid());
// 		reset();
// 	}

//     inline void Release()
//     {
//         if(use_count() < 2)
//             Destroy();
//     }

//     inline int GetRefCount() {return use_count();}

// };

namespace Utility
{
	template<typename T>
	Ref<T> MakeRef(T* obj)
	{
		Ref<T> ret = std::make_shared<T>();
		ret.reset(obj);
		return ret;
	}
	template<typename F, typename T>
	Ref<T> CastRef(const Ref<F>& a) {
		return std::dynamic_pointer_cast<T>(a);
	}
} // namespace Utility
