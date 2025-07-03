#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <array>

namespace fslib {

//-----------------------------------------------------------------------------
// PolyMorphContainer
//-----------------------------------------------------------------------------

// The idea is that an FS module can be created inside this container using placement new.
// the container can hold multiple module types, and they can be accessed via their base class
// pointer. The benefit is polymorphism, without heap allocations.
// The downside is that it adds complexity and makes it harder to debug.
// Because of this, it will not be enabled by default.

template <class BaseType, int alloc_size>
class PolyMorphContainer{
    protected:

    // The buffer to create the module in
    char m_storage[alloc_size];

    // base pointer to the derived type. It will be nullptr if there is nothing there
    BaseType* ptr {nullptr};
    
    // An empty function with a static assert to make sure that the object requested is not too large for the buffer
    // This was done within a function so that the compiler error would print out the requested size
    template <int storage_size, int class_size>
    void assert_storage_size() const {
        static_assert(storage_size >= class_size, "the object is too large to fit in the statically allocated buffer! Try increasing alloc_size");
    }

    public:

    /**
     * @brief Construct an object of type T using placement new inside this container
     * 
     * @tparam T 
     */
    template <class T>
    void createModule(){
        reset();
        assert_storage_size<alloc_size, sizeof(T)>();
        ptr = new (&m_storage) T();
    }

    /**
     * @brief Get a base pointer to the object in this container
     * 
     * @return BaseType* 
     */
    BaseType* operator->() {
        return ptr;
    }

    /**
     * @brief Get a const base pointer to the object in this container
     * 
     * @return const BaseType* 
     */
    const BaseType* operator->() const {
        return ptr;
    }

    /**
     * @brief Same as -> operator. For compatibliity with unique_ptr
     * 
     * @return const BaseType* 
     */
    const BaseType* get() const {
        return ptr;
    }

    /**
     * @brief Same as -> operator. For compatibliity with unique_ptr
     * 
     * @return BaseType* 
     */
    BaseType* get() {
        return ptr;
    }

    /**
     * @brief Get a const reference to the object inside the container
     * 
     * @return const BaseType& 
     */
    const BaseType& operator*() const {
        return *ptr;
    }

    /**
     * @brief Get a reference to the object inside the container
     * 
     * @return BaseType& 
     */
    BaseType& operator*() {
        return *ptr;
    }

    /**
     * @brief Destructor. It will call the destructor on the base type pointer, but will not free the memory
     * 
     */
    virtual ~PolyMorphContainer(){
        // destructor should destroy the object inside, but not try to free the memory, since it's not actually heap allocated
        reset();
    }

    /**
     * @brief Destroy the object inside and reset the ptr to nullptr.
     * 
     */
    void reset() {
        if (ptr != nullptr){
            ptr->~BaseType();
            ptr = nullptr;
        }
    }

    /**
     * @brief Construct an empty container with nullptr inside
     * 
     */
    PolyMorphContainer(){};

    // None of these constructors and operators are safe to use because it is not known what type is in the other container.
    // FSModule_t does not have a copy constructor, due to some shortlwts taken, so these shouldn't exist either
    // std::variant instead of char array could make this safer and enable these, but it would complicate the dependency structure
    PolyMorphContainer operator=(const PolyMorphContainer& other) = delete;
    PolyMorphContainer operator&&(PolyMorphContainer&& other) = delete;
    PolyMorphContainer(const PolyMorphContainer& other) = delete;
    PolyMorphContainer(PolyMorphContainer&& other) = delete;
};


//-----------------------------------------------------------------------------
// StaticVector
//-----------------------------------------------------------------------------

/**
 * @brief StaticVector is like a simple vector with a max size. It does not support most of features of std::vector.
 * The reason it's needed is because PolyMorphContainer does not support copying or moving, which is needed by std::vector
 * This class is basically just a wrapper around an std::array.
 * 
 * @tparam T 
 * @tparam max_size the max size allowed
 */
template <class T, int max_size>
class StaticVector {
    std::array<T, max_size> inner_array;
    size_t m_size {0};

    // Re-use the iterator of std::array
    using iterator = typename std::array<T, max_size>::iterator;
    using const_iterator = typename std::array<T, max_size>::const_iterator;

    public:
    /**
     * @brief Resize the vector. It will throw an exception if the requested size is larger than the inner_array
     * 
     * @param sz 
     */
    void resize(size_t sz){
        if(sz > inner_array.size()){
            throw FSLibException("requested size is larger than the max capacity!");
        }
        m_size = sz;
    }

    /**
     * @brief Return the current size
     * 
     * @return size_t 
     */
    size_t size() const {
        return m_size;
    }

    T& at(size_t i){
        if(i >= m_size){
            throw FSLibException("called .at() with index of bounds!");
        }
        return inner_array.at(i);
    }

    const T& at(size_t i) const {
        if(i >= m_size){
            throw FSLibException("called .at() with index of bounds!");
        }
        return inner_array.at(i);
    }

    T& operator[](size_t i) {
        return inner_array[i];
    }

    const T& operator[](size_t i) const {
        return inner_array[i];
    }

    /**
     * @brief Get an iterator to the beginning of the array
     * 
     * @return iterator 
     */
    iterator begin() {
        return inner_array.begin();
    }

    const_iterator begin() const {
        return inner_array.begin();
    }

    const_iterator cbegin() const {
        return inner_array.cbegin();
    }

    /**
     * @brief Return an iterator to the index at m_size, which is 1 past the end
     * 
     * @return iterator 
     */
    iterator end() {
        return inner_array.begin() + m_size;
    }

    const_iterator end() const {
        return inner_array.begin() + m_size;
    }

    const_iterator cend() const {
        return inner_array.cbegin() + m_size;
    }
    
};

//-----------------------------------------------------------------------------
// BaseVectorView
//-----------------------------------------------------------------------------

#ifdef USEBASEVECTORVIEW

/**
 * @brief BaseVectorView provides an efficient way to access the base class pointers of an arbitrary vector of derived class pointers
 * 
 * 
 * @tparam BaseType 
 * @tparam variant_type 
 */
template <class BaseType, class variant_type>
class BaseVectorView {

    using BaseTypeNoPtr = typename std::remove_pointer<BaseType>::type;
    using BaseTypeNoPtrNoConst = typename std::remove_const<BaseTypeNoPtr>::type;
    using NonConstBaseType = typename std::add_pointer<BaseTypeNoPtrNoConst>::type;
    using BaseTypeNoPtrConst = typename std::add_const<BaseTypeNoPtr>::type;
    using ConstBaseType = typename std::add_pointer<BaseTypeNoPtrConst>::type;    
    using ConstCorrectReturn = typename std::conditional<std::is_same<BaseType, ConstBaseType>::value, ConstBaseType, NonConstBaseType>::type;

    /**
     * @brief forward iterator for iterating over all ptrs in the view with for loops
     * 
     */
    class iterator {

        private:
        // pointer back to the BaseVectorView
        const BaseVectorView* m_view_ptr{nullptr};
        // current index into the vector
        size_t m_lwrrent_idx{0};

        public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = BaseType;

        /**
         * @brief Get the element pointed to by this iterator
         * 
         * @return value_type 
         */
        value_type operator*() const {
            return (*m_view_ptr)[m_lwrrent_idx];
        }

        /**
         * @brief pre increment
         * 
         * @return iterator& 
         */
        iterator& operator++() {
            m_lwrrent_idx++; 
            return *this;
        }

        /**
         * @brief post increment
         * 
         * @return iterator 
         */
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        /**
         * @brief == operator
         * 
         * @param other 
         * @return true 
         * @return false 
         */
        bool operator==(const iterator& other) const {
            if (m_view_ptr != other.m_view_ptr){
                return false;
            }
            if (m_lwrrent_idx != other.m_lwrrent_idx){
                return false;
            }
            return true;
        }

        /**
         * @brief != operator
         * 
         * @param other 
         * @return true 
         * @return false 
         */
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        iterator(const BaseVectorView* adapter_ptr, size_t start_idx) : m_view_ptr(adapter_ptr), m_lwrrent_idx(start_idx) {
        }

    };

    public:

    // A variant containing the pointer to the actual vector with derived class pointers
    variant_type m_vector_ptr;

    private:
    /**
     * @brief private class. visitor for returning the size of the derived ptr vector
     * 
     */
    class SizeVisitor {
        public:
        template <class T>
        size_t operator()(T vector_ptr) const {
            if (vector_ptr == nullptr){
                return 0;
            }
            return vector_ptr->size();
        }
    };

    /**
     * @brief Visitor for calling []
     * 
     */
    class BracketOperatorVisitor {
        size_t idx;
        public:
        BracketOperatorVisitor(size_t idx_in) : idx(idx_in) {
        }
        template <class T>
        ConstBaseType operator()(T vector_ptr) const {
            auto ptr1 = (*vector_ptr)[idx].get();
            return ptr1;
        }
    };

    /**
     * @brief Visitor for calling .at()
     * 
     */
    class AtOperatorVisitor {
        size_t idx;
        public:
        AtOperatorVisitor(size_t idx_in) : idx(idx_in) {
        }
        template <class T>
        ConstBaseType operator()(T vector_ptr) const {
            return static_cast<ConstBaseType>((*vector_ptr).at(idx).get());
        }
    };

    public:
    /**
     * @brief Get the size of the vector pointed to by this view
     * 
     * @return size_t 
     */
    size_t size() const {
        return std::visit(SizeVisitor(), m_vector_ptr);
    }

    /**
     * @brief Call the [] operator on the vector pointed to by this view
     * 
     * @param idx 
     * @return ConstCorrectReturn 
     */
    ConstCorrectReturn operator[](size_t idx) const {
        return const_cast<ConstCorrectReturn>(std::visit(BracketOperatorVisitor(idx), m_vector_ptr));
    }

    /**
     * @brief Call the .at() function on the vector pointed to by this view
     * 
     * @param idx 
     * @return ConstCorrectReturn 
     */
    ConstCorrectReturn at(size_t idx) const {
        return const_cast<ConstCorrectReturn>(std::visit(AtOperatorVisitor(idx), m_vector_ptr));
    }

    /**
     * @brief Get an iterator pointing to the beginning of the vector pointed to by this view
     * 
     * @return iterator 
     */
    iterator begin() const {
        return iterator(this, 0);
    }

    /**
     * @brief Get an iterator pointing to the end of the vector pointed to by this view
     * 
     * @return iterator 
     */
    iterator end() const {
        return iterator(this, size());
    }

    /**
     * @brief Construct a new BaseVectorView object from a vector to derived class objects
     * 
     * @tparam List 
     * @param list_ref 
     */
    template <class List>
    BaseVectorView(List& list_ref) : m_vector_ptr(&list_ref) {
        static_assert(std::is_const_v<List> == std::is_const_v<BaseTypeNoPtr> || std::is_const_v<BaseTypeNoPtr>);
        // Using a static assert here to enforce the const correctness
        // There is probably a cleaner way with SFINAE, but this works
        // If you find this assert triggering, it's because you need to make the BaseVectorView<>'s template param
        // a const, or try calling this constructor on a non-const
    }

    /**
     * @brief Return another view to the same vector, but allow modifying that vector
     * 
     * @return BaseVectorView<NonConstBaseType, variant_type> 
     */
    BaseVectorView<NonConstBaseType, variant_type> removeConst() const {
        BaseVectorView<NonConstBaseType, variant_type> adapter_non_const;
        adapter_non_const.m_vector_ptr = m_vector_ptr;
        return adapter_non_const;
    }

    /**
     * @brief Construct a new Base Vector View object, with m_vector_ptr pointing to nullptr
     * 
     */
    BaseVectorView() {
        using variant_type_0 = typename std::variant_alternative<0, variant_type>::type;
        m_vector_ptr = variant_type_0(nullptr);
    }
};

#endif


} // namespace fslib
#endif
