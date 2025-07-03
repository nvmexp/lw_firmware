/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010,2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_CIRLWLARBUFFER_H
#define INCLUDED_CIRLWLARBUFFER_H
#ifndef MASSERT_H__
#include "massert.h"
#endif
#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

// Note1: this is not thread safe (ie no internal mutex)
// Note2: Please make sure that class types where destructor frees resources is
//        not used here. The destructor is not called automatically
template <typename T>
class CirBuffer
{
public:
    explicit CirBuffer(size_t Size)
    : m_BeginIdx(0),
      m_Size(0)
    {
        m_Buf.resize(Size + 1); // add 1 for the Tail bubble
    };

    CirBuffer()
    : m_BeginIdx(0),
      m_Size(0)
    {
        m_Buf.resize(1);
    };

    CirBuffer(const CirBuffer& Src)
    {
        m_BeginIdx = Src.m_BeginIdx;
        m_Size     = Src.m_Size;
        m_Buf      = Src.m_Buf;
    };

    class const_iterator
    {
    public:
        const_iterator():m_pCirBuff(NULL){};
        const_iterator(const CirBuffer *pCirBuff, size_t LwrIdx)
        : m_pCirBuff(pCirBuff),
          m_LwrIdx(LwrIdx){};

        const_iterator & operator= (const const_iterator & Src)
        {
            m_pCirBuff = Src.m_pCirBuff;
            m_LwrIdx   = Src.m_LwrIdx;
            return *this;
        };

        bool operator==(const const_iterator & Src) const
        { return (m_pCirBuff == Src.m_pCirBuff) && (m_LwrIdx == Src.m_LwrIdx); };
        bool operator!=(const const_iterator & Src) const
        { return (m_pCirBuff != Src.m_pCirBuff) || (m_LwrIdx != Src.m_LwrIdx); };

        const_iterator& operator++()
        {
            MASSERT(m_pCirBuff->EndIdx() != m_LwrIdx);
            m_LwrIdx = m_pCirBuff->FixIdx(m_LwrIdx + 1);
            return *this;
        };
        const_iterator operator++(int)
        {
            const_iterator temp = *this;
            operator++();
            return temp;
        };

        const_iterator& operator--()
        {
            MASSERT(m_pCirBuff->RendIdx() != m_LwrIdx);
            m_LwrIdx = m_pCirBuff->FixIdx(m_LwrIdx - 1);
            return *this;
        };
        const_iterator operator--(int)
        {
            const_iterator temp = *this;
            operator--();
            return temp;
        };

        const T* operator->() const {return &(m_pCirBuff->m_Buf[m_LwrIdx]);};
        const T& operator*() const {return m_pCirBuff->m_Buf[m_LwrIdx];};

    protected:
        const CirBuffer* m_pCirBuff;
        size_t           m_LwrIdx;
    };

    class iterator : public const_iterator
    {
    public:
        iterator() : const_iterator() {};
        iterator(CirBuffer *pCirBuff, size_t LwrIdx)
        : const_iterator(pCirBuff, LwrIdx) {};

        iterator & operator= (const iterator & Src)
        {
            const_iterator::operator=(Src);
            return *this;
        };
        T* operator->() const {return const_cast<T*>(const_iterator::operator->());};
        T& operator*() const {return const_cast<T&>(const_iterator::operator*());};
    };

    class const_reverse_iterator : public const_iterator
    {
    public:
        const_reverse_iterator() : const_iterator() {};
        const_reverse_iterator(CirBuffer *pCirBuff, size_t LwrIdx)
        : const_iterator(pCirBuff, LwrIdx) {};

        const_reverse_iterator & operator=(const const_reverse_iterator & Src)
        {
            const_iterator::operator=(Src);
            return *this;
        }

        const_reverse_iterator& operator++()
        {
            MASSERT(this->m_pCirBuff->RendIdx() != this->m_LwrIdx);
            this->m_LwrIdx = this->m_pCirBuff->FixIdx(this->m_LwrIdx - 1);
            return *this;
        };
        const_reverse_iterator operator++(int)
        {
            const_reverse_iterator temp = *this;
            operator++();
            return temp;
        };
        const_reverse_iterator& operator--()
        {
            MASSERT(this->m_pCirBuff->EndIdx() != this->m_LwrIdx);
            this->m_LwrIdx = this->m_pCirBuff->FixIdx(this->m_LwrIdx + 1);
            return *this;
        };
        const_reverse_iterator operator--(int)
        {
            const_reverse_iterator temp = *this;
            operator--();
            return temp;
        };

        const T* operator->() const {return const_iterator::operator->();};
        const T& operator*() const {return const_iterator::operator*();};
    };

    class reverse_iterator : public const_reverse_iterator
    {
    public:
        reverse_iterator() : const_reverse_iterator() {};
        reverse_iterator(CirBuffer *pCirBuff, size_t LwrIdx)
        : const_reverse_iterator(pCirBuff, LwrIdx) {};

        reverse_iterator & operator= (const reverse_iterator & Src)
        {
            const_reverse_iterator::operator=(Src);
            return *this;
        }
        T* operator->() const {return const_cast<T*>(const_reverse_iterator::operator->());};
        T& operator*() const {return const_cast<T&>(const_reverse_iterator::operator*());};
    };

    void push_back(const T & NewT)
    {
        // m_BeginIdx needs to move when the buffer is full
        if (m_Size == capacity())
            m_BeginIdx = FixIdx(m_BeginIdx + 1);
        else
            m_Size++;

        m_Buf[TailIdx()] = NewT;
    };

    T pop_back()
    {
        MASSERT(size());
        const T & Temp = m_Buf[TailIdx()];
        m_Size--;
        return Temp;
    };

    T pop_front()
    {
        MASSERT(size());
        const T & Temp = m_Buf[m_BeginIdx];
        m_BeginIdx = FixIdx(m_BeginIdx + 1);
        m_Size--;
        return Temp;
    };

    //! Get the number of elements that can be in the cirlwlar buffer
    size_t capacity() const{return m_Buf.size()-1;};

    //! change the capacity of the cirlwlar buffer.
    void reserve(size_t NewSize)
    {
        if (NewSize <= m_Buf.size())
            return;

        // Need to take account of the wrapped around buffer - can't just do
        // straight memory copy
        vector<T> TempBuf(NewSize + 1);
        size_t TempBufSize = 0;
        while ((size() > 0) && (TempBufSize < NewSize))
        {
            TempBuf[TempBufSize] = pop_front();
            TempBufSize++;
        }
        m_BeginIdx = 0;
        m_Size     = TempBufSize;
        TempBuf.swap(m_Buf);
    }

    //! resize = change the number of elements in the buffer
    // Note: resize from begin. So if there's a resize that is smaller than before,
    //       take out from the back (newest)
    // Note: doesn't affect allocated size
    void resize(size_t NewSize, const T & NewT)
    {
        if (NewSize > capacity())
            return;

        if (NewSize > size())
        {
            size_t LwrSize = size();
            for (; LwrSize < NewSize; LwrSize++)
                push_back(NewT);
        }
        else
        {
            resize(NewSize);
        }
    }
    // the one argument resize() only works with smaller NewSize then current size
    void resize(size_t NewSize)
    {
        if (NewSize >= size())
            return;

        m_Size = NewSize;
    }

    void clear(){resize(0);};

    //! Get the number of elements in the buffer
    size_t size() const
    {
        return m_Size;
    };

    // This points to the Head element (oldest)
    const_iterator begin() const{return const_iterator(this, m_BeginIdx);};
    iterator begin(){return iterator(this, m_BeginIdx);};

    // this points to Tail element
    const_reverse_iterator rbegin() const
    { return const_reverse_iterator(this, TailIdx());};

    reverse_iterator rbegin()
    { return reverse_iterator(this, TailIdx());};

    // this points to the invalid element before Head
    const_reverse_iterator rend() const
    {return const_reverse_iterator(this, RendIdx());};

    reverse_iterator rend()
    {return reverse_iterator(this, RendIdx());};

    // this points to the invalid element after Tail
    const_iterator end() const
    {return const_iterator(this, EndIdx());};

    iterator end()
    {return iterator(this, EndIdx());};

    T & front(){return m_Buf[m_BeginIdx];};
    const T & front() const {return m_Buf[m_BeginIdx];};

    T & back(){return m_Buf[TailIdx()];};
    const T & back() const{return m_Buf[TailIdx()];};

#ifdef INCLUDE_UNITTEST
    // DELETE ME IN FUTURE: (Debug only)
    const size_t GetBeginIdx()const {return m_BeginIdx;};
    const size_t GetEndIdx()const {return EndIdx();};
    T GetValAtIdx(size_t Idx){return m_Buf[Idx];};
#endif

    CirBuffer& operator=(const CirBuffer& Src)
    {
        if (this != &Src)
        {
            m_BeginIdx = Src.m_BeginIdx;
            m_Size     = Src.m_Size;
            m_Buf      = Src.m_Buf;
        }
        return *this;
    }

    // todo: operator[] ?

private:
    friend class const_iterator;

    size_t FixIdx(size_t i) const { return (i + m_Buf.size()) % m_Buf.size(); }
    size_t EndIdx() const  { return FixIdx(m_BeginIdx + m_Size); }
    size_t RendIdx() const { return FixIdx(m_BeginIdx - 1); }
    size_t TailIdx() const { return FixIdx(m_BeginIdx + m_Size - 1); }

    size_t m_BeginIdx;  // index of front element (oldest)
    size_t m_Size;      // current size of cirlwlar buffer
    vector<T> m_Buf;
};

#endif
