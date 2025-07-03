#pragma once
#include <cstring>
#include <cassert>
#include <climits>
#if CHAR_BIT != 8
#error This code assume an eight-bit byte
#endif

#include <streambuf>
#include <iostream>
#include <limits>


namespace cask {
namespace internal {

enum endian_flags {
    endian_big        = 0x1,
    endian_little     = 0x2
};

static inline bool is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1; 
}

static inline void reverse_bytes(void* addr, std::size_t size) {
    char* address = (char*)addr;
    char * first = address;
    char * last = first + size - 1;
    for (; first < last; ++first, --last) {
        char x = *last;
        *last = *first;
        *first = x;
    }
}

template<typename Archive, typename CharT, typename Traits>
class BinaryOutputPrimitive {
private:
    std::basic_streambuf<CharT, Traits>& sb_;
    int flags_;
    bool need_reverse_;
    void save_binary(const void* address, std::size_t count) {
        count = (count + sizeof(CharT) - 1) / sizeof(CharT);
        std::streamsize scount = sb_.sputn(
            static_cast<const CharT*>(address),
            static_cast<std::streamsize>(count)
        );

        if (count != scount) {
            std::cerr << "output stream error" << std::endl;
        }
    }

public:
    Archive* This() {
        return static_cast<Archive*>(this);
    }

    template<class T>
    void save(const T& t) {
        T tmp = t;
        char * cptr = reinterpret_cast<char *>(&tmp);
        if(need_reverse_) reverse_bytes(cptr, sizeof(T));
        save_binary(cptr, sizeof(T));
    }

    void save(const std::string& t) {
        std::size_t size = static_cast<std::size_t>(t.size());
        this->save(size);
        save_binary(t.data(), size);
    }

    void save(const char* t) {
        std::size_t size = std::strlen(t);
        this->save(size);
        save_binary(t, size);
    }

    void save(const void* t, std::size_t count) {
        this->save(count);
        save_binary(t, count);
    }

    void save(const float& t) {
        const int32_t tmp = *reinterpret_cast<const int32_t*>(&t);
        this->save(tmp);

    }

    void save(const double& t) {
        const int64_t tmp = *reinterpret_cast<const int64_t*>(&t);
        this->save(tmp);
    }

    void save(const char& t) {
        save_binary(&t, 1);
    }

public:
    BinaryOutputPrimitive(std::basic_streambuf<CharT, Traits>& sb, int flags)
        : sb_(sb), flags_(flags)
    {
        bool big_endian = is_big_endian();
        if (big_endian != (flags & endian_big)) {
            need_reverse_ = true;
        } else {
            need_reverse_ = false;
        }
    }
};

template <typename Archive, typename CharT, typename Traits>
class BinaryInputPrimitive {
private:
    std::basic_streambuf<CharT, Traits>& sb_;
    int32_t flags_;
    bool need_reverse_;

    void load_binary(void* address, std::size_t count) {
        assert(static_cast<std::streamsize>(count / sizeof(CharT)) <= 
            std::numeric_limits<std::streamsize>::max());
        std::streamsize s = static_cast<std::streamsize>(count / sizeof(CharT));
        std::streamsize scount = sb_.sgetn(static_cast<CharT *>(address), s);
        assert(scount == s);
    }

public:
    BinaryInputPrimitive(std::basic_streambuf<CharT, Traits>& sb, int32_t flags)
        : sb_(sb), flags_(flags)
    {
        bool big_endian = is_big_endian();
        if (big_endian != (flags & endian_big)) {
            need_reverse_ = true;
        } else {
            need_reverse_ = false;
        }
    }

    Archive* This() {
        return static_cast<Archive*>(this);
    }
    template<typename T>
    void load(T& t) {
        T tmp;
        load_binary(&tmp, sizeof(T));
        if(flags_ & endian_big)
            reverse_bytes(&tmp, sizeof(T));
        t = tmp;
    }

    void load(std::string& t) {
        std::size_t size;
        this->load(size);
        t.resize(size);

        if (0 < size) {
            load_binary(const_cast<char*>(t.data()), size);
        }
    }

    void load(char* t) {
        std::size_t size;
        this->load(size);
        load_binary(t, size);
        t[size] = '\0';
    }

    void load(float & t) {
        int32_t tmp;
        this->load(tmp);
        t = *reinterpret_cast<float*>(&tmp);
    }

    void load(double & t) {
        int64_t tmp;
        this->load(tmp);
        t = *reinterpret_cast<double*>(&tmp);
    }

    void load(char & t) {
        load_binary(&t, 1);
    }

    void load(void* t, std::size_t count) {
        std::size_t actual_count;
        this->load(actual_count);
        char* buf = new char[actual_count];
        this->load_binary(buf, actual_count);
        std::size_t copy_count = std::min(count, actual_count);
        memcpy(t, buf, copy_count);
        delete[] buf;
    }

    void push_back(std::size_t size) {
        sb_.pubseekoff(-size, std::ios_base::cur, std::ios_base::in);
    }

};
} // namespace internal
} // namespace cask