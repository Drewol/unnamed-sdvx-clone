#pragma once
#include "Shared/Bindable.hpp"

/*
	Action is an objects that holds the neccesary information to call a single static function / member function / lambda
*/
template<typename R = void, typename... A>
class Action : public Unique
{
public:
	Action() = default;
	Action(R(*staticFunction)(A...))
	{
		m_binding = new StaticBinding<R, A...>(staticFunction);
	}
	template<typename L>
	Action(L&& lambda)
	{
		m_binding = new LambdaBinding<L, R, A...>(std::forward<L>(lambda));
	}
	Action(Action&& other)
	{
		m_binding = other.m_binding;
		other.m_binding = nullptr;
	}
	Action& operator=(Action&& other)
	{
		Clear();
		m_binding = other.m_binding;
		other.m_binding = nullptr;
		return *this;
	}
	void Bind(R(*staticFunction)(A...))
	{
		Clear();
		m_binding = new StaticBinding<R, A...>(staticFunction);
	}
	template<typename T>
	void Bind(T* obj, R(T::* memberFunc)(A...))
	{
		Clear();
		m_binding = new ObjectBinding<T, R, A...>(obj, memberFunc);
	}
	template<typename L>
	void BindLambda(L&& lambda)
	{
		Clear();
		m_binding = new LambdaBinding<L, R, A...>(std::forward<L>(lambda));
	}
	~Action()
	{
		Clear();
	}
	R Call(A... args)
	{
		assert(IsBound());
		return m_binding->Call(args...);
	}
	bool IsBound() const
	{
		return m_binding != nullptr;
	}
	void Clear()
	{
		if (m_binding)
			delete m_binding;
		m_binding = nullptr;
	}

private:
	IFunctionBinding<R, A...>* m_binding = nullptr;
};

/*
	Bindable property
	This field either acts just as a normal variable or acts a a property using Get/Set methods to interface with the underlying value
*/
template<typename T>
class Property
{
public:
	Property() = default;
	Property(T val)
		: m_value(val)
	{
	}
	Property(Action<T> get, Action<void, T> set)
		: Get(move(get)), Set(move(set))
	{
	}

	// Get
	inline operator T() const
	{
		return m_Get();
	}
	// Set
	inline Property& operator=(const T& other)
	{
		m_Set(other);
		return *this;
	}

	Action<T> Get;
	Action<void, T> Set;
protected:
	inline T m_Get() const
	{
		if (Get.IsBound())
			return const_cast<Property*>(this)->Get.Call();
		else
			return m_value;
	}
	inline void m_Set(const T& val)
	{
		if (Set.IsBound())
			Set.Call(val);
		else
			m_value = val;
	}
	T m_value;
};
