/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLLWSWITCH_H
#define INCLUDED_UTLLWSWITCH_H

#if defined(INCLUDE_LWSWITCH)
#include "ctrl_dev_lwswitch.h"
#include "ctrl_dev_internal_lwswitch.h"
#endif
#include "gpu/include/testdevice.h"
#include "utlpython.h"
#include "utllwswitchregctrl.h"

class IRegisterClass;
class IRegisterMap;
class RefManual;

// UTL LwSwitch API described here:
// https://confluence.lwpu.com/display/ArchMods/LwSwitch

// UTL LwSwitch control implementation overview:
// This interface maps to UTL a subset of the structs, enums, and defines found
// in the LwSwitch driver interface (ctrl_dev_lwswitch.h). ControlStruct objects
// represent on the Python side the structs that we pass to the LwSwitch driver
// control call. These structs can have in them other structs, arrays of values,
// or arrays of other structs. We store these ControlStruct and Array objects in
// shared_ptrs so they can persist when the parent struct object goes away and
// references to these objects are still around in Python. For this reason,
// these Python objects have their own local values rather than directly point
// to the source struct's fields. These local values are copied to the final
// struct before the control call to the LwSwitch driver, and are copied back
// after the control call. Field objects store the local values of the various
// struct field types. These Field objects are not directly visible to Python,
// and are stored in unique_ptrs owned by the parent ControlStruct. Structs and
// arrays fields have reference semantics, and for now, we forbid assignment by
// struct or array. Integral fields have value semantics.

// This class represents the C++ backing for the UTL Python LwSwitch object.
class UtlLwSwitch
{
public:
    static void Register(py::module module);

    using PythonLwSwitch = py::class_<UtlLwSwitch>;

    UtlLwSwitch(UtlLwSwitch&) = delete;
    UtlLwSwitch& operator=(UtlLwSwitch&) = delete;

    // This is a dummy class for holding parameters on Python side.
    class ControlParams
    {
    };

    using PythonControlParams = py::class_<ControlParams>;

    // This class represents the C++ backing for the base class of a UTL Python
    // LwSwitch control struct.
    class ControlStruct
    {
    public:
        ControlStruct() = default;
        ControlStruct(ControlStruct&) = delete;
        ControlStruct& operator=(ControlStruct&) = delete;

        // Get a pointer to the source control struct
        virtual void* Get() = 0;

        // Return the string representation of this struct
        virtual string ToString() const = 0;

        // Get the size of the source control struct
        virtual size_t GetSize() const = 0;

        // Copy the Python field values to the source struct
        virtual void CopyToSource() const = 0;

        // Copy the Python field values from the source control struct
        virtual void CopyFromSource() const = 0;
    };

    using ControlStructPtr = shared_ptr<ControlStruct>;
    using PythonControlStruct = py::class_<ControlStruct, ControlStructPtr>;

    template <typename T>
    static void BindStruct
    (
        const PythonControlParams& pythonControlParams,
        const string& name
    )
    {
        PythonControlStructImpl<T> pythonControlStructImpl(
            pythonControlParams, name.c_str());
        ControlStructImpl<T>::Create()->Bind(pythonControlStructImpl);
    }

    template <typename T>
    static void BindIntegralArray
    (
        const PythonControlParams& pythonControlParams,
        const string& name
    )
    {
        BindArray<T>(pythonControlParams, name, false);
    }

    template <typename T>
    static void BindStructArray
    (
        const PythonControlParams& pythonControlParams,
        const string& name
    )
    {
        // Read only = true. Disallow struct assignment.
        BindArray<ControlStructImplPtr<T>>(pythonControlParams, name, true);
    }

    static void CreateLwSwitches();
    static void FreeLwSwitches();
    static py::list GetLwSwitches();

    UtlLwSwitchRegCtrl* GetRegCtrl(py::kwargs kwargs);
    std::unique_ptr<IRegisterClass> FindRegister(string regName);
    std::unique_ptr<IRegisterClass> FindRegister(UINT32 regAddress);

    enum class ControlCommand
    {
        GET_INFO,
        SET_SWITCH_PORT_CONFIG,
        GET_INTERNAL_LATENCY,
        SET_LATENCY_BINS,
        GET_LWLIPT_COUNTERS,
        SET_LWLIPT_COUNTER_CONFIG,
        GET_LWLIPT_COUNTER_CONFIG,
        SET_REMAP_POLICY,
        SET_ROUTING_ID,
        SET_ROUTING_LAN,
        SET_MC_RID_TABLE
    };

    void Control
    (
        LwLinkDevIf::LibInterface::LibControlId libControlId,
        void* controlParams,
        UINT32 paramSize
    );
    void ControlPy
    (
        py::kwargs kwargs
    );

private:
    using LwSwitchPtr = unique_ptr<UtlLwSwitch>;

    UtlLwSwitch
    (
        LwLinkDevIf::LibIfPtr pLibIf,
        LwLinkDevIf::LibInterface::LibDeviceHandle libDeviceHandle
    );

    // This is the list that has ownership of the LwSwitch objects.
    static vector<LwSwitchPtr> s_LwSwitches;

    // This is the list that is passed to Python for the GetLwSwitches function.
    // The m_LwSwitches member can't be used, otherwise Python will take
    // ownership of the LwSwitch objects due to the unique_ptr.
    static vector<UtlLwSwitch*> s_PythonLwSwitches;

    LwLinkDevIf::LibIfPtr m_pLibIf;
    LwLinkDevIf::LibInterface::LibDeviceHandle m_LibDeviceHandle;

    std::unique_ptr<IRegisterMap> m_RegMap;
    std::unique_ptr<RefManual> m_RefMan;
    string GetRefManualFile();
    std::unique_ptr<RefManual> GetRefManual(string manual) const;
    void ParseRefManual();

    template <typename T>
    class Array;
    template <typename T>
    using ArrayPtr = shared_ptr<Array<T>>;
    template <typename T>
    using ConstArrayPtr = shared_ptr<const Array<T>>;
    template <typename T>
    using PythonArray = py::class_<Array<T>, ArrayPtr<T>>;

    // This class represents the C++ backing for an array in a UTL Python
    // LwSwitch control struct. Ownership of an Array object is shared between
    // the parent ControlStruct object and any references to this object in
    // Python.
    template <typename T>
    class Array
    {
        vector<T> m_Vector;
        Array
        (
            size_t size
        ) :
            m_Vector(size)
        {
        }

    public:
        Array(Array&) = delete;
        Array& operator=(Array&) = delete;

        static ArrayPtr<T> Create
        (
            size_t size
        )
        {
            return move(ArrayPtr<T>(new Array<T>(size)));
        }

        T& operator[]
        (
            size_t position
        )
        {
            return m_Vector[position];
        };

        const T& operator[]
        (
            size_t position
        ) const
        {
            return m_Vector[position];
        }

        typename vector<T>::iterator Begin
        (
        ) noexcept
        {
            return m_Vector.begin();
        }

        typename vector<T>::iterator End
        (
        ) noexcept
        {
            return m_Vector.end();
        }

        size_t Size
        (
        ) const noexcept
        {
            return m_Vector.size();
        }
    };

    template <typename T>
    static void BindArray
    (
        const PythonControlParams& pythonControlParams,
        const string& name,
        bool readOnly
    )
    {
        const auto arrayName = string("Array__") + name;

        PythonArray<T> pythonArray(pythonControlParams, arrayName.c_str());

        pythonArray
            .def("__iter__",
                []
                (
                    Array<T> &array
                )
                {
                    return py::make_iterator(array.Begin(), array.End());
                },
                py::keep_alive<0, 1>())
            .def("__getitem__",
                []
                (
                    const Array<T> &array,
                    size_t index
                )
                {
                    if (index >= array.Size())
                    {
                        throw py::index_error();
                    }
                    return array[index];
                });

        if (!readOnly)
        {
            pythonArray
                .def("__setitem__",
                    []
                    (
                        Array<T> &array,
                        size_t index,
                        T value
                    )
                    {
                        if (index >= array.Size())
                        {
                            throw py::index_error();
                        }
                        array[index] = value;
                    });
        }
    }

    template <typename T>
    class ControlStructImpl;
    template <typename T>
    using ControlStructImplPtr = shared_ptr<ControlStructImpl<T>>;
    template <typename T>
    using PythonControlStructImpl =
        py::class_<ControlStructImpl<T>, ControlStructImplPtr<T>, ControlStruct>;

    // This class represents the C++ backing for a UTL Python LwSwitch control
    // struct. Ownership of this object is shared between the parent
    // ControlStruct object (if one exists), and references to this object in
    // Python.
    template <typename STRUCT_T>
    class ControlStructImpl : public ControlStruct
    {
        // This class represents the C++ backing for a single field of a UTL
        // Python LwSwitch control struct. Field objects are owned by the
        // parent ControlStruct object. Only the field value is bound to Python,
        // not the Field object itself.

        // Field objects store local values of the parameters from the
        // source control struct for use by Python. These local values are
        // copied to the source struct before being sent to the LwSwitch driver.
        class Field
        {
        public:
            Field() = default;
            Field(Field&) = delete;
            Field& operator=(Field&) = delete;

            // Return the string representation of this field
            virtual string ToString() const = 0;

            // Copy the local value(s) of this field to the source struct
            virtual void CopyToSource() const = 0;

            // Copy the local value(s) of this field from the source struct
            virtual void CopyFromSource() = 0;

            // Bind this as a field of this control struct
            virtual void Bind
            (
                PythonControlStructImpl<STRUCT_T>& pythonControlStructImpl
            ) = 0;

            // Get the name of this field
            virtual const string& GetName() const = 0;
        };

        using FieldPtr = unique_ptr<Field>;

        template <typename FIELD_T>
        class FieldImpl : public Field
        {
            const string m_Name;
            FIELD_T* const m_pSource;
        protected:
            FieldImpl
            (
                const string& name,
                FIELD_T* pSource
            ) :
                m_Name(name),
                m_pSource(pSource)
            {
            }

            FIELD_T* GetSource() const
            {
                return m_pSource;
            }

        public:
            FieldImpl(FieldImpl&) = delete;
            FieldImpl& operator=(FieldImpl&) = delete;

            const string& GetName() const override
            {
                return m_Name;
            }
        };

        template <typename FIELD_T>
        class IntegralField : public FieldImpl<FIELD_T>
        {
            FIELD_T m_Value;
            IntegralField
            (
                const string& name,
                FIELD_T* pSource
            ) :
                FieldImpl<FIELD_T>(name, pSource),
                m_Value()
            {
            }

        public:
            IntegralField(IntegralField&) = delete;
            IntegralField& operator=(IntegralField&) = delete;

            static FieldPtr Create
            (
                const string& name,
                FIELD_T* pSource
            )
            {
                return move(FieldPtr(
                        new IntegralField<FIELD_T>(name, pSource)));
            }

            virtual string ToString() const override
            {
                UINT64 value = m_Value;
                return Utility::StrPrintf("Field %s: 0x%llx\n",
                    this->GetName().c_str(), value);
            }

            virtual void CopyToSource() const override
            {
                *this->GetSource() = m_Value;
            }

            virtual void CopyFromSource() override
            {
                m_Value = *this->GetSource();
            }

            virtual void Bind
            (
                PythonControlStructImpl<STRUCT_T>& pythonControlStructImpl
            ) override
            {
                const auto& name = this->GetName();

                pythonControlStructImpl.def_property(name.c_str(),
                    [name]
                    (
                        const ControlStructImpl<STRUCT_T> &controlStructImpl
                    )
                    {
                        const auto pField = controlStructImpl.GetField(name);
                        MASSERT(pField != nullptr);
                        auto pThisField = static_cast<
                            const IntegralField<FIELD_T>*>(pField);
                        return pThisField->Get();
                    },
                    [name]
                    (
                        ControlStructImpl<STRUCT_T> &controlStructImpl,
                        FIELD_T value
                    )
                    {
                        auto pField = controlStructImpl.GetField(name);
                        MASSERT(pField != nullptr);
                        auto pThisField = static_cast<
                            IntegralField<FIELD_T>*>(pField);
                        pThisField->Set(value);
                    });
            }

            FIELD_T Get() const
            {
                return m_Value;
            }

            void Set
            (
                FIELD_T value
            )
            {
                m_Value = value;
            }
        };

        template <typename FIELD_T>
        class StructField : public FieldImpl<FIELD_T>
        {
            ControlStructImplPtr<FIELD_T> m_pControlStructImpl;
            StructField
            (
                const string& name,
                FIELD_T* pSource
            ) :
                FieldImpl<FIELD_T>(name, pSource),
                m_pControlStructImpl(ControlStructImpl<FIELD_T>::Create())
            {
            }

        public:
            StructField(StructField&) = delete;
            StructField& operator=(StructField&) = delete;

            static FieldPtr Create
            (
                const string& name,
                FIELD_T* pSource
            )
            {
                return move(FieldPtr(
                        new StructField<FIELD_T>(name, pSource)));
            }

            virtual string ToString() const override
            {
                return Utility::StrPrintf("Struct %s\n",
                    this->GetName().c_str()) +
                    m_pControlStructImpl->ToString();
            }

            virtual void CopyToSource() const override
            {
                m_pControlStructImpl->CopyToSource();
                *this->GetSource() =
                    *static_cast<const FIELD_T*>(m_pControlStructImpl->Get());
            }

            virtual void CopyFromSource() override
            {
                *static_cast<FIELD_T*>(m_pControlStructImpl->Get()) =
                    *this->GetSource();
                m_pControlStructImpl->CopyFromSource();
            }

            virtual void Bind
            (
                PythonControlStructImpl<STRUCT_T>& pythonControlStructImpl
            ) override
            {
                const auto& name = this->GetName();

                // Read only. Disallow struct assignment.
                pythonControlStructImpl.def_property_readonly(name.c_str(),
                    [name]
                    (
                        const ControlStructImpl<STRUCT_T> &controlStructImpl
                    )
                    {
                        const auto pField = controlStructImpl.GetField(name);
                        MASSERT(pField != nullptr);
                        auto pThisField = static_cast<
                            const StructField<FIELD_T>*>(pField);
                        return pThisField->Get();
                    });
            }

            ControlStructImplPtr<FIELD_T> Get() const
            {
                return m_pControlStructImpl;
            }
        };

        template <typename FIELD_T>
        class IntegralArrayField : public FieldImpl<FIELD_T>
        {
            ArrayPtr<FIELD_T> m_pArray;
            IntegralArrayField
            (
                const string& name,
                FIELD_T* pSource,
                size_t size
            ) :
                FieldImpl<FIELD_T>(name, pSource),
                m_pArray(Array<FIELD_T>::Create(size))
            {
            }

        public:
            IntegralArrayField(IntegralArrayField&) = delete;
            IntegralArrayField& operator=(IntegralArrayField&) = delete;

            static FieldPtr Create
            (
                const string& name,
                FIELD_T* pSource,
                size_t size
            )
            {
                return move(FieldPtr(new IntegralArrayField<FIELD_T>(
                            name, pSource, size)));
            }

            virtual string ToString() const override
            {
                string str;
                for (size_t i = 0; i < m_pArray->Size(); ++i)
                {
                    UINT64 value = (*m_pArray)[i];
                    str += Utility::StrPrintf("Field %s[%lu]: 0x%llx\n",
                        this->GetName().c_str(), i, value);
                }
                return str;
            }

            virtual void CopyToSource() const override
            {
                for (size_t i = 0; i < m_pArray->Size(); ++i)
                {
                    this->GetSource()[i] = (*m_pArray)[i];
                }
            }

            virtual void CopyFromSource() override
            {
                for (size_t i = 0; i < m_pArray->Size(); ++i)
                {
                     (*m_pArray)[i] = this->GetSource()[i];
                }
            }

            virtual void Bind
            (
                PythonControlStructImpl<STRUCT_T>& pythonControlStructImpl
            ) override
            {
                const auto& name = this->GetName();

                // Read only. Disallow array assignment.
                pythonControlStructImpl.def_property_readonly(name.c_str(),
                    [name]
                    (
                        const ControlStructImpl<STRUCT_T> &controlStructImpl
                    )
                    {
                        const auto pField = controlStructImpl.GetField(name);
                        MASSERT(pField != nullptr);
                        auto pThisField = static_cast<
                            const IntegralArrayField<FIELD_T>*>(pField);
                        return pThisField->Get();
                    });
            }

            ArrayPtr<FIELD_T> Get() const
            {
                return m_pArray;
            }
        };

        template <typename FIELD_T>
        class StructArrayField : public FieldImpl<FIELD_T>
        {
            ArrayPtr<ControlStructImplPtr<FIELD_T>> m_pArray;
            StructArrayField
            (
                const string& name,
                FIELD_T* pSource,
                size_t size
            ) :
                FieldImpl<FIELD_T>(name, pSource),
                m_pArray(Array<ControlStructImplPtr<FIELD_T>>::Create(size))
            {
                for (UINT32 i = 0; i < size; ++i)
                {
                    (*m_pArray)[i] = ControlStructImpl<FIELD_T>::Create();
                }
            }
        public:
            StructArrayField(StructArrayField&) = delete;
            StructArrayField& operator=(StructArrayField&) = delete;

            static FieldPtr Create
            (
                const string& name,
                FIELD_T* pSource,
                size_t size
            )
            {
                return move(FieldPtr(new StructArrayField<FIELD_T>(
                            name, pSource, size)));
            }

            virtual string ToString() const override
            {
                string str;
                for (size_t i = 0; i < m_pArray->Size(); ++i)
                {
                    str += Utility::StrPrintf("StructArray %s[%lu]\n",
                        this->GetName().c_str(), i);
                    str += (*m_pArray)[i]->ToString();
                }
                return str;
            }

            virtual void CopyToSource() const override
            {
                for (size_t i = 0; i < m_pArray->Size(); ++i)
                {
                    (*m_pArray)[i]->CopyToSource();
                    this->GetSource()[i] = *static_cast<const FIELD_T*>(
                        (*m_pArray)[i]->Get());
                }
            }

            virtual void CopyFromSource() override
            {
                for (size_t i = 0; i < m_pArray->Size(); ++i)
                {
                    *static_cast<FIELD_T*>((*m_pArray)[i]->Get()) =
                        this->GetSource()[i];
                    (*m_pArray)[i]->CopyFromSource();
                }
            }

            virtual void Bind
            (
                PythonControlStructImpl<STRUCT_T>& pythonControlStructImpl
            ) override
            {
                const auto& name = this->GetName();

                // Read only. Disallow array assignment.
                pythonControlStructImpl.def_property_readonly(name.c_str(),
                    [name]
                    (
                        const ControlStructImpl<STRUCT_T> &controlStructImpl
                    )
                    {
                        const auto pField = controlStructImpl.GetField(name);
                        MASSERT(pField != nullptr);
                        auto pThisField = static_cast<
                            const StructArrayField<FIELD_T>*>(pField);
                        return pThisField->Get();
                    });
            }

            ConstArrayPtr<ControlStructImplPtr<FIELD_T>> Get() const
            {
                return m_pArray;
            }
        };

        // Source struct that we pass to the LwSwitch driver
        STRUCT_T m_Struct;

        // List of fields for this struct that we bind to Python
        vector<FieldPtr> m_Fields;

        ControlStructImpl()
        {
            memset(&m_Struct, 0, sizeof(STRUCT_T));
            Build(&m_Struct);
        }

        void AddField
        (
            FieldPtr pField
        )
        {
            m_Fields.push_back(move(pField));
        }

        template <typename FIELD_T>
        void AddIntegralField
        (
            const string& name,
            FIELD_T* pStruct
        )
        {
            AddField(move(IntegralField<FIELD_T>::Create(name, pStruct)));
        }

        template <typename FIELD_T>
        void AddStructField
        (
            const string& name,
            FIELD_T* pStruct
        )
        {
            AddField(move(StructField<FIELD_T>::Create(name, pStruct)));
        }

        template <typename FIELD_T>
        void AddIntegralArrayField
        (
            const string& name,
            FIELD_T* pStruct,
            size_t size
        )
        {
            AddField(move(IntegralArrayField<FIELD_T>::Create(
                        name, pStruct, size)));
        }

        template <typename FIELD_T>
        void AddStructArrayField
        (
            const string& name,
            FIELD_T* pStruct,
            size_t size
        )
        {
            AddField(move(StructArrayField<FIELD_T>::Create(
                        name, pStruct, size)));
        }

#if defined(INCLUDE_LWSWITCH)
        // Each Build function defines the fields of the struct we bind to
        // Python.
        void Build
        (
            LWSWITCH_GET_INFO* pStruct
        )
        {
            AddIntegralField<LwU32>("count", &pStruct->count);
            AddIntegralArrayField<LwU32>("index",
                &pStruct->index[0], LWSWITCH_GET_INFO_COUNT_MAX);
            AddIntegralArrayField<LwU32>("info",
                &pStruct->info[0], LWSWITCH_GET_INFO_COUNT_MAX);
        }

        void Build
        (
            LWSWITCH_REMAP_POLICY_ENTRY* pStruct
        )
        {
            AddIntegralField<LwBool>("entryValid", &pStruct->entryValid);
            AddIntegralField<LwU32>("targetId", &pStruct->targetId);
            AddIntegralField<LwU32>("irlSelect", &pStruct->irlSelect);
            AddIntegralField<LwU32>("flags", &pStruct->flags);
            AddIntegralField<LwU64>("address", &pStruct->address);
            AddIntegralField<LwU32>("reqCtxMask", &pStruct->reqCtxMask);
            AddIntegralField<LwU32>("reqCtxChk", &pStruct->reqCtxChk);
            AddIntegralField<LwU32>("reqCtxRep", &pStruct->reqCtxRep);
            AddIntegralField<LwU64>("addressOffset", &pStruct->addressOffset);
            AddIntegralField<LwU64>("addressBase", &pStruct->addressBase);
            AddIntegralField<LwU64>("addressLimit", &pStruct->addressLimit);
        }

        void Build
        (
            LWSWITCH_SET_REMAP_POLICY* pStruct
        )
        {
            AddIntegralField<LwU32>("portNum", &pStruct->portNum);
            AddIntegralField<LwU32>("tableSelect", (LwU32*)(&pStruct->tableSelect));
            AddIntegralField<LwU32>("firstIndex", &pStruct->firstIndex);
            AddIntegralField<LwU32>("numEntries", &pStruct->numEntries);
            AddStructArrayField<LWSWITCH_REMAP_POLICY_ENTRY>("remapPolicy",
                &pStruct->remapPolicy[0], LWSWITCH_REMAP_POLICY_ENTRIES_MAX);
        }

        void Build
        (
            LWSWITCH_ROUTING_ID_DEST_PORT_LIST* pStruct
        )
        {
            AddIntegralField<LwU32>("vcMap", &pStruct->vcMap);
            AddIntegralField<LwU32>("destPortNum", &pStruct->destPortNum);
        }

        void Build
        (
            LWSWITCH_ROUTING_ID_ENTRY* pStruct
        )
        {
            AddIntegralField<LwBool>("entryValid", &pStruct->entryValid);
            AddIntegralField<LwBool>("useRoutingLan", &pStruct->useRoutingLan);
            AddIntegralField<LwBool>("enableIrlErrResponse",
                &pStruct->enableIrlErrResponse);
            AddIntegralField<LwU32>("numEntries", &pStruct->numEntries);
            AddStructArrayField<LWSWITCH_ROUTING_ID_DEST_PORT_LIST>("portList",
                &pStruct->portList[0], LWSWITCH_ROUTING_ID_DEST_PORT_LIST_MAX);
        }

        void Build
        (
            LWSWITCH_SET_ROUTING_ID* pStruct
        )
        {
            AddIntegralField<LwU32>("portNum", &pStruct->portNum);
            AddIntegralField<LwU32>("firstIndex", &pStruct->firstIndex);
            AddIntegralField<LwU32>("numEntries", &pStruct->numEntries);
            AddStructArrayField<LWSWITCH_ROUTING_ID_ENTRY>("routingId",
                &pStruct->routingId[0], LWSWITCH_ROUTING_ID_ENTRIES_MAX);
        }

        void Build
        (
            LWSWITCH_ROUTING_LAN_PORT_SELECT* pStruct
        )
        {
            AddIntegralField<LwU32>("groupSelect", &pStruct->groupSelect);
            AddIntegralField<LwU32>("groupSize", &pStruct->groupSize);
        }

        void Build
        (
            LWSWITCH_ROUTING_LAN_ENTRY* pStruct
        )
        {
            AddIntegralField<LwBool>("entryValid", &pStruct->entryValid);
            AddIntegralField<LwU32>("numEntries", &pStruct->numEntries);
            AddStructArrayField<LWSWITCH_ROUTING_LAN_PORT_SELECT>("portList",
                &pStruct->portList[0], LWSWITCH_ROUTING_LAN_GROUP_SEL_MAX);
        }

        void Build
        (
            LWSWITCH_SET_ROUTING_LAN* pStruct
        )
        {
            AddIntegralField<LwU32>("portNum", &pStruct->portNum);
            AddIntegralField<LwU32>("firstIndex", &pStruct->firstIndex);
            AddIntegralField<LwU32>("numEntries", &pStruct->numEntries);
            AddStructArrayField<LWSWITCH_ROUTING_LAN_ENTRY>("routingLan",
                &pStruct->routingLan[0], LWSWITCH_ROUTING_LAN_ENTRIES_MAX);
        }

        void Build
        (
            LWSWITCH_INTERNAL_LATENCY_BINS* pStruct
        )
        {
            AddIntegralField<LwU64>("low", &pStruct->low);
            AddIntegralField<LwU64>("medium", &pStruct->medium);
            AddIntegralField<LwU64>("high", &pStruct->high);
            AddIntegralField<LwU64>("panic", &pStruct->panic);
        }

        void Build
        (
            LWSWITCH_GET_INTERNAL_LATENCY* pStruct
        )
        {
            AddIntegralField<LwU32>("vc_selector", &pStruct->vc_selector);
            AddIntegralField<LwU64>("elapsed_time_msec",
                &pStruct->elapsed_time_msec);
            AddStructArrayField<LWSWITCH_INTERNAL_LATENCY_BINS>("egressHistogram",
                &pStruct->egressHistogram[0], LWSWITCH_MAX_PORTS);
        }

        void Build
        (
            LWSWITCH_LATENCY_BIN* pStruct
        )
        {
            AddIntegralField<LwU32>("lowThreshold", &pStruct->lowThreshold);
            AddIntegralField<LwU32>("medThreshold", &pStruct->medThreshold);
            AddIntegralField<LwU32>("hiThreshold", &pStruct->hiThreshold);
        }

        void Build
        (
            LWSWITCH_SET_LATENCY_BINS* pStruct
        )
        {
            AddStructArrayField<LWSWITCH_LATENCY_BIN>("bin",
                &pStruct->bin[0], LWSWITCH_MAX_VCS);
        }

        void Build
        (
            LWSWITCH_SET_SWITCH_PORT_CONFIG* pStruct
        )
        {
            AddIntegralField<LwU32>("portNum", &pStruct->portNum);
            AddIntegralField<LwU32>("type", &pStruct->type);
            AddIntegralField<LwU32>("requesterLinkID", &pStruct->requesterLinkID);
            AddIntegralField<LwBool>("acCoupled", &pStruct->acCoupled);
            AddIntegralField<LwBool>("enableVC1", &pStruct->enableVC1);
        }

        void Build
        (
            LWSWITCH_LWLIPT_COUNTER* pStruct
        )
        {
            AddIntegralField<LwU64>("txCounter0", &pStruct->txCounter0);
            AddIntegralField<LwU64>("txCounter1", &pStruct->txCounter1);
            AddIntegralField<LwU64>("rxCounter0", &pStruct->rxCounter0);
            AddIntegralField<LwU64>("rxCounter1", &pStruct->rxCounter1);
        }

        void Build
        (
            LWSWITCH_GET_LWLIPT_COUNTERS* pStruct
        )
        {
            AddStructArrayField<LWSWITCH_LWLIPT_COUNTER>("liptCounter",
                &pStruct->liptCounter[0], LWSWITCH_MAX_PORTS);
        }

        void Build
        (
            LWLIPT_COUNTER_CONFIG* pStruct
        )
        {
            AddIntegralField<LwU32>("ctrl_0", &pStruct->ctrl_0);
            AddIntegralField<LwU32>("ctrl_1", &pStruct->ctrl_1);
            AddIntegralField<LwU32>("req_filter", &pStruct->req_filter);
            AddIntegralField<LwU32>("rsp_filter", &pStruct->rsp_filter);
            AddIntegralField<LwU32>("misc_filter", &pStruct->misc_filter);
            AddIntegralField<LwU64>("addr_filter", &pStruct->addr_filter);
            AddIntegralField<LwU64>("addr_mask", &pStruct->addr_mask);
        }

        void Build
        (
            LWSWITCH_SET_LWLIPT_COUNTER_CONFIG* pStruct
        )
        {
            AddIntegralField<LwU64>("link_mask", &pStruct->link_mask);
            AddStructField<LWLIPT_COUNTER_CONFIG>("tx0", &pStruct->tx0);
            AddStructField<LWLIPT_COUNTER_CONFIG>("tx1", &pStruct->tx1);
            AddStructField<LWLIPT_COUNTER_CONFIG>("rx0", &pStruct->rx0);
            AddStructField<LWLIPT_COUNTER_CONFIG>("rx1", &pStruct->rx1);
        }

        void Build
        (
            LWSWITCH_GET_LWLIPT_COUNTER_CONFIG* pStruct
        )
        {
            AddIntegralField<LwU32>("link", &pStruct->link);
            AddStructField<LWLIPT_COUNTER_CONFIG>("tx0", &pStruct->tx0);
            AddStructField<LWLIPT_COUNTER_CONFIG>("tx1", &pStruct->tx1);
            AddStructField<LWLIPT_COUNTER_CONFIG>("rx0", &pStruct->rx0);
            AddStructField<LWLIPT_COUNTER_CONFIG>("rx1", &pStruct->rx1);
        }

        void Build
        (
            LWSWITCH_SET_MC_RID_TABLE_PARAMS* pStruct
        )
        {
            AddIntegralField<LwU32>("portNum", &pStruct->portNum);
            AddIntegralField<LwU32>("index", &pStruct->index);
            AddIntegralField<LwBool>("extendedTable", &pStruct->extendedTable);
            AddIntegralArrayField<LwU32>("ports", 
                &pStruct->ports[0], LWSWITCH_MC_MAX_PORTS);
            AddIntegralArrayField<LwU8>("vcHop", 
                &pStruct->vcHop[0], LWSWITCH_MC_MAX_PORTS);
            AddIntegralField<LwU32>("mcSize", &pStruct->mcSize);
            AddIntegralField<LwU32>("numSprayGroups", &pStruct->numSprayGroups);
            AddIntegralArrayField<LwU32>("portsPerSprayGroup", 
                &pStruct->portsPerSprayGroup[0], LWSWITCH_MC_MAX_SPRAYGROUPS);
            AddIntegralArrayField<LwU32>("replicaOffset", 
                &pStruct->replicaOffset[0], LWSWITCH_MC_MAX_SPRAYGROUPS);
            AddIntegralArrayField<LwBool>("replicaValid", 
                &pStruct->replicaValid[0], LWSWITCH_MC_MAX_SPRAYGROUPS);
            AddIntegralField<LwU32>("extendedPtr", &pStruct->extendedPtr);
            AddIntegralField<LwBool>("extendedValid", &pStruct->extendedValid);
            AddIntegralField<LwBool>("noDynRsp", &pStruct->noDynRsp);
            AddIntegralField<LwBool>("entryValid", &pStruct->entryValid);
        }

#endif

    public:
        ControlStructImpl(ControlStructImpl&) = delete;
        ControlStructImpl& operator=(ControlStructImpl&) = delete;

        static ControlStructImplPtr<STRUCT_T> Create()
        {
            return move(ControlStructImplPtr<STRUCT_T>(
                    new ControlStructImpl<STRUCT_T>));
        }

        void Bind
        (
            PythonControlStructImpl<STRUCT_T>& pythonControlStructImpl
        )
        {
            pythonControlStructImpl.def(py::init(&Create));
            for (const auto& field : m_Fields)
            {
                field->Bind(pythonControlStructImpl);
            }
        }

        Field* GetField
        (
            const string& name
        ) const
        {
            for (const auto& field : m_Fields)
            {
                if (field->GetName() == name)
                {
                    return field.get();
                }
            }
            return nullptr;
        }

        virtual void* Get() override
        {
            return &m_Struct;
        }

        virtual string ToString() const override
        {
            string str;
            for (const auto& pField : m_Fields)
            {
                str += pField->ToString();
            }
            return str;
        };

        virtual size_t GetSize() const override
        {
            return sizeof(STRUCT_T);
        }

        virtual void CopyToSource() const override
        {
            for (const auto& pField : m_Fields)
            {
                pField->CopyToSource();
            }
        }

        virtual void CopyFromSource() const override
        {
            for (auto& pField : m_Fields)
            {
                pField->CopyFromSource();
            }
        }
    };
};

#endif
