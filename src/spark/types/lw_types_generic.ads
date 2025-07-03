-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  Generic package for arithmetic operation.
--
--  @description
--  This package contains functions for arithmetic operations on generic types.
--  Each instantiation of this package will enable usage of these operators on
--  the corresponding generic type.
generic
   type BASE_TYPE is mod <>;
package Lw_Types_Generic
with SPARK_Mode => On
is

   type N_TYPE is new BASE_TYPE;

   overriding

     --  Overload "+" operator so that default wrap around semantics of '+' on mod types can be eliminated using
     --  pre-condition.
     --  Inline_Always is used to ensure that there is no run time overhead of pre-condition checking or
     --  even a function call for formally proven code.
   function "+" (Left, Right : N_TYPE) return N_TYPE
   is (N_TYPE (BASE_TYPE (Left) + BASE_TYPE (Right)))
     with Pre => (Left <= N_TYPE'Last - Right),
     Inline_Always,
     Global => null,
     Depends => ( "+"'Result => ( Left, Right ) );

   overriding

     --  Overload "-" operator so that default wrap around semantics of '-' on mod types can be eliminated using
     --  pre-condition.
     --  Inline_Always is used to ensure that there is no run time overhead of pre-condition checking or
     --  even a function call for formally proven code.
   function "-" (Left, Right : N_TYPE) return N_TYPE
   is (N_TYPE (BASE_TYPE (Left) - BASE_TYPE (Right)))
     with Pre => (Left >= Right),
     Inline_Always,
     Global => null,
     Depends => ( "-"'Result => ( Left, Right ) );

   overriding

     --  Overload "/" operator so that default wrap around semantics of '/' on mod types can be eliminated using
     --  pre-condition.
     --  Inline_Always is used to ensure that there is no run time overhead of pre-condition checking or
     --  even a function call for formally proven code.
   function "/" (Left, Right : N_TYPE) return N_TYPE
   is (N_TYPE (BASE_TYPE (Left) / BASE_TYPE (Right)))
     with Pre => (Right /= 0),
     Inline_Always,
     Global => null,
     Depends => ( "/"'Result => ( Left, Right ) );

   overriding

     --  Overload "*" operator so that default wrap around semantics of '*' on mod types can be eliminated using
     --  pre-condition.
     --  Inline_Always is used to ensure that there is no run time overhead of pre-condition checking or
     --  even a function call for formally proven code.
   function "*" (Left, Right : N_TYPE) return N_TYPE
   is (N_TYPE (BASE_TYPE (Left) * BASE_TYPE (Right)))
     with Pre => (Right = 0 or else Left <= N_TYPE'Last / Right),
     Inline_Always,
     Global => null,
     Depends => ( "*"'Result => ( Left, Right ) );

end Lw_Types_Generic;
