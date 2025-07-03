#ifndef FS_UTILS_H
#define FS_UTILS_H

namespace fslib {

unsigned int NBitsSet(unsigned int n) {
    return (unsigned int)((0x1ull << n) - 1);
}

bool isDisabled(unsigned int index, unsigned int mask) {
    return mask & (0x1 << index);
}

bool allDisabled(unsigned int index_low, unsigned int index_high,
                 unsigned int mask) {
    for (unsigned int i = index_low; i <= index_high; i++) {
        if (!(mask & (0x1 << i))) {
            return false;
        }
    }
    return true;
}

unsigned int SetBits(unsigned int index_low, unsigned int index_high,
                     unsigned int mask) {
    for (unsigned int i = index_low; i <= index_high; i++) {
        mask |= (0x1 << i);
    }
    return mask;
}

unsigned int countDisabled(unsigned int index_low, unsigned int index_high, unsigned int mask) {
    unsigned int count = 0;
    for (unsigned int i = index_low; i<= index_high; i++) {
        if (mask & (0x1 << i)) {
            count++;
        }
    }
    return count;
}

unsigned int swapL2SliceMask(unsigned int length, unsigned int mask) {
    assert(length%2 == 0);
    unsigned int lowMask = (1 << (length/2)) - 1 ;
    unsigned int hiMask = lowMask << (length/2) ;
    unsigned int swapMask = ((mask & lowMask) << (length/2)) | ((mask & hiMask) >> (length/2));
    return swapMask;
}


} // namespace fslib
#endif
