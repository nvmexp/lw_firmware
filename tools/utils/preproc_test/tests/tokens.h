// For the purpose of testing, define some macros.
#define e * // This should not be expanded.  If it is expanded, it's a bug.
#define ok / // This should be expanded.  If it is not expanded, it's a bug.

begin single line comments test
// Single line comment
ok
//e
ok
// e
ok
ok//e
ok// e
//\t
visible
end single line comments test

begin multi line comments test
ok/*e*/ok
/* e */
/* e Multi-line e
 * e comment e
 * /* nesting is not supported
e*/ok
end multi line comments test

begin numbers test
ok 0123456 ok
ok 1e ok
ok 0.0 ok
ok 0. ok
ok 0.1e2 ok
ok 1.e-3 ok
ok 2.e+4 ok
end numbers test

begin strings test
ok"e"ok
ok"\r"ok
ok"\""ok
ok"\\"ok
ok"'"ok
ok"'e'\"e'"ok
ok"e\
e\
e"ok
ok'e'ok
ok'\r'ok
ok'\"'ok
ok'\\'ok
ok'"'ok
ok'"e"\'e"'ok
ok'e\
e\
e'ok
end strings test

begin punctuators test
~ok!ok!=ok!==ok%ok%=
^ok^=ok&ok&&ok&=ok*ok*=ok(ok)
-ok--ok-=ok+ok++ok+=ok=ok==ok===
{ok}ok[ok]ok|ok||ok:ok;
<ok<<ok<<=ok<=ok>ok>>ok>>>ok>>ok>>=
?ok,ok.ok/ok/=
end punctuators test
