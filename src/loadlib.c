/*
** $Id: loadlib.c,v 1.52 2006/04/10 18:27:23 roberto Exp $
** Dynamic library loader for Lua/Agena
** See Copyright Notice in agena.h
** Revision 08.08.2009 22:13:44
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Darwin (Mac OS X), an
** implementation for Windows, and a stub for other systems.
*/


#include <stdlib.h>
#include <string.h>

#define loadlib_c
#define LUA_LIB

#include "agena.h"
#include "agenalib.h"
#include "agnxlib.h"
#include "lstate.h"


/* prefix for open functions in C libraries */
#define LUA_POF    "luaopen_"

/* separator for open functions in C libraries */
#define LUA_OFSEP  "_"


#define LIBPREFIX  "LOADLIB: "

#define POF        LUA_POF
#define LIB_FAIL   "open"


/* error codes for ll_loadfunc */
#define ERRLIB     1
#define ERRFUNC    2


static void ll_unloadlib (void *lib);
static void *ll_load (lua_State *L, const char *path);
static lua_CFunction ll_sym (lua_State *L, void *lib, const char *sym);


#if defined(LUA_DL_DLOPEN)
/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface.
** The dlfcn interface is available in Linux, SunOS, Solaris, IRIX, FreeBSD,
** NetBSD, AIX 4.2, HPUX 11, and  probably most other Unix flavors, at least
** as an emulation layer on top of native functions.
** =========================================================================
*/

#include <dlfcn.h>

static void ll_unloadlib (void *lib) {
  dlclose(lib);
}


static void *ll_load (lua_State *L, const char *path) {
  void *lib = dlopen(path, RTLD_NOW);
  const char *errmessage = dlerror();
  if (lib == NULL) {
    if (L->settings & 16)
      fprintf(stderr, "   ll_load: `%s`\n", errmessage);
    lua_pushstring(L, errmessage);
  }
  return lib;
}


static lua_CFunction ll_sym (lua_State *L, void *lib, const char *sym) {
  lua_CFunction f = (lua_CFunction)dlsym(lib, sym);
  if (f == NULL) lua_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(LUA_DL_DLL)
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>

static void pusherror (lua_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,  /* Lua 5.1.2 patch */
      NULL, error, 0, buffer, sizeof(buffer), NULL))
    lua_pushstring(L, buffer);
  else
    lua_pushfstring(L, "system error %d\n", error);
}

static void ll_unloadlib (void *lib) {
  FreeLibrary((HINSTANCE)lib);
}


static void *ll_load (lua_State *L, const char *path) {
  HINSTANCE lib = LoadLibraryA(path); /* Lua 5.1.2 patch */
  if (lib == NULL) pusherror(L);
  return lib;
}


static lua_CFunction ll_sym (lua_State *L, void *lib, const char *sym) {
  lua_CFunction f = (lua_CFunction)GetProcAddress((HINSTANCE)lib, sym);
  if (f == NULL) pusherror(L);
  return f;
}

/* }====================================================== */



#elif defined(LUA_DL_DYLD)
/*
** {======================================================================
** Native Mac OS X / Darwin Implementation
** =======================================================================
*/

#include <mach-o/dyld.h>


/* Mac appends a `_' before C function names */
#undef POF
#define POF  "_" LUA_POF


static void pusherror (lua_State *L) {
  const char *err_str;
  const char *err_file;
  NSLinkEditErrors err;
  int err_num;
  NSLinkEditError(&err, &err_num, &err_file, &err_str);
  lua_pushstring(L, err_str);
}


static const char *errorfromcode (NSObjectFileImageReturnCode ret) {
  switch (ret) {
    case NSObjectFileImageInappropriateFile:
      return "file is not a bundle";
    case NSObjectFileImageArch:
      return "library is for wrong CPU type";
    case NSObjectFileImageFormat:
      return "bad format";
    case NSObjectFileImageAccess:
      return "cannot access file";
    case NSObjectFileImageFailure:
    default:
      return "unable to load library";
  }
}


static void ll_unloadlib (void *lib) {
  NSUnLinkModule((NSModule)lib, NSUNLINKMODULE_OPTION_RESET_LAZY_REFERENCES);
}


static void *ll_load (lua_State *L, const char *path) {
  NSObjectFileImage img;
  NSObjectFileImageReturnCode ret;
  /* this would be a rare case, but prevents crashing if it happens */
  if(!_dyld_present()) {
    lua_pushliteral(L, "dyld not present");
    return NULL;
  }
  ret = NSCreateObjectFileImageFromFile(path, &img);
  if (ret == NSObjectFileImageSuccess) {
    NSModule mod = NSLinkModule(img, path, NSLINKMODULE_OPTION_PRIVATE |
                       NSLINKMODULE_OPTION_RETURN_ON_ERROR);
    NSDestroyObjectFileImage(img);
    if (mod == NULL) pusherror(L);
    return mod;
  }
  lua_pushstring(L, errorfromcode(ret));
  return NULL;
}


static lua_CFunction ll_sym (lua_State *L, void *lib, const char *sym) {
  NSSymbol nss = NSLookupSymbolInModule((NSModule)lib, sym);
  if (nss == NULL) {
    lua_pushfstring(L, "symbol " LUA_QS " not found", sym);
    return NULL;
  }
  return (lua_CFunction)NSAddressOfSymbol(nss);
}

/* }====================================================== */



#else
/*
** {======================================================
** Fallback for other systems
** =======================================================
*/

#undef LIB_FAIL
#define LIB_FAIL  "absent"


#define DLMSG  "dynamic libraries not enabled in strict ANSI C version of Agena"


static void ll_unloadlib (void *lib) {
  (void)lib;  /* to avoid warnings */
}


static void *ll_load (lua_State *L, const char *path) {
  (void)path;  /* to avoid warnings */
  lua_pushliteral(L, DLMSG);
  return NULL;
}


static lua_CFunction ll_sym (lua_State *L, void *lib, const char *sym) {
  (void)lib; (void)sym;  /* to avoid warnings */
  lua_pushliteral(L, DLMSG);
  return NULL;
}

/* }====================================================== */
#endif



static void **ll_register (lua_State *L, const char *path) {
  void **plib;
  lua_pushfstring(L, "%s%s", LIBPREFIX, path);
  lua_gettable(L, LUA_REGISTRYINDEX);  /* check library in registry? */
  if (!lua_isnil(L, -1))  /* is there an entry? */
    plib = (void **)lua_touserdata(L, -1);
  else {  /* no entry yet; create one */
    agn_poptop(L);
    plib = (void **)lua_newuserdata(L, sizeof(const void *));
    *plib = NULL;
    luaL_getmetatable(L, "_LOADLIB");
    lua_setmetatable(L, -2);
    lua_pushfstring(L, "%s%s", LIBPREFIX, path);
    lua_pushvalue(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);
  }
  return plib;
}


/*
** __gc tag method: calls library's `ll_unloadlib' function with the lib
** handle
*/
static int gctm (lua_State *L) {
  void **lib = (void **)luaL_checkudata(L, 1, "_LOADLIB");
  if (*lib) ll_unloadlib(*lib);
  *lib = NULL;  /* mark library as closed */
  return 0;
}


static int ll_loadfunc (lua_State *L, const char *path, const char *sym) {
  void **reg = ll_register(L, path);
  if (*reg == NULL) *reg = ll_load(L, path);  /* pushes a literal */
  if (*reg == NULL)
    return ERRLIB;  /* return 1; -> unable to load library */
  else {
    lua_CFunction f = ll_sym(L, *reg, sym);  /* pushes a literal */
    if (f == NULL)
      return ERRFUNC;  /* return 2; -> unable to find function */
    lua_pushcfunction(L, f);
    return 0;  /* return function */
  }
}

/*
** {======================================================
** 'require' function
** =======================================================
*/


static int readable (const char *filename) {
  FILE *f = fopen(filename, "r");  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}


static void loaderror (lua_State *L, const char *filename) {
  luaL_error(L, "Error loading module " LUA_QS " from file " LUA_QS ":\n\t%s",
                lua_tostring(L, 1), filename, lua_tostring(L, -1));
}


static const char *mkfuncname (lua_State *L, const char *modname) {
  const char *funcname;
  const char *mark = strchr(modname, *LUA_IGMARK);
  if (mark) modname = mark + 1;
  funcname = luaL_gsub(L, modname, ".", LUA_OFSEP);
  funcname = lua_pushfstring(L, POF"%s", funcname);
  lua_remove(L, -2);  /* remove 'gsub' result */
  return funcname;
}


/* 0.12.0 lazy hack to read DLLs from readlib, initialises the dynamic link file, pushes nothing;
   modified 0.25.4, August 01, 2009; 1.6.0 Valgrind mostly rewritten;
   on *success:
    0 : DLL file not found
    1 : DLL successfully initialised
   -1 : DLL corrupt
   -2 : call to luaopen_<pkgname>() function failed

   The function returns the function name of the library opener as a string, or NULL on failure. */
const char *fastloader_C (lua_State *L, const char *filename, const char *packagename, int *success, int printerror) {
  const char *funcname = NULL;
  *success = 0;  /* file not found */
  if (readable(filename)) {
    int n1, n2;
    n1 = lua_gettop(L);
    funcname = mkfuncname(L, packagename);  /* creates luaopen_<pkgname> string */
    /* now push luaopen_<pkgname>() function or an error message on stack */
    if (ll_loadfunc(L, filename, funcname) != 0) {  /* error, stack will be levelled at the end */
      *success = -1;  /* file corrupt */
      if (printerror) loaderror(L, filename);  /* issues an error ! 0.29.0 patch */
    } else {
      /* run loaded module and catch error if any; if printerror is 0 and the DLL loader did not run
         successful, the error string will be popped below; 7.3.5 hardening */
      if (lua_pcall(L, 0, 0, 0) != 0) {
        *success = -2;  /* initialisation failed */
        if (printerror) loaderror(L, filename);  /* issues an error ! 0.29.0 patch */
      } else {
        *success = 1;
        /* register package name in _READLIBBED set of the registry */
        agn_setreadlibbed(L, packagename);
      }
      /* library loaded and run successfully */
    }
    n2 = lua_gettop(L);
    if (n2 > n1) lua_pop(L, n2 - n1);  /* restore stack to its previous state */
  }
  return funcname;
}

static int pkg_loadclib (lua_State *L) {  /* changed 1.5.0, 1.6.0 Valgrind */
  int success;
  #if !defined(__OS2__) && !defined(LUA_ANSI)  /* 0.27.1 patch */
  const char *s, *dirpath;
  char *fn;
  s = luaL_checkstring(L, 1);  /* name of package */
  dirpath = luaL_checkstring(L, 2);  /* path to the package (no filename, no trailing slash !) */
  /* try finding and reading CLIB (DLLs, SOs) */
  fn = str_concat(dirpath, "/", s, AGN_CLIB, NULL);
  if (fn == NULL)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "package.loadclib");
  /* first increase the stack top before inserting values into the argument list
     to prevent invalid reads. 7.3.5 fix */
  lua_settop(L, 3);
  lua_pushstring(L, fn);
  lua_replace(L, 3);
  xfree(fn);
  fastloader_C(L, lua_tostring(L, 3), s, &success, 1);
  #else
  success = 0;
  #endif
  lua_pushboolean(L, success == 1);
  return 1;
}


/* Returns all standard Agena packages initialised at startup, does not include additional libraries manually
   invoked in a session. 2.29.7 */
static int pkg_packages (lua_State *L) {
  luaL_standardlibs(L);  /* pushes onto the stack the names of all standard libraries that are initialised at startup */
  lua_pushstring(L, "coroutine");
  lua_srawset(L, -2);
  return 1;
}


/* Returns a sorted list of all the C functions in standard library `libname'. If you want the names of the base library,
   just pass the empty string or nmo argument at all. 4.6.5. */
static int pkg_getcfuncs (lua_State *L) {
  const char *libname = luaL_optstring(L, 1, "");
  if (!luaL_libcfuncs(L, libname)) {
    luaL_error(L, "Error in " LUA_QS ": standard library " LUA_QS " does not exist.", "package.cfuncs", libname);
  }
  return 1;
}


/* }====================================================== */

static const luaL_Reg pk_funcs[] = {
  {"getcfuncs", pkg_getcfuncs},  /* added December 18, 2024 */
  {"loadclib", pkg_loadclib},  /* added July 08, 2008 */
  {"packages", pkg_packages},  /* added July 25, 2022 */
  {NULL, NULL}
};


LUALIB_API int luaopen_package (lua_State *L) {
  /* create new type _LOADLIB */
  luaL_newmetatable(L, "_LOADLIB");
  lua_pushcfunction(L, gctm);
  lua_setfield(L, -2, "__gc");
  /* create `package' table */
  luaL_register(L, LUA_LOADLIBNAME, pk_funcs);
  lua_pushvalue(L, -1);
  lua_replace(L, LUA_ENVIRONINDEX);
  /* set field `loaded' */
  luaL_findtable(L, LUA_REGISTRYINDEX, "_LOADED", 2);
  lua_setfield(L, -2, "loaded");
  /* create a _READLIBBED entry in the registry */
  lua_pushstring(L, "_READLIBBED");
  agn_createset(L, 0);
  lua_settable(L, LUA_REGISTRYINDEX);
  /* DEPRECATE: assign package.readlibbed set, referring to the registry */
  lua_getfield(L, LUA_REGISTRYINDEX, "_READLIBBED");
  lua_setfield(L, -2, "readlibbed");
  lua_pop(L, 1);
  return 1;  /* return 'package' table */
}

