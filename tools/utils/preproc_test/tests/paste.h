#define EXPAND( a ) a
+EXPAND(+)+
EXPAND(1)2
EXPAND(a)2
EXPAND(1)a
+EXPAND(1.e+1)+
*EXPAND(1)*
.EXPAND(1).
.EXPAND(.1).
'a'EXPAND('b')'c'
EXPAND(1+)_

#define NUMBER()     1
#define UNDERSCORE() _
NUMBER()UNDERSCORE()
UNDERSCORE()NUMBER()
