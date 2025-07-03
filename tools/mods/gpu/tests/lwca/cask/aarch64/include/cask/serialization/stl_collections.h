#pragma once

#include <map>
#include <vector>
#include <utility>
#include <type_traits>

namespace cask {
namespace internal {
struct serialize_primitive_vector {
    template <class Archive, class Type, class Allocator>
    static void invoke(Archive& ar, const std::vector<Type, Allocator> &t) {
        std::size_t count = t.size() * sizeof(Type);
        ar.save(t.data(), count);
    }

};

struct serilize_general_vector {
    template <class Archive, class Type, class Allocator>
    static void invoke(Archive& ar, const std::vector<Type, Allocator> &t) {
        std::size_t count = t.size();
        ar.save(count);
        for (std::size_t i=0; i<count; ++i) {
            ar << t[i];
        }        
    }
};

struct deserialize_primitive_vector {
    template <class Archive, class Type, class Allocator>
    static void invoke(Archive& ar, std::vector<Type, Allocator> &t) {
        std::size_t size;
        ar.load(size);
        t.resize(size / sizeof(Type));
        ar.push_back(sizeof(std::size_t));
        ar.load(t.data(), size);
    }

};

struct deserilize_general_vector {
    template <class Archive, class Type, class Allocator>
    static void invoke(Archive& ar, std::vector<Type, Allocator> &t) {
        std::size_t count;
        ar.load(count);
        while(count-- > 0) {
            Type data;
            ar >> data;
            t.push_back(data);    
        }
    }
};

template <class Archive, class Type, class Allocator>
inline void save(Archive & ar, const std::vector<Type, Allocator> &t) {
    using typex = typename std::conditional<std::is_scalar<Type>::value,
        serialize_primitive_vector,
        serilize_general_vector>::type;
    
    typex::invoke(ar, t);    
}

template <class Archive, class Type, class Allocator>
inline void load(Archive & ar, std::vector<Type, Allocator> &t) {
    using typex = typename std::conditional<std::is_scalar<Type>::value,
        deserialize_primitive_vector,
        deserilize_general_vector>::type;
    
    typex::invoke(ar, t);

}

template<class Archive, class Type, class Key, class Compare, class Allocator >
inline void save(
    Archive & ar,
    const std::map<Key, Type, Compare, Allocator> &t
) {
    int32_t count = (int32_t)t.size();
    ar << count;
    for (auto iter : t) {
        ar << iter.first;
        ar << iter.second;
    }
}

template<class Archive, class Type, class Key, class Compare, class Allocator >
inline void load(Archive & ar, std::map<Key, Type, Compare, Allocator> &t) {
    
    int32_t count;
    ar >> count;

    while(count-- > 0){
        using value_type = typename std::map<Key, Type, Compare, Allocator>::mapped_type;
        using key_type = typename std::map<Key, Type, Compare, Allocator>::key_type;

        value_type value;
        key_type key;

        ar >> key;
        ar >> value;

        t.emplace(std::make_pair(key, value));
    }
}
} // namespace internal
} // namespace cask