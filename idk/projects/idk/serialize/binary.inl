#pragma once

#include "binary.h"
#include <res/Guid.h>

namespace idk
{
    namespace detail
    {
        template<bool IsBigEndian>
        struct binary_serializer
        {
            string output;

            template<typename T>
            void operator<<(const T& obj)
            {
                if constexpr (std::is_arithmetic_v<std::decay_t<T>>)
                {
                    if constexpr (sizeof(T) == 1)
                        output += obj;
                    else // handle endianness
                    {
                        if constexpr (IsBigEndian) // reverse
                        {
                            const char* end = reinterpret_cast<const char*>(&obj) - 1;
                            const char* begin = end + sizeof(obj);
                            while (begin != end)
                                output += *begin--;
                        }
                        else
                            output += string_view(reinterpret_cast<const char*>(&obj), sizeof(obj));
                    }
                }
                else if constexpr (std::is_same_v<std::decay_t<T>, string> || std::is_same_v<std::decay_t<T>, string_view>)
                {
                    *this << obj.size();
                    output += obj;
                }
                else if constexpr (std::is_same_v<std::decay_t<T>, const char*>)
                {
                    *this << strlen(obj);
                    output += obj;
                }
                else if constexpr (std::is_same_v<std::decay_t<T>, Guid>)
                {
                    *this << obj.Data1;
                    *this << obj.Data2;
                    *this << obj.Data3;
                    *this << obj.Data4[0];
                    *this << obj.Data4[1];
                    *this << obj.Data4[2];
                    *this << obj.Data4[3];
                    *this << obj.Data4[4];
                    *this << obj.Data4[5];
                    *this << obj.Data4[6];
                    *this << obj.Data4[7];
                }
                else if constexpr (is_basic_serializable_v<T>)
                {
                    string str(obj);
                    *this << str.size();
                    output += str;
                }
                else if constexpr (is_container_v<T>)
                {
                    *this << std::size(obj);
                    for (const auto& elem : obj)
                    {
                        if constexpr (std::is_same_v<std::decay_t<T>::value_type, reflect::dynamic>)
                        {
                            if (elem.valid())
                                output += elem.type.name();
                            output += '\0';
                        }
                        *this << elem;
                    }
                }
                else if constexpr (is_template_v<std::decay_t<T>, std::pair>)
                {
                    *this << obj.first;
                    if constexpr (std::is_same_v<std::decay_t<T>::second_type, reflect::dynamic>)
                    {
                        if (obj.second.valid())
                            output += obj.second.name();
                        output += '\0';
                    }
                    *this << obj.second;
                }
                else
                    *this << reflect::dynamic{ obj };
            } // operator>>
        }; // struct binary_serializer

        template<>
        template<>
        void detail::binary_serializer<true>::operator<<(const reflect::dynamic& obj);
        template<>
        template<>
        void detail::binary_serializer<false>::operator<<(const reflect::dynamic& obj);



        template <typename T, typename = void>
        struct has_resize : std::false_type {};
        template <typename T>
        struct has_resize<T, std::void_t<decltype(std::declval<T>().clear())>> : std::true_type {};


        template<bool IsBigEndian>
        struct binary_parser
        {
            const char* begin;
            const char* const end;
            parse_error error = parse_error::none;

            template<typename T>
            void operator>>(T& obj)
            {
                if constexpr (std::is_arithmetic_v<std::decay_t<T>>)
                {
                    if constexpr (sizeof(T) == 1)
                        obj = *reinterpret_cast<const T*>(begin++);
                    else // handle endianness
                    {
                        if constexpr (IsBigEndian) // reverse
                        {
                            std::reverse_copy(begin, begin + sizeof(obj), reinterpret_cast<char*>(&obj));
                            begin += sizeof(obj);
                        }
                        else
                        {
                            std::copy(begin, begin + sizeof(obj), reinterpret_cast<char*>(&obj));
                            begin += sizeof(obj);
                        }
                    }
                }
                else if constexpr (std::is_same_v<std::decay_t<T>, string>)
                {
                    size_t sz = 0;
                    *this >> sz;
                    obj = string(begin, begin + sz);
                    begin += sz;
                }
                else if constexpr (is_macro_enum_v<T>)
                {
                    size_t sz;
                    *this >> sz;
                    obj = std::decay_t<T>::from_string(string_view{ begin, sz });
                    begin += sz;
                }
                else if constexpr (std::is_same_v<std::decay_t<T>, Guid>)
                {
                    *this >> obj.Data1;
                    *this >> obj.Data2;
                    *this >> obj.Data3;
                    *this >> obj.Data4[0];
                    *this >> obj.Data4[1];
                    *this >> obj.Data4[2];
                    *this >> obj.Data4[3];
                    *this >> obj.Data4[4];
                    *this >> obj.Data4[5];
                    *this >> obj.Data4[6];
                    *this >> obj.Data4[7];
                }
                else if constexpr (is_basic_serializable_v<T>)
                {
                    string str;
                    *this >> str;
                    obj = T(str);
                }
                else if constexpr (is_container_v<T>)
                {
                    size_t sz = 0;
                    *this >> sz;

                    if constexpr (has_resize<std::decay_t<T>>::value)
                        obj.clear();

                    for (size_t i = 0; i < sz; ++i)
                    {
                        if constexpr (!has_resize<std::decay_t<T>>::value)
                        {
                            *this >> obj[i];
                        }
                        else if constexpr (is_template_v<std::decay_t<T>::value_type, std::pair>)
                        {
                            typename std::decay_t<T>::key_type key;
                            typename std::decay_t<T>::mapped_type val;
                            *this >> key;
                            *this >> val;
                            if constexpr (is_sequential_container_v<T>)
                                obj.emplace_back(std::make_pair(key, val));
                            else if constexpr (is_associative_container_v<T>)
                                obj.emplace(key, val);
                        }
                        else
                        {
                            typename std::decay_t<T>::value_type val;
                            *this >> val;
                            if constexpr (is_sequential_container_v<T>)
                                obj.emplace_back(val);
                            else if constexpr (is_associative_container_v<T>)
                                obj.emplace(val);
                        }
                    }
                }
                else
                {
                    reflect::dynamic dyn{ obj };
                    *this >> dyn;
                }

                if (begin > end)
                    error = parse_error::binary_ill_formed;
            } // operator>>
        }; // struct binary_parser

        template<>
        template<>
        void detail::binary_parser<true>::operator>>(reflect::dynamic& obj);
        template<>
        template<>
        void detail::binary_parser<false>::operator>>(reflect::dynamic& obj);
    }



    template<typename T>
    string serialize_binary(const T& obj)
    {
        uint32_t num = 1;
        if (*(char*)&num == 1) // little endian
        {
            detail::binary_serializer<false> s;
            s << obj;
            return s.output;
        }
        else // big endian
        {
            detail::binary_serializer<true> s;
            s << obj;
            return s.output;
        }
    }

    template<typename T>
    monadic::result<T, parse_error> parse_binary(string_view sv)
    {
        T obj;
        const auto err = parse_binary(sv, obj);
        if (err != parse_error::none)
            return err;
        return obj;
    }

    template<typename T>
    parse_error parse_binary(string_view sv, T& obj)
    {
        uint32_t num = 1;
        if (*(char*)&num == 1) // little endian
        {
            detail::binary_parser<false> p{ sv.data(), sv.data() + sv.size() };
            p >> obj;
            return p.error;
        }
        else // big endian
        {
            detail::binary_parser<true> p{ sv.data(), sv.data() + sv.size() };
            p >> obj;
            return p.error;
        }
    }

}