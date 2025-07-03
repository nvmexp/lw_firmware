#pragma once

#include <cask/serialization/serializer.h>

namespace cask {
namespace internal {



template<typename Archive>
class InterfaceOutputArchive {
protected:
    InterfaceOutputArchive() {}
public:
    Archive* This() {
        return static_cast<Archive*>(this);
    }
    template<typename T>
    Archive& operator<<(const T& t) {
        this->This()->save_override(t);
        return *this->This();
    }
};

template<typename Archive>
class InterfaceInputArchive {
protected:
    InterfaceInputArchive() {}
public:
    Archive* This() {
        return static_cast<Archive*>(this);
    }

    template<typename T>
    Archive& operator>>(T& t) {
        this->This()->load_override(t);
        return *this->This();
    }
};

template <typename Archive>
class CommonOutputArchive : public InterfaceOutputArchive<Archive> {
public:
    template <typename T>
    void save_override(T& t) {
        cask::internal::save(*this->This(), t);
    }
};

template <typename Archive>
class CommonInputArchive : public InterfaceInputArchive<Archive> {
public:
    template <typename T>
    void load_override(T& t) {
        cask::internal::load(*this->This(), t);
    }
};

} // namespace internal
} // namespace cask