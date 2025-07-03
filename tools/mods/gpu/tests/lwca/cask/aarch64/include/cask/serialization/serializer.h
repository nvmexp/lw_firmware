#pragma once
#include <type_traits>
//#include <typeinfo>
#include <cask/serialization/stl_collections.h>

namespace cask {
namespace internal {
template <typename Archive, typename T>
struct serialize_primitive {
    static void invoke(Archive& ar, const T& t) {
        ar.save(t);
    }
};

template <typename Archive, typename T>
struct serialize_array {
    static void invoke(Archive& ar, const T& t) {
        ar.save(t, sizeof(T));
    }
};

template <typename Archive, typename T>
struct serialize_collection {
    static void invoke(Archive& ar, const T& t) {
        save(ar, t);
    }
};

template <typename Archive, typename T>
struct deserialize_primitive {
    static void invoke(Archive& ar, T& t) {
        ar.load(t);
    }
};

template <typename Archive, typename T>
struct deserialize_array {
    static void invoke(Archive& ar, T& t) {
        ar.load(t, sizeof(T));
    }
};

template <typename Archive, typename T>
struct deserialize_collection {
    static void invoke(Archive& ar, T& t) {
        load(ar, t);
    }
};

template<typename Archive, typename T>
inline void save(Archive& ar, const T& t) {
    using typex = typename std::conditional<std::is_scalar<T>::value || std::is_same<T, std::string>::value,
        serialize_primitive<Archive, T>,
        typename std::conditional<std::is_array<T>::value, serialize_array<Archive, T>,
                                                           serialize_collection<Archive, T>>::type>::type;

    typex::invoke(ar, t);
    return;
}

template<typename Archive, typename T>
inline void load(Archive& ar, T& t) {
    using typex = typename std::conditional<std::is_scalar<T>::value || std::is_same<T, std::string>::value,
        deserialize_primitive<Archive, T>,
        typename std::conditional<std::is_array<T>::value, deserialize_array<Archive, T>,
                                                           deserialize_collection<Archive, T>>::type>::type;

    typex::invoke(ar, t);
}

} // namespace internal
} // namespace cask
