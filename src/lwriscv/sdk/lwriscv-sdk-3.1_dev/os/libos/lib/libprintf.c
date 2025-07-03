/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * ----------------------------------------------------------------------
 * Copyright � 2005-2014 Rich Felker, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ----------------------------------------------------------------------
 */

#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include "libos-config.h"

void putc(int c);

#ifndef __SIZE_TYPE__
typedef unsigned long long size_t;
typedef int64_t ptrdiff_t;
typedef uint64_t uintptr_t;
#else
typedef __SIZE_TYPE__ size_t;
#endif
typedef unsigned short uint16_t;
typedef signed short   int16_t;
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned uint32_t;
typedef int      int32_t;
typedef unsigned long long uint64_t;
typedef signed long long    int64_t;
typedef uint64_t uintmax_t;
typedef int64_t intmax_t;

#ifndef INTMAX_MAX
#   define INTMAX_MAX  INT_MAX
#   define UINTMAX_MAX UINT_MAX
#endif

#ifndef NL_ARGMAX
#define NL_ARGMAX 9
#endif



static size_t strnlen(const char *s, size_t n)
{
    size_t l = 0;
    while (l < n && *s++ != 0)
		l++;
	return l;
}

#if !LIBOS_CONFIG_PRINTF_TINY
static int isdigit(int c) {
	return (unsigned)(c - '0') < 10;
}
#endif

#if LIBOS_CONFIG_FLOAT
/* Some useful macros */
static intmax_t min(intmax_t a, intmax_t b) { 
    return a < b ? a : b;
}
static intmax_t max(intmax_t a, intmax_t b) { 
    return a > b ? a : b;
}
#endif

/* Colwenient bit representation for modifier flags, which all fall
 * within 31 codepoints of the space character. */

#define ALT_FORM   (1U<<('#'-' '))
#define ZERO_PAD   (1U<<('0'-' '))
#define LEFT_ADJ   (1U<<('-'-' '))
#define PAD_POS    (1U<<(' '-' '))
#define MARK_POS   (1U<<('+'-' '))
#define GROUPED    (1U<<('\''-' '))

#define FLAGMASK (ALT_FORM|ZERO_PAD|LEFT_ADJ|PAD_POS|MARK_POS|GROUPED)

/* State machine to accept length modifiers + colwersion specifiers.
 * Result is 0 on failure, or an argument type to pop on success. */

enum {
    // @todo: Some of these classes are duplicates on RV64
	BARE, LPRE, 
#if !LIBOS_CONFIG_PRINTF_TINY
    LLPRE, 
#else   
    LLPRE = LPRE,
#endif    
#if !LIBOS_CONFIG_PRINTF_TINY
    HPRE, HHPRE, 
#endif    
#if LIBOS_CONFIG_FLOAT
    BIGLPRE,
#endif    
	ZJTPRE, 
	STOP,
#if !LIBOS_CONFIG_PRINTF_TINY
    S16, U8, U16,
#endif    
    S8, /* char */
	S32, S64,
    U32, U64,
#if LIBOS_CONFIG_FLOAT
	DBL, LDBL,
#endif    
};

#define S(x) [(x)-'A']

static const unsigned char states[]['z'-'A'+1] = {
	[BARE] = { /* 0: bare types */
		S('d') = S32, S('i') = S32,
		S('o') = U32, S('u') = U32, S('x') = U32, S('X') = U32,
#if LIBOS_CONFIG_FLOAT
		S('e') = DBL, S('f') = DBL, S('g') = DBL, S('a') = DBL,
		S('E') = DBL, S('F') = DBL, S('G') = DBL, S('A') = DBL,
#endif        
		S('c') = S8, S('C') = S32,
		S('s') = S64, S('S') = S64, S('p') = U64, S('n') = S64,
		S('l') = LPRE, 
#if !LIBOS_CONFIG_PRINTF_TINY
        S('h') = HPRE, 
#endif        

#if LIBOS_CONFIG_FLOAT
        S('L') = BIGLPRE,
#endif

		S('z') = ZJTPRE, S('j') = ZJTPRE, S('t') = ZJTPRE,
	}, 
    [LPRE] = { /* 1: l-prefixed */
		S('d') = S64, S('i') = S64,
		S('o') = U64, S('u') = U64, S('x') = U64, S('X') = U64,
#if LIBOS_CONFIG_FLOAT
		S('e') = DBL, S('f') = DBL, S('g') = DBL, S('a') = DBL,
		S('E') = DBL, S('F') = DBL, S('G') = DBL, S('A') = DBL,
#endif
		S('c') = S32, S('s') = S64, S('n') = S64,
		S('l') = LLPRE,
	}, 
#if !LIBOS_CONFIG_PRINTF_TINY
    [LLPRE] = { /* 2: ll-prefixed */
		S('d') = S64, S('i') = S64,
		S('o') = U64, S('u') = U64,
		S('x') = U64, S('X') = U64,
		S('n') = S64,
	}, 

    [HPRE] = { /* 3: h-prefixed */
		S('d') = S16, S('i') = S16,
		S('o') = U16, S('u') = U16,
		S('x') = U16, S('X') = U16,
		S('n') = S64,
		S('h') = HHPRE,
	}, 
    [HHPRE] = { /* 4: hh-prefixed */
		S('d') = S8, S('i') = S8,
		S('o') = U8, S('u') = U8,
		S('x') = U8, S('X') = U8,
		S('n') = S64,
	}, 
#endif    
#if LIBOS_CONFIG_FLOAT    
    [BIGLPRE] = { /* 5: L-prefixed */
		S('e') = LDBL, S('f') = LDBL, S('g') = LDBL, S('a') = LDBL,
		S('E') = LDBL, S('F') = LDBL, S('G') = LDBL, S('A') = LDBL,
		S('n') = S64,
	}, 
#endif        
    [ZJTPRE]= { /* 6: z- or t-prefixed (assumed to be same size) */
		S('d') = S64, S('i') = S64,
		S('o') = U64, S('u') = U64,
		S('x') = U64, S('X') = U64,
		S('n') = S64,
	}
};

#define OOB(x) ((unsigned)(x)-'A' > 'z'-'A')

union arg
{
	uintmax_t i;   
#if LIBOS_CONFIG_FLOAT
	long double f;
#endif    
	void *p;
};

static void pop_arg(union arg *arg, int type, va_list *ap)
{
	switch (type) {
        case U64: case S64:
            arg->i = va_arg(*ap, uintmax_t);   
            break;
        case S32:	arg->i = va_arg(*ap, int32_t);  break;
        case U32:	arg->i = va_arg(*ap, uint32_t); break;
#if !LIBOS_CONFIG_PRINTF_TINY
        case S16:	arg->i = (short)va_arg(*ap, int); break;
        case U16:   arg->i = (unsigned short)va_arg(*ap, int); break;
        case U8:	arg->i = (unsigned char)va_arg(*ap, int); break;
#endif
        case S8:	arg->i = (signed char)va_arg(*ap, int); break;
#if LIBOS_CONFIG_FLOAT
	case DBL:	arg->f = va_arg(*ap, double); break;
	case LDBL:	arg->f = va_arg(*ap, long double); break;
#endif    
		default:
			__builtin_unreachable();
	}
}

static void out(int (*put)(char c, void * f), void * f, const char *s, size_t l)
{
    while (l--)
        put(*s++, f);
}

#if !LIBOS_CONFIG_PRINTF_TINY
static void pad(int (*put)(char c, void * f), void * f, char c, int w, int l, int fl)
{
    if (fl & (LEFT_ADJ | ZERO_PAD)) 
	    return;

    for (; l < w; l++)
    {
        put(c,f);
    }
}
#endif

__attribute__((used,section(".rodata")))
static const char xdigits[16] = {
	"0123456789ABCDEF"
};

static char *fmt_num(uintmax_t x, char *s, char mask, int base)
{
	for (   ; x; x/=base) *--s = xdigits[x%base] | mask;
	return s;
}

/* Do not override this check. The floating point printing code below
 * depends on the float.h constants being right. If they are wrong, it
 * may overflow the stack. */
#if LDBL_MANT_DIG == 53
typedef char compiler_defines_long_double_incorrectly[9-(int)sizeof(long double)];
#endif

#if LIBOS_CONFIG_FLOAT
static int fmt_fp(printf_stream *f, long double y, int w, int p, int fl, int t)
{
	uint32_t big[(LDBL_MANT_DIG+28)/29 + 1          // mantissa expansion
		+ (LDBL_MAX_EXP+LDBL_MANT_DIG+28+8)/9]; // exponent expansion
	uint32_t *a, *d, *r, *z;
	int e2=0, e, i, j, l;
	char buf[9+LDBL_MANT_DIG/4], *s;
	const char *prefix="-0X+0X 0X-0x+0x 0x";
	int pl;
	char ebuf0[3*sizeof(int)], *ebuf=&ebuf0[3*sizeof(int)], *estr;

	pl=1;
	if (signbit(y)) {
		y=-y;
	} else if (fl & MARK_POS) {
		prefix+=3;
	} else if (fl & PAD_POS) {
		prefix+=6;
	} else prefix++, pl=0;

	if (!isfinite(y)) {
		char *s = (t&32)?"inf":"INF";
		if (y!=y) s=(t&32)?"nan":"NAN";
		pad(f, ' ', w, 3+pl, fl&~ZERO_PAD);
		out(f, prefix, pl);
		out(f, s, 3);
		pad(f, ' ', w, 3+pl, fl^LEFT_ADJ);
		return max(w, 3+pl);
	}

	y = frexpl(y, &e2) * 2;
	if (y) e2--;

	if ((t|32)=='a') {
		long double round = 8.0;
		int re;

		if (t&32) prefix += 9;
		pl += 2;

		if (p<0 || p>=LDBL_MANT_DIG/4-1) re=0;
		else re=LDBL_MANT_DIG/4-1-p;

		if (re) {
			round *= 1<<(LDBL_MANT_DIG%4);
			while (re--) round*=16;
			if (*prefix=='-') {
				y=-y;
				y-=round;
				y+=round;
				y=-y;
			} else {
				y+=round;
				y-=round;
			}
		}

		estr=fmt_num(e2<0 ? -e2 : e2, ebuf, 0, 10);
		if (estr==ebuf) *--estr='0';
		*--estr = (e2<0 ? '-' : '+');
		*--estr = t+('p'-'a');

		s=buf;
		do {
			int x=y;
			*s++=xdigits[x]|(t&32);
			y=16*(y-x);
			if (s-buf==1 && (y||p>0||(fl&ALT_FORM))) *s++='.';
		} while (y);

		if (p > INT_MAX-2-(ebuf-estr)-pl)
			return -1;
		if (p && s-buf-2 < p)
			l = (p+2) + (ebuf-estr);
		else
			l = (s-buf) + (ebuf-estr);

		pad(f, ' ', w, pl+l, fl);
		out(f, prefix, pl);
		pad(f, '0', w, pl+l, fl^ZERO_PAD);
		out(f, buf, s-buf);
		pad(f, '0', l-(ebuf-estr)-(s-buf), 0, 0);
		out(f, estr, ebuf-estr);
		pad(f, ' ', w, pl+l, fl^LEFT_ADJ);
		return max(w, pl+l);
	}
	if (p<0) p=6;

	if (y) y *= 0x1p28, e2-=28;

	if (e2<0) a=r=z=big;
	else a=r=z=big+sizeof(big)/sizeof(*big) - LDBL_MANT_DIG - 1;

	do {
		*z = y;
		y = 1000000000*(y-*z++);
	} while (y);

	while (e2>0) {
		uint32_t carry=0;
		int sh=min(29,e2);
		for (d=z-1; d>=a; d--) {
			uint64_t x = ((uint64_t)*d<<sh)+carry;
			*d = x % 1000000000;
			carry = x / 1000000000;
		}
		if (carry) *--a = carry;
		while (z>a && !z[-1]) z--;
		e2-=sh;
	}
	while (e2<0) {
		uint32_t carry=0, *b;
		int sh=min(9,-e2), need=1+(p+LDBL_MANT_DIG/3U+8)/9;
		for (d=a; d<z; d++) {
			uint32_t rm = *d & (1<<sh)-1;
			*d = (*d>>sh) + carry;
			carry = (1000000000>>sh) * rm;
		}
		if (!*a) a++;
		if (carry) *z++ = carry;
		/* Avoid (slow!) computation past requested precision */
		b = (t|32)=='f' ? r : a;
		if (z-b > need) z = b+need;
		e2+=sh;
	}

	if (a<z) for (i=10, e=9*(r-a); *a>=i; i*=10, e++);
	else e=0;

	/* Perform rounding: j is precision after the radix (possibly neg) */
	j = p - ((t|32)!='f')*e - ((t|32)=='g' && p);
	if (j < 9*(z-r-1)) {
		uint32_t x;
		/* We avoid C's broken division of negative numbers */
		d = r + 1 + ((j+9*LDBL_MAX_EXP)/9 - LDBL_MAX_EXP);
		j += 9*LDBL_MAX_EXP;
		j %= 9;
		for (i=10, j++; j<9; i*=10, j++);
		x = *d % i;
		/* Are there any significant digits past j? */
		if (x || d+1!=z) {
			long double round = 2/LDBL_EPSILON;
			long double small;
			if ((*d/i & 1) || (i==1000000000 && d>a && (d[-1]&1)))
				round += 2;
			if (x<i/2) small=0x0.8p0;
			else if (x==i/2 && d+1==z) small=0x1.0p0;
			else small=0x1.8p0;
			if (pl && *prefix=='-') round*=-1, small*=-1;
			*d -= x;
			/* Decide whether to round by probing round+small */
			if (round+small != round) {
				*d = *d + i;
				while (*d > 999999999) {
					*d--=0;
					if (d<a) *--a=0;
					(*d)++;
				}
				for (i=10, e=9*(r-a); *a>=i; i*=10, e++);
			}
		}
		if (z>d+1) z=d+1;
	}
	for (; z>a && !z[-1]; z--);
	
	if ((t|32)=='g') {
		if (!p) p++;
		if (p>e && e>=-4) {
			t--;
			p-=e+1;
		} else {
			t-=2;
			p--;
		}
		if (!(fl&ALT_FORM)) {
			/* Count trailing zeros in last place */
			if (z>a && z[-1]) for (i=10, j=0; z[-1]%i==0; i*=10, j++);
			else j=9;
			if ((t|32)=='f')
				p = min(p,max(0,9*(z-r-1)-j));
			else
				p = min(p,max(0,9*(z-r-1)+e-j));
		}
	}
	if (p > INT_MAX-1-(p || (fl&ALT_FORM)))
		return -1;
	l = 1 + p + (p || (fl&ALT_FORM));
	if ((t|32)=='f') {
		if (e > INT_MAX-l) return -1;
		if (e>0) l+=e;
	} else {
		estr=fmt_num(e<0 ? -e : e, ebuf, 0, 10);
		while(ebuf-estr<2) *--estr='0';
		*--estr = (e<0 ? '-' : '+');
		*--estr = t;
		if (ebuf-estr > INT_MAX-l) return -1;
		l += ebuf-estr;
	}

	if (l > INT_MAX-pl) return -1;
	pad(f, ' ', w, pl+l, fl);
	out(f, prefix, pl);
	pad(f, '0', w, pl+l, fl^ZERO_PAD);

	if ((t|32)=='f') {
		if (a>r) a=r;
		for (d=a; d<=r; d++) {
			char *s = fmt_num(*d, buf+9, 0, 10);
			if (d!=a) while (s>buf) *--s='0';
			else if (s==buf+9) *--s='0';
			out(f, s, buf+9-s);
		}
		if (p || (fl&ALT_FORM)) out(f, ".", 1);
		for (; d<z && p>0; d++, p-=9) {
			char *s = fmt_num(*d, buf+9, 0, 10);
			while (s>buf) *--s='0';
			out(f, s, min(9,p));
		}
		pad(f, '0', p+9, 9, 0);
	} else {
		if (z<=a) z=a+1;
		for (d=a; d<z && p>=0; d++) {
			char *s = fmt_num(*d, buf+9, 0, 10);
			if (s==buf+9) *--s='0';
			if (d!=a) while (s>buf) *--s='0';
			else {
				out(f, s++, 1);
				if (p>0||(fl&ALT_FORM)) out(f, ".", 1);
			}
			out(f, s, min(buf+9-s, p));
			p -= buf+9-s;
		}
		pad(f, '0', p+18, 18, 0);
		out(f, estr, ebuf-estr);
	}

	pad(f, ' ', w, pl+l, fl^LEFT_ADJ);

	return max(w, pl+l);
}
#endif

#if !LIBOS_CONFIG_PRINTF_TINY
static int getint(char **s) {
	int i;
	for (i=0; isdigit(**s); (*s)++) {
		if (i > INT_MAX/10U || **s-'0' > INT_MAX-10*i) i = -1;
		else 
            i = 10*i + (**s-'0');
	}
	return i;
}
#endif

int printf_core(int (*put)(char c, void * f), void * f, const char *fmt, va_list *ap
#if !LIBOS_CONFIG_PRINTF_TINY
, union arg *nl_arg, int *nl_type
#endif
)
{
	char *a, *z, *s=(char *)fmt;
	int w, p;
	union arg arg;
	unsigned fl;
#if !LIBOS_CONFIG_PRINTF_TINY
	unsigned l10n=0;
	int xp;
    int argpos;
	unsigned ps;
#endif    
	unsigned st;
	int cnt=0, l=0;
	char buf[
        sizeof(uintmax_t)*3+3
#if LIBOS_CONFIG_FLOAT
            +LDBL_MANT_DIG/4
#endif        
    ];
	const char *prefix;
	int t, pl;
#if LIBOS_CONFIG_PRINTF_UNICODE
	size_t i;
	wchar_t wc[2], *ws;
	char mb[4];
#endif	

	for (;;) {
#if !LIBOS_CONFIG_PRINTF_TINY
		/* This error is only specified for snprintf, but since it's
		 * unspecified for other forms, do the same. Stop immediately
		 * on overflow; otherwise %n could produce wrong results. */
		if (l > INT_MAX - cnt) goto overflow;
#endif
		/* Update output count, end loop when fmt is exhausted */
		cnt += l;
		if (!*s) break;

		/* Handle literal text and %% format specifiers */
		for (a=s; *s && *s!='%'; s++);
		for (z=s; s[0]=='%' && s[1]=='%'; z++, s+=2);
#if !LIBOS_CONFIG_PRINTF_TINY
		if (z-a > INT_MAX-cnt) goto overflow;
#endif		
		l = z-a;
		out(put, f, a, l);
		if (l) continue;

#if !LIBOS_CONFIG_PRINTF_TINY
		if (isdigit(s[1]) && s[2]=='$') {
			l10n=1;
			argpos = s[1]-'0';
			s+=3;
		} else 
#endif        
        {
			s++;
		}

#if !LIBOS_CONFIG_PRINTF_TINY
		/* Read modifier flags */
		for (fl=0; (unsigned)*s-' '<32 && (FLAGMASK&(1U<<*s-' ')); s++)
			fl |= 1U<<*s-' ';
#else	
		fl = 0;
#endif			

		/* Read field width */
#if !LIBOS_CONFIG_PRINTF_TINY
		if (*s=='*') {
			if (isdigit(s[1]) && s[2]=='$') {
				l10n=1;
				nl_type[s[1]-'0'] = S32;
				w = nl_arg[s[1]-'0'].i;
				s+=3;
			} else 
            if (!l10n) {
				w = f ? va_arg(*ap, int) : 0;
				s++;
			} else goto ilwal;
			if (w<0) fl|=LEFT_ADJ, w=-w;
		} 
        else if ((w=getint(&s))<0) goto overflow;
#else
		w = 0;		
#endif
#if LIBOS_CONFIG_FLOAT
		/* Read precision */
		if (*s=='.' && s[1]=='*') {
			if (isdigit(s[2]) && s[3]=='$') {
				nl_type[s[2]-'0'] = S32;
				p = nl_arg[s[2]-'0'].i;
				s+=4;
			} else 
            if (!l10n) {
				p = f ? va_arg(*ap, int) : 0;
				s+=2;
			} else goto ilwal;
			xp = (p>=0);
		} else if (*s=='.') {
			s++;
			p = getint(&s);
			xp = 1;
		} else 
#endif		
        {
			p = -1;
#if !LIBOS_CONFIG_PRINTF_TINY
			xp = 0;
#endif			
		}

		/* Format specifier state machine */
		st=0;
		do {
			if (OOB(*s)) goto ilwal;
#if !LIBOS_CONFIG_PRINTF_TINY
			ps=st;
#endif			
			st=states[st]S(*s++);
		} while (st-1<STOP);

		// coverity[dead_error_line]
		if (!st) goto ilwal;

		/* Check validity of argument type (nl/normal) */   
#if !LIBOS_CONFIG_PRINTF_TINY
        if (argpos>=0) nl_type[argpos]=st, arg=nl_arg[argpos];
        else 
#endif
        pop_arg(&arg, st, ap);
        
		z = buf + sizeof(buf);
		prefix = "-+   0X0x";
		pl = 0;
		t = s[-1];

#if !LIBOS_CONFIG_PRINTF_TINY
		/* Transform ls,lc -> S,C */
		if (ps && (t&15)==3) t&=~32;

		/* - and 0 flags are mutually exclusive */
		if (fl & LEFT_ADJ) fl &= ~ZERO_PAD;
#endif

		switch(t) {
#if !LIBOS_CONFIG_PRINTF_TINY
		case 'n':
			switch(ps) {
			case BARE: *(int *)arg.p = cnt; break;
            
			case LLPRE:     
            case ZJTPRE:
            case LPRE: *(long *)arg.p = cnt; break;
			case HPRE: *(unsigned short *)arg.p = cnt; break;
			case HHPRE: *(unsigned char *)arg.p = cnt; break;
			}
			continue;
#endif			
		// coverity[unterminated_case]
		case 'p':
#if !LIBOS_CONFIG_PRINTF_TINY
			p = max(p, 2*sizeof(void*));
#endif			
			t = 'x';
			fl |= ALT_FORM;

		case 'x': case 'X':
			a = fmt_num(arg.i, z, t&32, 16);
			if (arg.i && (fl & ALT_FORM)) prefix+=(t>>4), pl=2;
			if (0) {
		case 'o':
			a = fmt_num(arg.i, z, 0, 8);
			// coverity[assigned_value]
			if ((fl&ALT_FORM) && p<z-a+1) p=z-a+1;
			} if (0) {
		case 'd': case 'i':
			pl=1;
			if (((int64_t)arg.i) < 0) {
				arg.i=-arg.i;
			} else if (fl & MARK_POS) {
				// coverity[dead_error_line]
				prefix++;
			} else if (fl & PAD_POS) {
				// coverity[dead_error_line]
				prefix+=2;
			} else pl=0;
		case 'u':
			a = fmt_num(arg.i, z,0, 10);
			}
#if !LIBOS_CONFIG_PRINTF_TINY			
			if (xp && p<0) goto overflow;
			if (xp) fl &= ~ZERO_PAD;
			if (!arg.i && !p) {
				a=z;
				break;
			}
			p = max(p, z-a + !arg.i);
#else
			p = !arg.i;
#endif			
			break;
		case 'c':
			*(a=z-(p=1))=arg.i;
			// coverity[assigned_value]
			fl &= ~ZERO_PAD;
			break;
		case 's':
#if !LIBOS_CONFIG_PRINTF_TINY
			a = arg.p ? arg.p : "(null)";
#else
			a = arg.p;			
#endif
			// coverity[dead_error_line]
			z = a + strnlen(a, p<0 ? INT_MAX : p);
#if !LIBOS_CONFIG_PRINTF_TINY
			if (p<0 && *z) goto overflow;
			fl &= ~ZERO_PAD;
#endif
			p = z-a;
			break;

#if LIBOS_CONFIG_PRINTF_UNICODE
		case 'C':
			wc[0] = arg.i;
			wc[1] = 0;
			arg.p = wc;
			p = -1;
		case 'S':
			ws = arg.p;
			for (i=l=0; i<p && *ws && (l=wctomb(mb, *ws++))>=0 && l<=p-i; i+=l);
			if (l<0) return -1;
			if (i > INT_MAX) goto overflow;
			p = i;
			pad(f, ' ', w, p, fl);
			ws = arg.p;
			for (i=0; i<0U+p && *ws && i+(l=wctomb(mb, *ws++))<=p; i+=l)
				out(put, f, mb, l);
			pad(put, f, ' ', w, p, fl^LEFT_ADJ);
			l = w>p ? w : p;
			continue;
#endif          
#if LIBOS_CONFIG_FLOAT  
		case 'e': case 'f': case 'g': case 'a':
		case 'E': case 'F': case 'G': case 'A':
			if (xp && p<0) goto overflow;
			l = fmt_fp(f, arg.f, w, p, fl, t);
			if (l<0) goto overflow;
			continue;
#endif        
		}

#if !LIBOS_CONFIG_PRINTF_TINY
		if (p < z-a) p = z-a;
		if (p > INT_MAX-pl) goto overflow;
		if (w < pl+p) w = pl+p;
		if (w > INT_MAX-cnt) goto overflow;
		pad(put, f, ' ', w, pl+p, fl);
#endif
		out(put, f, prefix, pl);

#if !LIBOS_CONFIG_PRINTF_TINY
		pad(put, f, '0', w, pl+p, fl^ZERO_PAD);
		pad(put, f, '0', p, z-a, 0);
#else
        if (p)
            put('0',f);
#endif

		// coverity[overrun-local]
		out(put, f, a, z-a);
#if !LIBOS_CONFIG_PRINTF_TINY
		pad(put, f, ' ', w, pl+p, fl^LEFT_ADJ);
#endif
		l = w;
	}

	return cnt;

#if !LIBOS_CONFIG_PRINTF_TINY
	for (i=1; i<=NL_ARGMAX && nl_type[i]; i++)
		pop_arg(nl_arg+i, nl_type[i], ap);
	for (; i<=NL_ARGMAX && !nl_type[i]; i++);
	if (i<=NL_ARGMAX) goto ilwal;
#endif    
	return 1;

ilwal:
#if !LIBOS_CONFIG_PRINTF_TINY
overflow:
#endif
	return -1;
}

static int putc_internal(char ch, void * ctx) {
	putc(ch);
	return 0;
}

__attribute__((used)) void LibosPrintf(const char* format, ...)
{
    va_list argptr;
    va_start(argptr, format);
#if !LIBOS_CONFIG_PRINTF_TINY
	union arg nl_arg[NL_ARGMAX];
	int nl_type[NL_ARGMAX];
    printf_core(putc_internal, 0, format, &argptr, &nl_arg, &nl_type);
#else
	printf_core(putc_internal, 0, format, &argptr);
#endif	
    va_end(argptr);
}
