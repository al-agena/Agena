/*
** $Id: agenalib.h, 2009/03/01 15:39:42 $
** $Id: lualib.h,v 1.36 2005/12/27 17:12:00 roberto Exp $
** Agena standard libraries
** See Copyright Notice in agena.h
*/

#ifndef agenalib_h
#define agenalib_h

#include "agena.h"

#define LUA_COLIBNAME   "coroutine"
LUALIB_API int (luaopen_base) (lua_State *L);

#define LUA_TABLIBNAME  "tables"
LUALIB_API int (luaopen_tables) (lua_State *L);

#define LUA_IOLIBNAME   "io"
LUALIB_API int (luaopen_io) (lua_State *L);

#define LUA_OSLIBNAME   "os"
LUALIB_API int (luaopen_os) (lua_State *L);

#define LUA_STRLIBNAME  "strings"
LUALIB_API int (luaopen_strings) (lua_State *L);

#define LUA_MATHLIBNAME "math"
LUALIB_API int (luaopen_math) (lua_State *L);

#define LUA_DBLIBNAME   "debug"
LUALIB_API int (luaopen_debug) (lua_State *L);

#define LUA_LOADLIBNAME "package"
LUALIB_API int (luaopen_package) (lua_State *L);

#define AGENA_UTILSLIBNAME "utils"
LUALIB_API int (luaopen_utils) (lua_State *L);

#define AGENA_BINIOLIBNAME "binio"
LUALIB_API int (luaopen_binio) (lua_State *L);

#define AGENA_RTABLELIBNAME "rtable"
LUALIB_API int (luaopen_rtable) (lua_State *L);

#define AGENA_ENVIRONLIBNAME "environ"
LUALIB_API int (luaopen_environ) (lua_State *L);

#define AGENA_SEQLIBNAME "sequences"
LUALIB_API int (luaopen_sequences) (lua_State *L);

#define AGENA_SETSLIBNAME "sets"
LUALIB_API int (luaopen_sets) (lua_State *L);

#define AGENA_REGLIBNAME "registers"
LUALIB_API int (luaopen_registers) (lua_State *L);

#define AGENA_REGISTRYLIBNAME "registry"
LUALIB_API int (luaopen_registry) (lua_State *L);  /* 2.9.2 */

#define AGENA_STACKLIBNAME "stack"
LUALIB_API int (luaopen_stack) (lua_State *L);     /* 2.9.4 */

#define AGENA_MEMFILELIBNAME "memfile"
LUALIB_API int (luaopen_memfile) (lua_State *L);   /* 2.17.8, 2.21.0 */

#define AGENA_BFIELDLIBNAME "bfield"
LUALIB_API int (luaopen_bfield) (lua_State *L);    /* 2.31.4 */

#define AGENA_TUPLESLIBNAME "tuples"               /* 2.36.1 */
LUALIB_API int luaopen_tuples (lua_State *L);

#define AGENA_VECINTLIBNAME "vecint"               /* 3.10.2 */
LUALIB_API int luaopen_vecint (lua_State *L);

#define AGENA_UNITSLIBNAME "units"                 /* 3.11.1 */
LUALIB_API int luaopen_units (lua_State *L);

#define AGENA_FZYLIBNAME "fzy"                     /* 4.8.2 */
LUALIB_API int luaopen_fzy (lua_State *L);

#define AGENA_TRIELIBNAME "trie"
LUALIB_API int (luaopen_trie) (lua_State *L);      /* 5.4.2 */

/* From here on follow all the former plus packages integrated into the main interpreter with 3.7.0 */

#define AGENA_BAGSLIBNAME "bags"
LUALIB_API int (luaopen_bags) (lua_State *L);

#define AGENA_LOOKUPLIBNAME "lookup"
LUALIB_API int (luaopen_lookup) (lua_State *L);

#define AGENA_CALCLIBNAME "calc"
LUALIB_API int (luaopen_calc) (lua_State *L);

#define AGENA_LINALGLIBNAME "linalg"
LUALIB_API int (luaopen_linalg) (lua_State *L);

#define AGENA_STATSLIBNAME "stats"
LUALIB_API int (luaopen_stats) (lua_State *L);

#define AGENA_COMBINATLIBNAME "combinat"
LUALIB_API int (luaopen_combinat) (lua_State *L);

#define AGENA_NUMTHEORYLIBNAME "numtheory"
LUALIB_API int (luaopen_numtheory) (lua_State *L);

#define AGENA_LONGLIBNAME "long"                   /* 2.33.0 ArcaOS change */
LUALIB_API int (luaopen_long) (lua_State *L);

#define AGENA_INTSLIBNAME "ints"
LUALIB_API int (luaopen_ints) (lua_State *L);      /* 6.2.0 */

#define AGENA_DUALLIBNAME "dual"
LUALIB_API int (luaopen_dual) (lua_State *L);      /* 2.14.0 */

#define AGENA_XBASELIBNAME "xbase"
LUALIB_API int (luaopen_xbase) (lua_State *L);

#define AGENA_KEYFILELIBNAME "keyfile"             /* 7.6.3 */
LUALIB_API int (luaopen_keyfile) (lua_State *L);

#define AGENA_XMLLIBNAME "xml"
LUALIB_API int (luaopen_xml) (lua_State *L);       /* 2.3.0 eCS change */

#define AGENA_LLISTLIBNAME "llist"
LUALIB_API int (luaopen_llist) (lua_State *L);

#define AGENA_ULISTLIBNAME "ulist"
LUALIB_API int (luaopen_ulist) (lua_State *L);

#define AGENA_DLISTLIBNAME "dlist"
LUALIB_API int (luaopen_dlist) (lua_State *L);

#define AGENA_FACTORYLIBNAME "factory"
LUALIB_API int (luaopen_factory) (lua_State *L);   /* 2.12.3 */

#define AGENA_BYTESLIBNAME "bytes"
LUALIB_API int (luaopen_bytes) (lua_State *L);     /* 2.13.0 */

#define AGENA_SEMALIBNAME "sema"
LUALIB_API int (luaopen_sema) (lua_State *L);      /* 2.14.0 */

#define AGENA_NUMARRAYLIBNAME "numarray"
LUALIB_API int (luaopen_numarray) (lua_State *L);  /* 2.9.0 */

#define AGENA_DBLHASHLIBNAME "dblhash"
LUALIB_API int (luaopen_dblhash) (lua_State *L);   /* 6.5.10 */

#define AGENA_UTF8LIBNAME "utf8"
LUALIB_API int (luaopen_utf8) (lua_State *L);      /* 2.14.4 */

#define AGENA_INILIBNAME "ini"
LUALIB_API int (luaopen_ini) (lua_State *L);       /* 4.6.6 */

#define AGENA_REGEXLIBNAME "regex"
LUALIB_API int (luaopen_regex) (lua_State *L);     /* 2.39.0/1 ArcaOS/DJGPP change */

#define AGENA_HASHESLIBNAME "hashes"
LUALIB_API int (luaopen_hashes) (lua_State *L);    /* 2.3.1 */

#define AGENA_BLOOMLIBNAME "bloom"
LUALIB_API int (luaopen_bloom) (lua_State *L);     /* 2.12.0 RC 4 */

#define AGENA_CUCKOOLIBNAME "cuckoo"
LUALIB_API int (luaopen_cuckoo) (lua_State *L);    /* 3.12.2 */

#define AGENA_NUMCUCKOOLIBNAME "numcuckoo"
LUALIB_API int (luaopen_numcuckoo) (lua_State *L); /* 5.6.10 */

#define AGENA_NUMFILTERLIBNAME "numfilter"
LUALIB_API int (luaopen_numfilter) (lua_State *L); /* 7.5.13 */

#define AGENA_FIFOLIBNAME "fifo"
LUALIB_API int (luaopen_fifo) (lua_State *L);      /* 5.4.0 */

#define AGENA_LIFOLIBNAME "lifo"
LUALIB_API int (luaopen_lifo) (lua_State *L);      /* 5.4.0 */

#define AGENA_RBTREELIBNAME "rbtree"
LUALIB_API int (luaopen_rbtree) (lua_State *L);    /* 3.9.3 */

#define AGENA_CLOCKLIBNAME "clock"                 /* 3.16.0 */
LUALIB_API int (luaopen_clock) (lua_State *L);

#define AGENA_BIMAPSLIBNAME "bimaps"
LUALIB_API int (luaopen_bimaps) (lua_State *L);    /* 4.7.2 */

#define AGENA_HEAPSLIBNAME "avl"  /* not "heaps" ! */
LUALIB_API int (luaopen_heaps) (lua_State *L);     /* 2.27.8 */

#define AGENA_DDLIBNAME "dd"                       /* 7.3.10 */
LUALIB_API int (luaopen_dd) (lua_State *L);

#define AGENA_UINTMAPLIBNAME "uintmap"
LUALIB_API int (luaopen_uintmap) (lua_State *L);  /* 7.7.9 */

#define AGENA_INTMAPLIBNAME "intmap"
LUALIB_API int (luaopen_intmap) (lua_State *L);   /* 7.7.3/7.7.11 */

#define AGENA_DBLMAPLIBNAME "dblmap"
LUALIB_API int (luaopen_dblmap) (lua_State *L);  /* 7.7.11 */

#define AGENA_SUFFIXLIBNAME "suffix"
LUALIB_API int (luaopen_suffix) (lua_State *L);   /* 7.9.0 */

/* *** For DOS, OS/2 and ANSI ***********************************************************/

#if defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI)
/* 0.27.1: compile the following packages into the Agena executable */

#define AGENA_MAPMLIBNAME "mapm"
LUALIB_API int (luaopen_mapm) (lua_State *L);

#define AGENA_LZLIBLIBNAME "gzip"
LUALIB_API int (luaopen_gzip) (lua_State *L);      /* 2.3.0 RC 2 eCS change */

#define AGENA_ADSLIBNAME "ads"
LUALIB_API int (luaopen_ads) (lua_State *L);

#define AGENA_NETLIBNAME "net"
LUALIB_API int (luaopen_net) (lua_State *L);

#define AGENA_ASTROLIBNAME "astro"
LUALIB_API int (luaopen_astro) (lua_State *L);

#define AGENA_SKYCRANELIBNAME "skycrane"
LUALIB_API int (luaopen_skycrane) (lua_State *L);

#define AGENA_CORDICLIBNAME "cordic"
LUALIB_API int (luaopen_cordic) (lua_State *L);

#define AGENA_ZXLIBNAME "zx"
LUALIB_API int (luaopen_zx) (lua_State *L);        /* 2.9.3 */

#define AGENA_FASTMATHLIBNAME "fastmath"
LUALIB_API int (luaopen_fastmath) (lua_State *L);  /* 2.13.0 */

#define AGENA_GMPLIBNAME "gmp"
LUALIB_API int (luaopen_gmp) (lua_State *L);       /* 2.16.3 */

#define AGENA_MPFRLIBNAME "mpfr"
LUALIB_API int (luaopen_mpfr) (lua_State *L);      /* 2.21.9 */

#define AGENA_COMLIBNAME "com"
LUALIB_API int (luaopen_com) (lua_State *L);       /* 2.18.2 */

#define AGENA_ICONVLIBNAME "iconv"
LUALIB_API int (luaopen_iconv) (lua_State *L);     /* 2.26.1 */

#define AGENA_TESTLIBLIBNAME "testlib"
LUALIB_API int (luaopen_testlib) (lua_State *L);   /* 3.1.3 */

#define AGENA_DOUBLELIBNAME "double"               /* 3.7.1 */
LUALIB_API int (luaopen_double) (lua_State *L);

#define AGENA_ABCILIBNAME "abci"
LUALIB_API int (luaopen_abci) (lua_State *L);     /* 7.7.1 addition */

#define AGENA_STRHASHLIBNAME "strhash"
LUALIB_API int (luaopen_strhash) (lua_State *L);  /* 7.7.2 */

#define AGENA_STRMAPLIBNAME "strmap"
LUALIB_API int (luaopen_strmap) (lua_State *L);   /* 7.7.2 */

#endif

#if defined(__OS2__) || defined(__DJGPP__)           /* 2.17.0 extended for DOS */

#define AGENA_FRACTLIBNAME "fractals"
LUALIB_API int (luaopen_fractals) (lua_State *L);  /* 2.21.11 ArcaOS change */

#define AGENA_IVALLIBNAME "ival"                   /* 5.0.0 */
LUALIB_API int luaopen_ival (lua_State *L);

#define AGENA_SQLITELIBNAME "sqlite"               /* 5.2.1 */
LUALIB_API int luaopen_sqlite (lua_State *L);

#define AGENA_MINIZIPLIBNAME "minizip"             /* 5.3.0 */
LUALIB_API int luaopen_minizip (lua_State *L);

#define AGENA_CURSESLIBNAME "curses"
LUALIB_API int (luaopen_curses) (lua_State *L);    /* 4.7.5 addition */

#endif

#if defined(__OS2__)

#define AGENA_NETLIBNAME "net"
LUALIB_API int (luaopen_net) (lua_State *L);

#define AGENA_GDILIBNAME "gdi"
LUALIB_API int (luaopen_gdi) (lua_State *L);       /* 2.21.11 ArcaOS change */

#endif

/* open all previous libraries */
LUALIB_API void (luaL_openlibs) (lua_State *L);
LUALIB_API int  (luaL_isstandardlib) (lua_State *L, const char *libname);
LUALIB_API int  (luaL_standardlibs) (lua_State *L);
LUALIB_API int  (luaL_libcfuncs) (lua_State *L, const char *libname);

#ifndef lua_assert
#define lua_assert(x)   ((void)0)
#endif

#endif

