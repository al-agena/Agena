/*
** $Id: linit.c,v 1.14 2005/12/29 15:32:11 roberto Exp $
** Initialisation of libraries for agena.c
** See Copyright Notice in agena.h
*/

#define linit_c
#define LUA_LIB

#include <string.h>

#include "agena.h"
#include "agenalib.h"
#include "agnxlib.h"


static const luaL_Reg lualibs[] = {
  {"", luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_TABLIBNAME, luaopen_tables},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_strings},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_DBLIBNAME, luaopen_debug},
  {AGENA_UTILSLIBNAME, luaopen_utils},
  {AGENA_BINIOLIBNAME, luaopen_binio},
  {AGENA_RTABLELIBNAME, luaopen_rtable},
  {AGENA_ENVIRONLIBNAME, luaopen_environ},
  {AGENA_REGLIBNAME, luaopen_registers},
  {AGENA_SEQLIBNAME, luaopen_sequences},
  {AGENA_SETSLIBNAME, luaopen_sets},
  {AGENA_REGISTRYLIBNAME, luaopen_registry},  /* 2.9.2 */
  {AGENA_STACKLIBNAME, luaopen_stack},        /* 2.9.4 */
  {AGENA_MEMFILELIBNAME, luaopen_memfile},    /* 2.17.8, 2.21.0 */
  {AGENA_BFIELDLIBNAME, luaopen_bfield},      /* 2.31.4 */
  {AGENA_TUPLESLIBNAME, luaopen_tuples},      /* 2.36.1 */
  {AGENA_INILIBNAME, luaopen_ini},            /* 4.6.6 */
  /* Plus packages moved to the core interpreter, 3.7.0 */
  {AGENA_BAGSLIBNAME, luaopen_bags},
  {AGENA_BYTESLIBNAME, luaopen_bytes},        /* 2.13.0 */
  {AGENA_CALCLIBNAME, luaopen_calc},
  {AGENA_COMBINATLIBNAME, luaopen_combinat},
  {AGENA_FACTORYLIBNAME, luaopen_factory},    /* 2.12.3 */
  {AGENA_FZYLIBNAME, luaopen_fzy},            /* 4.8.2 */
  {AGENA_INTSLIBNAME, luaopen_ints},          /* 6.2.0 */
  {AGENA_LINALGLIBNAME, luaopen_linalg},
  {AGENA_LLISTLIBNAME, luaopen_llist},
  {AGENA_LONGLIBNAME, luaopen_long},          /* 2.33.0 DOS emu */
  {AGENA_LOOKUPLIBNAME, luaopen_lookup},
  {AGENA_NUMARRAYLIBNAME, luaopen_numarray},  /* 2.9.0 */
  {AGENA_DBLHASHLIBNAME, luaopen_dblhash},    /* 6.5.10 */
  {AGENA_NUMTHEORYLIBNAME, luaopen_numtheory},
  {AGENA_DUALLIBNAME, luaopen_dual},          /* 2.14.2 */
  {AGENA_REGEXLIBNAME, luaopen_regex},        /* 2.39.0/1 */
  {AGENA_SEMALIBNAME, luaopen_sema},          /* 2.14.0 */
  {AGENA_STATSLIBNAME, luaopen_stats},
  {AGENA_TRIELIBNAME, luaopen_trie},          /* 5.4.2 */
  {AGENA_UNITSLIBNAME, luaopen_units},        /* 3.11.1 */
  {AGENA_UTF8LIBNAME, luaopen_utf8},          /* 2.14.4 */
  {AGENA_VECINTLIBNAME, luaopen_vecint},      /* 3.10.2 */
  {AGENA_XBASELIBNAME, luaopen_xbase},
  {AGENA_KEYFILELIBNAME, luaopen_keyfile},    /* 7.6.3 */
  {AGENA_XMLLIBNAME, luaopen_xml},
  {AGENA_FIFOLIBNAME, luaopen_fifo},          /* 5.4.0 */
  {AGENA_HASHESLIBNAME, luaopen_hashes},      /* 2.3.1 */
  {AGENA_BIMAPSLIBNAME, luaopen_bimaps},      /* 4.7.2 */
  {AGENA_BLOOMLIBNAME, luaopen_bloom},        /* 2.12.0 RC 4 */
  {AGENA_CUCKOOLIBNAME, luaopen_cuckoo},      /* 3.13.2 */
  {AGENA_NUMCUCKOOLIBNAME, luaopen_numcuckoo},/* 5.6.10 */
  {AGENA_NUMFILTERLIBNAME, luaopen_numfilter},/* 7.5.13 */
  {AGENA_CLOCKLIBNAME, luaopen_clock},        /* 3.16.0 */
  {AGENA_RBTREELIBNAME, luaopen_rbtree},      /* 3.9.3 */
  {AGENA_HEAPSLIBNAME, luaopen_heaps},        /* 2.27.8, we use a different opening function in DOS & OS/2 */
  {AGENA_LIFOLIBNAME, luaopen_lifo},          /* 5.4.0 */
  {AGENA_DDLIBNAME, luaopen_dd},              /* 7.3.10 */
  {AGENA_UINTMAPLIBNAME, luaopen_uintmap},    /* 7.7.9 */
  {AGENA_INTMAPLIBNAME, luaopen_intmap},      /* 7.7.3/7.7.11 */
  {AGENA_DBLMAPLIBNAME, luaopen_dblmap},      /* 7.7.11 */
  {AGENA_SUFFIXLIBNAME, luaopen_suffix},      /* 7.9.0 */
#if defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI)
  {AGENA_ICONVLIBNAME, luaopen_iconv},        /* 2.26.1 */
  {AGENA_ADSLIBNAME, luaopen_ads},
  {AGENA_ASTROLIBNAME, luaopen_astro},
  {AGENA_COMLIBNAME, luaopen_com},            /* 2.18.2 */
  {AGENA_CORDICLIBNAME, luaopen_cordic},
  {AGENA_CURSESLIBNAME, luaopen_curses},      /* 4.7.5 ArcaOS */
  {AGENA_DOUBLELIBNAME, luaopen_double},      /* 3.7.1 */
  {AGENA_ABCILIBNAME, luaopen_abci},          /* 7.7.1 */
  {AGENA_STRHASHLIBNAME, luaopen_strhash},    /* 7.7.2 */
  {AGENA_STRMAPLIBNAME, luaopen_strmap},      /* 7.7.2 */
  {AGENA_FASTMATHLIBNAME, luaopen_fastmath},  /* 2.13.0 */
  {AGENA_LZLIBLIBNAME, luaopen_gzip},         /* 2.3.0 RC 2 eCS change */
  {AGENA_MAPMLIBNAME, luaopen_mapm},          /* 0.31.2 or earlier */
  {AGENA_MINIZIPLIBNAME, luaopen_minizip},    /* 5.3.0 */
  {AGENA_MPFRLIBNAME, luaopen_mpfr},          /* 2.21.9 */
  {AGENA_GMPLIBNAME, luaopen_gmp},            /* 2.16.3 */
  {AGENA_SKYCRANELIBNAME, luaopen_skycrane},
  {AGENA_TESTLIBLIBNAME, luaopen_testlib},    /* 3.1.3 */
  {AGENA_ZXLIBNAME, luaopen_zx},              /* 2.9.3 */
#endif
#if defined(__DJGPP__) || defined(__OS2__)    /* 2.17.0 extended for DOS */
  {AGENA_FRACTLIBNAME, luaopen_fractals},     /* 2.21.11 ArcaOS */
  {AGENA_IVALLIBNAME, luaopen_ival},          /* 5.0.0 */
  {AGENA_SQLITELIBNAME, luaopen_sqlite},      /* 5.2.1 */
#endif
#if defined(__OS2__)
  {AGENA_GDILIBNAME, luaopen_gdi},            /* 2.21.11 ArcaOS */
  {AGENA_NETLIBNAME, luaopen_net},            /* 2.21.11 ArcaOS */
#endif
  {NULL, NULL}
};


LUALIB_API void luaL_openlibs (lua_State *L) {
  const luaL_Reg *lib = lualibs;
  for (; lib->func; lib++) {
    luaL_checkstack(L, 2, "not enough stack space");  /* 4.6.5 fix */
    lua_pushcfunction(L, lib->func);
    lua_pushstring(L, lib->name);
    lua_call(L, 1, 0);
  }
}


/* Checks whether package belongs to those packages initialised at startup */
LUALIB_API int luaL_isstandardlib (lua_State *L, const char *libname) {  /* 2.29.7 */
  const luaL_Reg *lib = lualibs;
  for (; lib->name; lib++) {
    if (tools_streq(lib->name, libname)) return 1;
  }
  return 0;
}


LUALIB_API int luaL_standardlibs (lua_State *L) {
  const luaL_Reg *lib = lualibs;
  agn_createset(L, luaI_libsize(lib));  /* 4.6.5 improvement */
  for (; lib->name; lib++) {
    lua_pushstring(L, tools_streq(lib->name, "") ? "(baselib)" : lib->name);
    lua_srawset(L, -2);
  }
  return 1;
}


/* Pushes a sorted table of all the C functions in standard library `libname' onto the stack. If you want the names of
   the base library, just pass the empty string for `libname'. In case of an error, pushes nothing and returns 0, and 1
   otherwise. 4.6.5 */
LUALIB_API int luaL_libcfuncs (lua_State *L, const char *libname) {
  int c = 0;
  luaL_checkstack(L, 5, "not enough stack space");
  lua_getfield(L, LUA_REGISTRYINDEX, "_origG");
  if (!lua_istable(L, -1)) {
    agn_poptop(L);
    return 0;
  }
  if (!tools_streq("", libname)) {
    lua_getfield(L, -1, libname);
    if (!lua_istable(L, -1)) {
      agn_poptoptwo(L);
      return 0;
    }
    lua_remove(L, -2);  /* drop origG */
  }
  lua_createtable(L, 4, 0);
  lua_pushnil(L);
  while (lua_next(L, -3)) {
    if (lua_iscfunction(L, -1)) {
      lua_pushvalue(L, -2);  /* push key */
      lua_rawseti(L, -4, ++c);
    }
    agn_poptop(L);
  }
  lua_remove(L, -2);  /* drop library table */
  lua_getglobal(L, "sort");
  lua_pushvalue(L, -2);
  lua_call(L, 1, 0);
  return 1;
}

