#pragma once

// MW @todo: Rewrite everything!
// Super messy, could be written better from scratch.
// Use handles instead of searching through array
// Optimisation: lots of dynamic allocations

template <typename ReturnType, typename... ArgTypes>
class IDelegate
{
public:
	virtual ~IDelegate() = default;

	virtual ReturnType Execute(ArgTypes &&... args) = 0;
};

template <typename ReturnType, typename... ArgTypes>
class StaticDelegate : public IDelegate<ReturnType, ArgTypes...>
{
public:
	~StaticDelegate() override = default;

	using FunctionType = ReturnType(*)(ArgTypes...);

	explicit StaticDelegate(FunctionType function)
		: m_Function(function)
	{
	}

	ReturnType Execute(ArgTypes &&... args) override
	{
		return m_Function(std::forward<ArgTypes>(args)...);
	}

	NODISCARD FORCEINLINE FunctionType GetFunction() { return m_Function; }

private:
	FunctionType m_Function;
};

template <typename Lambda, typename ReturnType, typename... ArgTypes>
class LambdaDelegate : public IDelegate<ReturnType, ArgTypes...>
{
public:
	~LambdaDelegate() override = default;

	explicit LambdaDelegate(Lambda &&lambda)
		: m_Lambda(std::forward<Lambda>(lambda))
	{
	}

	ReturnType Execute(ArgTypes &&... args) override
	{
		return m_Lambda(std::forward<ArgTypes>(args)...);
	}

	NODISCARD FORCEINLINE Lambda GetLambda() { return m_Lambda; }

private:
	Lambda m_Lambda;
};

template <typename Class, typename ReturnType, typename... ArgTypes>
class MethodDelegate : public IDelegate<ReturnType, ArgTypes...>
{
public:
	~MethodDelegate() override = default;

	using MethodType = ReturnType(Class::*)(ArgTypes...);

	explicit MethodDelegate(Class *object, MethodType method)
		: m_Object(object), m_Method(method)
	{
	}

	ReturnType Execute(ArgTypes &&... args) override
	{
		return (m_Object->*m_Method)(std::forward<ArgTypes>(args)...);
	}

	NODISCARD FORCEINLINE Class* GetObject() { return m_Object; }
	NODISCARD FORCEINLINE MethodType GetMethod() { return m_Method; }

private:
	Class *    m_Object;
	MethodType m_Method;
};

template <typename ReturnType, typename... ArgTypes>
class Delegate
{
public:
	using DelegateType = IDelegate<ReturnType, ArgTypes...>;

	FORCEINLINE void Bind(Ref<DelegateType> delegate)
	{
		m_Delegate = delegate;
	}

	FORCEINLINE void BindLambda(auto &&lambda)
	{
		Bind(CreateRef<LambdaDelegate<decltype(lambda), ReturnType, ArgTypes...>>(
			std::forward<decltype(lambda)>(lambda)));
	}

	FORCEINLINE void BindStatic(ReturnType (*function)(ArgTypes...))
	{
		Bind(CreateRef<StaticDelegate<ReturnType, ArgTypes...>>(function));
	}

	template <typename T>
	FORCEINLINE void BindMethod(T *object, auto method)
	{
		Bind(CreateRef<MethodDelegate<T, ReturnType, ArgTypes...>>(object, method));
	}

	void Unbind()
	{
		m_Delegate = nullptr;
	}

	FORCEINLINE ReturnType Execute(ArgTypes &&... args)
	{
		PAPI_ASSERT(m_Delegate != nullptr);
		return m_Delegate->Execute(std::forward<ArgTypes>(args)...);
	}

	FORCEINLINE ReturnType ExecuteIfBound(ArgTypes &&... args)
	{
		if (m_Delegate != nullptr)
			return m_Delegate->Execute(std::forward<ArgTypes>(args)...);
		return ReturnType();
	}

	NODISCARD FORCEINLINE bool IsBound() { return m_Delegate != nullptr; }

private:
	Ref<DelegateType> m_Delegate = nullptr;
};

template <typename... ArgTypes>
class MulticastDelegate
{
public:
	using DelegateType = IDelegate<void, ArgTypes...>;

	void Bind(Ref<DelegateType> delegate)
	{
		m_Delegates.push_back(delegate);
	}

	void Unbind(Ref<DelegateType> delegate)
	{
		auto it = std::find(m_Delegates.begin(), m_Delegates.end(), delegate);
		if (it != m_Delegates.end())
			m_Delegates.erase(it);
	}

	void BindStatic(void (*function)(ArgTypes...))
	{
		Bind(CreateRef<StaticDelegate<void, ArgTypes...>>(function));
	}

	void UnbindStatic(void (*function)(ArgTypes...))
	{
		auto it = std::find_if(m_Delegates.begin(), m_Delegates.end(), [&](const Ref<DelegateType> &delegate)
		{
			auto staticDelegate = std::dynamic_pointer_cast<StaticDelegate<void, ArgTypes...>>(delegate);
			return staticDelegate && staticDelegate->GetFunction() == function;
		});
		if (it != m_Delegates.end())
			m_Delegates.erase(it);
	}

	void BindLambda(auto &&lambda)
	{
		Bind(CreateRef<LambdaDelegate<decltype(lambda), void, ArgTypes...>>(std::forward<decltype(lambda)>(lambda)));
	}

	void UnbindLambda(auto &&lambda)
	{
		auto it = std::find_if(m_Delegates.begin(), m_Delegates.end(), [&](const Ref<DelegateType> &delegate)
		{
			auto lambdaDelegate = std::dynamic_pointer_cast<LambdaDelegate<
				decltype(lambda), void, ArgTypes...>>(delegate);
			return lambdaDelegate && lambdaDelegate->GetLambda() == lambda;
		});
		if (it != m_Delegates.end())
			m_Delegates.erase(it);
	}

	template <typename T>
	void BindMethod(T *object, auto method)
	{
		Bind(CreateRef<MethodDelegate<T, void, ArgTypes...>>(object, method));
	}

	template <typename T>
	void UnbindMethod(T *object, auto method)
	{
		auto it = std::find_if(m_Delegates.begin(), m_Delegates.end(), [&](const Ref<DelegateType> &delegate)
		{
			auto methodDelegate = std::dynamic_pointer_cast<MethodDelegate<T, void, ArgTypes...>>(delegate);
			return methodDelegate && methodDelegate->GetObject() == object && methodDelegate->GetMethod() == method;
		});
		if (it != m_Delegates.end())
			m_Delegates.erase(it);
	}

	void UnbindAll()
	{
		m_Delegates.clear();
	}

	void Execute(ArgTypes &&... args)
	{
		for (auto &delegate : m_Delegates)
			delegate->Execute(std::forward<ArgTypes>(args)...);
	}

private:
	std::vector<Ref<DelegateType>> m_Delegates;
};

// MW @copypaste: most of the code is the same as MulticastDelegate, but with a different return type and Execute logic.
template <bool ContinueIf = true, typename... ArgTypes>
class CascadingMulticastDelegate
{
public:
	using DelegateType = IDelegate<bool, ArgTypes...>;

	void Bind(Ref<DelegateType> delegate)
	{
		m_Delegates.push_back(delegate);
	}

	void Unbind(Ref<DelegateType> delegate)
	{
		auto it = std::find(m_Delegates.begin(), m_Delegates.end(), delegate);
		if (it != m_Delegates.end())
			m_Delegates.erase(it);
	}

	void BindStatic(bool (*function)(ArgTypes...))
	{
		Bind(CreateRef<StaticDelegate<bool, ArgTypes...>>(function));
	}

	void UnbindStatic(bool (*function)(ArgTypes...))
	{
		auto it = std::find_if(m_Delegates.begin(), m_Delegates.end(), [&](const Ref<DelegateType> &delegate)
		{
			auto staticDelegate = std::dynamic_pointer_cast<StaticDelegate<bool, ArgTypes...>>(delegate);
			return staticDelegate && staticDelegate->GetFunction() == function;
		});
		if (it != m_Delegates.end())
			m_Delegates.erase(it);
	}

	void BindLambda(auto &&lambda)
	{
		Bind(CreateRef<LambdaDelegate<decltype(lambda), bool, ArgTypes...>>(std::forward<decltype(lambda)>(lambda)));
	}

	void UnbindLambda(auto &&lambda)
	{
		auto it = std::find_if(m_Delegates.begin(), m_Delegates.end(), [&](const Ref<DelegateType> &delegate)
		{
			auto lambdaDelegate = std::dynamic_pointer_cast<LambdaDelegate<
				decltype(lambda), bool, ArgTypes...>>(delegate);
			return lambdaDelegate && lambdaDelegate->GetLambda() == lambda;
		});
		if (it != m_Delegates.end())
			m_Delegates.erase(it);
	}

	template <typename T>
	void BindMethod(T *object, auto method)
	{
		Bind(CreateRef<MethodDelegate<T, bool, ArgTypes...>>(object, method));
	}

	template <typename T>
	void UnbindMethod(T *object, auto method)
	{
		auto it = std::find_if(m_Delegates.begin(), m_Delegates.end(), [&](const Ref<DelegateType> &delegate)
		{
			auto methodDelegate = std::dynamic_pointer_cast<MethodDelegate<T, bool, ArgTypes...>>(delegate);
			return methodDelegate && methodDelegate->GetObject() == object && methodDelegate->GetMethod() == method;
		});
		if (it != m_Delegates.end())
			m_Delegates.erase(it);
	}

	void UnbindAll()
	{
		m_Delegates.clear();
	}

	bool Execute(ArgTypes &&... args)
	{
		for (auto &delegate : m_Delegates)
		{
			if (delegate->Execute(std::forward<ArgTypes>(args)...) != ContinueIf)
				return false;
		}
		return true;
	}

private:
	std::vector<Ref<DelegateType>> m_Delegates;
};
