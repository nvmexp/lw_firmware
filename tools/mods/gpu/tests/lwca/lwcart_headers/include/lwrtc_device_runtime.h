// analogue of crt/device_runtime.h
#ifndef __LWRTC_DEVICE_RUNTIME_H__
#define __LWRTC_DEVICE_RUNTIME_H__


#if __cplusplus >= 201103L

namespace __lwrtc_util {
  template<typename T>
  struct remove_ref { typedef T type; };

  template<typename T>
  struct remove_ref<T&> { typedef T type; };

  template<typename T>
  struct remove_ref<T&&> { typedef T type; };

  template<class T>
  struct is_lvalue_ref { static const bool val = false; };
  
  template<class T>
  struct is_lvalue_ref<T&> { static const bool val = true; };
}

// set by compiler driver
#if __LW_BUILTIN_MOVE_FORWARD
namespace std {
template<typename T>
 constexpr T&& 
 __device__ forward(typename __lwrtc_util::remove_ref<T>::type &__in) noexcept
 { return static_cast<T&&>(__in); }

 template<typename T>
 constexpr T&&
 __device__ forward(typename __lwrtc_util::remove_ref<T>::type && __in) noexcept
  {
    static_assert(!__lwrtc_util::is_lvalue_ref<T>::val, 
                  "incorrect forward call!");
    return static_cast<T&&>(__in);
  }

 template<typename T>
 constexpr typename __lwrtc_util::remove_ref<T>::type &&
 __device__ move(T&& __in) noexcept
  { 
    return static_cast<typename __lwrtc_util::remove_ref<T>::type &&>(__in); 
  }
}
#endif /* __LW_BUILTIN_MOVE_FORWARD */

// set by compiler driver
#if __LW_BUILTIN_INITIALIZER_LIST
namespace std {
  template<class T>
  class initializer_list
  {
    public:
      typedef T value_type;
      typedef const T& reference;
      typedef const T& const_reference;
      typedef size_t size_type;
      typedef const T* iterator;
      typedef const T* const_iterator;

    private:
      const T* buf_ptr;
      size_t length;

      constexpr initializer_list(const_iterator in, size_type in_length)
        : buf_ptr(in), length(in_length) { }

    public:
      constexpr initializer_list() noexcept
        : buf_ptr(0), length(0) { }

      constexpr size_type size() const noexcept { return length; }
      constexpr const_iterator begin() const noexcept { return buf_ptr; }
      constexpr const_iterator end() const noexcept { return buf_ptr + length; }
  };
}
#endif /* __LW_BUILTIN_INITIALIZER_LIST */

#endif /* __cplusplus >= 201103L */


// NOTE NOTE NOTE: The functions in this file are helper functions that
// are looked up by the compiler during processing. These functions
// must be defined early on, before any user/builtin code that could
// potentially result in the compiler requiring these helpers.

extern "C" inline __host__ __device__ void  __cxa_vec_ctor(void *n,
                                                    size_t num,
                                                    size_t size,
                                                    void (*c) (void *),
                                                    void (*d) (void *))
{
  if (!c)
    return; 

  for (size_t i = 0; i < num; i++) 
    c((void*)((unsigned char *)n + i*size));
}                                

extern "C" inline __host__ __device__ void __cxa_vec_cctor(void *dest,
                                                    void *src,
                                                    size_t num,
                                                    size_t size,
                                                    void (*c) (void *, void *),
                                                    void (*d) (void *))
{
  if (!c)
    return; 

  for (size_t i = 0; i < num; i++) {
    c((void*)((unsigned char *)dest + i*size), 
      (void*)((unsigned char *)src + i*size));
  }
}

extern "C" inline __host__ __device__ void __cxa_vec_dtor(void *n,
                                                   size_t num,
                                                   size_t size,
                                                   void (*d) (void *))
{
  if (!d)
    return;

  for (size_t i = num-1;  i > 0; i--) 
    d((void*)((unsigned char *)n + i*size)); 
   
  d((void*)((unsigned char *)n + 0*size)); 
}

extern "C" inline __host__ __device__ void * __cxa_vec_new2(size_t num,
                                                     size_t size,
                                                     size_t pad,
                                                     void (*c) (void *),
                                                     void (*d) (void *),
                                                     void *(*m) (size_t),
                                                     void (*f) (void *))
{
  unsigned char *t = (unsigned char *)m(num*size + pad); 
  *(size_t*)t = num;
  (void)__cxa_vec_ctor((void *)(t+pad), num, size, c, d); 
  return (void *)(t+pad);
}


#if defined(_MSC_VER)
extern "C" __host__ __device__ __lwdart_builtin__ void* malloc(size_t);
extern "C" __host__ __device__ __lwdart_builtin__ void free(void*);
#else /* !_MSC_VER */
extern "C" __host__ __device__ __lwdart_builtin__ __attribute__((used)) void* malloc(size_t);
extern "C" __host__ __device__ __lwdart_builtin__ __attribute__((used)) void free(void*);
#endif /* _MSC_VER */

/* wrapper functions because some of the helper functions below need to take the address of
 * malloc/free, which is not lwrrently supported */
inline __host__ __device__ void *__lwrtc_malloc_wrapper(size_t in) { return malloc(in); }
inline __host__ __device__ void __lwrtc_free_wrapper(void *in) { free(in); }

extern "C" inline __host__ __device__ void * __cxa_vec_new(size_t num,
                                                     size_t size,
                                                     size_t pad,
                                                     void (*c) (void *),
                                                     void (*d) (void *))
{
  return __cxa_vec_new2(num, size, pad, c, d, __lwrtc_malloc_wrapper, __lwrtc_free_wrapper);
}


extern "C" inline __host__ __device__ void * __cxa_vec_new3(size_t num,
                                                     size_t size,
                                                     size_t pad,
                                                     void (*c) (void *),
                                                     void (*d) (void *),
                                                     void *(*m) (size_t),
                                                     void (*f)(void *, size_t))
{
  return __cxa_vec_new2(num, size, pad, c, d, m, 0);
}

extern "C" inline __host__ __device__ void __cxa_vec_delete2(void *n,
                                                      size_t size,
                                                      size_t pad,
                                                      void (*d) (void *),
                                                      void (*f) (void *))
{
  unsigned char *ptr = (unsigned char *)(n);
  if (ptr) {
    unsigned char *t = ptr - pad; 
    size_t num = *(size_t*)t;
    __cxa_vec_dtor(ptr, num, size, d); 
    f((void *)t);
  }
}

extern "C" inline __host__ __device__ void __cxa_vec_delete(void *n,
                                                     size_t size,
                                                     size_t pad,
                                                     void (*d) (void *))
{
  __cxa_vec_delete2(n, size, pad, d, __lwrtc_free_wrapper);
}
	
extern "C" inline __host__ __device__  void __cxa_vec_delete3(void *n, 
                                                       size_t size, 
                                                       size_t pad, 
                                                       void (*d)(void *), 
                                                       void (*f)(void *,size_t))
{
  unsigned char *ptr = (unsigned char *)(n);
  if (ptr) {
    unsigned char *t = ptr - pad; size_t num = *(size_t*)t;
    size_t tsize = num*size+pad;
    __cxa_vec_dtor(ptr, num, size, d); 
    f((void *)t, tsize);
  }
}
  
  
extern "C" inline __host__ __device__ void *__gen_lwvm_memcpy_aligned1(void *dest, 
                                                                void *src, 
                                                                size_t size)
{
  __lwvm_memcpy((unsigned char *)dest, (unsigned char *)src, size, 1);
  return dest;
}

extern "C" inline __host__ __device__ void *__gen_lwvm_memcpy_aligned2(void *dest, 
                                                                void *src, 
                                                                size_t size)
{
  if (((unsigned long long)(dest) % 2) == 0 && 
      ((unsigned long long)(src) % 2) == 0) 
  {
    __lwvm_memcpy((unsigned char *)dest, (unsigned char *)src, size, 2); 
  } else {
    __lwvm_memcpy((unsigned char *)dest, (unsigned char *)src, size, 1);
  }
  return dest;
}

extern "C" inline __host__ __device__ void *__gen_lwvm_memcpy_aligned4(void *dest, 
                                                                void *src, 
                                                                size_t size)
{
  if (((unsigned long long)(dest) % 4) == 0 && 
      ((unsigned long long)(src) % 4) == 0)
  {
    __lwvm_memcpy((unsigned char *)dest, (unsigned char *)src, size, 4);
  } else {
    __lwvm_memcpy((unsigned char *)dest, (unsigned char *)src, size, 1);
  }
  return dest;
}

extern "C" inline __host__ __device__ void * __gen_lwvm_memcpy_aligned8(void *dest, 
                                                                 void *src, 
                                                                 size_t size)
{
  if (((unsigned long long)(dest) % 8) == 0 && 
       ((unsigned long long)(src) % 8) == 0)
  {
    __lwvm_memcpy((unsigned char *)dest, (unsigned char *)src, size, 8);
  } else {
    __lwvm_memcpy((unsigned char *)dest, (unsigned char *)src, size, 1);
  }
  return dest;
}

extern "C" inline __host__ __device__ void * __gen_lwvm_memcpy_aligned16(void *dest, 
                                                                  void *src, 
                                                                  size_t size)
{ 
  if (((unsigned long long)(dest) % 16) == 0 && 
      ((unsigned long long)(src) % 16) == 0)
  {
    __lwvm_memcpy((unsigned char *)dest, (unsigned char *)src, size, 16);
  } else {
    __lwvm_memcpy((unsigned char *)dest, (unsigned char *)src, size, 1);
  }
  return dest;
}


extern "C" inline __host__ __device__ void __cxa_pure_virtual(void) 
{ 
  /* cause a runtime failure */
  volatile int *ptr = 0;
  *ptr = 1;
}


extern "C" inline __host__ __device__ void* __gen_lwvm_memset(void *dest, int c, 
                                                              size_t n)
{
  __lwvm_memset((unsigned char *)dest, (unsigned char)c, n, 
                /*alignment=*/1);
  return dest;
}


extern "C" inline __host__ __device__ void* __gen_lwvm_memcpy(void *dest, 
                                                              const void *src, 
                                                              size_t n)
{
  __lwvm_memcpy((unsigned char *)dest, (unsigned char *)src, n, 
                /*alignment=*/ 1);
  return dest;
}

#endif /* __LWRTC_DEVICE_RUNTIME_H */
