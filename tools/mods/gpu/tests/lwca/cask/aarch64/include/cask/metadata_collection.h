#ifndef INCLUDE_GUARD_CASK_METADATA_COLLECTION_H
#define INCLUDE_GUARD_CASK_METADATA_COLLECTION_H


#include <set>
#include <cassert>


namespace cask {


typedef enum {
    VAL_TYPE_STRING,
    VAL_TYPE_INT32,
    VAL_TYPE_UINT32,
    VAL_TYPE_INT64,
    VAL_TYPE_UINT64,
    VAL_TYPE_BOOL,
    VAL_TYPE_MMA_SP_RATIO,
    VAL_TYPE_MMA_SHAPE,
    VAL_TYPE_SPLITK_MODE,
    VAL_TYPE_TRIANGULAR_KIND,
    VAL_TYPE_SCALAR_TYPE,
    VAL_TYPE_EDGE_TYPE,
    VAL_TYPE_MUL_ALGO_TYPE,
    VAL_TYPE_UNKNOWN
} MdValueType;


class MetadataVariant {
public:

    MetadataVariant(): val_type_name_(VAL_TYPE_UNKNOWN) {}

    MetadataVariant(const MetadataVariant& ref);

    MetadataVariant(const char*               );
    MetadataVariant(int32_t                   );
    MetadataVariant(uint32_t                  );
    MetadataVariant(int64_t                   );
    MetadataVariant(uint64_t                  );
    MetadataVariant(bool                      );
    MetadataVariant(ScalarType                );
    MetadataVariant(ScalarType::Label         );
    MetadataVariant(md::MmaInstrSpRatio       );
    MetadataVariant(md::MmaInstrSpRatio::Label);
    MetadataVariant(md::MmaInstrShape         );
    MetadataVariant(md::MmaInstrShape::Label  );
    MetadataVariant(md::SplitKMode            );
    MetadataVariant(md::SplitKMode::Label     );
    MetadataVariant(md::TriangularKind        );
    MetadataVariant(md::TriangularKind::Label );
    MetadataVariant(md::EdgeType              );
    MetadataVariant(md::EdgeType::Label       );
    MetadataVariant(md::MultiplicationAlgorithm);
    MetadataVariant(md::MultiplicationAlgorithm::Label);

    MetadataVariant& operator = (const MetadataVariant& ref) {
        md_val_ = ref.md_val_;
        val_type_name_ = ref.val_type_name_;
        return *this;
    }

    inline bool hasValue() const {
        return val_type_name_ != VAL_TYPE_UNKNOWN;
    }

    inline MdValueType valueType() const {
        return val_type_name_;
    }

    bool operator == (const MetadataVariant& ref) const;

    template<typename storageType>
    storageType get() const;

private:

    template<typename comparableType>
    inline bool equal(comparableType a, comparableType b) const { return a == b; }

    uint64_t    md_val_;
    MdValueType val_type_name_;

};



template<typename metadataName, template <metadataName> class metadataType, int32_t Size>
class MetadataCollection {

public:

    MetadataCollection() {}

    MetadataCollection(const MetadataCollection& ref) {
        for (int32_t i = 0; i < Size; i++) {
            mda_[i] = ref.mda_[i];
        }
    }

    typedef std::set<metadataName> MdSet_t;

    /////////////////////////////////////////////////////////////////////////////
    /// \brief Check if a dynamic metadata is supported by kernel.
    ///
    /// \returns true if the kernel supports the given metadata name,
    /// otherwise return false
    ///
    /////////////////////////////////////////////////////////////////////////////
    inline bool hasMetadata(const metadataName& md_name) const {
        return mda_[md_name].hasValue();
    }

    /////////////////////////////////////////////////////////////////////////////
    /// \brief Get certain metadata's value.
    /////////////////////////////////////////////////////////////////////////////
    template<metadataName md_name>
    inline typename metadataType<md_name>::type getMetadata() const {
        const MetadataVariant& meta = mda_[md_name];
        assert(meta.hasValue());
        return meta.get<typename metadataType<md_name>::type>();
    }

    /////////////////////////////////////////////////////////////////////////////
    /// \brief Get certain metadata's value, and return a default value provided
    /// by user if the metadata is not supported.
    /////////////////////////////////////////////////////////////////////////////
    template<metadataName md_name>
    inline typename metadataType<md_name>::type getMetadata(typename metadataType<md_name>::type default_value) const {
        const MetadataVariant& meta = mda_[md_name];
        if (meta.hasValue()) {
            return meta.get<typename metadataType<md_name>::type>();
        }
        else {
            return default_value;
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    /// \brief Get metadata data type used for storing certain metadata
    /// value inside CASK.
    ///
    /// \returns an enum value defined in MdValueType. If the metadata
    /// is unknown, then returns VAL_TYPE_UNKNOWN.
    ///
    /////////////////////////////////////////////////////////////////////////////
    inline MdValueType getValueType(const metadataName& md_name) const {
        return mda_[md_name].valueType();
    }

    /////////////////////////////////////////////////////////////////////////////
    /// \brief Get all of the valid metadata names for the current kernel.
    ///
    /// \returns a set of metadata names.
    ///
    /////////////////////////////////////////////////////////////////////////////
    MdSet_t getMetadataNames() const {
        MdSet_t mdset;
        for (int32_t i = 0; i < Size; i++) {
            metadataName md_name = metadataName(i);
            if (hasMetadata(md_name)) {
                mdset.insert(md_name);
            }
        }
        return mdset;
    }

    /////////////////////////////////////////////////////////////////////////////
    /// \brief Check if kernel's metadata can match the provided value set.
    ///
    /// \returns true if metadata value is matched, otherwise false.
    ///
    /////////////////////////////////////////////////////////////////////////////
    template<metadataName md_name>
    bool matchMetadata(typename metadataType<md_name>::type mdVal) const {
        bool matched = false;
        if (hasMetadata(md_name)) {
            matched = (mda_[md_name] == mdVal);
        }
        return matched;
    }

    template<metadataName md_name>
    inline void initMetadata(typename metadataType<md_name>::type val) {
        static_assert(md_name < Size, "");
        mda_[md_name] = MetadataVariant(val);
    }

private:

    MetadataVariant mda_[Size];

};

} // end namespace cask

#endif // INCLUDE_GUARD_CASK_METADATA_COLLECTION_H
