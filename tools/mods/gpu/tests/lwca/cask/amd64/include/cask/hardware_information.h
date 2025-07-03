#ifndef SAFE_INCLUDE_HARDWARE_INFO_H
#define SAFE_INCLUDE_HARDWARE_INFO_H

#include <cstdint>
#include <limits>
#include <map>

#include <lwda_runtime.h>
#include <lwca.h>

#include <cask/safe/preprocess.h>
#include <cask/safe/run_info.h>

namespace cask {

struct SmIsa {

  int32_t maj = 5;
  int32_t min = 0;

  SmIsa() = default;
  SmIsa(int32_t maj, int32_t min = 0);

  static Error queryFromDefaultDevice(SmIsa&);
  static Error queryFromDevice(SmIsa&, LWdevice);

  int32_t majorVersion() const;
  int32_t minorVersion() const;

};

bool operator<(const SmIsa& l, const SmIsa& r);
bool operator>(const SmIsa& l, const SmIsa& r);
bool operator<=(const SmIsa& l, const SmIsa& r);
bool operator>=(const SmIsa& l, const SmIsa& r);
bool operator==(const SmIsa& l, const SmIsa& r);
bool operator!=(const SmIsa& l, const SmIsa& r);

//? We don't need the instruction models just yet, so we'll hold off
//on adding those until a later update.

//? struct InstructionModel {
//?
//?     InstructionModel() = default;
//?
//?     int32_t issueInterval = 0;
//?     //!< The number of cycles before another of
//?     //!< this instruction can be issued on the same
//?     //!< datapath.
//?
//?     int32_t datapathsPerMultiprocessor = 0;
//?     //!< The number of datapaths within a multiprocessor
//?     //!< that support this instruction.
//?
//?     int32_t throttle = 1;
//?     //!< TODO
//?
//?     //! Is this instruction model valid, i.e. properly initialized?
//?     bool isValid() const { return datapathsPerMultiprocessor != 0; }
//? };
//?
//? struct MmaInstruction {
//?
//?     MmaInstruction() = default;
//?
//?     MmaShape Shape;
//?     NumericTraits A;
//?     NumericTraits B;
//?     NumericTraits C;
//?     NumericTraits D;
//?     NumericTraits Aclwmulator;
//?
//?     struct Hasher {
//?         size_t operator()(const MmaInstruction&);
//?     };
//? };

struct SmModel {

    SmModel() = default;
    SmModel(
        const SmIsa&,
        int32_t availableSharedMemory, int32_t numRegisters);
    SmModel(
        const SmIsa&,
	int32_t availableSharedMemory, int32_t numRegisters,
	int32_t l2ReadBwPerCycleInBytes, int32_t l2WriteBwPerCycleInBytes);

    SmIsa isa = {};                       //!< The supported compute capability
    int32_t availableSharedMemory = 0;    //!< The allocatable shared memory
    int32_t numRegisters = 0;             //!< The available registers

    int32_t l2ReadBwPerCycleInBytes = 0;  //!< The L2 read bandwidth per cycle
    int32_t l2WriteBwPerCycleInBytes = 0; //!< The L2 read bandwidth per cycle

    //? InstructionModel getInstructionModel(const MmaInstruction&) const;

    static Error queryFromDefaultDevice(SmModel&);
    static Error queryFromDevice(SmModel&, LWdevice);
};

/**
 *
 * */
class GpcAttributes {

    int32_t multiProcessorCount_ = 0;        /// Number of SMs

public:

    //
    // Methods
    //

    GpcAttributes();

    GpcAttributes(int32_t sm_count);

    ~GpcAttributes();

    /// Gets the multiprocessor count
    int32_t multiProcessorCount() const;
};

/**
 * The GPU skyline describes available GPC configurations
 * */
class GpcSkyline {
public:

    /// Iterator over GpcAttribute objects
    using Iterator = GpcAttributes *;

    /// Const Iterator over GpcAttribute objects
    using ConstIterator = GpcAttributes const *;

    /// Maximum supported GPC count
    static const int32_t MAX_GPC_COUNT = 32;

private:

    /// number of GPCs
    size_t size_;

    /// Array of GPC attributes
    GpcAttributes gpcs_[MAX_GPC_COUNT];

public:

    //
    // Methods
    //

    /// Ctor
    GpcSkyline();

    /// Ctor
    GpcSkyline(int32_t size);

    /// Ctor
    GpcSkyline(int32_t size, GpcAttributes const * start);

    /// Ctor
    GpcSkyline(GpcAttributes const * start, GpcAttributes const *end);

    /// Dtor
    ~GpcSkyline();

    /**
     *  Returns true if size() == 0
     *
     * */
    bool empty() const;

    /**
     *  Returns the number of GPCs
     * */
    size_t size() const;

    /**
     * Returns the maximum number of GPCs
     * */
    static int32_t maxGpcCount();

    /**
     *  Gets the GPC by index
     * */
    GpcAttributes &gpc(size_t idx);

    /**
     *  Gets the GPC by index
     * */
    GpcAttributes const &gpc(size_t idx) const;

    /**
     *
     * */
    Iterator begin();

    /**
     *
     * */
    Iterator end();

    /**
     *
     * */
    ConstIterator begin() const;

    /**
     *
     * */
    ConstIterator end() const;
};

/**
 * \brief Information about the target device hardward
 *
 * CASK host and device buffer do not necessarily need to be
 * initialized on the system where they are run.  However, some
 * initialization may use hardware-specific information for
 * heuristics. The POD HardwareInformation struct provides that
 * information. Lwrrently used for CTA Swizzle heuristics.
 *
 */
struct HardwareInformation
{
    HardwareInformation();

    //! Copy constructor taking an existing hardware information but applying
    //! a custom skyline.
    HardwareInformation(const HardwareInformation &, const GpcSkyline &);

    //! @Deprecated Use @ref HardwareInformation(GpcSkyline,int32_t,int32_t,int32_t)
    //! in place of this function. Define the target hardware platform in terms of
    //! multiprocessor count and available resources.
    HardwareInformation(
	int32_t multiprocessorCount,   //!< The available SM processor count
        int32_t availableSharedMemory, //!< The allocatable shared memory
        int32_t registersPerSm         //!< The available registers per SM
        );

    //! Describe the target hardware architecture in terms of multiprocessor count.
    HardwareInformation(
	SmIsa isa,                     //!< The compute capability of target architecture
	int32_t multiprocessorCount,   //!< The available SM processor count
        int32_t availableSharedMemory, //!< The allocatable shared memory
        int32_t registersPerSm         //!< The available registers per SM
        );

    //! Describe the target hardware architecture in terms of the GPC skyline.
    HardwareInformation(
	SmIsa isa,                     //!< The compute capability of target architecture
        GpcSkyline const& skyline,     //!< Defines the GPC configuration of the target GPU
        int32_t availableSharedMemory, //!< The allocatable shared memory
        int32_t registersPerSm         //!< The available registers per SM
        );

    //! Describe the target hardware architecture in terms of the GPC skyline.
    HardwareInformation(
	SmModel const& smModel,        //!< The multiprocessor model
	int32_t multiprocessorCount,   //!< The available SM processor count
        float effectiveSmClockRateInMhz = 0, //!< The SM clock rate
        float effectiveMemClockRateInMhz = 0 //!< The global memory clock rate
        );

    //! Describe the target hardware architecture in terms of the GPC skyline.
    HardwareInformation(
	SmModel const& smModel,        //!< The multiprocessor model
        GpcSkyline const& skyline,      //!< Defines the GPC configuration of the target GPU
        float effectiveSmClockRateInMhz = 0, //!< The SM clock rate
        float effectiveMemClockRateInMhz = 0 //!< The global memory clock rate
        );

    HardwareInformation(const HardwareInformation&);
    HardwareInformation& operator=(const HardwareInformation&);

    //
    // Data members
    //

     /**
     *  The multiprocessor configuration and capabilities
     */
    SmModel smModel;

    /**
     *  The compute capabilility of the target device.
     */
    SmIsa isa() const;

    /**
     * GPC skyline
     */
    GpcSkyline skyline;

    /**
     *  multiProcessorCount as returned by lwdaDeviceProp
     *  of the target device.
     */
    int32_t multiProcessorCount = 0;

    /**
     *  sharedMemPerMultiprocessor as returned by lwdaDeviceProp of
     *  the target device.
     *
     *  @deprecated Use @ref getSharedMemPerMultiprocessor(). This
     *  field must be kept in sync with SmModel settings.
     */
    int32_t sharedMemPerMultiprocessor = 0;

    /**
     *  regsPerMultiprocessor as returned by lwdaDeviceProp of the
     *  target device.
     *
     *  @deprecated Use @ref getRegistersPerMultiprocessor(). This
     *  field must be kept in sync with SmModel settings.
     */
    int32_t registersPerMultiprocessor = 0;

    /**
     *  Effective SM clock rate. This should describe the clock rate a
     *  compute intensive kernel should stabilize at for unlocked
     *  clocks, not necessarily the maximum or boost clock
     *  frequencies.
     */
    float effectiveSmClockRateInMhz = 1000;

    /**
     *  Effective global memory clock rate
     */
    float effectiveMemClockRateInMhz = 1000;

public:

    //! Generate a legacy numeric compute capability value by
    //! multiplying the major and minor versions by a factor and then
    //! summing them. e.g. SmIsa(5, 1).cc(1000, 10) = 5010.
    int32_t cc(int32_t majorFactor = 10, int32_t minorFactor = 1) const;

    /**
     *  sharedMemPerMultiprocessor as returned by lwdaDeviceProp of
     *  the target device.
     */
    int32_t getSharedMemPerMultiprocessor() const;

    /**
     *  regsPerMultiprocessor as returned by lwdaDeviceProp of the
     *  target device.
     */
    int32_t getRegistersPerMultiprocessor() const;

    //! Returns the ratio of the sm to global memory clock rates @ref
    //! effectiveSmClockRateInMhz and @ref
    //! effectiveMemoryClockRateInMhz respectively.
    //! @return sm_clock / mem_clock.
    float smToMemClockRatio() const;

public:

    //! Initializes the HardwareInformation struct via calls to the LWCA Driver API for the
    //! default device.
    static Error queryFromDefaultDevice(HardwareInformation&);

    //! Initializes the HardwareInformation struct via calls to the LWCA Driver API for the
    //! device given.
    static Error queryFromDevice(HardwareInformation&, LWdevice);
};

//! Computes the effective FMA throughput per SM cycle for the given
//! @ref HardwareInformation and the @ref NumericTraits base compute
//! type. @returns @ref Error::OK if query was succesful, another
//! @ref Error if unable to determine throughput for given parameters.
Error getFmaThroughputPerCycle(
    const HardwareInformation&, NumericTraits baseComputeType,
    int32_t& throughput);

//! Computes the effective L2 bandwidths from the SMs perspective
//! taking into account the relative clock frequencies of the L2 and
//! SM domains.
//! @returns Error::OK if bandwidths can be computed, another @Error if unsuccessful.
Error getL2BandwidthsPerSmCycleInBytes(
    const HardwareInformation&,
    int32_t& readBandwidth, int32_t& writeBandwidth);

} // namespace cask

#endif // SAFE_INCLUDE_HARDWARE_INFO_H
