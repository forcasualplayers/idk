#pragma once

#include "typed_context.h"
#include <serialize/text.inl>
#include <reflect/reflect.inl>
#include <ds/result.inl>

namespace idk::reflect::detail
{

	template <typename T>
	struct is_pair_assignable : std::false_type
	{};
	template <typename T, typename U>
	struct is_pair_assignable<std::pair<T, U>> : std::true_type
	{};
	template <typename T, typename U>
	struct is_pair_assignable<std::pair<const T, U>> : std::false_type
	{};
	template <typename T, typename U>
	struct is_pair_assignable<std::pair<T, const U>> : std::false_type
	{};
	template <typename T, typename U>
	struct is_pair_assignable<std::pair<const T, const U>> : std::false_type
	{};
	template <typename T>
	inline constexpr bool is_pair_assignable_v = is_pair_assignable<T>::value;

    template <typename T, typename = void>
    struct has_on_parse : std::false_type
    {};
    template <typename T>
    struct has_on_parse<T, std::void_t<decltype(std::declval<T>().on_parse())>> : std::true_type
    {};



	struct typed_context_base
	{
		const string_view name;
		const string_view fully_qualified_name;
		const detail::table& table;
		const span<constructor_entry_base* const> ctors;
		const size_t hash;
		const bool is_container;
        const bool is_enum_type;
        const bool is_basic_serializable;
        const bool in_mega_variant;

		typed_context_base(string_view name, string_view fully_qualified_name, const detail::table& table, span<constructor_entry_base* const> ctors, size_t hash,
                           bool is_container, bool is_enum_type, bool is_basic_serializable, bool in_mega_variant)
            : name{ name }, fully_qualified_name{ fully_qualified_name }, table{ table }, ctors{ ctors }, hash{ hash },
              is_container{ is_container }, is_enum_type{ is_enum_type }, is_basic_serializable{ is_basic_serializable },
              in_mega_variant{ in_mega_variant }
		{}
		virtual ~typed_context_base() = default;


		virtual const enum_type::data* get_enum_data() const = 0;


		virtual void copy_assign(void* lhs, const void* rhs) const = 0;
		virtual void variant_assign(void* lhs, const ReflectedTypes& rhs) const = 0;
		virtual void variant_assign(ReflectedTypes& lhs, const void* rhs) const = 0;
		virtual ReflectedTypes get_mega_variant(void* obj) const = 0;
		virtual dynamic default_construct() const = 0;
		virtual dynamic copy_construct(void* obj) const = 0;
		virtual dynamic string_construct(const string& str) const = 0;
		virtual string to_string(void* obj) const = 0;
		virtual uni_container to_container(void* obj) const = 0;
		virtual enum_value to_enum_value(void* obj) const = 0;
		virtual vector<dynamic> unpack(void* obj) const = 0;
		virtual dynamic get_variant_value(void* obj) const = 0;
		virtual void set_variant_value(void* obj, const dynamic& val) const = 0;
        virtual void on_parse(void* obj) const = 0;

		template<typename... Ts>
		dynamic construct(Ts&& ... args) const
		{
			// inputs should be all dynamics at this stage
			static_assert(std::conjunction_v<std::is_same<Ts, dynamic>...>);

			if constexpr (sizeof...(Ts) == 0)
				return default_construct();
			else
			{
				if constexpr (sizeof...(Ts) == 1)
				{
					if (((args.type._context == this) && ...))
						return copy_construct((args._ptr->get())...);
                    if (is_basic_serializable && ((args.type == get_type<string>()) && ...))
                        return string_construct(args.get<string>());
				}

				dynamic o;
				for (auto* c : ctors)
				{
					c->try_construct(&o, args...);
					if (o.valid()) break;
				}

				//assert(o.valid());
				return o;
			}
		}
	};

    template<typename T1, typename T2>
    static void _assign(T1& lhs, T2& rhs)
    {
        (lhs); (rhs);
        if constexpr (std::is_arithmetic_v<std::decay_t<T1>> && std::is_arithmetic_v<std::decay_t<T2>>)
            lhs = static_cast<T1>(rhs);
        else if constexpr (std::is_convertible_v<T2, T1>)
        {
            if constexpr (std::is_assignable_v<T1, T2>)
                lhs = rhs;
            else if constexpr (std::is_assignable_v<T1, T1>)
                lhs = T1(rhs);
            else
            {
                LOG_CRASH_TO(LogPool::SYS, "Cannot assign lhs = rhs");
                throw;
            }
        }
        else
        {
            LOG_CRASH_TO(LogPool::SYS, "Cannot assign lhs = rhs");
            throw;
        }
    }

	template<typename T>
	struct typed_context : typed_context_base
	{
		typed_context(string_view name, const detail::table& table, span<constructor_entry_base* const> ctors)
			: typed_context_base(name, fully_qualified_nameof<T>(), table, ctors, typehash<T>(),
                                 is_sequential_container_v<T> || is_associative_container_v<T>,
                                 is_macro_enum_v<T>,
                                 is_basic_serializable_v<T>,
                                 is_variant_member_v<std::decay_t<T>, ReflectedTypes>)
		{}
		typed_context()
			: typed_context(
				type_definition<T>::m_Name
				, type_definition<T>::m_Table
				, span<constructor_entry_base* const>{
						type_definition<T>::m_Storage.ctors.data(),
						type_definition<T>::m_Storage.ctors.data() + type_definition<T>::m_Storage.ctors.size() }
			)
		{}


		virtual const enum_type::data* get_enum_data() const override
		{
			if constexpr (is_macro_enum<T>::value)
			{
				constexpr static enum_type::data e{ sizeof(T::UnderlyingType), T::count, T::values, T::names };
				return &e;
			}
			else
				throw;
		}


		void copy_assign(void* lhs, const void* rhs) const override
		{
            lhs; rhs;
            if constexpr (is_template_v<T, std::pair> && !is_pair_assignable_v<T> || !std::is_copy_assignable_v<T>)
                throw "Cannot copy assign";
            else
                *static_cast<T*>(lhs) = *static_cast<const T*>(rhs);
		}

		virtual void variant_assign(void* lhs, const ReflectedTypes& rhs) const override
		{
			std::visit([&lhs](auto&& arg)
			{
                _assign(*static_cast<T*>(lhs), arg);
			}, rhs);
		}
        virtual void variant_assign(ReflectedTypes& lhs, const void* rhs) const override
        {
            std::visit([&rhs](auto&& arg)
            {
                _assign(arg, *static_cast<const T*>(rhs));
            }, lhs);
        }
		virtual ReflectedTypes get_mega_variant(void* obj) const override
		{
			obj;
			if constexpr (is_variant_member_v<std::decay_t<T>, ReflectedTypes>)
				return *static_cast<T*>(obj);
			else
				throw "Not part of idk::reflect::ReflectedTypes!";
		}

		dynamic default_construct() const override
		{
            if constexpr (is_macro_enum_v<T>)
                return T{ T::values[0] };
			else if constexpr (!std::is_default_constructible_v<T>)
				throw "Cannot default construct";
			else
				return T{};
		}

		dynamic copy_construct(void* obj) const override
		{
			obj;
			if constexpr (!std::is_copy_constructible_v<T>)
				throw "Cannot copy construct";
			else
				return T{ *static_cast<const T*>(obj) };
		}

        dynamic string_construct(const string& str) const override
        {
            str;
            if constexpr (!is_basic_serializable_v<T>)
                throw "Cannot string construct";
            else
                return std::move(*parse_text<T>(str));
        }

        string to_string(void* obj) const override
        {
            obj;
            if constexpr (is_basic_serializable_v<T> || std::is_same_v<T, const char*>)
                return serialize_text(*static_cast<T*>(obj));
            else
                throw "not serializable!";
        }

		uni_container to_container(void* obj) const override
		{
			obj;
			if constexpr (is_sequential_container_v<T> || is_associative_container_v<T>)
				return uni_container{ *static_cast<T*>(obj) };
			else
				throw "not a container!";
		}

		enum_value to_enum_value(void* obj) const override
		{
			obj;
			if constexpr (is_macro_enum_v<T>)
				return get_type<T>().as_enum_type().from_value(*static_cast<T*>(obj));
			else
				throw "not an enum!";
		}

		template<size_t... Is>
		void unpack_helper(vector<dynamic>& vec, void* obj, std::index_sequence<Is...>) const
		{
			(vec.push_back(std::get<Is>(*static_cast<T*>(obj))), ...);
		}

		vector<dynamic> unpack(void* obj) const override
		{
			obj;
			if constexpr (is_template_v<T, std::pair> || is_template_v<T, std::tuple>)
			{
				vector<dynamic> vec;
				unpack_helper(vec, obj, std::make_index_sequence<std::tuple_size<T>::value>());
				return vec;
			}
			else
				throw "not a tuple!";
		}

		template<typename... Ts>
		dynamic get_variant_value(variant<Ts...>& var) const
		{
#define VARIANT_CASE(I) case I: return std::get<I>(var);
#define VARIANT_SWITCH(N, ...) if constexpr (sizeof...(Ts) == N) { switch (var.index()) { IDENTITY(FOREACH(VARIANT_CASE, __VA_ARGS__)) default: throw "Unhandled case?"; } }
                 VARIANT_SWITCH(1, 0)
            else VARIANT_SWITCH(2, 1, 0)
            else VARIANT_SWITCH(3, 2, 1, 0)
            else VARIANT_SWITCH(4, 3, 2, 1, 0)
            else VARIANT_SWITCH(5, 4, 3, 2, 1, 0)
            else VARIANT_SWITCH(6, 5, 4, 3, 2, 1, 0)
            else VARIANT_SWITCH(7, 6, 5, 4, 3, 2, 1, 0)
            else VARIANT_SWITCH(8, 7, 6, 5, 4, 3, 2, 1, 0)
            else VARIANT_SWITCH(9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
            else VARIANT_SWITCH(10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
            else throw "Unhandled case?";
#undef VARIANT_SWITCH
#undef VARIANT_CASE
		}
        template<typename... Ts>
        void set_variant_value(variant<Ts...>& var, const dynamic& val) const
        {
#define VARIANT_CASE(I) case typehash<std::variant_alternative_t<I, variant<Ts...>>>(): var = val.get<std::variant_alternative_t<I, variant<Ts...>>>(); return;
#define VARIANT_SWITCH(N, ...) if constexpr (sizeof...(Ts) == N) { switch (val.type.hash()) { IDENTITY(FOREACH(VARIANT_CASE, __VA_ARGS__)) default: throw "Unhandled case?"; } }
                 VARIANT_SWITCH(1, 0)
            else VARIANT_SWITCH(2, 1, 0)
            else VARIANT_SWITCH(3, 2, 1, 0)
            else VARIANT_SWITCH(4, 3, 2, 1, 0)
            else VARIANT_SWITCH(5, 4, 3, 2, 1, 0)
            else VARIANT_SWITCH(6, 5, 4, 3, 2, 1, 0)
            else VARIANT_SWITCH(7, 6, 5, 4, 3, 2, 1, 0)
            else VARIANT_SWITCH(8, 7, 6, 5, 4, 3, 2, 1, 0)
            else VARIANT_SWITCH(9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
            else VARIANT_SWITCH(10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
            else throw "Unhandled case?";
#undef VARIANT_SWITCH
#undef VARIANT_CASE
        }

		virtual dynamic get_variant_value(void* obj) const override
		{
			obj;
			if constexpr (is_template_v<T, std::variant>)
				return get_variant_value(*static_cast<T*>(obj));
			else
				throw "not a variant!";
		}
        virtual void set_variant_value(void* obj, const dynamic& val) const override
        {
            (obj); (val);
            if constexpr (is_template_v<T, std::variant>)
                set_variant_value(*static_cast<T*>(obj), val);
            else
                throw "not a variant!";
        }

        virtual void on_parse(void* obj) const override
        {
            obj;
            if constexpr (has_on_parse<T>::value)
                return static_cast<T*>(obj)->on_parse();
        }

	};

	template<typename T>
	struct typed_context_nodef : typed_context<T>
	{
		detail::table empty_table{ 0, nullptr, nullptr, nullptr };
		typed_context_nodef()
			: typed_context<T>(
				fully_qualified_nameof<T>()
				, empty_table
				, span<constructor_entry_base* const>{ nullptr, nullptr }
			)
		{}
	};

}