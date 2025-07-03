/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOSTMRCALLBACK_H
#define LWOSTMRCALLBACK_H

/*!
 * @file     lwostmrcallback.h
 *
 * @brief    Centralized timer callback infrastructure for ucode.
 *
 * @details  This unit provides a timer callback infrastructure with supported
 *           types of callbacks and supported operations that users can perform
 *           to interact with the callbacks. The infrastructure is centralized
 *           in the sense that all the scheduled callbacks are captured within a
 *           single queue of the infrastructure. The infrastructure supports
 *           dual-mode operation (`NORMAL` mode and `SLEEP` mode), one mode at a
 *           a time. However, for some cases, `SLEEP` mode can be disabled to
 *           simplify the procedure.
 *
 *           In order to start using the infrastructure, it's recommended to
 *           start with the following sections:
 *           - @ref OS_TMR_CALLBACK_MODE to understand callback operation modes.
 *           - @ref OS_TMR_CALLBACK_TYPE to see available types of callbacks.
 *           - @ref OS_TMR_CALLBACK_STATE to understand state machine of a
 *             callback and the operation/condition for each transition.
 *           - @ref OsTmrCallbackFunc to see what callback function that the
 *             clients need to implement and how it's ilwoked.
 *
 *           Here are some assumptions that the infrastructure is based on:
 *           1. @ref lwrtosHookTimerTick is ilwoked once every OS tick in order
 *              to guarantee callback expiration detection at the granularity of
 *              OS ticks.
 *           2. @ref lwrtosQueueSendFromISRWithStatus is expected to deliver
 *              callback expiration notifications through LWRTOS queues, not
 *              through interrupts.
 *           3. Critical sections created by @ref lwrtosENTER_CRITICAL are
 *              mutually exclusive to protect callbacks data consistency.
 *           5. Compiler recognizes `__attribute__ ((__packed__))` syntax.
 *
 *           Build time configurations:
 *           1. Callback statistics:
 *              - It uses significant amount of DMEM and is disabled by default.
 *              - Set @ref OS_CALLBACKS_WSTATS to true to turn it on.
 *           2. `SLEEP` mode support:
 *              - The `SLEEP` mode is supported by default, along with the 
 *                complementary `NORMAL` mode.
 *              - Set @ref OS_CALLBACKS_SINGLE_MODE to true to turn it off.
 */

/* ------------------------ System includes --------------------------------- */
#include "lwuproc.h"
#include "lwrtos.h"
#include "lwostmrcmn.h"

/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief    Enumeration of supported callback infrastructure operation modes.
 *
 * @details  The centralized callback infrastructure supports 2 runtime modes,
 *           `NORMAL` mode and `SLEEP` mode, and can switch mode at any time and
 *           will take effect at next OS tick. If @ref OS_CALLBACKS_SINGLE_MODE
 *           is set to true at build time, `SLEEP` mode is unavailable. Each
 *           individual callback can choose to support at least one mode with a
 *           valid period. When the callback infrastructure runs in one mode,
 *           callbacks with valid period for the mode will be enabled. The
 *           convention is to have callbacks to operate at higher frequency in
 *           `NORMAL` mode (high power) and lower frequency in `SLEEP` mode
 *           (low power).
 *
 * @note     Common use cases:
 *           - Longer `SLEEP` mode period (default)
 *           - Same periods: runs in both modes the same way
 *           - `NORMAL` mode period only: completely turned off during `SLEEP`
 *
 */
typedef enum __attribute__ ((__packed__))
{
    OS_TMR_CALLBACK_MODE_NORMAL = 0x0U,
#ifndef OS_CALLBACKS_SINGLE_MODE
#ifndef OS_CALLBACKS_RELAXED_MODE
    OS_TMR_CALLBACK_MODE_SLEEP_ALWAYS  = 0x1U,
    OS_TMR_CALLBACK_MODE_COUNT  = 0x2U
#else
    OS_TMR_CALLBACK_MODE_SLEEP_ALWAYS  = 0x1U,
    OS_TMR_CALLBACK_MODE_SLEEP_RELAXED = 0x2U,
    OS_TMR_CALLBACK_MODE_COUNT  = 0x3U
#endif // OS_CALLBACKS_RELAXED_MODE 
#else
    OS_TMR_CALLBACK_MODE_COUNT  = 0x1U
#endif // OS_CALLBACKS_SINGLE_MODE
} OS_TMR_CALLBACK_MODE;

/*!
 * @brief    Defines for SLEEP_RELAXED MODE
 *
 * @details  SLEEP_RELAXED mode can either use the period from NORMAL mode 
 *           or SLEEP mode. The boolean passed into osTmrCallbackCreate is
 *           defined using more explicit naming for readability.
 *
 */
#define OS_TIMER_RELAXED_MODE_USE_NORMAL LW_FALSE
#define OS_TIMER_RELAXED_MODE_USE_SLEEP  LW_TRUE

/*!
 * @brief    Use stored tick base or current tick as base
 *
 * @details  Update function can update the new expiry time using either
 *           the current timer tick as base or using the old "stored" 
 *           tick base (expiry tick - period) as base.
 */
#define OS_TIMER_UPDATE_USE_BASE_STORED  LW_FALSE
#define OS_TIMER_UPDATE_USE_BASE_LWRRENT LW_TRUE

/*!
 * @brief    Enumeration of supported callback types.
 *
 * @details  The callbacks can be either `ONE_SHOT` or `RELWRRENT`. `RELWRRENT`
 *           ones allow several sub-types depending on rescheduling method. Each
 *           instance of a `RELWRRENT` callback is scheduled to expire at:
 *           - `expiration time (ET) = base time (BT) + period (P)`.
 *
 *           All callbacks share the same key equation above, but `BT` is
 *           defined differently to provide different callback properties.
 *
 *           Supporting `SLEEP` mode brings more complexity. Each callback is
 *           now tracked with up to 2 periods, one for each mode, normal period
 *           (`NP`) and sleep period (`SP`), and hence has a set of two
 *           expiration times, `ET_normal` and `ET_sleep` for `NORMAL` and
 *           `SLEEP` mode respectively:
 *           - `ET_normal = BT + NP`
 *           - `ET_sleep = BT + SP`
 *
 *           One of the expiration time will trigger depending on the
 *           infrastructure operation mode at the moment of the OS tick. If
 *           rescheduled, a pair of expiration times will be generated. This
 *           way, mode switch at random OS tick is supported efficiently.
 *
 * @note     The differences between the `RELWRRENT` types are best shown by an
 *           example. Feel free to use this example to cross check your
 *           understanding of each callback types. For the sack of completeness
 *           this example has `SLEEP` mode turned on:
 *           1. Assumptions (unit: OS tick):
 *             - `NORMAL` mode period, `NP = 10`
 *             - `SLEEP` mode period, `SP = 155`,
 *               intentionally avoiding multiple of `NP`
 *             - callback infrastructure starts in `SLEEP` mode
 *           2. Events:
 *             - Tick 0: Callback created and scheduled
 *                  - `BT = 0`
 *                  - `ET_normal = 0 + 10 = 10`
 *                  - `ET_sleep = 0 + 155 = 155`
 *             - Tick 155: Expiration was detected by code embedded in ISR
 *                  - `ET_actual` is
 *                    `ET_sleep` since the callback expired in `SLEEP` mode
 *             - Tick 158: Callback dispatcher runs after a few OS ticks
 *                  - The callback notification was sent to the queue of the
 *                    task
 *             - Tick 161: User defined callback function exelwtion starts after
 *               some more delay
 *                  - `START = 161`
 *             - Tick 164: User defined callback function exelwtion ends
 *                  - `END = 164`
 *           3. Analyze different `RELWRRENT` callback types:
 *             - @ref OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_NO_SKIP
 *               - `BT_next = 10 + floor(155 - 10, 10) x 10 = 150`
 *               - `ET_next_normal = BT_next + NP = 150 + 10 = 160`
 *               - `ET_next_sleep = BT_next + SP = 150 + 155 = 305`
 *               - If mode switch happened at 154, `ET_normal` as 10 would have
 *                 been immediately detected, and
 *                 `BT_next = 10 + floor(10 - 10, 10) x 10 = 10`.
 *             - @ref OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED
 *               - `BT_next = 10 + floor(164 - 10, 10) x 10 = 160`
 *             - @ref OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_ACTUAL_START
 *               - `BT_next = START = 161`
 *             - @ref OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_ACTUAL_END
 *               - `BT_next = END = 164`
 */
typedef enum __attribute__ ((__packed__))
{
    /*!
     * @brief    The callback will only be scheduled once and not repeatedly.
     *
     * @details  `ONE_SHOT` callback can be used to schedule one instance of
     *           delayed action. The callback will not be rescheduled after
     *           exelwtion. But the callback structure can be reused and
     *           scheduled again **outside it's own callback function**.
     */
    OS_TMR_CALLBACK_TYPE_ONE_SHOT                          = 0x0U,
    /*!
     * @brief    Callback is automatically rescheduled with `ET` aligned and no
     *           missed instances skipped.
     *
     * @details  The key idea of this type of callback is to have strictly
     *           aligned and spaced `ET` for each instance despite the fact that
     *           in reality the callback exelwtion can be delayed and even
     *           missed. `ET` does not drift over time and the missed instances
     *           are scheduled and exelwted as soon as possible.
     *
     *           In terms of formula (`SLEEP` mode disabled):
     *               - `BT_next = ET`
     *               - `ET_next = BT_next + P = ET + P`
     *
     *           Most of the extra complexity brought by `SLEEP` mode comes to
     *           this type. The formula to callwlate `ET_next` is generalized
     *           with the following two points:
     *           1. Valid `NP` is used to align `BT`, otherwise `SP` is used.
     *           2. Since `SP` doesn't have to be multiple of `NP`, when the
     *              callback is expired in `SLEEP` mode, `BT_next` has to be the
     *              largest aligned `BT` less or equal to previous expiration
     *              time set for `SLEEP` mode, `ET_sleep`.
     *
     *           Again, in terms of formula (`SLEEP` mode enabled):
     *               - `BT_next = ET_normal + floor(ET_actual - ET_normal, NP) x NP`
     *               - `ET_next_normal = BT_next + NP`
     *               - `ET_next_sleep  = BT_next + SP`
     *
     * @note     It is suggested to have `NP` and `SP` the same for this type of
     *           callback if both are valid. Mode switch can happen in the
     *           following sequence and can cause issue:
     *           1. switch to `SLEEP` mode
     *           2. before `SLEEP` mode expire but very close to that, switch
     *              back to `NORMAL` mode
     *
     * @note     If `SLEEP` mode period is much larger than `NORMAL` mode
     *           period, we are creating missed instances to be made up for in
     *           burst.
     *
     */
    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_NO_SKIP     = 0x1U,
    /*!
     * @brief    Callback is automatically rescheduled with `ET` aligned and
     *           missed instances skipped.
     *
     * @details  This is less restricted than
     *           @ref OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_NO_SKIP.
     *           `ETs` are spaced with multiples of `P`. But the missed
     *           instances are skipped. The next instance is scheduled beyond
     *           the time when the current instance finishes (`END`). Here is
     *           how we callwlate `ET_next` so that instances between `ET` and
     *           the actual end time of the current instance are skipped:
     *               - `BT_next = ET_previous + floor(END - ET, P) x P`
     *               - `ET_next = BT_next + p`
     *
     *           When `SLEEP` mode is turned on, the formula is generalized to:
     *               - `BT_next = ET_normal + floor(END - ET_normal, NP) x NP`
     *               - `ET_next_normal = BT_next + NP`
     *               - `ET_next_sleep  = BT_next + SP`
     */
    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED = 0x2U,
    /*!
     * @brief    Callback is automatically rescheduled, and the `BT` to schedule
     *           next instance is set to the actual callback's exelwtion start
     *           time (`START`).
     *
     * @details  Time it takes from `ET` to the actual `START` contributes
     *           to the delay to schedule next instance.
     */
    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_ACTUAL_START         = 0x3U,
    /*!
     * @brief    Callback is automatically rescheduled, and the `BT` to schedule
     *           next instance is set to the actual callback's exelwtion end
     *           time (`END`).
     *
     * @details  Time it takes from `ET` to the actual `END` contributes to
     *           the delay to schedule next instance.
     * 
     * @note     Both @ref OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_ACTUAL_START and
     *           @ref OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_ACTUAL_END callbacks are
     *           ideal to form a chain of action with dynamically callwlated
     *           delay between each instances.
     */
    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_ACTUAL_END           = 0x4U,
    /*!
     * @brief    Number of supported types. 
     */
    OS_TMR_CALLBACK_TYPE_COUNT                             = 0x5U
} OS_TMR_CALLBACK_TYPE;

/*!
 * @brief  Macro to set the callback infrastructure operation mode captured in
 *         @ref OsTmrCallback.
 */
#define OS_TMR_CALLBACK_MODE_SET(setMode)                                      \
    do {                                                                       \
        OsTmrCallback.mode = OS_TMR_CALLBACK_MODE_##setMode;                   \
    } while (LW_FALSE)

/*!
 * @brief    Enumeration of states of a callback.
 */
typedef enum __attribute__ ((__packed__))
{
    /*!
     * @brief    Compulsory state (all fields are zero) before calling create.
     *
     * @details  `CREATED` state is the only legal next state for `START`:
     *           @startuml "State transitions for START"
     *              START -> CREATED : create
     *           @enduml
     */
    OS_TMR_CALLBACK_STATE_START     = 0x0U,
    /*!
     * @brief    Callback is initialized and ready to be scheduled.
     *
     * @details  `CREATED` eventually leads to `SCHEDULED` state, and also
     *           provides an opportunity for update:
     *           @startuml "State transitions for CREATED"
     *              CREATED -->> CREATED : update
     *              CREATED -> SCHEDULED : schedule
     *           @enduml
     */
    OS_TMR_CALLBACK_STATE_CREATED   = 0x1U,
    /*!
     * @brief    Callback is waiting to get expired. Allows cancelation without
     *           notifying the task.
     *
     * @details  Here are the possible state transitions for `SCHEDULED`:
     *           @startuml "State transitions for SCHEDULED"
     *              SCHEDULED -->> SCHEDULED : update
     *              SCHEDULED -->> CREATED : cancel
     *              SCHEDULED -> QUEUED : if expired on OS timer tick
     *           @enduml
     */
    OS_TMR_CALLBACK_STATE_SCHEDULED = 0x2U,
    /*!
     * @brief    Callback is in queue of respective task waiting for exelwtion.
     *
     * @details  Allows cancelation however task will get notified since
     *           callback expiration notification has been sent. Once the task
     *           receives the notification, it should call `execute` which will
     *           either ilwoke the registered callback function or handle
     *           pending cancellation. Here are the possible state transitions
     *           for `QUEUED`:
     *           @startuml "State transitions for QUEUED"
     *              QUEUED -->> QUEUED : update
     *              QUEUED -->> "QUEUED (cancelation pending)" : cancel
     *              QUEUED -> EXELWTED : execute
     *              "QUEUED (cancelation pending)" -->> "QUEUED (cancelation pending)" : update
     *              "QUEUED (cancelation pending)" -->> QUEUED : schedule
     *              "QUEUED (cancelation pending)" -->> CREATED : execute
     *           @enduml
     */
    OS_TMR_CALLBACK_STATE_QUEUED    = 0x3U,
    /*!
     * @brief    Callback is being exelwted and will be rescheduled if
     *           relwrrent. Allows cancelation by skipping callback's
     *           rescheduling.
     *
     * @details  This is the state that the callback function sees. Inside the
     *           callback function, it has the option to make the following
     *           transitions:
     *           @startuml "State transitions for EXELWTED"
     *              EXELWTED -->> EXELWTED : update
     *              EXELWTED -->> "EXELWTED (cancelation pending)" : cancel
     *              EXELWTED -> CREATED : if (type == OS_TMR_CALLBACK_TYPE_ONE_SHOT)
     *              EXELWTED -> SCHEDULED : if (type != OS_TMR_CALLBACK_TYPE_ONE_SHOT)
     *              "EXELWTED (cancelation pending)" -->> "EXELWTED (cancelation pending)" : update
     *              "EXELWTED (cancelation pending)" -->> EXELWTED : schedule
     *              "EXELWTED (cancelation pending)" -->> CREATED : if no other action performed
     *           @enduml
     */
    OS_TMR_CALLBACK_STATE_EXELWTED  = 0x4U,
    /*!
     * @brief    Callback is in error state and requires clearing before
     *           re-creating.
     *
     * @details  Please refer to provided APIs for more information.
     */
    OS_TMR_CALLBACK_STATE_ERROR     = 0xFFU
} OS_TMR_CALLBACK_STATE;

/* ------------------------ Macros ------------------------------------------ */
/*!
 * @brief  Period value specifying that callback never expires. It is also
 *         considered as invalid period.
 */
#define OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER                                   0U

/*!
 * @brief  Retrieve the expiration time (`ET`, in OS ticks) with witch the
 *         expiration was detected.
 *
 * @param[in]   _pCB    Pointer to the callback.
 */
#define OS_TMR_CALLBACK_TICKS_EXPIRED_GET(_pCB)                                \
    ((_pCB)->modeData[(_pCB)->modeExpiredIn].ticksNext)

/*!
 * @brief    Check if callback was already created in past
 *
 * @details  Callback can be at any state other than `START` if it was created
 *           in past.
 *
 * @param[in]   _pCB    Pointer to the callback.
 */
#define OS_TMR_CALLBACK_WAS_CREATED(_pCB)                                      \
    ((_pCB)->state != OS_TMR_CALLBACK_STATE_START)

/* ------------------------ Types definitions ------------------------------- */
/*!
 * @copydoc OS_TMR_CALLBACK
 */
typedef struct OS_TMR_CALLBACK OS_TMR_CALLBACK;

/*!
 * @brief    Prototype for callback function.
 *
 * @details  The function should not be called by the clients directly but
 *           through @ref osTmrCallbackExelwte.
 *
 *           There is no limit on how long a callback function can take to
 *           execute. Callback functions does not block other callbacks but
 *           can delay it's own rescheduling since a relwrrent callback is
 *           not rescheduled until callback function finishes
 *           (@ref osTmrCallbackExelwte).
 *
 *           The callback operations allowed in the function is limited to:
 *              @ref osTmrCallbackUpdate
 *              @ref osTmrCallbackCancel
 *           Usage of other interfaces may lead to undefined behavior.
 *
 * @param[in]  pCallback  Pointer to the timer callback structure that the
 *                        callback function can interact with through allowed
 *                        infrastructure interfaces.
 */
#define OsTmrCallbackFunc(fname) FLCN_STATUS (fname)(OS_TMR_CALLBACK *pCallback)
typedef OsTmrCallbackFunc(*OsTmrCallbackFuncPtr);

/*!
 * @brief  Define for index into callback map
 */
typedef LwU8 OS_TMR_CB_MAP_HANDLE;

// Define NULL as 0 for callback map
#define CB_MAP_HANDLE_NULL 0

/*!
 * @brief  Per mode (@ref OS_TMR_CALLBACK_MODE) tracking data embedded in
 *         @ref OS_TMR_CALLBACK. Unit of time: OS tick
 */
typedef struct
{
    /*!
     * @brief  Period used when rescheduling.
     */
    LwU32                   ticksPeriod;
    /*!
     * @brief  Expiration time of the next callback instance.
     */
    LwU32                   ticksNext;
    /*!
     * @brief  Indices to maintain prev and next struct in double linked list
     */
    OS_TMR_CB_MAP_HANDLE    prevHandle;
    OS_TMR_CB_MAP_HANDLE    nextHandle;
} OS_TMR_CALLBACK_MODE_DATA;

#if (OS_CALLBACKS_WSTATS)
/*!
 * @brief  Callback's run-time statistics embedded in @ref OS_TMR_CALLBACK.
 *
 * @note   Unit of time: OS tick
 *         Exelwtion time of each callback instance: START - END
 *         Drift time of each callback instance: START - ET
 */
typedef struct
{
    /*!
     * @brief  Shortest callback's exelwtion time.
     */
    LwU32 ticksTimeExecMin;
    /*!
     * @brief  Longest callback's exelwtion time.
     */
    LwU32 ticksTimeExecMax;
    /*!
     * @brief  Total callback's exelwtion time.
     */
    LwU64 totalTicksTimeExelwted;
    /*!
     * @brief  Total callback's drift time.
     */
    LwU64 totalTicksTimeDrifted;
    /*!
     * @brief  Total callback's drift caused by waiting on full queue.
     */
    LwU64 totalTicksTimeDriftedQF;
    /*!
     * @brief  Count of callback's exelwtions.
     */
    LwU32 countExec;
} OS_TMR_CALLBACK_STATS;
#endif // OS_CALLBACKS_WSTATS

/*!
 * @brief    Callback descriptor that contains all necessary data to create and
 *           interact with a callback.
 *
 * @details  All `OS_TMR_CALLBACK` structures shall be declared as DMEM resident
 *           (be available to RTOS routines).
 *
 *           Callback use-cases requiring callback parameters should inherit
 *           base callback structure and include required callback parameters.
 *           Callback implementation can then cast callback structure pointer to
 *           respective type and access those parameters.
 *
 *           Example:
 *           \code{.c}
 *               typedef struct
 *               {
 *                   OS_TMR_CALLBACK super;
 *                   LwU32           exampleParameter;
 *               } OS_TMR_CALLBACK_EXAMPLE;
 *           \endcode
 */
struct OS_TMR_CALLBACK
{
    /*!
     * @brief  The type of the callback.
     */
    OS_TMR_CALLBACK_TYPE           type;
    /*!
     * @brief  The current state of the callback.
     */
    volatile OS_TMR_CALLBACK_STATE state;
    /*!
     * @brief  If set, client has requested cancelation of this callback and
     *         appropriate actions should be performed ASAP (at the next state
     *         transition, for more details, please see @ref
     *         OS_TMR_CALLBACK_STATE).
     */
    LwBool                         bCancelPending;
    /*!
     * @brief  Mode in which callback has expired.
     */
    OS_TMR_CALLBACK_MODE           modeExpiredIn;
    /*!
     * @brief  Index of IMEM overlay to attach before exelwting this callback
     *         and detach after the exelwtion if related infrastructure is
     *         enabled.
     */
    LwU8                           ovlImem;
    /*!
     * @brief  Index (RM_PMU_TASK_ID_*) of the task creating the callback.
     *
     * @note   The field is for the client to consume, not consumed by the
     *         infrastructure at the moment.
     */
    LwU8                           taskId;
    /*!
     * @brief   Index of callback struct in mapping table
     */
    OS_TMR_CB_MAP_HANDLE           mapIdx;
    /*!
     * @brief  Reserved for the future expansion.
     */
    LwU8                           rsvd[1];
    /*!
     * @brief  Callback function.
     */
    OsTmrCallbackFuncPtr           pTmrCallbackFunc;
    /*!
     * @brief  Handle of the queue to receive callback expiration notification.
     */
    LwrtosQueueHandle              queueHandle;
    /*!
     * @brief  Internal structure to maintain per operation mode callback queue.
     */
    OS_TMR_CALLBACK_MODE_DATA      modeData[OS_TMR_CALLBACK_MODE_COUNT];
#if (OS_CALLBACKS_WSTATS)
    /*!
     * @brief  Optional statistics (@ref OS_CALLBACKS_WSTATS)
     */
    OS_TMR_CALLBACK_STATS          stats;
#endif
};

/*!
 * @brief  Structure encapsulating RTOS's callback tracking data.
 */
typedef struct
{
    /*!
     * @brief  Callback infrastructure operation mode.
     */
    OS_TMR_CALLBACK_MODE mode;
    /*!
     * @brief  Reserved for the future expansion.
     */
    LwU8                 rsvd[3];
    /*!
     * @brief  Index to head of doubly linked lists (per callback mode) of
     *         callbacks of `SCHEDULED` state, sorted by increased value of 
     *         `ET`.
     */
    OS_TMR_CB_MAP_HANDLE headHandle[OS_TMR_CALLBACK_MODE_COUNT];
} OS_TMR_CALLBACK_STRUCT;

/* ------------------------ External definitions ---------------------------- */
extern OS_TMR_CALLBACK_STRUCT OsTmrCallback;

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @brief    Populates empty callback structure making it ready for `schedule`.
 *
 * @details  Internally calls @ref osTmrCallbackUpdate. Please refer to
 *           @ref osTmrCallbackUpdate for more details.
 *
 * @param[in, out]  pCallback        Callback to create / populate.
 * @param[in]       type             Callback type.
 * @param[in]       ovlImem          IMEM overlay to be attached on exelwtion.
 * @param[in]       pTmrCallbackFunc Callback function.
 * @param[in]       queueHandle      Queue to receive callback notification.
 * @param[in]       periodNormalUs   `NORMAL` mode period in [us] (high power).
 * @param[in]       periodSleepUs    `SLEEP` mode period in [us] (low power).
 * @param[in]       bRelaxedUseSleep Use Sleep period or not for SLEEP_RELAXED mode
 * @param[in]       taskId           Index of the task. It is populated to the
 *                                   structure but not lwrrently used.
 *
 * @return FLCN_OK                    On success.
 * @return FLCN_ERR_ILWALID_ARGUMENT  If pCallback is NULL.
 * @return FLCN_ERR_ILWALID_ARGUMENT  If pTmrCallbackFunc is NULL.
 * @return FLCN_ERR_ILLEGAL_OPERATION If callback state is not `START`.
 * @return FLCN_ERR_*                 If osTmrCallbackUpdate fails.
 */
#define osTmrCallbackCreate(...) MOCKABLE(osTmrCallbackCreate)(__VA_ARGS__)
FLCN_STATUS
osTmrCallbackCreate_IMPL
(
    OS_TMR_CALLBACK     *pCallback,
    OS_TMR_CALLBACK_TYPE type,
    LwU8                 ovlImem,
    OsTmrCallbackFuncPtr pTmrCallbackFunc,
    LwrtosQueueHandle    queueHandle,
    LwU32                periodNormalUs,
    LwU32                periodSleepUs,
    LwBool               bRelaxedUseSleep,
    LwU8                 taskId
);

/*!
 * @brief    Schedules the callback or lift pending cancellation.
 *
 * @details  Expiration time is set when @ref osTmrCallbackUpdate is called,
 *           not this API.
 *
 * @param[in, out]  pCallback   Callback to schedule.
 *
 * @return FLCN_OK                    On success.
 * @return FLCN_ERR_ILWALID_ARGUMENT  If pCallback is NULL.
 * @return FLCN_ERR_ILLEGAL_OPERATION If callback state is not `CREATED` and
 *                                    no cancellation pending.
 */
#define osTmrCallbackSchedule(...) MOCKABLE(osTmrCallbackSchedule)(__VA_ARGS__)
FLCN_STATUS osTmrCallbackSchedule_IMPL(OS_TMR_CALLBACK *pCallback);

/*!
 * @brief    Updates scheduling info of created callback structure.
 *
 * @details  Callbacks that are not required in certain mode of the operation
 *           should provide value @ref OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER,
 *           however period for at least one operation mode must have non-zero
 *           value. The base time `BT` with which to callwlate the expiration
 *           time `ET` is set at the moment of calling this API unless called
 *           during callback exelwting.
 *
 * @param[in, out]  pCallback          Callback to update.
 * @param[in]       periodNewNormalUs  Normal (high power) perion in [us].
 * @param[in]       periodNewSleepUs   Sleep (low power) period in [us].
 * @param[in]       bRelaxedUseSleep   Use Sleep period or not for SLEEP_RELAXED mode
 * @param[in]       bUpdateUseLwrrent  Update expiry ticks using current tick base or stored
 *
 * @return FLCN_OK                    On success.
 * @return FLCN_ERR_ILWALID_ARGUMENT  If pCallback is NULL.
 * @return FLCN_ERR_ILWALID_ARGUMENT  If no valid period is provided.
 * @return FLCN_ERR_ILLEGAL_OPERATION If callback state is `START` or `ERROR`.
 * @return FLCN_ERR_ILWALID_STATE     If callback state corrupted.
 */
#define osTmrCallbackUpdate(...) MOCKABLE(osTmrCallbackUpdate)(__VA_ARGS__)
FLCN_STATUS
osTmrCallbackUpdate_IMPL
(
    OS_TMR_CALLBACK *pCallback,
    LwU32            periodNewNormalUs,
    LwU32            periodNewSleepUs,
    LwBool           bRelaxedUseSleep,
    LwBool           bUpdateUseLwrrent
);

/*!
 * @brief    Exelwtes expired callback and re-schedules if appropriate.
 *
 * @details  Client task will get notification when the callback expires. The
 *           client should then call this interface to
 *           1. Collect statistics if needed.
 *           2. Attach and detach associated overlay if needed.
 *           3. Ilwoke associated callback function if not cancellation pending.
 *           4. Handle pending cancellation prior or during exelwtion. Callback
 *              state will change to `CREATED` for rescheduling.
 *           5. Reschedule the callback if needed.
 *
 * @param[in, out]  pCallback   Callback to execute.
 *
 * @return FLCN_OK                    On success.
 * @return FLCN_ERR_ILWALID_ARGUMENT  If pCallback is NULL.
 * @return FLCN_ERR_ILWALID_ARGUMENT  If no valid period is provided.
 * @return FLCN_ERR_ILLEGAL_OPERATION If callback state is not `QUEUED`.
 * @return FLCN_ERR_ILWALID_STATE     If callback state corrupted.
 * @return other errors               Propagates callback handling and/or exelwtion errors.
 */
#define osTmrCallbackExelwte(...) MOCKABLE(osTmrCallbackExelwte)(__VA_ARGS__)
FLCN_STATUS osTmrCallbackExelwte_IMPL(OS_TMR_CALLBACK *pCallback);

/*!
 * @brief    Attempts callback cancelation (state dependent).
 *
 * @details  This interface just initiates callback cancelation. Depending on
 *           the current state of the callback it can be terminated:
 *           - immediately (`SCHEDULED`)
 *           - at next state transition (`QUEUED`)
 *           - after exelwtion by preventing re-scheduling (`EXELWTED`)
 *
 * @param[in, out]  pCallback   Callback to cancel.
 *
 * @return  LW_FALSE    If in the state that does not allow cancelation.
 * @return  LW_TRUE     When cancelation was successfully initiated.
 *
 */
#define osTmrCallbackCancel(...) MOCKABLE(osTmrCallbackCancel)(__VA_ARGS__)
LwBool osTmrCallbackCancel_IMPL(OS_TMR_CALLBACK *pCallback);

/*!
 * @brief  Following interface must be implemented within respective falcon's
 *         ucode. Make them explicitly resident to avoid ambiguity at
 *         implementation.
 */
LwBool osTmrCallbackNotifyTaskISR(OS_TMR_CALLBACK *pCallback)
    GCC_ATTRIB_SECTION("imem_resident", "osTmrCallbackNotifyTaskISR");

/*!
 * @brief  Init callback map
 */
void osTmrCallbackMapInit(void)
    GCC_ATTRIB_SECTION("imem_osTmrCallbackInit", "osTmrCallbackMapInit");

/*!
 * @brief  Returns the max number of callbacks that can be registered
 */
LwU32 osTmrCallbackMaxCountGet(void)
    GCC_ATTRIB_SECTION("imem_osTmrCallbackInit", "osTmrCallbackMaxCountGet");

/* ------------------------ Inline Functions -------------------------------- */

#endif // LWOSTMRCALLBACK_H
