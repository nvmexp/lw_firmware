#pragma once
#include <ostream>
#include <streambuf>

#include <cask/serialization/binary_primitive.h>
#include <cask/serialization/common_archive.h>

namespace cask {
namespace internal {
class BinaryOutputArchive :
    public BinaryOutputPrimitive<BinaryOutputArchive, std::ostream::char_type, std::ostream::traits_type>,
    public CommonOutputArchive<BinaryOutputArchive> {
    using primitive_base_t = BinaryOutputPrimitive<BinaryOutputArchive, std::ostream::char_type, std::ostream::traits_type>;
    using archive_base_t = CommonOutputArchive<BinaryOutputArchive>;

public:
    BinaryOutputArchive(std::ostream& os, unsigned int flags=endian_little)
        : primitive_base_t(*os.rdbuf(), flags) {

    }

    BinaryOutputArchive(std::basic_streambuf<std::ostream::char_type, std::ostream::traits_type>& bsb, unsigned int flags=0)
        : primitive_base_t(bsb, flags) {
    }

};

class BinaryInputArchive
    : public BinaryInputPrimitive<BinaryInputArchive, std::istream::char_type, std::istream::traits_type>,
    public CommonInputArchive<BinaryInputArchive> {
    using primitive_base_t = BinaryInputPrimitive<BinaryInputArchive, std::istream::char_type, std::istream::traits_type>;
    using archive_base_t = CommonInputArchive<BinaryInputArchive>;

public:
    BinaryInputArchive(std::istream& os, unsigned int flags=endian_little)
        : primitive_base_t(*os.rdbuf(), flags) {

    }

    BinaryInputArchive(std::basic_streambuf<std::istream::char_type, std::istream::traits_type>& bsb, unsigned int flags=0)
        : primitive_base_t(bsb, flags) {
    }
    

};
} // namespace internal
} // namespace cask
