#pragma once

#include <reflect/reflect.h>

namespace idk::reflect
{

	struct dynamic::base
	{
		virtual void* get() const = 0;
		virtual dynamic copy() const = 0;
		virtual ~base() {}
	};

	template<typename T>
	struct dynamic::derived : dynamic::base
	{
		T obj;
		using DecayedT = std::decay_t<T>;

		template<typename U>
		derived(U&& obj)
			: obj{ std::forward<U>(obj) }
		{}

		void* get() const override
		{
			return const_cast<void*>(static_cast<const void*>(&obj));
		}

		dynamic copy() const override
		{
            if constexpr (!std::is_copy_constructible_v<std::decay_t<T>>)
                throw "object is not copy constructible!";
            else
    			return dynamic(static_cast<std::decay_t<T>>(obj));
		}
	};

	struct dynamic::voidptr : dynamic::base
	{
		reflect::type t;
		void* obj;

		voidptr(reflect::type t, void* obj)
			: t{ t }, obj{ obj }
		{}

		void* get() const override
		{
			return obj;
		}

		dynamic copy() const override
		{
			return t._context->copy_construct(obj);
		}
	};

	class dynamic::property_iterator
	{
	public:
		property_iterator(const dynamic& obj, size_t index = 0);
		property_iterator& operator++(); //prefix increment
		bool operator==(const property_iterator&);
		bool operator!=(const property_iterator&);
		property operator*() const;

	private:
		const dynamic& obj;
		size_t index;
	};

	template<typename T, typename>
	dynamic::dynamic(T&& obj)
		: type{ get_type<T>() }, _ptr{ std::make_shared<derived<T>>(std::forward<T>(obj)) }
	{}

	template<typename T, typename>
	dynamic& dynamic::operator=(T&& rhs)
	{
        if (is<T>())
            type._context->copy_assign(_ptr->get(), &rhs);
        else
        {
            if constexpr (std::is_same_v<std::decay_t<T>, string>)
            {
                if (type.is_basic_serializable())
                {
                    const auto dyn = type._context->string_construct(rhs);
                    type._context->copy_assign(_ptr->get(), dyn._ptr->get());
                }
                else
                {
                    if constexpr (is_variant_member_v<T, ReflectedTypes>)
                        type._context->variant_assign(_ptr->get(), rhs);
                    else
                        throw "Invalid assignment!";
                }
            }
            else if constexpr (is_variant_member_v<T, ReflectedTypes>)
                type._context->variant_assign(_ptr->get(), rhs);
            else
                throw "Invalid assignment!";
        }

		return *this;
	}

	template<typename T>
	bool dynamic::is() const
	{
		return type.hash() == typehash<T>();
	}

	template<typename T>
	T& dynamic::get() const
	{
		return *static_cast<T*>(_ptr->get());
	}

	// recursively visit all members
	// visitor must be a function with signature:
	//  (auto&& key, auto&& value, int depth_change) -> bool/void
	// 
	// key:
	//     name of property (const char*), or
	//     container key when visiting container elements ( K = std::decay_t<decltype(key)> )
	//     for sequential containers, it will be size_t. for associative, it will be type K
	//     for held object of variants, it will be the held type (as a reflect::type)
	// value:
	//     the value, use T = std::decay_t<decltype(value)> to get the type
	// depth_change: (int)
	//     change in depth. -1 (up a level), 0 (stay same level), or 1 (down a level)
	// 
	// return false to stop recursion. if function doesn't return, it always recurses
	template<typename Visitor>
	void dynamic::visit(Visitor&& visitor) const
	{
		int depth = 0;
		int last_visit_depth = -1;
		detail::visit(_ptr->get(), type, std::forward<Visitor>(visitor), depth, last_visit_depth);
	}

}