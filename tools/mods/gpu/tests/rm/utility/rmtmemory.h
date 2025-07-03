/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef RM_UTILITY_MEMORY_H_
#define RM_UTILITY_MEMORY_H_

#include <vector>

namespace rmt
{
    //!
    //! \brief      Constant Cloning Pointer
    //!
    //! \details    This class is analogous to the 'std::auto-ptr' except that
    //!             it clones the object and therefore avoids the double-reference
    //!             problem of 'std::auto-ptr'.
    //!
    //!             For most functionalitym the template parameter must specify
    //!             a class that contains a 'clone' function to copy the object
    //!             into a newly allocated heap object.
    //!
    template <class T> class ConstClonePointer
    {
    protected:
        //! \brief      Cloned heap object
        const T *clone;

    public:

        //! \brief      Construction to NULL pointer
        explicit inline ConstClonePointer() throw()
        {
            clone = NULL;
        }

        //! \brief      Construction from pointer
        explicit inline ConstClonePointer(const T *p) throw()
        {
            if (p)
                clone = p->clone();
            else
                clone = NULL;
        }

        //! \brief      Copy Construction
        inline ConstClonePointer(const ConstClonePointer &cp) throw()
        {
            if (cp.clone)
                clone = cp.clone->clone();
            else
                clone = NULL;
        }

        //! \brief      Construction from alternate type
        template<class U> inline ConstClonePointer(const ConstClonePointer<U> &cp) throw()
        {
            if (cp.clone)
                clone = cp.clone->clone();
            else
                clone = NULL;
        }

        //! \brief      Destruction
        inline ~ConstClonePointer() throw()
        {
            if (clone)
                delete clone;
        }

        //!
        //! \brief      Assign without cloning
        //!
        //! \warning    This ConstClonePointer object becomes the owner of the
        //!             object referenced by the parameter and will delete it on
        //!             destruction, reassignment, etc.
        //!
        inline void take(const T *p)
        {
            if (clone && clone != p)
                delete clone;
            clone = p;
        }

        //! \brief      Assignment from pointer
        const void reset(const T *p = NULL) throw()
        {
            if (clone && clone != p)
                delete clone;

            if (!p)
                clone = NULL;
            else if (clone != p)
                clone = p->clone();
        }

        //! \brief      Assignment
        inline ConstClonePointer &operator=(const ConstClonePointer &cp) throw()
        {
            if (clone && clone != cp.clone)
                delete clone;

            if (!cp.clone)
                clone = NULL;
            else if (clone != cp.clone)
                clone = cp.clone->clone();

            return *this;
        }

        //! \brief      Assignment from pointer
        inline ConstClonePointer &operator=(const T *p) throw()
        {
            if (clone && clone != p)
                delete clone;

            if (!p)
                clone = NULL;
            else if (clone != p)
                clone = p->clone();

            return *this;
        }

        //! \brief      Assignment from alternate type
        template<class U> inline ConstClonePointer &operator=(const ConstClonePointer<U> &cp) throw()
        {
            if (clone && (const void *) clone != (const void *) cp.clone)
                delete clone;

            if (!cp.clone)
                clone = NULL;
            else if ((const void *) clone != (const void *) cp.clone)
                clone = cp.clone->clone();

            return *this;
        }

        //! \brief      Dereference
        inline const T &operator*() const throw()
        {
            return *clone;
        }

        //! \brief      Dereference
        inline const T *operator->() const throw()
        {
            return clone;
        }

        //!
        //! \brief      Pointer value
        //!
        //! \warning    The pointer refers to the object that is still owned
        //!             by this ConstClonePointer object and may dangle if this
        //!             ConstClonePointer object deletes it from the heap.
        //!
        inline const T *get() const throw()
        {
            return clone;
        }

        //! \brief      Release cloned object
        inline const T *release() const throw()
        {
            const T *p = clone;
            if (clone)
            {
                delete clone;
                clone = NULL;
            }

            return p;
        }

        //! \brief      Nullity check
        inline operator bool() const throw()
        {
            return !!clone;
        }
    };

    //!
    //! \brief      Cloning Pointer
    //!
    //! \details    This class is analogous to the 'std::auto-ptr' except that
    //!             it clones the object and therefore avoids the double-reference
    //!             problem of 'std::auto-ptr'.
    //!
    //!             For most functionalitym the template parameter must specify
    //!             a class that contains a 'clone' function to copy the object
    //!             into a newly allocated heap object.
    //!
    template <class T> class ClonePointer
    {
    protected:
        //! \brief      Cloned heap object
        T *clone;

    public:

        //! \brief      Construction from pointer
        explicit inline ClonePointer(const T *p = NULL) throw()
        {
            if (p)
                clone = p->clone();
            else
                clone = NULL;
        }

        //! \brief      Copy Construction
        inline ClonePointer(const ClonePointer &cp) throw()
        {
            if (cp.clone)
                clone = cp.clone->clone();
            else
                clone = NULL;
        }

        //! \brief      Construction from alternate type
        template<class U> inline ClonePointer(const ClonePointer<U> &cp) throw()
        {
            if (cp.clone)
                clone = cp.clone->clone();
            else
                clone = NULL;
        }

        //! \brief      Construction from ConstClonePointer
        inline ClonePointer(const ConstClonePointer<T> &cp) throw()
        {
            if (cp.clone)
                clone = cp.clone->clone();
            else
                clone = NULL;
        }

        //! \brief      Construction from alternate ConstClonePointer type
        template<class U> inline ClonePointer(const ConstClonePointer<U> &cp) throw()
        {
            if (cp.clone)
                clone = cp.clone->clone();
            else
                clone = NULL;
        }

        //! \brief      Destruction
        inline ~ClonePointer() throw()
        {
            if (clone)
                delete clone;
        }

        //!
        //! \brief      Assign without cloning
        //!
        //! \warning    This ConstPointer object becomes the owner of the object
        //!             referenced by the parameter and will delete it on
        //!             destruction, reassignment, etc.
        //!
        inline void take(const T *p)
        {
            if (clone && clone != p)
                delete clone;
            clone = p;
        }

        //! \brief      Assignment from pointer
        const void reset(const T *p = NULL) throw()
        {
            if (clone && clone != p)
                delete clone;

            if (p)
                clone = NULL;
            else if (clone != p)
                clone = p->clone();
        }

        //! \brief      Assignment
        inline ClonePointer &operator=(const ClonePointer &cp) throw()
        {
            if (clone && clone != cp.clone)
                delete clone;

            if (!cp.clone)
                clone = NULL;
            else if (clone != cp.clone)
                clone = cp.clone->clone();

            return *this;
        }

        //! \brief      Assignment from pointer
        inline ClonePointer &operator=(const T *p) throw()
        {
            if (clone && clone != p)
                delete clone;

            if (!p)
                clone = NULL;
            else if (clone != p)
                clone = p->clone();

            return *this;
        }

        //! \brief      Assignment from alternate type
        template<class U> inline ClonePointer &operator=(const ClonePointer<U> &cp) throw()
        {
            if (clone && (const void *) clone != (const void *) cp.clone)
                delete clone;

            if (!cp.clone)
                clone = NULL;
            else if ((const void *) clone != (const void *) cp.clone)
                clone = cp.clone->clone();

            return *this;
        }

        //! \brief      Dereference
        inline T &operator*() const throw()
        {
            return *clone;
        }

        //! \brief      Dereference
        inline T *operator->() const throw()
        {
            return clone;
        }

        //!
        //! \brief      Pointer value
        //!
        //! \warning    The pointer refers to the object that is still owned
        //!             by this ClonePointer object and may dangle if this
        //!             ClonePointer object deletes it from the heap.
        //!
        inline T *get() const throw()
        {
            return clone;
        }

        //! \brief      Release cloned object
        inline T *release() const throw()
        {
            const T *p = clone;
            if (clone)
            {
                delete clone;
                clone = NULL;
            }

            return p;
        }

        //! \brief      Nullity check
        inline operator bool() const throw()
        {
            return !!clone;
        }
    };
};

#endif // RM_UTILITY_MEMORY_H_

