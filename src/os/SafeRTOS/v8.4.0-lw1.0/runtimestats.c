/*
    Copyright (C)2006 onward WITTENSTEIN aerospace & simulation limited. All rights reserved.

    This file is part of the SafeRTOS product, see projdefs.h for version number
    information.

    SafeRTOS is distributed exclusively by WITTENSTEIN high integrity systems,
    and is subject to the terms of the License granted to your organization,
    including its warranties and limitations on use and distribution. It cannot be
    copied or reproduced in any way except as permitted by the License.

    Licenses authorize use by processor, compiler, business unit, and product.

    WITTENSTEIN high integrity systems is a trading name of WITTENSTEIN
    aerospace & simulation ltd, Registered Office: Brown's Court, Long Ashton
    Business Park, Yanley Lane, Long Ashton, Bristol, BS41 9LB, UK.
    Tel: +44 (0) 1275 395 600, fax: +44 (0) 1275 393 630.
    E-mail: info@HighIntegritySystems.com

    www.HighIntegritySystems.com
*/

#define KERNEL_SOURCE_FILE

#include "SafeRTOS.h"
#include "task.h"

/*---------------------------------------------------------------------------*/
/* Constant Definitions.                                                     */
/*---------------------------------------------------------------------------*/
#define rtsPERCENT_SCALING                      ( 100U )    /* Scaled to 2 decimal places. */
#define rtsCALLWLATION_ERROR                    ( 0xFFFFFFFFU )

/* Callwlating the usage percentage ilwolves multiplying a run time counter
 * value by (100 * rtsPERCENT_SCALING). This is the maximum such number that
 * leads to a result that fits inside a 64 bit number. */
#define rtsMAXIMUM_RUN_TIME_FOR_CALLWLATION     ( 0x00068DB8BAC710CBULL )

/*---------------------------------------------------------------------------*/
/* Macros                                                                    */
/*---------------------------------------------------------------------------*/
#define prvMAX( a, b )                          ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

/*---------------------------------------------------------------------------*/
/* Type Definitions.                                                         */
/*---------------------------------------------------------------------------*/
typedef portUInt64Type xRTS_COUNTER;

/*---------------------------------------------------------------------------*/
/* Variables.                                                                */
/*---------------------------------------------------------------------------*/
KERNEL_DATA static volatile portUInt32Type ulTotalRunTimeCounter1 = 0U;
KERNEL_DATA static volatile portUInt32Type ulTotalRunTimeCounter2 = 0U;
KERNEL_DATA static volatile portUInt32Type ulTaskSwitchedInTime = 0U;

/*---------------------------------------------------------------------------*/
/* Function prototypes.                                                      */
/*---------------------------------------------------------------------------*/
/* THESE FUNCTIONS MUST ONLY BE CALLED FROM WITHIN A CRITICAL SECTION as they
 * access file scope variables and use pointers to access structures. */
KERNEL_FUNCTION static portUInt32Type prvGetOverallUsage( const xRTS *const pxRTS );
KERNEL_FUNCTION static portUInt32Type prvGetPeriodUsage( const xRTS *const pxRTS );

/* MCDC Test Point: PROTOTYPE */

/*---------------------------------------------------------------------------*/
/* Public API functions                                                      */
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xCallwlateCPUUsage( portTaskHandleType xHandle, xPERCENTAGES *const pxPercentages )
{
    portBaseType xReturn = pdPASS;
    xTCB *const pxTCB = taskGET_TCB_FROM_HANDLE( xHandle );
    xRTS *const pxRTS = &( pxTCB->xRunTimeStats );
    xPERCENTAGES xTempPercentages;

    if( NULL == pxPercentages )
    {
        xReturn = errILWALID_PERCENTAGE_HANDLE;

        /* MCDC Test Point: STD_IF "xCallwlateCPUUsage" */
    }
    else
    {
        portENTER_CRITICAL_WITHIN_API();
        {
            if( xPortIsTaskHandleValid( pxTCB ) == pdFALSE )
            {
                xReturn = errILWALID_TASK_HANDLE;

                /* MCDC Test Point: STD_IF "xCallwlateCPUUsage" */
            }
            else
            {
                /* Retrieve the current statistics for this task - these are
                 * needed so that the correct 'max' values are used in the
                 * subsequent callwlations. */
                if( pdPASS != portCOPY_DATA_FROM_TASK( &xTempPercentages, pxPercentages, ( portUnsignedBaseType ) sizeof( xPERCENTAGES ) ) )
                {
                    xReturn = errILWALID_DATA_RANGE;

                    /* MCDC Test Point: STD_IF "xCallwlateCPUUsage" */
                }
                else
                {
                    /* Ensure the latest data is used in the callwlation. */
                    vUpdateRunTimeStatistics();

                    /* Callwlate overall CPU usage. */
                    xTempPercentages.xOverall.ulLwrrent = prvGetOverallUsage( pxRTS );
                    if( rtsCALLWLATION_ERROR != xTempPercentages.xOverall.ulLwrrent )
                    {
                        /* MCDC Test Point: EXP_IF_MACRO "prvMAX" "xTempPercentages.xOverall.ulLwrrent > xTempPercentages.xOverall.ulMax" */
                        xTempPercentages.xOverall.ulMax = prvMAX( xTempPercentages.xOverall.ulLwrrent, xTempPercentages.xOverall.ulMax );

                        /* MCDC Test Point: STD_IF "xCallwlateCPUUsage" */

                    }
                    else
                    {
                        xReturn = errRTS_CALLWLATION_ERROR;

                        /* MCDC Test Point: STD_ELSE "xCallwlateCPUUsage" */
                    }

                    /* Callwlate periodic CPU usage. */
                    xTempPercentages.xPeriod.ulLwrrent = prvGetPeriodUsage( pxRTS );
                    if( rtsCALLWLATION_ERROR != xTempPercentages.xPeriod.ulLwrrent )
                    {
                        /* MCDC Test Point: EXP_IF_MACRO "prvMAX" "xTempPercentages.xPeriod.ulLwrrent > xTempPercentages.xPeriod.ulMax" */
                        xTempPercentages.xPeriod.ulMax = prvMAX( xTempPercentages.xPeriod.ulLwrrent, xTempPercentages.xPeriod.ulMax );

                        /* MCDC Test Point: STD_IF "xCallwlateCPUUsage" */
                    }
                    else
                    {
                        xReturn = errRTS_CALLWLATION_ERROR;

                        /* MCDC Test Point: STD_ELSE "xCallwlateCPUUsage" */
                    }

                    /* Copy the data back (with any callwlation error oclwrred). */
                    if( pdPASS != portCOPY_DATA_TO_TASK( pxPercentages, &xTempPercentages, ( portUnsignedBaseType ) sizeof( xPERCENTAGES ) ) )
                    {
                        xReturn = errILWALID_DATA_RANGE;

                        /* MCDC Test Point: STD_IF "xCallwlateCPUUsage" */
                    }
                    else
                    {
                        /* Update the counters. */
                        pxRTS->ulLastRunTimeCounter1 = pxRTS->ulRunTimeCounter1;
                        pxRTS->ulLastRunTimeCounter2 = pxRTS->ulRunTimeCounter2;
                        pxRTS->ulPeriodBaseTime1     = ulTotalRunTimeCounter1;
                        pxRTS->ulPeriodBaseTime2     = ulTotalRunTimeCounter2;

                        /* MCDC Test Point: STD_ELSE "xCallwlateCPUUsage" */
                    }

                    /* MCDC Test Point: STD_ELSE "xCallwlateCPUUsage" */
                }

                /* MCDC Test Point: STD_ELSE "xCallwlateCPUUsage" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();
        /* MCDC Test Point: STD_ELSE "xCallwlateCPUUsage" */
    }

    return xReturn;
}

/*---------------------------------------------------------------------------*/
/* Kernel private functions                                                  */
/*---------------------------------------------------------------------------*/

KERNEL_INIT_FUNCTION void vInitialiseRunTimeStatistics( void )
{
    /* Set up the initial delay time. */
    ulTotalRunTimeCounter1 = 0U;
    ulTotalRunTimeCounter2 = 0U;
    ulTaskSwitchedInTime   = 0U;

    /* Start the RTS timer. */
    portRTS_TIMER_INITIALISATION();

    /* MCDC Test Point: STD "vInitialiseRunTimeStatistics" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vUpdateRunTimeStatistics( void )
{
    portUInt32Type ulElapsedTime = 0U;
    portUInt32Type *const pulElapsedTime = &ulElapsedTime;
    volatile portUInt32Type *const pulTaskSwitchedInTime = &ulTaskSwitchedInTime;
    xTCB *const pxLwrrentTCBCopy = pxLwrrentTCB;    /* pxLwrrentTCB is declared volatile so take a copy. */
    portUInt32Type ulCopyOfCounter1;    /* ulTotalRunTimeCounter1 is declared volatile so take a copy. */

    /* Add the amount of time the task has been running to the
     * aclwmulated time so far. The time the task started running
     * was stored in ulTaskSwitchedInTime. */

    /* Get elapsed time since task was switched in. */
    portGET_ELAPSED_CPU_TIME( pulTaskSwitchedInTime, pulElapsedTime );

    ulCopyOfCounter1 = ulTotalRunTimeCounter1 + ulElapsedTime;
    ulTotalRunTimeCounter1 = ( ulCopyOfCounter1 & 0x7FFFFFFFU );
    ulTotalRunTimeCounter2 += ( ulCopyOfCounter1 >> 31U );

    pxLwrrentTCBCopy->xRunTimeStats.ulRunTimeCounter1 += ulElapsedTime;
    pxLwrrentTCBCopy->xRunTimeStats.ulRunTimeCounter2 += ( pxLwrrentTCBCopy->xRunTimeStats.ulRunTimeCounter1 >> 31U );
    pxLwrrentTCBCopy->xRunTimeStats.ulRunTimeCounter1 &= 0x7FFFFFFFU;

    /* MCDC Test Point: STD "vUpdateRunTimeStatistics" */
}
/*---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION void vInitialiseTaskRunTimeStatistics( portTaskHandleType xHandle )
{
    /* Since this function is only called by the kernel, it can be assumed that
     * xHandle is valid and no further checks are required here. */
    xRTS *const pxRTS = &( ( ( xTCB * )xHandle )->xRunTimeStats );

    /* Initialise the run-time statistics associated with the task. */
    portENTER_CRITICAL_WITHIN_API();
    {
        pxRTS->ulRunTimeCounter1     = 0U;
        pxRTS->ulRunTimeCounter2     = 0U;
        pxRTS->ulLastRunTimeCounter1 = 0U;
        pxRTS->ulLastRunTimeCounter2 = 0U;
        pxRTS->ulPeriodBaseTime1     = ulTotalRunTimeCounter1;
        pxRTS->ulPeriodBaseTime2     = ulTotalRunTimeCounter2;

        /* MCDC Test Point: STD "vInitialiseTaskRunTimeStatistics" */
    }
    portEXIT_CRITICAL_WITHIN_API();
}

/*---------------------------------------------------------------------------*/
/* File private functions                                                    */
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portUInt32Type prvGetOverallUsage( const xRTS *const pxRTS )
{
    portUInt32Type ulRetVal;
    xRTS_COUNTER xRTSCounter, xTotalRTSCounter;

    /* THIS FUNCTION MUST ONLY BE CALLED FROM WITHIN A CRITICAL SECTION. */

    /* Determine the values of the counters. */
    xRTSCounter  = ( xRTS_COUNTER ) pxRTS->ulRunTimeCounter2 << 31U;
    xRTSCounter += ( xRTS_COUNTER ) pxRTS->ulRunTimeCounter1;

    xTotalRTSCounter  = ( xRTS_COUNTER ) ulTotalRunTimeCounter2 << 31U;
    xTotalRTSCounter += ( xRTS_COUNTER ) ulTotalRunTimeCounter1;

    /* Check for possible divide-by-zero. */
    if( 0U == xTotalRTSCounter )
    {
        ulRetVal = rtsCALLWLATION_ERROR;

        /* MCDC Test Point: STD_IF "prvGetOverallUsage" */
    }
    else if( xRTSCounter > rtsMAXIMUM_RUN_TIME_FOR_CALLWLATION )
    {
        /* Multiplying xRTSCounter by 10000 will cause an overflow and therefore
         * an incorrect result. */
        ulRetVal = rtsCALLWLATION_ERROR;

        /* MCDC Test Point: STD_ELSE_IF "prvGetOverallUsage" */
    }
    else
    {
        /* Callwlate overall percentage * 100 scaled for required no. of decimal
         * places. */
        ulRetVal = ( portUInt32Type )( ( xRTSCounter * ( xRTS_COUNTER )( 100U * rtsPERCENT_SCALING ) ) / xTotalRTSCounter );

        /* MCDC Test Point: STD_ELSE "prvGetOverallUsage" */
    }

    return ulRetVal;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portUInt32Type prvGetPeriodUsage( const xRTS *const pxRTS )
{
    portUInt32Type ulRetVal;
    xRTS_COUNTER xCPUUsage, xLastRTSCounter, xPeriodBaseTime, xPeriodTime, xLwrrentRTSCounter, xTotalRTSCounter;

    /* THIS FUNCTION MUST ONLY BE CALLED FROM WITHIN A CRITICAL SECTION. */

    /* Determine the values of the counters. */
    xLwrrentRTSCounter  = ( xRTS_COUNTER ) pxRTS->ulRunTimeCounter2 << 31U;
    xLwrrentRTSCounter += ( xRTS_COUNTER ) pxRTS->ulRunTimeCounter1;
    xLastRTSCounter  = ( xRTS_COUNTER ) pxRTS->ulLastRunTimeCounter2 << 31U;
    xLastRTSCounter += ( xRTS_COUNTER ) pxRTS->ulLastRunTimeCounter1;
    xPeriodBaseTime  = ( xRTS_COUNTER ) pxRTS->ulPeriodBaseTime2 << 31U;
    xPeriodBaseTime += ( xRTS_COUNTER ) pxRTS->ulPeriodBaseTime1;
    xTotalRTSCounter  = ( xRTS_COUNTER ) ulTotalRunTimeCounter2 << 31U;
    xTotalRTSCounter += ( xRTS_COUNTER ) ulTotalRunTimeCounter1;

    /* Check for possible divide-by-zero or other invalid values. */
    if( xPeriodBaseTime < xTotalRTSCounter )
    {
        /* Callwlate period percentage * 100 scaled for required no. of decimal
         * places. */
        xPeriodTime = xTotalRTSCounter - xPeriodBaseTime;
        xCPUUsage = xLwrrentRTSCounter - xLastRTSCounter;

        if( xCPUUsage > rtsMAXIMUM_RUN_TIME_FOR_CALLWLATION )
        {
            /* Multiplying xCPUUsage by 10000 will cause an overflow and
             * therefore an incorrect result. */
            ulRetVal = rtsCALLWLATION_ERROR;

            /* MCDC Test Point: STD_IF "prvGetPeriodUsage" */
        }
        else
        {
            ulRetVal = ( portUInt32Type )( ( xCPUUsage * ( xRTS_COUNTER )( 100U * rtsPERCENT_SCALING ) ) / xPeriodTime );

            /* MCDC Test Point: STD_ELSE "prvGetPeriodUsage" */
        }
    }
    else
    {
        ulRetVal = rtsCALLWLATION_ERROR;

        /* MCDC Test Point: STD_ELSE "prvGetPeriodUsage" */
    }

    return ulRetVal;
}
/*---------------------------------------------------------------------------*/

#ifdef SAFERTOS_TEST
    #include "SectionLocationCheckList_runtimestatsc.h"
#endif

#ifdef SAFERTOS_MODULE_TEST
    #include "RTSCTestHeaders.h"
    #include "RTSCTest.h"
#endif /* SAFERTOS_MODULE_TEST */
