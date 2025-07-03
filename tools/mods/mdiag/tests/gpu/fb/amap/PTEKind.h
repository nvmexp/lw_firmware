#ifndef _PTE_KIND__H_
#define _PTE_KIND__H_

#include <string>

struct PTEKind {
    enum Format { PITCH, BLOCKLINEAR, BLOCKZ64 };
    enum Size { VM64K, VM128K };
    PTEKind(Format format=PITCH, Size size=VM64K) : m_format(format), m_size(size) {}
    PTEKind(const PTEKind &_pk) : m_format(_pk.m_format), m_size(_pk.m_size) {}
    bool isPitch() const { return m_format==PITCH; }
    bool isBlockLinear() const { return m_format==BLOCKLINEAR; }
    bool isZ64() const { return m_format==BLOCKZ64; }
    bool is64kPage() const { return m_size == VM64K; }
    bool is128kPage() const { return m_size == VM128K; }
    void setFormat(Format format) { m_format = format; }
    Format getFormat() const { return m_format; }

    std::string getString() const {
        std::ostringstream s;
        s << "format=" << (isPitch() ? "PITCH" : "BLOCKLINEAR") << (is64kPage() ? ":VM64K" : ":VM128K");
        return s.str();
    }
private:
    // more members to be added later as needed
    Format m_format;
    Size   m_size;
};

#endif
