/* Header  */

%{

#include <emscripten/bind.h>
#include <boost/optional.hpp>

using namespace emscripten;

namespace
{
// Base class of boost::optional is boost::optional_detail::optional_base which has a protected
// destructor and so can't be exposed to javascript (at least with current embind 1.21.0).
// To expose boost::optional_detail::optional_base methods we need to wrap them.
template <class T>
struct OptionalWrapper
{
    static bool isSet(boost::optional<T>& opt)
    {
        return opt.is_initialized();
    }

    static void destroy(boost::optional<T>& opt)
    {
        return opt.reset();
    }
};

template <typename T>
class_<boost::optional<T>> registerOptional(const char* name)
{
    return class_<boost::optional<T>>(name)
        .function("isSet", &OptionalWrapper<T>::isSet)
        .function("get", select_overload<T&(void)>(&boost::optional<T>::get))
        .function("destroy", &OptionalWrapper<T>::destroy);
}

#define CO_REGISTER_OPTIONAL(type, optionalTypeName)                                                                 \
    function("make_" #optionalTypeName, select_overload<boost::optional<type>(const type&)>(&boost::make_optional)); \
    registerOptional<type>(#optionalTypeName);

} // anon namespace

namespace emscripten_bindings {
%}

/* Footer  */

%init %{
    } // namespace emscripten_bindings    
%}

/* Register Optional Macro, works only for emscripten  */

%define EMSCRIPTEN_REGISTER_OPTIONAL(optCls,optName) 

%feature ("emscripten_register_optional")  ____regopt_ ## optName %{ CO_REGISTER_OPTIONAL (optCls,"optName"); %}
void  ____regopt_ ## optName ();

%enddef


/* Register Vector Macro, works only for emscripten  */

%define EMSCRIPTEN_REGISTER_VECTOR(type,vecName) 

%feature ("emscripten_register_vector")  ____regvec_ ## vecName %{ emscripten::register_vector<type>("vecName"); %}
void ____regvec_ ## vecName ();

/* Register Map Macro, works only for emscripten  */

%define EMSCRIPTEN_REGISTER_MAP(type1,type2,mapName) 

%feature ("emscripten_register_map")  ____regmap_ ## mapName %{ emscripten::register_map<type1, type2>("mapName"); %}
void ____regmap_ ## mapName ();

%enddef

