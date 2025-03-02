#pragma once

#include <idk.h>
#include <ds/result.h>
#include <serialize/parse_error.h>

namespace idk
{

    // forward decls
    namespace reflect { class dynamic; class type; }
    class Scene;



    template<typename T>
    string serialize_binary(const T& obj);

    template<> // serialize scene
    string serialize_binary(const Scene& scene);



    template<typename T>
    monadic::result<T, parse_error> parse_binary(string_view sv);

    template<typename T>
    parse_error parse_binary(string_view sv, T& obj);

    template<> // parse scene
    parse_error parse_binary(string_view sv, Scene& scene);

    monadic::result<reflect::dynamic, parse_error> parse_binary(string_view sv, reflect::type type);

}

