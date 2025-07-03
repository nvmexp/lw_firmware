#define LINE line __LINE__
LINE == 2

#define FILE file __FILE__
FILE == "macros.h"

#define MANY_TOKENS   test   +   123   *   "string"
>MANY_TOKENS<
LINE == 9

#define STR(a) >#a<
STR(  a  +  1  .  )

#define CONCAT_LEFT(a) +1##a+
CONCAT_LEFT(b c)

#define CONCAT_RIGHT(a) +a##1+
CONCAT_RIGHT(b c)

#define CONCAT_BOTH(a, b)+a##b+
CONCAT_BOTH(x, 1)

#define NO_CONCAT(a, b) +a x##y b+
NO_CONCAT(1, 2)

#define ARGS0() +LINE+
ARGS0() LINE == 27
ARGS0(
)LINE == 29
ARGS0(

)LINE == 32

#define ARGS1(a) +LINE+#a+arg##a+
ARGS1(test)LINE == 35
ARGS1(
test
)LINE == 38
ARGS1()

#define ARGS2(a, b) \
        a##b        \
        -> #a -> #b \
        ((a), (b))
ARGS2(1, 2)
ARGS2(,)

#define SELF(SELF) SELF #SELF 1##SELF
SELF(2)

#define RELWRSIVE_INNER(x) >RELWRSIVE_OUTER(x)<
#define RELWRSIVE_OUTER(x) [RELWRSIVE_INNER(x)]
RELWRSIVE_OUTER(x) // The inner call to RELWRSIVE_OUTER should not be expanded

LINE == 55

#define SUM(a,b) (a+b)
#define MUL(a,b) (a*b)
MUL(SUM(1,MUL(2,3)),4) // the MUL arg should expand, it's not relwrsive

#define STR_WITH_SPACES(  a  )  +  #  /**/  a  +
STR_WITH_SPACES(  /**/  123  /**/  xyz  /**/  (  /**/  )  /**/  ) // Produces "123 xyz ( )"

#define CONCAT_WITH_SPACES(  a  ,  b  )  +  a  /**/  ##  /**/  b  +
CONCAT_WITH_SPACES(  /**/  1  /**/  ,  /**/  2  /**/  ) // Produces 12

#define CONCAT_LINE_BAD1(  a  )  a  ##  __LINE__
CONCAT_LINE_BAD1(  line  ) // __LINE__ remains unexpanded

#define CONCAT(  a  ,  b  )  a  ##  b
#define CONCAT_LINE_BAD2(  a  )  CONCAT   (   a   ,   __LINE__   )
CONCAT_LINE_BAD2(  line  ) // __LINE__ remains unexpanded

#define CONCAT2(  a  ,  b  )  CONCAT  (  a  ,  b  )
#define CONCAT_LINE_GOOD(  a  )  CONCAT2  (   a   ,   __LINE__   )
CONCAT_LINE_GOOD(  line  ) // __LINE__ is expanded

#define PREFIX_SELF prefix PREFIX_SELF
#define IDENTITY_FUNC(a) a
IDENTITY_FUNC(PREFIX_SELF)

#define CALL0(x) x()
#define CALL1(x) x(1)

CALL0+
CALL0()
CALL1()
CALL0(STR)
CALL1(STR)
CALL0(LINE)
CALL1(LINE)

#define AA     1
#define AABB   2
#define AABBCC 3
#define CONCAT3(a, b, c) a ## b ## c
#define CONCAT_CONCAT3 CON ## CAT3(AA, BB, CC)
CONCAT3(AA, BB, CC)
CONCAT_CONCAT3

#define SOME_MACRO( x ) x
#define CALL_MACRO_WITH_ARG( a , b ) a ( b )
#define CALL_MACRO_NAME( a , b , c ) CALL_MACRO_WITH_ARG ( a ## b, c )
CALL_MACRO_NAME
( SOME_ , MACRO , 4 )

CALL_MACRO_NAME
do not expand

#define MACRO_CALL1(i) (i+1)
#define MACRO_CALL2 MACRO_CALL1
#define MACRO_CALL3 MACRO_CALL2 (
MACRO_CALL2(1)
line 1
MACRO_CALL3 2 )
line 2

#define CALL_MACRO_BY_NAME_INNER(macro, arg) macro(arg)
#define CALL_MACRO_BY_NAME_OUTER(macro, arg) CALL_MACRO_BY_NAME_INNER(macro, arg)
CALL_MACRO_BY_NAME_OUTER(SOME_MACRO, 4) after
line 3

#define RELWRSIVE(x) x(1) #x
RELWRSIVE(RELWRSIVE)
#define RELWRSIVE2(x) RELWRSIVE2(x)
RELWRSIVE2(RELWRSIVE2)
