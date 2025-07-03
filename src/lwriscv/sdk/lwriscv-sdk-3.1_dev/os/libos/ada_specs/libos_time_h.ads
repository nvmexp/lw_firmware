pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with lwtypes_h;
with libos_manifest_h;
with libos_status_h;

package libos_time_h is

  --*
  -- *  @brief Returns the current time in nanoseconds
  -- *
  -- *  @note Time is monotonic across call tolibosSuspendProcessor
  --  

   function LibosTimeNs return lwtypes_h.LwU64  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_time.h:6
   with Import => True, 
        Convention => C, 
        External_Name => "LibosTimeNs";

  --*
  -- *  @brief Sets a kernel timer object to being counting down.
  -- *      
  -- * 
  -- *  @params
  -- *      @param[in] timer  
  -- *          Timer timer to set
  -- * 
  -- *      @param[in] timestamp OPTIONAL
  -- *          The wakeup time for the timer. 
  -- *
  --  

   function LibosTimerSet (timer : libos_manifest_h.LibosPortId; timestamp : lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_time.h:22
   with Import => True, 
        Convention => C, 
        External_Name => "LibosTimerSet";

  --*
  -- *  @brief Clears a kernel timer object.
  -- *      
  -- * 
  -- *  @params
  -- *      @param[in] timer  
  -- *          Timer timer to set/clear
  -- * 
  --  

   function LibosTimerClear (timer : libos_manifest_h.LibosPortId) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_time.h:37
   with Import => True, 
        Convention => C, 
        External_Name => "LibosTimerClear";

end libos_time_h;
