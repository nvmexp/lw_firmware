/**
 *  @brief Returns the current time in nanoseconds
 *
 *  @note Time is monotonic across call tolibosSuspendProcessor
 */
LwU64 LibosTimeNs(void);


/**
 *  @brief Sets a kernel timer object to being counting down.
 *      
 * 
 *  @params
 *      @param[in] timer  
 *          Timer timer to set
 * 
 *      @param[in] timestamp OPTIONAL
 *          The wakeup time for the timer. 
 *
 */

LibosStatus LibosTimerSet(
    LibosPortId timer,
    LwU64       timestamp
);

/**
 *  @brief Clears a kernel timer object.
 *      
 * 
 *  @params
 *      @param[in] timer  
 *          Timer timer to set/clear
 * 
 */

LibosStatus LibosTimerClear(
    LibosPortId timer
);