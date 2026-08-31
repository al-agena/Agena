/*
** $Id: loslib.c,v 1.19 2006/04/26 18:19:49 roberto Exp $
** Standard Operating System library
** See Copyright Notice in agena.h
*/

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>  /* for trunc */
#include <limits.h>

#ifdef __linux__
#define _GNU_SOURCE
#include <stdint.h>
#endif

#include "prepdefs.h"

#ifdef CPUID
#include <cpuid.h>
#endif

#include <sys/timeb.h>  /* ftime, for milliseconds */

/* instead of time.h: */
#include "agnt64.h"
#include "sofa.h"
#include "sdncal.h"

#include <sys/stat.h> /* for mkdir, stat */

#ifdef __DJGPP__
#include <io.h>
#include <dos.h>
#include <direct.h>
#include <pc.h>          /* for sound */
#include <sys/farptr.h>  /* for farpeek */
#include <dpmi.h>        /* for os_codepage */
#include <go32.h>        /* for os_codepage */
#include <bios.h>        /* for _bios_memsize */
#include <sys/ioctl.h>   /* for ioctl */
#endif

/* for os.fstat to determine correct sizes of files > 4 GB */
#ifdef _WIN32
  #undef stat
  #define stat  _stati64
  #undef fstat
  #define fstat _fstati64
  #undef wstat
  #define wstat _wstati64
#elif defined(__SOLARIS)
  #undef stat
  #define stat stat64
  #undef lstat
  #define lstat lstat64
  #include <utmpx.h>  /* for getutxent in os.uptime */
  #include <sys/mount.h>  /* for umount2, 2.8.5 fix */
  #include <sys/types.h>    /* 2.9.8 fix: for stsvfs */
  #include <sys/statvfs.h>  /* 2.9.8 fix: for stsvfs */
#elif defined(__unix__) && !defined(__DJGPP__)
  #undef _FILE_OFFSET_BITS
  #define _FILE_OFFSET_BITS  64
#endif


#if _WIN32
/* If you need Win32 API features newer than Win95 and WinNT then you must
 * define WINVER before including windows.h or any other method of including
 * the windef.h header. */
#ifndef WINVER         /* 2.10.0 correction */
#define WINVER 0x0500  /* 2.3.0 RC 3 */
#endif

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501

#define INITGUID

#ifndef WIN32_LEAN_AND_MEAN  /* 7.1.7a to prevent compilation errors in GCC 15.2.0 */
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>   /* for os_memstate */
#include <winsock2.h>  /* 7.1.7a to prevent compilation errors in GCC 15.2.0 */
#include <ws2tcpip.h>  /* ditto */
#include <winnetwk.h>  /* net drive functions used by os.netuse */
#include <io.h>        /* for access */
#include <ctype.h>     /* for toupper, tolower */
#include <tchar.h>     /* for BOOL ? */
#include <process.h>   /* for getpid & getppid */
#include <errno.h>     /* symlink */
#include <ole2.h>      /* symlink */
#include <shlobj.h>    /* symlink */
#include <winbase.h>   /* MemoryStatusEx, GetProcessHandleCount */
#include <winioctl.h>  /* for os.cdrom */
#include <mmsystem.h>  /* for MCI functions, see os.cdrom */
#include <wingdi.h>    /* for colour depth, GetDeviceCaps */
#include <PowrProf.h>  /* for SetSuspendState */
#include <wincon.h>    /* for GetConsoleOutputCP, etc. */
#include <time.h>      /* for nanosleep */
#include <direct.h>    /* for _getdrive */
#include <winnls.h>    /* for GetUserDefaultLangID */
#include <iphlpapi.h>  /* for getadapter */
#include <shlwapi.h>   /* for PathAppend */
/* #define PSAPI_VERSION 1 ;  for GCC 10.2.0 */
#include <psapi.h>     /* for os.getloadeddlls */
#include <tlhelp32.h>  /* for os_getprocesses */
#include <psapi.h>     /* for os_getprocesses */
#include <winver.h>    /* for os_getprocesses */
#include <setupapi.h>  /* for os.getusbdevices, SetupDi functions */
#include <initguid.h>  /* for os.getusbdevices, to define GUIDs like GUID_DEVINTERFACE_USB_DEVICE */
/* Define GUID_DEVINTERFACE_USB_DEVICE directly, since it might be missing in some older or
   incomplete MinGW header sets. */
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE,
    0xA5DCBF10, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);
#include <devguid.h>   /* for os.getusbdevices, definitions for various device class GUIDs */

HANDLE OpenVolume (TCHAR cDriveLetter, int fixedvolume);
BOOL CloseVolume (HANDLE hVolume);
int GetTrimFlag (HANDLE hDevice);

#endif


/* This OS2 header must be placed before including unistd, utsname, ncurses,
   otherwise GCC will not compile successfully. 0.13.3 */
#ifdef __OS2__
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_NOPM
#define INCL_DOSPROCESS  /* for DosBeep */
#define INCL_DOSFILEMGR
#include <os2.h>     /* for memory status queries */
#include <sys/ioctl.h>  /* for ioctl */
#endif

#if defined(__unix__) || defined(__OS2__) || defined(__APPLE__) || defined(__HAIKU__)
#include <unistd.h>       /* for access, sleep, usleep */
#include <sys/utsname.h>  /* for uname */
#include <ctype.h>   /* for tolower, toupper */
#endif

#if !defined(__DJGPP__) && (defined(__unix__) || defined(__OS2__) || defined(__APPLE__))
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

/* In ArcaOS there is no library featuring getaddrinfo(), freeaddrinfo(), getnameinfo() and defines,
   so import them directly. 7.5.16 */
#ifdef __OS2__
#include "getaddrinfo.h"
#endif

#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__)
#include <signal.h>  /* for signal and alarm */
#include <pwd.h>     /* for getpwuid, getpwnam */
#include <grp.h>     /* for getgrid, getgrnam getgrnam */
#endif

#ifdef __APPLE__
  #include <mach/mach.h>
  #include <mach/mach_time.h>
  #include <crt_externs.h>  /* for os.environ */
  #include <stdint.h>
  #include <sys/types.h>
  #include <sys/sysctl.h>
  #include <sys/mount.h>  /* for os.unmount, os.drivestat */
  #include <sys/param.h>  /* for os.drivestat */
  #include <time.h>       /* for os.uptime */
  #define environ (*_NSGetEnviron())      /* for os.environ */
  #include <CoreServices/CoreServices.h>  /* for os.terminate */
  #include <Carbon/Carbon.h>              /* for os.terminate */
  #include <sys/ioctl.h>                  /* for os.vga */
  #include <ApplicationServices/ApplicationServices.h>  /* for os.vga */
  #include <IOKit/IOKitLib.h>             /* for os.vga */
  #include <CoreFoundation/CoreFoundation.h>  /* for os.vga */
#elif defined(__unix__) || defined(__OS2__)
  extern char **environ;  /* for os.environ */
#endif

#ifdef __linux__  /* for uptime and CD-ROM function(s) */
  #include <fcntl.h>
  #include <linux/cdrom.h>
  #include <sys/ioctl.h>
  #include <sys/types.h>
  #include <sys/mount.h>
  /* for os.uptime */
  #include <linux/unistd.h>       /* for _syscallX macros/related stuff */
  #include <linux/kernel.h>       /* for struct sysinfo */
  #include <sys/sysinfo.h>
  /* for os.cdrom */
  #include <linux/cdrom.h>
  /* for statfs, 2.9.8 patch */
  #include <sys/statfs.h>
  #include <time.h>
  /* for os.vga */
  #include <X11/Xlib.h>                /* Requires linking -lX11 */
  #include <X11/extensions/Xrandr.h>   /* Requires linking -lXrandr for refresh rate */
#endif

#ifdef __SOLARIS  /* for os.vga */
  #include <sys/ioctl.h>
  #include <sys/termios.h> /* Required on Solaris for terminal ioctl definitions */
  #include <sys/swap.h>    /* for os.drivestat */
  #include <sys/stat.h>    /* for os.drivestat */
  #include <unistd.h>
  #include <X11/Xlib.h>
#endif

#if !(defined(__DJGPP__) || defined(__OS2__))
#include <sched.h>
#endif

#if (defined(_WIN32) || defined(__unix__) || defined(__APPLE__) || defined(__DJGPP__))
#include <utime.h>  /* for utime() */
#endif


#include <dirent.h>  /* used by io_ls function */
#include <errno.h>   /* used by io_ls function */

#define loslib_c
#define LUA_LIB

#include "agena.h"

#include "agnxlib.h"
#include "agenalib.h"
#include "agncmpt.h"
#include "agnconf.h"  /* for LUA_TMPNAMBUFSIZE */
#include "agnhlps.h"
#include "charbuf.h"
/* for agn_get_argc(L) = ((L)->agn_argc), agn_get_argv(L) = ((L)->agn_argv) */
#include "lstate.h"
#include "network.h"

#ifdef __OS2__
#include <ctype.h>
#include <sys/utime.h>  /* 3.0.0 for utime in fattrib, request to just <utime.h> does not work */
#endif

#if defined(__APPLE__) || defined(__unix__)  /* 2.4.0, 2.8.5 fix: UNIX: umount2 */
#include "agncmpt.h"
#endif


static int os_pushresult (lua_State *L, int i, const char *filename, const char *procname, int issueerror) {
  int en = errno;    /* calls to Lua API may change this value */
  if (i == -1) {     /* function failed ? */
    if (issueerror)  /* 2.21.11 extension */
      luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": %s.", procname, filename, my_ioerror(en));  /* Agena 1.2, changed 2.10.4 */
    else
      lua_pushfail(L);
  } else
    lua_pushtrue(L);
  return 1;
}


/* ... If any option is given, the function runs the given command str and returns the entire output of the command as one string. Any carriage
   returns ('\r') are removed from the result. See also: `io.pcall`. 2.16.10 */
static int os_execute (lua_State *L) {
  const char *str = luaL_optstring(L, 1, NULL);
  int nooption = lua_gettop(L) < 2;
  if (nooption) {
    if (system(NULL) == 0)  /* 1.9.1 */
      luaL_error(L, "Error in " LUA_QS ": command processor is not available.", "os.execute");
    lua_pushinteger(L, system(str));
  } else {  /* 2.16.10 */
    if (!str) luaL_typeerror(L, 1, "string expected", lua_type(L, 1));
    agnL_pexecute(L, str, "os.execute");
  }
  return 1;
}


/* even when deleting 100k or more files, accepting tables, sets, or sequences of file names did not speed up this function at
   least on a MacBook Pro manufactured in 2011 running Mac OS 10.7 */
static int os_remove (lua_State *L) {
  const char *filename = agn_checkstring(L, 1);
  int noerror = agnL_optboolean(L, 2, 0);
  if (tools_exists(filename))  /* 0.26.0 patch, 2.16.11 change */
    return os_pushresult(L, remove(filename), filename, "os.remove", 1);  /* Agena 1.9.5 fix */
  else {
    if (!noerror)  /* 2.16.11 */
      luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": file or directory does not exist.", "os.remove", filename);  /* Agena 1.2 */
    else
      lua_pushfail(L);
    return 1;
  }
}


static int os_move (lua_State *L) {
  const char *fromname = agn_checkstring(L, 1);
  const char *toname = agn_checkstring(L, 2);
  int noerror = agnL_optboolean(L, 3, 0);
  if (!noerror && tools_exists(toname))  /* 2.16.11 improvement */
    luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": file or directory already exists.", "os.move", toname);
  if (tools_exists(fromname))  /* 0.26.0 patch, 2.16.11 change */
    return os_pushresult(L, rename(fromname, toname), fromname, "os.move", 1);  /* Agena 1.9.5 */
  else {
    if (!noerror)  /* 2.16.11 */
      luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": file or directory does not exist.", "os.move", fromname);  /* Agena 1.2 */
    else
      lua_pushfail(L);
    return 1;
  }
}


static int os_tmpname (lua_State *L) {
  char buff[LUA_TMPNAMBUFSIZE];
  int err;
#ifndef __OS2__
  lua_tmpnam(buff, err);
#else
  err = (tmpnam(buff) == NULL);
#endif
  if (err)
    return luaL_error(L, "Error in " LUA_QS ": unable to generate a unique filename.", "os.tmpname");
  lua_pushstring(L, buff);
  return 1;
}


/* Creates a unique temporary directory from pattern t which ends in six 'X' characters and returns its name. By default,
   the function uses the pattern 'agn_XXXXXX' (non-DOS) or 'agXXXXXX' (DOS).

   In case of an error, the function returns `null` and an error message. You have to manually remove the directory if it is
   not needed any longer. See also: os.tmpname, io.tmpfile.

   Taken from luaposix, see:
   https://github.com/luaposix/luaposix/blob/master/ext/posix/stdlib.c, 2.28.2

   luaposix is the work of several authors (see git history for contributors). It is based on two earlier libraries:

   An earlier version of luaposix (up to 5.1.11):
   Copyright Reuben Thomas 2010-2011
   Copyright Natanael Copa 2008-2010
   Clean up and bug fixes by Leo Razoumov 2006-10-11
   Luiz Henrique de Figueiredo 07 Apr 2006 23:17:49
   Based on original by Claudio Terra for Lua 3.x, with contributions by Roberto Ierusalimschy. */

#if (0 && defined(__MINGW32__) && (100*__GNUC__+__GNUC_MINOR__ >= 1000))
static char *mkdtemp (char *template) {
  if (!*mktemp(template) || mkdir(template)) return NULL;
  return template;
}
#endif

static int os_tmpdir (lua_State *L) {
  size_t l;
#ifndef __DJGPP__
  const char *path = luaL_optlstring(L, 1, "agn_XXXXXX", &l);
#else
  const char *path = luaL_optlstring(L, 1, "agXXXXXX", &l);
#endif
  size_t path_len = l + 1;
  void *ud;
  lua_Alloc lalloc;
  char *tmppath = NULL, *r = NULL;
  luaL_checknargs(L, 1);
  lalloc = lua_getallocf(L, &ud);  /* returns the memory-allocation function of the state */
  if ((tmppath = lalloc(ud, NULL, 0, path_len)) == NULL)  /* allocates new memory */
    return luaL_pusherror(L, "lalloc");
  strcpy(tmppath, path);
  if ( (r = tools_mkdtemp(tmppath)) ) {
    lua_pushstring(L, tmppath);
  }
  lalloc(ud, tmppath, path_len, 0);  /* frees memory */
  return (r == NULL) ? luaL_pusherror(L, path) : 1;
}


static int os_getenv (lua_State *L) {
/* rewritten in 2.16.11; tested in Windows, Mac OS X, Solaris 10, Linux, OS/2, DOS */
  char *result = tools_getenv(agn_checkstring(L, 1));
  if (result)  /* explicitly check for non-NULL as Solaris will crash otherwise ! */
    lua_pushstring(L, result);
  else
    lua_pushnil(L);
  xfree(result);
  return 1;
}


/* code originally written by * Copyright 2007 Mark Edgar < medgar at gmail com >,
   taken from https://code.google.com/archive/p/lua-ex-api/source/default/source, file lua-ex-api/trunk/w32api/ex.c
   extension by Alexander Walz, tested in Windows, Mac OS X, Solaris 10, Linux, OS/2, DOS */
static int os_setenv (lua_State *L) {  /* Agena 1.0.2 */
#ifdef _WIN32
  int r;
#else
  int err;
#endif
  const char *nam, *val;
  nam = luaL_checkstring(L, 1);
  val = NULL;
  if (lua_gettop(L) != 2)
    luaL_error(L, "Error in " LUA_QS ": expected two arguments.", "os.setenv");
  if (tools_strlen(nam) > 32766)  /* 2.17.8 tweak */
    luaL_error(L, "Error in " LUA_QS ": first argument too long.", "os.setenv");
  if (lua_isnoneornil(L, 2)) {
    /* 2.8.6: delete environment variable */
  } else if (agn_isstring(L, 2) || agn_isnumber(L, 2)) {
    val = lua_tostring(L, 2);
#if defined(_WIN32)
    /* 2.8.6 patch; contrary to the MSDN documentation, SetEnvironmentVariable crashed if given a string longer than 32766 chars (with MinGW/GCC) */
    if (tools_strlen(val) > 32766)  /* 2.17.8 tweak */
      luaL_error(L, "Error in " LUA_QS ": second argument too long.", "os.setenv");
#endif
  } else
    return luaL_typeerror(L, 2, "string, number or null expected", lua_type(L, 2));
#if defined(_WIN32)
  r = SetEnvironmentVariable(luaL_checkstring(L, 1), val);
  if (!r) {
    int en = GetLastError();
    luaL_error(L, "Error in " LUA_QS ": %s.", "os.setenv", my_ioerror(en));  /* 2.11.5 fix */
  } else
    lua_pushtrue(L);
#else
  err = 0;
  val = lua_tostring(L, 2);
  #ifdef __DJGPP__
  err = (val ? setenv(nam, val, 1) : putenv((char *)nam));  /* 2.14.0, if val is NULL, the environment variable is deleted, 2.17.1 fix */
  #elif !defined(__APPLE__)
  err = (val ? setenv(nam, val, 1) : unsetenv(nam));  /* if val is NULL, the environment variable is deleted */
  #else
  if (val == 0)
    unsetenv(nam);  /* unsetenv is of type void in Mac OS X */
  else
    err = setenv(nam, val, 1);
  #endif
  if (err == -1)
    luaL_error(L, "Error in " LUA_QS ": could not %s environment variable.", "os.setenv",
      val == 0 ? "unset" : "set");
  lua_pushtrue(L);
#endif
  return 1;
}


static int os_environ (lua_State *L) {  /* Agena 1.0.2,
  written by * Copyright 2007 Mark Edgar < medgar at gmail com >,
  taken from https://code.google.com/archive/p/lua-ex-api/source/default/source, file lua-ex-api/trunk/w32api/ex.c
  Tested in Windows, Mac OS X, Solaris 10, Linux, OS/2, DOS */
#if defined(_WIN32)
  const char *nam, *val, *end;
  char *envs = GetEnvironmentStrings();
  if (!envs) {
    luaL_error(L, "Error in " LUA_QS ": could not get environment.", "os.environ");
  }
  lua_newtable(L);
  for (nam = envs; *nam; nam = end + 1) {
    end = strchr(val = strchr(nam, '=') + 1, '\0');
    if (val - nam - 1 != 0) {  /* do not enter entry of the key is the empty string */
      lua_pushlstring(L, nam, val - nam - 1);
      lua_pushlstring(L, val, end - val);
      lua_settable(L, -3);
    }
  }
  FreeEnvironmentStrings(envs);  /* 7.5.15 memory leak fix */
#elif defined(__unix__) || defined(__APPLE__) || defined(__OS2__)  /* works in DOS, too */
  const char *nam, *val, *end;
  const char **env;
  lua_newtable(L);
  for (env = (const char **)environ; (nam = *env); env++) {
    end = strchr(val = strchr(nam, '=') + 1, '\0');
    lua_pushlstring(L, nam, val - nam - 1);
    lua_pushlstring(L, val, end - val);
    lua_settable(L, -3);
  }
#else
  lua_pushfail(L);
#endif
  return 1;
}


static int os_getwinsysdirs (lua_State *L) {
#ifdef _WIN32
  char infoBuf[MAX_PATH];
  UINT rc;
  /* get Windows directory */
  rc = GetWindowsDirectoryA(infoBuf, MAX_PATH);
  if (rc == 0 || rc > MAX_PATH)
    lua_pushfail(L);
  else
    lua_pushlstring(L, infoBuf, rc);
  /* get system directory */
  rc = GetSystemDirectoryA(infoBuf, MAX_PATH);
  if (rc == 0 || rc > MAX_PATH)
    lua_pushfail(L);
  else
    lua_pushlstring(L, infoBuf, rc);
#else
  lua_pushfail(L);
  lua_pushfail(L);
#endif
  return 2;
}


/* Taken from https://learn.microsoft.com/en-gb/windows/win32/psapi/enumerating-all-modules-for-a-process?redirectedfrom=MSDN
   See also: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentprocessid, 3.4.3 */
static int os_getloadeddlls (lua_State *L) {
#ifdef _WIN32
  HANDLE hProcess = NULL;
  HMODULE *hMods = NULL;
  DWORD cbNeeded, i, c = 0;
  DWORD processId = (DWORD)agnL_optuint32_t(L, 1, (uint32_t)GetCurrentProcessId());
  hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
  if (hProcess == NULL) goto cleanup;
  /* First pass: get required size */
  if (!EnumProcessModules(hProcess, NULL, 0, &cbNeeded)) goto cleanup;
  hMods = (HMODULE *)malloc(cbNeeded);
  if (hMods == NULL) goto cleanup;
  /* Second pass: retrieve modules */
  if (EnumProcessModules(hProcess, hMods, cbNeeded, &cbNeeded)) {
    lua_pushnumber(L, processId);
    lua_newtable(L);
    for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
      TCHAR szModName[MAX_PATH];
      if (GetModuleFileNameEx(hProcess, hMods[i], szModName, MAX_PATH)) {
        lua_pushstring(L, szModName);
        lua_rawseti(L, -2, ++c);
      }
    }
  }
cleanup:
  if (hMods) free(hMods);
  if (hProcess) CloseHandle(hProcess);

  if (c == 0) {
    lua_pushnil(L);
    return 1;
  }
  return 2;
#else
  lua_pushnil(L);
  return 1;
#endif
}


#ifdef _WIN32


#ifdef ZZZ
/* 7.9.4 u1, SECURE HELPER: Gets the File Description of an executable safely on Windows 11.
   Defines the function pointer type for QueryFullProcessImageName.
   This solution prevents crashes in Windows 10 and 11. Created by Gemini AI. */
typedef BOOL (WINAPI *PQueryFullProcessImageName)(HANDLE, DWORD, LPTSTR, PDWORD);

/* SECURE HELPER: Gets the File Description of an executable safely on Windows 11 */
static const char *GetProcessDescription (DWORD dwProcessId) {
  static char szDescription[1024];
  szDescription[0] = '\0'; /* Safe initialization */
  /* Fix 1: Windows 11 requires PROCESS_QUERY_LIMITED_INFORMATION for UWP/AppContainers */
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessId);
  if (hProcess == NULL) return "N/A";
  TCHAR szProcessPath[MAX_PATH];
  DWORD dwPathLen = MAX_PATH;
  BOOL bPathSuccess = FALSE;
  /* Fix 2: Dynamically resolve QueryFullProcessImageName to satisfy legacy MinGW toolchains */
  HMODULE hKernel32 = GetModuleHandle("kernel32.dll");
  if (hKernel32) {
#ifdef UNICODE
    PQueryFullProcessImageName pfnQueryPath = 
      (PQueryFullProcessImageName)GetProcAddress(hKernel32, "QueryFullProcessImageNameW");
#else
    PQueryFullProcessImageName pfnQueryPath = 
      (PQueryFullProcessImageName)GetProcAddress(hKernel32, "QueryFullProcessImageNameA");
#endif
    if (pfnQueryPath) {
      bPathSuccess = pfnQueryPath(hProcess, 0, szProcessPath, &dwPathLen);
    } else {
      /* Fallback for ancient pre-Vista environments (XP/2000 compatibility) */
      typedef DWORD (WINAPI *PGetModuleFileNameEx)(HANDLE, HMODULE, LPTSTR, DWORD);
      HMODULE hPsapi = LoadLibrary("psapi.dll");
      if (hPsapi) {
#ifdef UNICODE
        PGetModuleFileNameEx pfnGetModuleFile = 
          (PGetModuleFileNameEx)GetProcAddress(hPsapi, "GetModuleFileNameExW");
#else
        PGetModuleFileNameEx pfnGetModuleFile = 
          (PGetModuleFileNameEx)GetProcAddress(hPsapi, "GetModuleFileNameExA");
#endif
        if (pfnGetModuleFile) {
          bPathSuccess = (pfnGetModuleFile(hProcess, NULL, szProcessPath, MAX_PATH) > 0);
        }
        FreeLibrary(hPsapi);
      }
    }
  }
  /* Extract the file description version block if the path was resolved */
  if (bPathSuccess) {
    DWORD dwHandle = 0;
    DWORD dwSize = GetFileVersionInfoSize(szProcessPath, &dwHandle);    
    if (dwSize > 0) {
      LPVOID lpData = malloc(dwSize);
      if (lpData) {
        if (GetFileVersionInfo(szProcessPath, dwHandle, dwSize, lpData)) {
          TCHAR* lpDesc = NULL;
          UINT uiLen = 0;
          /* Common language/codepage for English */
          if (VerQueryValue(lpData, _T("\\StringFileInfo\\040904B0\\FileDescription"), (LPVOID*)&lpDesc, &uiLen)) {
            /* Fix 3: Rigid validation guarding against the 0-1 unsigned integer underflow */
            if (lpDesc != NULL && uiLen > 1) {
#ifdef UNICODE
              /* Explicitly convert Wide Character UTF-16 to ANSI safely for Lua */
              WideCharToMultiByte(CP_ACP, 0, lpDesc, -1, szDescription, sizeof(szDescription) - 1, NULL, NULL);
              szDescription[sizeof(szDescription) - 1] = '\0';
#else
              /* Standard ANSI safety allocation */
              size_t copyLen = (uiLen < sizeof(szDescription)) ? uiLen : sizeof(szDescription) - 1;
              strncpy(szDescription, lpDesc, copyLen);
              szDescription[copyLen] = '\0';
#endif
            }
          }
        }
        free(lpData);
      }
    }
  }
  CloseHandle(hProcess);
  return (szDescription[0] == '\0') ? "N/A" : szDescription;
}

static int os_getprocesses (lua_State *L) {
  struct WinVer winversion;
  if (getWindowsVersion(&winversion) < MS_WINXP) {
    lua_pushfail(L);
    return 1;
  }
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE) {
    luaL_error(L, "Error in " LUA_QS ": could not create process snapshot.", "os.getprocesses");
    return 0;
  }
  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);
  lua_newtable(L);
  if (Process32First(hSnapshot, &pe32)) {
    do {
      DWORD pid = pe32.th32ProcessID;
      lua_newtable(L);
      /* A. Process Name */
      lua_pushstring(L, "name");
      if (pid == 0) {
        lua_pushstring(L, "Idle");
      } else {
        lua_pushstring(L, pe32.szExeFile);
      }
      lua_settable(L, -3);
      /* B. Handle Count (Limited security query fallback) */
      lua_pushstring(L, "handles");
      HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
      if (hProc) {
        DWORD dwHandleCount = 0;
        if (GetProcessHandleCount(hProc, &dwHandleCount)) {
          lua_pushnumber(L, (lua_Number)dwHandleCount);
        } else {
          lua_pushnumber(L, 0);
        }
        CloseHandle(hProc);
      } else {
        lua_pushnumber(L, 0);
      }
      lua_settable(L, -3);
      /* C. Process Descriptions */
      lua_pushstring(L, "description");
      const char* desc = GetProcessDescription(pid);
      lua_pushstring(L, (desc != NULL && desc[0] != '\0') ? desc : "N/A");
      lua_settable(L, -3);
      /* D. Append sub-table into Master Table */
      lua_pushnumber(L, (lua_Number)pid);
      lua_pushvalue(L, -2);
      lua_settable(L, -4);
      lua_pop(L, 1);
    } while (Process32Next(hSnapshot, &pe32));
  }
  CloseHandle(hSnapshot);
  return 1;
}

#endif


/* Type signature for the dynamic module path lookup */
typedef DWORD (WINAPI *PGetModuleFileNameEx)(HANDLE, HMODULE, LPTSTR, DWORD);

/* UNIVERSAL HELPER: Safely extracts File Descriptions using runtime dynamic API mapping */
static const char *GetProcessDescriptionUniversal (DWORD dwProcessId) {
  static char szDescription[1024];
  szDescription[0] = '\0'; /* Safe initialization */
  /* Use the safest lowest-privilege access mask. 
     Windows 2000 ignores the modern mask bit and treats it as standard query access. */
  HANDLE hProcess = OpenProcess(0x1000 | PROCESS_VM_READ, FALSE, dwProcessId);
  if (hProcess == NULL) {
    /* Fallback for Windows 2000 / XP if limited query mask isn't recognized */
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId);
  }
  if (hProcess == NULL) return "N/A";
  TCHAR szProcessPath[MAX_PATH] = {0};
  BOOL bPathSuccess = FALSE;
  /* Dynamically locate GetModuleFileNameEx across generations (psapi.dll vs kernel32.dll) */
  HMODULE hModLib = GetModuleHandle(_T("kernel32.dll"));
  PGetModuleFileNameEx pfnGetModuleFile = NULL;
  if (hModLib) {
#ifdef UNICODE
    pfnGetModuleFile = (PGetModuleFileNameEx)GetProcAddress(hModLib, "GetModuleFileNameExW");
#else
    pfnGetModuleFile = (PGetModuleFileNameEx)GetProcAddress(hModLib, "GetModuleFileNameExA");
#endif
  }
  /* If missing from kernel32 (Windows 2000 / XP / Vista), load it explicitly from psapi.dll */
  if (!pfnGetModuleFile) {
    HMODULE hPsapi = LoadLibrary(_T("psapi.dll"));
    if (hPsapi) {
#ifdef UNICODE
      pfnGetModuleFile = (PGetModuleFileNameEx)GetProcAddress(hPsapi, "GetModuleFileNameExW");
#else
      pfnGetModuleFile = (PGetModuleFileNameEx)GetProcAddress(hPsapi, "GetModuleFileNameExA");
#endif
      if (pfnGetModuleFile && pfnGetModuleFile(hProcess, NULL, szProcessPath, MAX_PATH) > 0) {
        bPathSuccess = TRUE;
      }
      FreeLibrary(hPsapi);
    }
  } else {
    if (pfnGetModuleFile(hProcess, NULL, szProcessPath, MAX_PATH) > 0) {
      bPathSuccess = TRUE;
    }
  }
  /* Extract the file description version block if the path was securely resolved */
  if (bPathSuccess) {
    DWORD dwHandle = 0;
    DWORD dwSize = GetFileVersionInfoSize(szProcessPath, &dwHandle);
    if (dwSize > 0) {
      LPVOID lpData = malloc(dwSize);
      if (lpData) {
        if (GetFileVersionInfo(szProcessPath, dwHandle, dwSize, lpData)) {
          TCHAR* lpDesc = NULL;
          UINT uiLen = 0;
          /* Common language/codepage for English */
          if (VerQueryValue(lpData, _T("\\StringFileInfo\\040904B0\\FileDescription"), (LPVOID*)&lpDesc, &uiLen)) {
            /* Rigid validation guarding against string math underflows */
            if (lpDesc != NULL && uiLen > 1) {
#ifdef UNICODE
              /* Safe Wide Character UTF-16 conversion to standard ANSI for Lua */
              WideCharToMultiByte(CP_ACP, 0, lpDesc, -1, szDescription, sizeof(szDescription) - 1, NULL, NULL);
              szDescription[sizeof(szDescription) - 1] = '\0';
#else
              /* Standard ANSI bounds safety limits */
              size_t copyLen = (uiLen < sizeof(szDescription)) ? uiLen : sizeof(szDescription) - 1;
              strncpy(szDescription, lpDesc, copyLen);
              szDescription[copyLen] = '\0'; /* Explicitly seal layout boundary */
#endif
            }
          }
        }
        free(lpData);
      }
    }
  }
  CloseHandle(hProcess);
  return (szDescription[0] == '\0') ? "N/A" : szDescription;
}

/* MASTER FUNCTION: Backwards-Compatible Process Enumeration */
static int os_getprocesses (lua_State *L) {
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE) {
    luaL_error(L, "Error in os.getprocesses: Could not create process snapshot");
    return 0;
  }
  /* Dynamically switch access flags to prevent runtime drops on Windows 2000 */
  DWORD dwQueryFlag = PROCESS_QUERY_INFORMATION; 
  struct WinVer winversion;
  if (getWindowsVersion(&winversion) >= MS_WINVISTA) {
    dwQueryFlag = 0x1000; /* PROCESS_QUERY_LIMITED_INFORMATION Hex Literal */
  }
  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);
  lua_newtable(L);
  if (Process32First(hSnapshot, &pe32)) {
    do {
      DWORD pid = pe32.th32ProcessID;
      lua_newtable(L);
      /* A. Process Name */
      lua_pushstring(L, "name");
      if (pid == 0) {
        lua_pushstring(L, "Idle");
      } else {
        lua_pushstring(L, pe32.szExeFile);
      }
      lua_settable(L, -3);
      /* B. Handle Count */
      lua_pushstring(L, "handles");
      HANDLE hProc = OpenProcess(dwQueryFlag, FALSE, pid);
      if (hProc) {
        DWORD dwHandleCount = 0;
        /* Typed dynamic binding resolution for GetProcessHandleCount */
        typedef BOOL (WINAPI *PGetProcessHandleCount)(HANDLE, PDWORD);
        HMODULE hKernel32 = GetModuleHandle(_T("kernel32.dll"));
        PGetProcessHandleCount pfnGetHandleCount = NULL;
        if (hKernel32) {
          pfnGetHandleCount = (PGetProcessHandleCount)GetProcAddress(hKernel32, "GetProcessHandleCount");
        }
        /* Executes on XP SP1 up to Windows 11. Fails cleanly with 0 on Windows 2000. */
        if (pfnGetHandleCount && pfnGetHandleCount(hProc, &dwHandleCount)) {
          lua_pushnumber(L, (lua_Number)dwHandleCount);
        } else {
          lua_pushnumber(L, 0);
        }
        CloseHandle(hProc);
      } else {
        lua_pushnumber(L, 0);
      }
      lua_settable(L, -3);
      /* C. Process Descriptions (Resolved using the safe, dynamic handle-based method) */
      lua_pushstring(L, "description");
      const char* desc = GetProcessDescriptionUniversal(pid);
      lua_pushstring(L, (desc != NULL && desc[0] != '\0') ? desc : "N/A");
      lua_settable(L, -3);
      /* D. Append sub-table into Master Table */
      lua_pushnumber(L, (lua_Number)pid);
      lua_pushvalue(L, -2);
      lua_settable(L, -4);
      lua_pop(L, 1);

    } while (Process32Next(hSnapshot, &pe32));
  }
  CloseHandle(hSnapshot);
  return 1;
}


/* Helper function to convert WCHAR (Windows wide characters) to UTF-8 char*
   The caller is responsible for freeing the returned string using free(). */
static char *WideCharToUtf8 (const WCHAR *wideCharStr) {
  if (!wideCharStr) return NULL;
  /* Determine the required buffer size for the UTF-8 string */
  int len = WideCharToMultiByte(CP_UTF8, 0, wideCharStr, -1, NULL, 0, NULL, NULL);
  if (len == 0) return NULL;  /* Handle conversion error (e.g., invalid wideCharStr) */
  /* Allocate memory for the UTF-8 string */
  char* utf8Str = (char*)malloc(len);
  if (!utf8Str) return NULL;  /* Handle memory allocation failure */
  /* Perform the conversion */
  if (WideCharToMultiByte(CP_UTF8, 0, wideCharStr, -1, utf8Str, len, NULL, NULL) == 0) {
    /* Handle conversion error */
    free(utf8Str);
    return NULL;
  }
  return utf8Str;
}

/* Lists all USB devices currently attached to the host. The function returns a table where each
   entry is a table representing a USB device.
   Each device table contains the properties:
   - 'description': Device Description (e.g., "USB Composite Device"),
   - 'hardware_id': Hardware ID (e.g., "USB\VID_045E&PID_00CB"),
   - 'friendly_name': more user-readable Friendly Name (e.g., "Logitech USB Mouse"),
   - 'manufacturer': the manufacturer,
   - 'service': Service (e.g., "usbhub"),
   - 'instance_id': Device Instance ID (unique identifier for this specific device instance)

   The function is available in Windows XP and higher only.

   Written by gemini.google.com, put into the public domain. */

static int os_getusbdevices (lua_State *L) {
  HDEVINFO hDevInfo;
  SP_DEVINFO_DATA DeviceInfoData;
  DWORD i;
  int device_count = 0;  /* Counter for the number of devices found */
  struct WinVer winversion;
  if (getWindowsVersion(&winversion) < MS_WINXP) {
    lua_pushfail(L);
    return 1;
  }
  /* Create a new empty Lua table on the stack. This will hold all the device information.
     This table will be the return value of the Lua function. */
  lua_newtable(L); /* Stack: [main_table]
    Get a device information set for all present USB devices.
    GUID_DEVINTERFACE_USB_DEVICE is a standard GUID for USB devices (not controllers).
    DIGCF_PRESENT: Only include devices that are currently present in the system.
    DIGCF_DEVICEINTERFACE: Include devices that expose device interfaces (like USB devices). */
  hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_USB_DEVICE,  /* Class GUID for USB devices */
        NULL,                           /* Enumerator (NULL for all enumerators) */
        NULL,                           /* Parent window handle (not needed here) */
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE /* Flags */
    );
  /* Check if the device information set was successfully created. */
  if (hDevInfo == INVALID_HANDLE_VALUE) {
    lua_pushstring(L, "Failed to get device information set from SetupDiGetClassDevs.");
    lua_error(L);  /* Push error message and raise a Lua error */
    return 0;
  }
  /* Initialize the size of the SP_DEVINFO_DATA structure. This is crucial for
     SetupDiEnumDeviceInfo to work correctly. */
  DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
  /* Enumerate through all devices in the created device information set.
     The loop continues as long as SetupDiEnumDeviceInfo returns TRUE. */
  for (i=0; SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData); i++) {
    /* Create a new empty Lua table for the current USB device. Stack: [main_table, device_table] */
    lua_newtable(L);
    /* Buffer to retrieve device properties. MAX_PATH is generally sufficient. */
    WCHAR szBuffer[MAX_PATH];
    DWORD dwPropertyRegDataType;  /* To store the registry data type of the property */
    DWORD dwRequiredSize;         /* To store the required buffer size */
    /* Helper macro to retrieve a device property and add it to the Lua table
       If the property is not found or retrieval fails, it sets "N/A". */
    #define GET_AND_SET_PROPERTY(property_name, lua_key) \
    if (SetupDiGetDeviceRegistryPropertyW( \
          hDevInfo, \
          &DeviceInfoData, \
          property_name, \
          &dwPropertyRegDataType, \
          (PBYTE)szBuffer, \
          sizeof(szBuffer), \
          &dwRequiredSize)) { \
      char* utf8Str = WideCharToUtf8(szBuffer); \
      if (utf8Str) { \
        lua_pushstring(L, lua_key); \
        lua_pushstring(L, utf8Str); \
        lua_settable(L, -3); \
        free(utf8Str); \
      } else { \
        lua_pushstring(L, lua_key); \
        lua_pushstring(L, "N/A (Conversion Error)"); \
        lua_settable(L, -3); \
      } \
    } else { \
      lua_pushstring(L, lua_key); \
      lua_pushstring(L, "N/A"); \
      lua_settable(L, -3); \
    }
    /* Retrieve and set Device Description (e.g., "USB Composite Device") */
    GET_AND_SET_PROPERTY(SPDRP_DEVICEDESC, "description");
    /* Retrieve and set Hardware ID (e.g., "USB\VID_045E&PID_00CB") */
    GET_AND_SET_PROPERTY(SPDRP_HARDWAREID, "hardware_id");
    /* Retrieve and set Friendly Name (more user-readable, e.g., "Logitech USB Mouse") */
    GET_AND_SET_PROPERTY(SPDRP_FRIENDLYNAME, "friendly_name");
    /* Retrieve and set Manufacturer */
    GET_AND_SET_PROPERTY(SPDRP_MFG, "manufacturer");
    /* Retrieve and set Service (e.g., "usbhub") */
    GET_AND_SET_PROPERTY(SPDRP_SERVICE, "service");
    /* Get Device Instance ID (a unique identifier for this specific device instance)
       This is often useful for uniquely identifying a device across reboots. */
    if (SetupDiGetDeviceInstanceIdW(
      hDevInfo, &DeviceInfoData, szBuffer, MAX_PATH, 0)) {
      char* utf8InstanceId = WideCharToUtf8(szBuffer);
      if (utf8InstanceId) {
        lua_pushstring(L, "instance_id");
        lua_pushstring(L, utf8InstanceId);
        lua_settable(L, -3);
        free(utf8InstanceId);
      } else {
        lua_pushstring(L, "instance_id");
        lua_pushstring(L, "N/A (Conversion Error)");
        lua_settable(L, -3);
      }
    } else {
      lua_pushstring(L, "instance_id");
      lua_pushstring(L, "N/A");
      lua_settable(L, -3);
    }
    /* Add the current device's table to the main result table. */
    lua_rawseti(L, -2, ++device_count);
  }
  /* Clean up the device information set to free system resources. */
  SetupDiDestroyDeviceInfoList(hDevInfo);
  /* Return the main table containing all USB device information. */
  /* Stack: [main_table] -> empty (after return) */
  return 1;
}
#endif


/* Generates unique thread IDs and returns the system-specific value as the first result and the
   POSIX Thread ID (Pthread ID) as a second.

   On Windows, the first ID returned depicts the thread ID assigned by the operating system on
   which Agena is running. The second return is always zero as POSIX Thread IDs cannot be generated
   there.

   In Mac OS X, the first ID is the Mach thread identifier and the second the POSIX Thread ID.

   In Solaris, the first ID is the the Lightweight Process (LWP) ID and the second the POSIX Thread ID.

   The first result may be zero on other platforms.

   You can use the return as a seed. See also: `os.getthreadseed`. 5.6.2 */
static int os_getthreadid (lua_State *L) {
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushnumber(L, tools_getthreadid());
  lua_pushnumber(L, tools_getposixthreadid());
  return 2;
}


/* Generates an unsigned 32-bit integer seed XORing the platform specific thread ID and the current
   time in seconds. See also: `os.getthreadid`. 5.6.2 */
static int os_getthreadseed (lua_State *L) {
  lua_pushnumber(L, tools_getthreadseed());
  return 1;
}


/*
** {======================================================
** Time/Date operations
** { year=%Y, month=%m, day=%d, hour=%H, min=%M, sec=%S,
**   wday=%w+1, yday=%j, isdst=? }
** =======================================================
*/

#define setfield(L, key, value)       lua_rawsetstringinteger(L, -1, key, value)

static int getboolfield (lua_State *L, const char *key) {
  int res;
  lua_getfield(L, -1, key);
  res = lua_isnil(L, -1) ? -1 : lua_toboolean(L, -1);
  agn_poptop(L);
  return res;
}

static int getfield (lua_State *L, const char *key, int d) {
  int res;
  lua_getfield(L, -1, key);
  if (agn_isnumber(L, -1))
    res = (int)lua_tointeger(L, -1);
  else {
    if (d < 0)
      return luaL_error(L, "field " LUA_QS " missing in date table.", key);
    res = d;
  }
  agn_poptop(L);
  return res;
}

static int os_date (lua_State *L) {
  struct TM *stm;
#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__)
  struct timeb tp;
#elif defined(__OS2__)
  DATETIME tp;
#endif
  int ye, mo, da, ho, mi, se, msecs, nargs;
  const char *s;
  Time64_T t;
  nargs = lua_gettop(L);
  if (nargs != 0 && !agn_isstring(L, 1)) {
    luaL_error(L, "Error in " LUA_QS ": first argument must be a format string.", "os.date");
  }
  if (nargs > 2 ||  /* 2.10.3 improvement, 2.37.0 fix */
    (nargs == 2 && (lua_istable(L, 2) || lua_isseq(L, 2) || lua_isreg(L, 2)))) {  /* 5.5.3 fix */
    t = agnL_datetosecs(L, 2, "os.date", 1, 0, &msecs);
    if (t < 0) luaL_error(L, "Error in " LUA_QS ": could not determine date, may be out-of-range.", "os.date");  /* 2.16.2 fix */
  } else if (nargs == 2 && agn_isstring(L, 2)) {  /* 5.5.7, split timestamp into its components */
    char *result;
    struct tm parsed_time;
    memset(&parsed_time, 0, sizeof(struct tm));
    result = str_strptime(agn_checkstring(L, 2), agn_tostring(L, 1), &parsed_time, &msecs);
    if (result != NULL) {
      lua_createtable(L, 6 + (msecs != -1), 0);
      lua_rawsetinumber(L, -1, 1, parsed_time.tm_year + 1900);
      lua_rawsetinumber(L, -1, 2, parsed_time.tm_mon + 1);
      lua_rawsetinumber(L, -1, 3, parsed_time.tm_mday);
      lua_rawsetinumber(L, -1, 4, parsed_time.tm_hour);
      lua_rawsetinumber(L, -1, 5, parsed_time.tm_min);
      lua_rawsetinumber(L, -1, 6, parsed_time.tm_sec);
      if (msecs != -1) {
        lua_rawsetinumber(L, -1, 7, msecs);
      }
    } else {
      lua_pushnil(L);
    }
    return 1;
  } else {
    t = luaL_opt(L, (Time64_T)agnL_checknumber, 2, time(NULL));
  }
  s = luaL_optstring(L, 1, "%c");
  if (*s == '!') {  /* UTC? */
    stm = gmtime64(&t);
    s++;  /* skip `!' */
  } else {
    stm = localtime64(&t);
  }
  if (stm == NULL) { /* invalid date ? 5.5.5 fix */
    lua_pushnil(L);
    return 1;
  }
  if (nargs < 2) {  /* current date */
    msecs = -1;
    /* 1.11.7, get milliseconds */
#ifdef _WIN32
    tools_ftime(&tp);
    msecs = tp.millitm;
#elif defined(__unix__) || defined(__APPLE__)
    if (tools_ftime(&tp) == 0)  /* set milliseconds only when query has been successful */
      msecs = tp.millitm;
#elif defined (__OS2__)  /* 2.3.0 RC 2 eCS extension */
    DosGetDateTime(&tp);
    msecs = tp.hundredths;
#endif
  } else {
    msecs = 0;
  }
  ye = stm->tm_year + 1900;
  mo = stm->tm_mon + 1;
  da = stm->tm_mday;
  ho = stm->tm_hour;
  mi = stm->tm_min;
  se = stm->tm_sec;
  if (lua_gettop(L) == 0) {  /* current date */
    char *rstr, *year, *month, *day, *hour, *minute, *second, *msecond;
    year    = tools_dtoa(ye);
    month   = tools_dtoa(mo);
    day     = tools_dtoa(da);
    hour    = tools_dtoa(ho);
    minute  = tools_dtoa(mi);
    second  = tools_dtoa(se);
    msecond = tools_dtoa(msecs);
    rstr = str_concat(year, "/",
      (mo < 10 ? "0" : ""), month, "/",
      (da < 10 ? "0" : ""), day, " ",
      (ho < 10 ? "0" : ""), hour, ":",
      (mi < 10 ? "0" : ""), minute, ":",
      (se < 10 ? "0" : ""), second, ".", NULL);
    if (!rstr) {
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.date");
    }
    luaL_checkstack(L, 3, "not enough stack space");  /* streamlining 5.5.4 */
    lua_pushstring(L, rstr);
    lua_pushstring(L, msecs == -1 || msecs > 99 ? "" : (msecs > 9 ? "0" : "00"));
    lua_pushstring(L, msecs == -1 ? "000" : msecond);
    lua_concat(L, 3);
    xfreeall(rstr, year, month, day, hour, minute, second, msecond);  /* 2.9.8 */
  } else if (tools_streq(s, "*t")) {  /* 2.16.12 tweak */
    luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
    lua_createtable(L, 0, 10);  /* 9 = number of fields */
    setfield(L, "sec", se);
    setfield(L, "min", mi);
    setfield(L, "hour", ho);
    setfield(L, "day", da);
    setfield(L, "month", mo);
    setfield(L, "year", ye);
    setfield(L, "wday", agn_getiso8601(L) ?
      (stm->tm_wday == 0 ? 7 : stm->tm_wday) :  /* 2.9.8 patch, use ISO 8601 weekday numbers: 1 = Monday, 7 = Sunday */
      stm->tm_wday + 1);
    setfield(L, "yday", stm->tm_yday + 1);  /* stm->tm_yday is in [0, 365], so add one to it */
    lua_rawsetstringboolean(L, -1, "isdst", stm->tm_isdst);
    setfield(L, "msec", msecs == -1 ? 0 : msecs);  /* set milliseconds, 5.5.4 streamlining */
  } else if (tools_streq(s, "*j")) {  /* 2.8.0, 2.16.12 tweak */
    /* Test cases checked with https://aa.usno.navy.mil/data/JulianDate: all results correct:
       os.date('*j', os.datetosecs(2016,6,8,12,30)) -> 2457548.0208333
       os.date('*j', os.datetosecs(2016,1,1,23,59)) -> 2457389.4993056 */
    double djm0, djm, djt;
    if (iauCal2jd(ye, mo, da, &djm0, &djm) >= 0 && iauTf2d('+', ho, mi, se, &djt) >= 0) {
      lua_pushnumber(L, djm0 + djm + djt);
    }
    else
      lua_pushfail(L);
  } else if (tools_streq(s, "*sdn")) {  /* 2.10.0: Julian Date for the _Julian_ calendar, 2.16.12 tweak */
    double djt;
    long int sdn;
    sdn = JulianToSdn(ye, mo, da);
    if (iauTf2d('+', ho, mi, se, &djt) < 0 || sdn == 0)
      lua_pushfail(L);
    else
      lua_pushnumber(L, sdn + djt);  /* fraction of day beginning at midnight */
  } else if (tools_streq(s, "*e")) {  /* 2.9.8, 2.16.12 tweak; issues an error with 1900/02/29 */
    /* Test cases checked with http://aa.usno.navy.mil/data/docs/JulianDate.php: all results correct:
       os.date('*j', os.datetosecs(2016,6,8,12,30)) -> 2457548.0208333
       os.date('*j', os.datetosecs(2016,1,1,23,59)) -> 2457389.4993056 */
    lua_pushnumber(L, tools_esd(ye, mo, da, ho, mi, se));
  } else if (tools_streq(s, "*l")) {  /* 2.8.0, 2.16.12 tweak */
    lua_Number r;
    double djm0, djm, djt;
    if (iauCal2jd(ye, mo, da, &djm0, &djm) >= 0 && iauTf2d('+', ho, mi, se, &djt) >= 0) {
      r = djm0 + djm + djt - 2415018.5 - (stm->tm_isdst == 1)*1.0/24.0;  /* 2.9.8 fix (double instead of integer division) */
      lua_pushnumber(L, r - (r < 61));  /* 2.9.8 fix, 5.5.4 1900/02/28 fix */
    } else
      lua_pushfail(L);
  } else if (tools_streq(s, "*asc")) {  /* 5.5.7: return a date formatted like the C function asctime() does */
    char *r;
    char buffer[64];  /* man asctime mentions 26 bytes */
    /* The format is always DayOfWeek Month Day Hour:Minute:Second Year. The Day field is always padded to two characters. */
    r = asctime64_r(stm, buffer);
    if (r)
      lua_pushstring(L, r);
    else
      lua_pushfail(L);
  } else if (tools_streq(s, "*iso")) {  /* 5.5.7: returns a timestamp formatted according to the ISO 8601 standard. */
    size_t reslen;
    char buff[256];
    tools_bzero(buff, 256*sizeof(char));
    reslen = str_strftime(buff, sizeof(buff), "%Y-%m-%dT%H:%M:%SZ", stm, msecs);
    if (reslen == 0) {
      luaL_error(L, "Error in " LUA_QS ": conversion result too big.", "os.date");
    } else {
      lua_pushstring(L, buff);
    }
  } else if (tools_streq(s, "*apa")) {  /* 5.5.7: returns a timestamp formatted according to the Apache Common Log Format. */
    size_t reslen;
    char buff[256];
    tools_bzero(buff, 256*sizeof(char));
    reslen = str_strftime(buff, sizeof(buff), "[%d/%b/%Y:%H:%M:%S %z]", stm, msecs);
    if (reslen == 0) {
      luaL_error(L, "Error in " LUA_QS ": conversion result too big.", "os.date");
    } else {
      lua_pushstring(L, buff);
    }
  } else {  /* Lua 5.1.2 patch, patched Agena 1.12.4 */
    char cc[3];
    luaL_Buffer b;
    cc[0] = '%'; cc[2] = '\0';
    luaL_buffinit(L, &b);
    for (; *s; s++) {
      if (*s != '%' || *(s + 1) == '\0')  /* no conversion specifier? */
        luaL_addchar(&b, *s);  /* add character to resulting string */
      else {
        size_t reslen;
        char buff[256];  /* should be big enough for any conversion result */
        cc[1] = *(++s);
        /* see: http://www.gnu.org/software/libc/manual/html_node/Formatting-Calendar-Time.html */
        reslen = str_strftime(buff, sizeof(buff), cc, stm, msecs);  /* 5.5.3 fix for Windows */
        if (reslen == 0 && *cc != '%') {  /* 5.5.3 fix, Gemini AI, The *cc != '%' is an extra check for an empty format string */
          luaL_error(L, "Error in " LUA_QS ": conversion result too big.", "os.date");
        } else {
          luaL_addlstring(&b, buff, reslen);
        }
      }
    }
    luaL_pushresult(&b);
  }
  return 1;
}


static int os_time (lua_State *L) {
  Time64_T t;  /* 1.12.4 */
  int msecs, r, flag, withmsecs, nargs;
#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__) || defined(__OS2__)
  struct timeb tp;
#endif
  nargs = lua_gettop(L);
  withmsecs = 0;
  if (nargs > 0 && lua_istrue(L, nargs)) {
    withmsecs = 1;
    lua_settop(L, nargs - 1);
  }
  r = 1; flag = 1;
  t = (Time64_T)(-1);  /* 2.12.6 patch */
  if (!lua_isnoneornil(L, 1)) {  /* called with at least one arg ? */
    struct TM ts;  /* 1.12.4 */
    flag = 0;
    switch (lua_type(L, 1)) {
      case LUA_TTABLE: {
        int year;
        lua_getfield(L, 1, "year");
        if (lua_isnumber(L, -1)) {
          year = lua_tonumber(L, -1);
          agn_poptop(L);
          lua_settop(L, 1);  /* make sure table is at the top */
          ts.tm_sec = getfield(L, "sec", 0);
          ts.tm_min = getfield(L, "min", 0);
          ts.tm_hour = getfield(L, "hour", 12);
          ts.tm_mday = getfield(L, "day", -1);
          ts.tm_mon = getfield(L, "month", -1) - 1;
          ts.tm_year = year - 1900;
          ts.tm_isdst = getboolfield(L, "isdst");
          t = mktime64(&ts);  /* Agena 1.8.0 */
        } else {  /* 2.9.8 */
          agn_poptop(L);  /* pop null */
          t = agnL_datetosecs(L, 1, "os.time", 1, 0, &msecs);
        }
        break;
      }
      case LUA_TSEQ: {
        t = agnL_datetosecs(L, 1, "os.time", 1, 0, &msecs);
        break;
      }
      default: {
        t = time(NULL);  /* to avoid compiler warnings */
        luaL_error(L, "Error in " LUA_QS ": first argument must either be a table or sequence.", "os.time");
      }
    }
  }
  msecs = -1;
  /* 1.11.7, get milliseconds */
#if defined(_WIN32) || defined(__OS2__)  /* 2.12.6 patch */
  tools_ftime(&tp);
  if (flag) t = tp.time;
  msecs = tp.millitm;
#elif defined(__unix__) || defined(__APPLE__)
  if (tools_ftime(&tp) == 0) {  /* set milliseconds only when query has been successful */
    msecs = tp.millitm;
    if (flag) t = tp.time;
  }
#endif
  if (t == (Time64_T)(-1))  /* 2.1.3 */
    lua_pushnil(L);
  else {
    if (withmsecs) {
      lua_pushnumber(L, (lua_Number)t + msecs/1000.0); r = 1;
    } else {
      lua_pushnumber(L, (lua_Number)t);
    }
    if (msecs != -1 && !withmsecs) {
      lua_pushnumber(L, msecs);
      r = 2;
    }
  }
  return r;
}


static int os_difftime (lua_State *L) {
  lua_pushnumber(L, difftime((Time64_T)(agnL_checknumber(L, 1)),  /* 2.1.3 */
                             (Time64_T)(agnL_optnumber(L, 2, 0))));  /* 2.1.3 */
  return 1;
}


/* Taken from:
   https://github.com/luau-lang/luau
   file: VM/src/lperf.cpp
   Licence: MIT.

   Copyright (c) 2019-2024 Roblox Corporation
   Copyright (c) 1994–2019 Lua.org, PUC-Rio.

   Returns the the computer's clock timing (period) in seconds */
static lua_Number aux_period () {
#if defined(_WIN32)
  LARGE_INTEGER result = {};
  QueryPerformanceFrequency(&result);
  return 1.0/(lua_Number)(result.QuadPart);
#elif defined(__APPLE__)
  mach_timebase_info_data_t result = {};
  mach_timebase_info(&result);
  return (lua_Number)(result.numer)/(lua_Number)(result.denom)*1e-9;
#else  /* also for Linux, 5.6.2 change */
  return 1.0/(lua_Number)(CLOCKS_PER_SEC);
#endif
}

static int os_period (lua_State *L) {
  lua_pushnumber(L, aux_period());
  return 1;
}


/* Taken from:
   https://github.com/luau-lang/luau
   file: VM/src/lperf.cpp

   Licence: MIT.

   Copyright (c) 2019-2024 Roblox Corporation
   Copyright (c) 1994–2019 Lua.org, PUC-Rio.

   Returns the current value of the computer's clock that increments monotonically in tick units. */

static lua_Number aux_timestamp () {
#if defined(_WIN32)
  LARGE_INTEGER result = {};
  QueryPerformanceCounter(&result);
  return (lua_Number)(result.QuadPart);
#elif defined(__APPLE__)
  return (lua_Number)(mach_absolute_time());
#elif defined(__linux__)
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return now.tv_sec*1e9 + now.tv_nsec;
#elif defined(__SOLARIS)
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (now.tv_sec*1e9 + now.tv_nsec)/1e3;
#else  /* DOS, etc. */
  return (lua_Number)(clock());
#endif
}

static int os_timestamp (lua_State *L) {
  lua_pushnumber(L, aux_timestamp());
  return 1;
}


static int os_ticker (lua_State *L) {
  lua_pushnumber(L, aux_timestamp()*aux_period());
  return 1;
}


/* }====================================================== */


static int os_setlocale (lua_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = luaL_optstring(L, 1, NULL);
  int op = luaL_checkoption(L, 2, "all", catnames);
  lua_pushstring(L, setlocale(cat[op], l));
  return 1;
}


/* Checks whether the given locale is supported by the operating system and returns `true` or `false`. If a locale is supported,
   the second return is its description. The function - contrary to `os.setlocale` - never changes the current locale. Examples
   for locales are 'UK' for United Kingdom (at least in Windows), 'he_IL' for Hebrew (Israel), 'de_AT' for Austrian and
   'zh_Hans_SG' for Simplified Chinese (Singapore). */
static int os_islocale (lua_State *L) {  /* 2.14.11 */
  char *oldlocale, *newlocale, *t;
  const char *loc = agn_checkstring(L, 1);
  t = setlocale(LC_CTYPE, NULL);
  if (!t)  /* 2.17.1 hardening */
    luaL_error(L, "Error in " LUA_QS ": could not determine current locale.", "os.islocale");
  oldlocale = tools_strdup(t);
  if (!oldlocale) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.islocale");
  newlocale = setlocale(LC_CTYPE, loc);
  if (newlocale == NULL) {
    xfree(oldlocale);
    lua_pushfalse(L);
    return 1;
  }
  lua_pushtrue(L);
  lua_pushstring(L, newlocale);
  setlocale(LC_CTYPE, oldlocale);
  xfree(oldlocale);
  return 2;
}


#ifdef _WIN32
static int os_setthreadlocale (lua_State *L) {  /* 4.5.3 */
  /* The LCID for Czech is 0x0405 */
  LCID lcid = luaL_checkinteger(L, 1);
  lua_pushboolean(L, SetThreadLocale(lcid));
  return 1;
}
#endif


static int os_exit (lua_State *L) {  /* 2.9.2, inspired by Dubiousjim's Lua 5.1 power patch
  published at http://lua-users.org/wiki/LuaPowerPatches; 2.9.6 clearing the state is now the default. */
  int status = agnL_optinteger(L, 1, EXIT_SUCCESS);
  agnL_onexit(L, 0);  /* 6.1.3 fix to prevent huge memory leaks, gc's and closes L->C */
  if (L) lua_close(L);  /* patched 7.6.6 for srglue/sragena. */
  exit(status);
  return 0;  /* to avoid warnings */
}


/* determine information on available memory on Windows platforms, 0.5.3, September 15, 2007;
   extended in 0.12.2 on September 29, 2008 to work in UNIX; extended to fully support OS/2 in 0.13.3;
   extended to support Mac OS X in 0.31.3; extended 2.10.0 */
#ifdef __DJGPP__
#define int86xx(x,r)  int86((x), (r), (r))
#define EMS_MASK    0xffff0000UL
/* EMS version with action = 0x46:
   major: ((ushort)return >> 4) & 0xf;
   minor: (ushort)return & 0xf; */
static int ems (unsigned char action) {
  union REGS r;
  r.h.ah = action;
  int86xx(0x67, &r);
  return r.x.ax;
}

/* EMS frame with action = 0x41, free EMS with action = 0x42 */
static long int ems_bx (unsigned char action) {
  union REGS r;
  r.h.ah = action;
  int86xx(0x67, &r);
  return ((long int)r.x.ax & EMS_MASK) | r.x.bx;
}

/* EMS size with action = 0x42 */
static long int ems_dx (unsigned char action) {
  union REGS r;
  r.h.ah = action;
  int86xx(0x67, &r);
  return ((long int)r.x.ax & EMS_MASK) | r.x.dx;
}

static int xms (int action) {
  union REGS r;
  r.x.ax = action;
  int86xx(0x2f, &r);
  return r.h.al;
}

#define HMA_FREE 0xFFFF
static unsigned int hma_free (void) {
  union REGS regs;
  regs.x.ax = 0x4A01;
  regs.x.bx = HMA_FREE;
  int86(0x2F, &regs, &regs);
  return regs.x.bx;
}

#define ems_size()  ems_dx(0x42)
#define ems_free()  ems_bx(0x42)
#define has_xms()   (xms(0x4300) != 0)
#define ems_major() ((ems(0x46) >> 4) & 0xf)
#define ems_minor() (ems(0x46) & 0xf)

#endif


#if defined(_WIN32) || defined(__unix__) || defined(__DJGPP__) || defined(__OS2__) || defined(__HAIKU__)  || defined(__APPLE__)
static int os_memstate (lua_State *L) {
#if defined(__APPLE__)  /* 2.4.0 change */
  lua_Number div;
  struct xsw_usage apple_xsu;  /* for os.memstate */
  size_t apple_xsu_len;        /* for os.memstate */
#else
  long long AvailPhys, TotalPhys, div;
#endif
#if defined(_WIN32)
  long long AvailVirtual, TotalVirtual, AvailPageFile, TotalPageFile;
  struct WinVer winversion;
  SYSTEM_INFO si;
#elif __OS2__
  APIRET result;
  ULONG memInfo[5];
  ULONG memInfoPageSize[1];
  (void)TotalPhys; (void)AvailPhys;  /* to avoid compiler warnings */
#elif defined(__APPLE__)
  vm_size_t pagesize;
  vm_statistics_data_t vminfo;
  mach_port_t system;
  unsigned int c;
  int sysmask[2];
  size_t length;
  uint64_t memsize;
  lua_Number f;
#endif
  static const char *const unit[] = {"bytes", "kbytes", "mbytes", "gbytes", "tbytes", NULL};  /* 2.10.0 */
  div = tools_intpow(1024, agnL_checkoption(L, 1, "bytes", unit, 0));
#if defined(_WIN32)
  if (getWindowsVersion(&winversion) < MS_WINXP) {
    MEMORYSTATUS memstat;  /* 2.3.0 RC 3 */
    memstat.dwLength = sizeof(MEMORYSTATUS);
    GlobalMemoryStatus(&memstat);
    AvailPhys = (long long)memstat.dwAvailPhys;  /* and following lines: 2.18.1 fix */
    TotalPhys = (long long)memstat.dwTotalPhys;
    AvailVirtual = (long long)memstat.dwAvailVirtual;
    TotalVirtual = (long long)memstat.dwTotalVirtual;
    AvailPageFile = (long long)memstat.dwAvailPageFile;
    TotalPageFile = (long long)memstat.dwTotalPageFile;
  } else {  /* 2.3.0 RC 3 extension */
    MEMORYSTATUSEX memstat;
    memstat.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memstat);
    AvailPhys = (DWORDLONG)memstat.ullAvailPhys;
    TotalPhys = (DWORDLONG)memstat.ullTotalPhys;
    AvailVirtual = (DWORDLONG)memstat.ullAvailVirtual;
    TotalVirtual = (DWORDLONG)memstat.ullTotalVirtual;
    AvailPageFile = (DWORDLONG)memstat.ullAvailPageFile;
    TotalPageFile = (DWORDLONG)memstat.ullTotalPageFile;
  }
  GetSystemInfo(&si);  /* get dwPageSize (in bytes), 2.6.1 */
  /* long MemoryLoad = (long)memstat.dwMemoryLoad; */
  /* int Length = (int)memstat.dwLength; */
  lua_newtable(L);
  lua_rawsetstringnumber(L, -1, "freephysical", (lua_Number)(AvailPhys)/div);
  lua_rawsetstringnumber(L, -1, "totalphysical", (lua_Number)(TotalPhys)/div);
  lua_rawsetstringnumber(L, -1, "freevirtual", (lua_Number)(AvailVirtual)/div);
  lua_rawsetstringnumber(L, -1, "totalvirtual", (lua_Number)(TotalVirtual)/div);    /* 0.26.0 patch */
  lua_rawsetstringnumber(L, -1, "freepagefile", (lua_Number)(AvailPageFile)/div);   /* 2.3.2 extension */
  lua_rawsetstringnumber(L, -1, "totalpagefile", (lua_Number)(TotalPageFile)/div);  /* 2.3.2 extension */
  lua_rawsetstringnumber(L, -1, "pagesize", (lua_Number)(si.dwPageSize)/div);       /* 2.6.1 extension */
#elif (defined(__unix__) && !defined(__DJGPP__)) || defined(__HAIKU__) /* for DJGPP */
  (void)AvailPhys; (void)TotalPhys;  /* to avoid compiler warnings */
  /* Declare standard 64-bit storage primitives matching Windows layout */
  long long t_phys = 0, f_phys = 0;
  long long t_swap = 0, f_swap = 0;
  long long t_virt = 0, f_virt = 0;
  long long page_size = sysconf(_SC_PAGESIZE);
  /* 1. Extract physical memory allocations universally via POSIX */
  f_phys = (long long)sysconf(_SC_AVPHYS_PAGES) * page_size;
  t_phys = (long long)sysconf(_SC_PHYS_PAGES) * page_size;
  /* 2. Platform-Specific Virtual / Swap Allocation Gathering */
  #if defined(__linux__)
  struct sysinfo si;
  if (sysinfo(&si) == 0) {
    /* Linux scales values by si.mem_unit on modern kernels to prevent overflow */
    long long unit = si.mem_unit ? (long long)si.mem_unit : 1;
    t_swap = (long long)si.totalswap * unit;
    f_swap = (long long)si.freeswap * unit;
    /* Virtual Memory on Linux corresponds to the total system space boundary layout */
    t_virt = (long long)si.totalram * unit + t_swap;
    f_virt = (long long)si.freeram * unit + f_swap;
  }
  #elif defined(__SOLARIS)
  /* Solaris Swap Space extraction via swapctl */
  struct anoninfo ai;
  if (swapctl(SC_AINFO, &ai) != -1) {
    /* ai.ani_max = total assignable space in pages, ai.ani_resv = reserved pages */
    t_swap = (long long)ai.ani_max * page_size;
    f_swap = (long long)(ai.ani_max - ai.ani_resv) * page_size;
    /* On Solaris, Virtual Memory represents aggregate physical memory + swap configurations */
    t_virt = t_phys + t_swap;
    f_virt = f_phys + f_swap;
  }
  #else
  /* Fallback default boundaries for generic BSD or Haiku distributions */
  t_swap = f_swap = t_virt = f_virt = 0;
  #endif
  /* Generate the output table layout matching the Windows signature */
  lua_newtable(L);
  lua_rawsetstringnumber(L, -1, "freephysical", (lua_Number)f_phys / div);
  lua_rawsetstringnumber(L, -1, "totalphysical", (lua_Number)t_phys / div);
  lua_rawsetstringnumber(L, -1, "pagesize", (lua_Number)page_size / div);
  if (t_virt > 0) {
    lua_rawsetstringnumber(L, -1, "freevirtual", (lua_Number)f_virt / div);
    lua_rawsetstringnumber(L, -1, "totalvirtual", (lua_Number)t_virt / div);
  }
  if (t_swap > 0) {
    lua_rawsetstringnumber(L, -1, "freepagefile", (lua_Number)f_swap / div);
    lua_rawsetstringnumber(L, -1, "totalpagefile", (lua_Number)t_swap / div);
  }
#elif defined(__OS2__)
  /* see: http://www.edm2.com/os2api/Dos/DosQuerySysInfo.html */
  result = DosQuerySysInfo(QSV_TOTPHYSMEM, QSV_MAXSHMEM, &memInfo[0], sizeof(memInfo));
  AvailPhys = 0;
  TotalPhys = 0;
    /* sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE); does not work for
      the _SC* names are not defined in GCC unistd.h */
  lua_newtable(L);
  if (result != 0) {  /* 2.6.1 */
    lua_rawsetstringnumber(L, -1, "totalphysical", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "freevirtual", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "resident", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "maxprmem", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "maxshmem", AGN_NAN);
  } else {
    lua_rawsetstringnumber(L, -1, "totalphysical", (lua_Number)memInfo[0]/div);
    lua_rawsetstringnumber(L, -1, "freephysical", (lua_Number)memInfo[3]/div);    /* QSV_MAXPRMEM = Maximum number of bytes available for this process RIGHT NOW. 2.37.5 extension. */
    lua_rawsetstringnumber(L, -1, "freevirtual", (lua_Number)memInfo[2]/div);  /* Maximum number of bytes available for all processes in the system RIGHT NOW.  */
    lua_rawsetstringnumber(L, -1, "resident", (lua_Number)memInfo[1]/div);
    lua_rawsetstringnumber(L, -1, "maxprmem", (lua_Number)memInfo[3]/div);  /* QSV_MAXPRMEM = Maximum number of bytes available for this process RIGHT NOW. 2.3.2 eCS extension. */
    lua_rawsetstringnumber(L, -1, "maxshmem", (lua_Number)memInfo[4]/div);  /* QSV_MAXSHMEM = Maximum number of shareable bytes available RIGHT NOW. 2.3.2 eCS extension. */
  }
  result = DosQuerySysInfo(QSV_PAGE_SIZE, QSV_PAGE_SIZE,  /* 2.6.1 */
      &memInfoPageSize[0], sizeof(memInfoPageSize));
  if (result != 0) {
    lua_rawsetstringnumber(L, -1, "pagesize", AGN_NAN);
  } else {
    lua_rawsetstringnumber(L, -1, "pagesize", (lua_Number)memInfoPageSize[0]/div);
  }
#elif defined(__APPLE__)
  sysmask[0] = CTL_HW;
  sysmask[1] = HW_MEMSIZE;
  length = sizeof(memsize);
  memsize = 0L;
  lua_newtable(L);
  c = HOST_VM_INFO_COUNT;
  system = mach_host_self();
  pagesize = 0;  /* 2.3.3 fix */
  if (host_page_size(system, &pagesize) != KERN_SUCCESS) pagesize = 4096;
  f = pagesize/div;
  if (host_statistics(system, HOST_VM_INFO, (host_info_t)(&vminfo), &c) != KERN_SUCCESS) {
    lua_rawsetstringnumber(L, -1, "freephysical", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "active", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "inactive", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "speculative", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "wireddown", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "reactivated", AGN_NAN);
  } else {
    lua_rawsetstringnumber(L, -1, "freephysical", ((vminfo.free_count - vminfo.speculative_count)) * f);
    lua_rawsetstringnumber(L, -1, "active", vminfo.active_count * f);
    lua_rawsetstringnumber(L, -1, "inactive", vminfo.inactive_count * f);
    lua_rawsetstringnumber(L, -1, "speculative", vminfo.speculative_count * f);
    lua_rawsetstringnumber(L, -1, "wireddown", vminfo.wire_count * f);
    lua_rawsetstringnumber(L, -1, "reactivated", vminfo.reactivations * f);
  }
  if (sysctl(sysmask, 2, &memsize, &length, NULL, 0 ) == -1) {
    lua_rawsetstringnumber(L, -1, "totalphysical", AGN_NAN);
  } else {
    lua_rawsetstringnumber(L, -1, "totalphysical", memsize/div);
  }
  /* Extracted Swap Space / Page File Metrics (Windows-equivalent properties) */
  apple_xsu_len = sizeof(apple_xsu);
  if (sysctlbyname("vm.swapusage", &apple_xsu, &apple_xsu_len, NULL, 0) == -1) {
    lua_rawsetstringnumber(L, -1, "freevirtual", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "totalvirtual", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "freepagefile", AGN_NAN);
    lua_rawsetstringnumber(L, -1, "totalpagefile", AGN_NAN);
  } else {
    /* Base Swap Calculations utilizing the new unique structural names */
    double t_swap = (double)apple_xsu.xsu_total / div;
    double f_swap = (double)apple_xsu.xsu_avail / div;
    /* Calculate Virtual boundaries using your existing physical counts */
    double t_phys = (memsize > 0L) ? ((double)memsize / div) : 0.0;
    double f_phys = (host_statistics(system, HOST_VM_INFO, (host_info_t)(&vminfo), &c) == KERN_SUCCESS) ?
                    ((double)((vminfo.free_count - vminfo.speculative_count)) * f) : 0.0;
    lua_rawsetstringnumber(L, -1, "freepagefile", f_swap);
    lua_rawsetstringnumber(L, -1, "totalpagefile", t_swap);
    if (t_phys > 0.0) {
      lua_rawsetstringnumber(L, -1, "freevirtual", f_phys + f_swap);
      lua_rawsetstringnumber(L, -1, "totalvirtual", t_phys + t_swap);
    } else {
      lua_rawsetstringnumber(L, -1, "freevirtual", AGN_NAN);
      lua_rawsetstringnumber(L, -1, "totalvirtual", AGN_NAN);
    }
  }
  lua_rawsetstringnumber(L, -1, "pagesize", pagesize/div);  /* 2.6.1 */
#elif defined(__DJGPP__)
  (void)AvailPhys; (void)TotalPhys; (void)div;
  lua_newtable(L);
  lua_rawsetstringnumber(L, -1, "ems_size", ems_size());
  lua_rawsetstringnumber(L, -1, "ems_free", ems_free());
  lua_rawsetstringnumber(L, -1, "has_xms", has_xms());
  lua_rawsetstringnumber(L, -1, "ems_major", ems_major());
  lua_rawsetstringnumber(L, -1, "ems_minor", ems_minor());
  lua_rawsetstringnumber(L, -1, "hma_free", hma_free());
  /* see: https://fragglet.github.io/dos-help-files/clang.hlp/x_at_L80f2.html */
  lua_rawsetstringnumber(L, -1, "totalphysical", _bios_memsize());  /* 2.39.11 */
#else
  lua_pushfail(L);
#endif
  return 1;
}
#endif


/* Returns all 31 OS/2 settings that can be queried via the C API function DosQuerySysInfo, in a table.
   See: http://www.edm2.com/os2api/Dos/DosQuerySysInfo.html */
#if defined(__OS2__)
static const char *const qsv[] = {
  "QSV_MAX_PATH_LENGTH",  /* 1, not zero ! */
  "QSV_MAX_TEXT_SESSIONS",
  "QSV_MAX_PM_SESSIONS",
  "QSV_MAX_VDM_SESSIONS",
  "QSV_BOOT_DRIVE",
  "QSV_DYN_PRI_VARIATION",
  "QSV_MAX_WAIT",
  "QSV_MIN_SLICE",
  "QSV_MAX_SLICE",
  "QSV_PAGE_SIZE",
  "QSV_VERSION_MAJOR",
  "QSV_VERSION_MINOR",
  "QSV_VERSION_REVISION",
  "QSV_MS_COUNT",
  "QSV_TIME_LOW",
  "QSV_TIME_HIGH",
  "QSV_TOTPHYSMEM",
  "QSV_TOTRESMEM",
  "QSV_TOTAVAILMEM",
  "QSV_MAXPRMEM",
  "QSV_MAXSHMEM",
  "QSV_TIMER_INTERVAL",
  "QSV_MAX_COMP_LENGTH",
  "QSV_FOREGROUND_FS_SESSION",
  "QSV_FOREGROUND_PROCESS",
  "QSV_NUMPROCESSORS",  /* 26 */
  "QSV_MAXHPRMEM",
  "QSV_MAXHSHMEM",
  "QSV_MAXPROCESSES",
  "QSV_VIRTUALADDRESSLIMIT",
  "QSV_INT10ENABLED",   /* 31 */
  /* "QSV_MAX", 32 */
  NULL
};

static int os_os2info (lua_State *L) {
  APIRET result;
  int nargs, nres, i;
  nargs = lua_gettop(L);
  if (nargs == 0) {
    nres = 1;
    ULONG Info[31];
    result = DosQuerySysInfo(QSV_MAX_PATH_LENGTH, QSV_INT10ENABLED, &Info[0], sizeof(Info));
    if (result != 0)
      lua_pushfail(L);
    else {
      lua_createtable(L, 0, 31);
      for (i=0; i < 31; i++) {
        lua_rawsetstringnumber(L, -1, qsv[i], (lua_Number)Info[i]);
      }
    }
  } else {
    int setting;
    luaL_checkstack(L, nargs, "too many arguments");
    for (i=1; i <= nargs; i++) {
      ULONG Info[1];
      setting = 1 + agnL_checkoption(L, i, "_", qsv, 1);  /* compare case-insensitively */
      result = DosQuerySysInfo(setting, setting, &Info[0], sizeof(Info));
      if (result != 0) lua_pushfail(L);
      else lua_pushnumber(L, (lua_Number)Info[0]);
    }
    nres = nargs;
  }
  return nres;
}
#endif


/* get the current Windows login name, 0.5.3, September 15, 2007;
   extended 0.12.2, October 12, 2008; changed to support OS/2 0.13.3 */
static int os_login (lua_State *L) {
#if defined(_WIN32)
  char buffer[257];
  DWORD len = 256;
  if (GetUserName(buffer, &len)) {  /* Agena 1.6.0 */
    buffer[len] = '\0';
    lua_pushstring(L, buffer);
  } else
    lua_pushfail(L);
#elif defined(__unix__) || defined(__OS2__) || defined(__APPLE__) || defined(__HAIKU__)
  char *LoginName;
  LoginName = getlogin();
  if (LoginName == NULL)
    lua_pushfail(L);
  else
    lua_pushstring(L, LoginName);
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* get the Windows computer name, 0.5.3, September 15, 2007;
   improved in 0.13.3, extended 2.3.0 RC 2 */
static int os_computername (lua_State *L) {
#if defined(_WIN32)
  char buffer[257];
  DWORD len = 256;
  if (lua_gettop(L) == 0) {
    if (GetComputerName(buffer, &len)) {  /* Agena 1.6.0 */
      buffer[len] = '\0';
      lua_pushstring(L, buffer);
    } else
      lua_pushfail(L);
  } else {
    /* Returns detailed information on the NetBIOS or DNS name associated with the local computer. 2.27.2
       Taken from: https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getcomputernameexa &
                   https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/countof-macro?view=msvc-170 */
    TCHAR buffer[1024];  /* 7.5.15 safety patch */
    TCHAR szDescription[8][32] = {TEXT("NetBIOS"),
          TEXT("DNS hostname"),
          TEXT("DNS domain"),
          TEXT("DNS fully-qualified"),
          TEXT("Physical NetBIOS"),
          TEXT("Physical DNS hostname"),
          TEXT("Physical DNS domain"),
          TEXT("Physical DNS fully-qualified")};
    int cnf = 0;
    if (tools_getwinver() < MS_WIN2K) {
      lua_pushfail(L);
      return 1;
    }
    luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
    lua_createtable(L, 0, ComputerNameMax);
    for (cnf=0; cnf < ComputerNameMax; cnf++) {
      DWORD dwSize = 1024;
      /* The API writes the string AND the null terminator */
      if (GetComputerNameEx((COMPUTER_NAME_FORMAT)cnf, buffer, &dwSize)) {
        lua_rawsetstringstring(L, -1, szDescription[cnf], buffer);
      } else {
        lua_rawsetstringstring(L, -1, szDescription[cnf], "unknown");
      }
    }
  }
#elif defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
  /* does not work with OS/2: needs a file `TCPIP32` (as does bash). If not
     present, Agena does not start. */
  char buffer[256];
  if (gethostname(buffer, sizeof buffer) == 0)
    lua_pushstring(L, buffer);
  else
    lua_pushfail(L);
#elif defined(__OS2__)  /* 2.3.0 RC 2 eCS extension */
  const char *str = getenv("HOSTNAME");
  lua_pushstring(L, str);
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* return amount of free memory; extended 0.12.2, 28.09.2008; OS/2 addon
   August 07, 2009, OS X addon 0.31.3 March 28, 2010, extended 2.10.0 */
static int os_freemem (lua_State *L) {
#if defined(_WIN32) || (defined(__unix__) && !defined(__DJGPP__)) || defined(__OS2__) || defined(__APPLE__) || defined(__HAIKU__)
  lua_Number div;
  static const char *const unit[] = {"bytes", "kbytes", "mbytes", "gbytes", "tybtes", NULL};  /* 2.10.0 */
  div = tools_intpow(1024, agnL_checkoption(L, 1, "bytes", unit, 0));
#if defined(_WIN32)
  MEMORYSTATUS memstat;
#elif __OS2__
  APIRET result;
  ULONG memInfo[3];
#elif defined(__APPLE__)
  vm_size_t pagesize;
  vm_statistics_data_t vminfo;
  mach_port_t system;
  unsigned int c;
#endif
#if defined(_WIN32)
  memstat.dwLength = sizeof(MEMORYSTATUS);
  GlobalMemoryStatus(&memstat);
  lua_pushnumber(L, (lua_Number)memstat.dwAvailPhys/div);
#elif defined(__unix__) && !defined(__DJGPP__) || defined(__HAIKU__)  /* excludes DJGPP */
  lua_pushnumber(L, (lua_Number)(sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE))/div);
#elif defined (__OS2__)
  result = DosQuerySysInfo(QSV_TOTPHYSMEM, QSV_TOTAVAILMEM,
      &memInfo[0], sizeof(memInfo));
  if (result != 0)  /* return free virtual memory */
    lua_pushfail(L);
  else
    lua_pushnumber(L, (lua_Number)(memInfo[2])/div);
#elif defined(__APPLE__)
  c = HOST_VM_INFO_COUNT;
  system = mach_host_self();
  pagesize = 0;  /* 2.3.3 fix */
  if (host_page_size(mach_host_self(), &pagesize) != KERN_SUCCESS) pagesize = 4096;
  if (host_statistics(system, HOST_VM_INFO, (host_info_t)(&vminfo), &c) != KERN_SUCCESS)
    lua_pushfail(L);
  else
    lua_pushnumber(L, (uint64_t)(vminfo.free_count - vminfo.speculative_count) * pagesize/div);
#endif
#else
  lua_pushfail(L);
#endif
  return 1;
}


#ifdef _WIN32
static int issymlink (const char *fn) {
  DWORD mode = GetFileAttributes(fn);
  return (mode & FILE_ATTRIBUTE_REPARSE_POINT) == 1024;
}
#endif


static int issysdir (const char *path) {
  struct stat st;
  int rcstat;
  size_t l;
#if defined(__OS2__)
  static FILESTATUS3 fstat, *fs;
  APIRET rc;
#endif
  (void)l;
#if defined(_WIN32) || defined(__OS2__) || defined(__DJGPP__)
  l = tools_strlen(path);
  while (l > 2 && (path[l - 1] == '/' || path[l - 1] == '\\')) l--;
  if (l > 0 && (path[l - 1] == '*' || path[l - 1] == '.')) l--;
  while (l > 2 && (path[l - 1] == '/' || path[l - 1] == '\\')) l--;
  if (l == 2 && isalpha(path[0]) && path[1] == ':')
    return 0;  /* drives are not considered system dirs */
#endif
  rcstat = stat(path, &st);
  if (rcstat != 0) return -1;
  if (!S_ISDIR(st.st_mode)) return 0;
#if defined(_WIN32)
    /* taken from: https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/winbase/io/extendedfileapis/ExtendedFileAPIs.cpp, 2.34.10 */
  if (tools_getwinver() >= MS_WIN2K) {  /* detect PerfLogs, System Volume Information, etc.; 2.39.9 extension to include W2K as CreateFileA also works there */
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
      return 1;
    else
      CloseHandle(hFile);
  }
#endif
#ifdef __OS2__
  rc = DosQueryPathInfo((PCSZ)path, FIL_STANDARD, &fstat, sizeof(fstat));
  if (rc == 0) {
    unsigned int attribs;
    fs = &fstat;  /* do not change this, otherwise Agena will crash */
    attribs = fs->attrFile;
    return (attribs & FILE_SYSTEM) != 0;
  } else {
    return -1;
  }
#else
  return 0;
#endif
}

static int os_issysdir (lua_State *L) {
  const char *path = agn_checkstring(L, 1);
  agn_pushboolean(L, (tools_streq(path, "..") || strstr(path, "/..") != NULL) ? -1 : issysdir(path));
  return 1;
}


enum AGN_FSOBJTYPE                     {FS_FILE, FS_DIR, FS_SYSDIR, FS_LINK, FS_CSFILE,      FS_BSFILE,       FS_OTHER, FS_ERROR};
static const char *const fsobjtype[] = {"FILE",  "DIR",  "SYSDIR",  "LINK",  "CHARSPECFILE", "BLOCKSPECFILE", "OTHER",  NULL};

struct stat getfsobjecttype (const char *fn, int *fso, int hnd) {
  mode_t mode;
  struct stat e;  /* MinGW does not know stat64 */
#ifdef __OS2__
  if (hnd == 0 && lstat(fn, &e) == 0) {  /* 2.37.6 fix */
#elif defined(__SOLARIS)
  if ((hnd == 0 && lstat(fn, &e) == 0) || (hnd != 0 && fstat64(hnd, &e) == 0)) {  /* 2.37.6 fix */
#elif (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
  if ((hnd == 0 && lstat(fn, &e) == 0) || (hnd != 0 && fstat(hnd, &e) == 0)) {  /* 1.12.1/2.37.5 */
#else
  /* In Windows, we will use stat() instead of GetFileAttributes() for backward-compatibility with W2K and unambiguous results.
     MinGW does not know lstat. */
  if ((hnd == 0 && stat(fn, &e) == 0) || (hnd != 0 && fstat(hnd, &e) == 0)) {  /* 1.12.1/2.37.5 */
#endif
    mode = e.st_mode;
    if      (S_ISDIR(mode)) *fso = (issysdir(fn)) ? FS_SYSDIR : FS_DIR;
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
    else if (S_ISLNK(mode)) *fso = FS_LINK;
#elif defined(_WIN32)
    else if (issymlink(fn)) *fso = FS_LINK;
#endif
    else if (S_ISREG(mode)) *fso = FS_FILE;
    else if (S_ISCHR(mode)) *fso = FS_CSFILE;
    else if (S_ISBLK(mode)) *fso = FS_BSFILE;
    else /* any other */    *fso = FS_OTHER;
  } else {  /* could not get file attributes */
    *fso = FS_ERROR;
  }
  return e;
}


#ifndef S_ISLNK  /* 2.25.0 adaption for MinGW/GCC 10.x */
#define S_IFLNK    0120000 /* Symbolic link */
#define S_ISLNK(x) (((x) & S_IFMT) == S_IFLNK)
#endif

static int aux_listcore (lua_State *L, int countonly) {
  DIR *dir;
  struct dirent *entry;
  int i, isdir, isfile, islink, isall, nops, push, hasfa, en, scrutinise, mode;
  const char *path0, *option, *fn, *pattern;
  char *tbf;  /* to be FREED after string has been pushed on the stack */
  size_t l;
  Charbuf path;  /* 2.39.12 */
#ifndef __SOLARIS
  unsigned char filetype;
#ifndef _DIRENT_HAVE_D_TYPE
  struct stat64 stbuf;
  size_t len;
#endif
#endif
  isdir = isfile = islink = hasfa = 0;
  isall = 1;
  pattern = NULL;  /* 1.9.4 */
  nops = lua_gettop(L);
  if (lua_isnoneornil(L, 1)) {  /* no arguments ?  Agena 1.6.0 Valgrind */
    path0 = "."; l = 1;  /* 2.39.12 simplification */
  } else {
    path0 = agn_checklstring(L, 1, &l);
    if (tools_streqx(path0, "*", "*.*", NULL)) {
      path0 = "."; l = 1;  /* 2.17.1 patch to prevent `position and size not aligned to block size` warning */
    }
  }
  charbuf_init(L, &path, 0, PATH_MAX < 513 ? PATH_MAX : 512, 1);  /* 2.39.12/13 */
  charbuf_append(L, &path, path0, l);
  str_charreplace(path.data, '\\', '/', 1);
  charbuf_finish(&path);
  for (i=2; i <= nops; i++) {
    option = luaL_checkstring(L, i);
    /* 2.16.12 tweaks, extensions 2.17.1 */
    if (     tools_streqx(option, "files", "file", NULL)) { isfile = 1; isall = 0; hasfa = 1; }
    else if (tools_streqx(option, "dirs",  "dir",  NULL)) { isdir = 1;  isall = 0; hasfa = 1; }
    else if (tools_streqx(option, "links", "link", NULL)) { islink = 1; isall = 0; hasfa = 1; }
    else if (tools_streq(option, "all")                 ) {                        hasfa = 1; }
    else pattern = option;
  }
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  dir = opendir(path.data);
  en = errno;
  if (dir == NULL) {
    lua_pushnil(L);
    lua_pushstring(L, my_ioerror(en));  /* changed 2.10.4 */
    charbuf_free(&path);
    return 2;
  }
  if (!countonly) { lua_newtable(L); }
  i = 0;
  while ((entry = readdir(dir)) != NULL) {  /* traverse subdirectories in current working directory */
    push = nops <= 1 || pattern != NULL;
    fn = entry->d_name;
    if (tools_streqx(fn, ".", "..", NULL) || (pattern != NULL && str_glob(pattern, fn) == 0)) continue;  /* 1.9.4 */
#ifndef __SOLARIS
#ifdef _DIRENT_HAVE_D_TYPE
    filetype = entry->d_type;
    scrutinise = (  /* skip reading file attributes explicitly if file type is already known, 2.17.1 */
      (isfile && filetype != DT_REG) ||
      (isdir  && filetype != DT_DIR) ||
      (islink && filetype != DT_LNK)
    );
#else
    /* The only method if d_type isn't available, otherwise this is a fallback for FSes where the kernel leaves it DT_UNKNOWN.
       https://stackoverflow.com/questions/39429803/how-to-list-first-level-directories-only-in-c/39430337#39430337
       2.25.0 adaption for MinGW/GCC 10.x. Add and later delete filename, 3.3.1 tweak */
    len = tools_strlen(fn);
    charbuf_appendchar(L, &path, '/');
    charbuf_append(L, &path, fn, len++);
    charbuf_finish(&path);
    if (stat64(path.data, &stbuf) != 0) {
      charbuf_free(&path);
      luaL_error(L, "Error in " LUA_QS ": could not stat: %s", "os.listcore", my_ioerror(errno));
    }
    charbuf_removelast(L, &path, len);
    filetype = stbuf.st_mode;
    scrutinise = (  /* skip reading file attributes explicitly if file type is already known, 2.17.1 */
      (isfile && S_ISREG(filetype)) ||
      (isdir  && S_ISDIR(filetype)) ||
      (islink && S_ISLNK(filetype))
    );
#endif
#else  /* Solaris */
    scrutinise = 1;
#endif
    if (scrutinise && hasfa) {  /* read file attributes; do not create `//...`, 0.31.4, tuned 1.9.5 */
      tbf = tools_streq(path.data, "/") ? str_concat("/", fn, NULL) : str_concat(path.data, "/", fn, NULL);  /* 4.10.7 change */
      getfsobjecttype(tbf, &mode, 0);
      xfree(tbf);  /* free memory, 1.10.4 */
      push = (
        (mode == FS_FILE  && isfile) ||
        (mode == FS_DIR   && isdir)  ||
        (mode == FS_LINK  && islink) ||
        (mode != FS_ERROR && isall)
      );
    }
    if (push || !scrutinise) {  /* 2.17.1 extension */
      i++;
      if (!countonly) {
        lua_rawsetistring(L, -1, i, fn);  /* 1.9.5 */
      }
    }
  }
  closedir(dir);
  charbuf_finish(&path);  /* better sure than sorry, 2.39.12 */
  charbuf_free(&path);
  if (countonly) lua_pushinteger(L, i);
  return 1;
}

static int os_listcore (lua_State *L) {
  return aux_listcore(L, 0);
}

static int os_countcore (lua_State *L) {  /* 4.10.7 */
  return aux_listcore(L, 1);
}


#define ispathsep(c)      ((c) == '/' || (c) == '\\')
#define isdriveletter(s)  (isalpha(s[0]) && s[1] == ':')

#ifndef _WIN32
/* Derived from listing published in (superb !) `Programming in Lua` 2nd Ed., pp 271f., by Roberto Ierusalimschy */
#define checkdiriterator(L, n) (*(DIR **)luaL_checkudata(L, n, "diriterator"))

#define readnextentry(L,entry,d) { \
  set_errno(0); \
  entry = readdir(d); \
  if (entry == NULL) { \
    lua_pushnil(L); \
    return 1; \
  } \
}

static int diriterator (lua_State *L) {
  size_t l;
  int mode, getmode;
  struct dirent *entry;
  DIR *d = checkdiriterator(L, lua_upvalueindex(1));  /* *(DIR **) cast already done in macro */
  const char *path = lua_tolstring(L, lua_upvalueindex(2), &l);
  getmode = lua_tointeger(L, lua_upvalueindex(3));
  set_errno(0);
  /* 4.12.10 extension: if d is null, that is you have no permission but the folder exists,
     the iterator just returns `null'. */
  if (d && ((entry = readdir(d)) != NULL)) {
    char *fn;
    int rewind = 0;
    const char *objecttype = NULL;
    if (tools_streq(entry->d_name, ".")) {  /* 2.39.7 improvement */
      readnextentry(L, entry, d);
    }
    if (tools_streq(entry->d_name, "..")) {  /* 2.39.7 improvement */
      readnextentry(L, entry, d);
    }
    /* At least in Debian Stretch (x86 Raspian), if previous readdir() resulted in "..", entry->d_name may now be "." ! */
    if (tools_streq(entry->d_name, ".")) {  /* 2.39.7 improvement */
      readnextentry(L, entry, d);
    }
    if (l == 3 && isdriveletter(path) && (path[2] == '/' || path[2] == '\\'))  /* 2.39.8 improvement */
      fn = str_concat(path, entry->d_name, NULL);
    else
      fn = str_concat(path, "/", entry->d_name, NULL);
    if (!fn)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.iterate");
    if (fn[0] == '.' && fn[1] == '/') { fn += 2; rewind = 1; }
    lua_pushstring(L, fn);
    if (getmode == 1) {
      /* we must pass the `semi-full` path, relative to cwd, including dir name, to getfsobjecttype, otherwise it cannot find the files */
      getfsobjecttype(fn, &mode, 0);
      objecttype = fsobjtype[mode];
      if (objecttype)
        lua_pushstring(L, objecttype);
      else
        lua_pushfail(L);
    }
    if (rewind) fn -= 2;  /* else free() would crash Agena */
    xfree(fn);
    return 1 + (getmode == 1);
  }
  lua_pushnil(L);  /* explicitly return `null` */
  return 1;
}

static int dir_gc (lua_State *L) {
  DIR *d = checkdiriterator(L, 1);  /* *(DIR **) cast already done in macro */
  if (d) { set_errno(0); closedir(d); d = NULL; }
  return 0;
}

#else  /* Windows */

#define checkdiriterator(L,n) (Dir *)luaL_checkudata(L, n, "diriterator")

#define FindFirst   FindFirstFile
#define FindNext    FindNextFile

#define readnextentry(L,d) { \
  set_errno(0); \
  if (FindNext(d->fh, &d->fd) == 0) { \
    lua_pushnil(L); \
    return 1; \
  } \
}

typedef struct Dir {
  WIN32_FIND_DATA fd;
  HANDLE fh;
  int closefile;
  int firstcall;
} Dir;

static int diriterator (lua_State *L) {
  size_t l;
  int mode, getmode;
  BOOL more;
  Dir *d = checkdiriterator(L, lua_upvalueindex(1));  /* *(DIR **) cast already done in macro */
  const char *path = lua_tolstring(L, lua_upvalueindex(2), &l);
  getmode = lua_tointeger(L, lua_upvalueindex(3));  /* return type, too */
  set_errno(0);
  if (d->closefile == 0) {  /* system directory */
    more = 0;
  } else if (d->firstcall) {
    d->fh = FindFirst(path, &d->fd);  /* cannot use d->fd.cFileName ! */
    if (d->fh == INVALID_HANDLE_VALUE) {
     size_t s, l;
     s = utf8_to_latin9_len(path, &l);  /* s = Latin length, l = UTF-8 length */
     if (s != l)
       luaL_error(L, "Error in " LUA_QS ": UTF-8 " LUA_QS " does not exist.", "os.iterate", path);
     else
       luaL_error(L, "Error in " LUA_QS ": " LUA_QS " does not exist.", "os.iterate", path);
    }
    more = 1;
    d->firstcall = 0;
  } else {
    more = FindNext(d->fh, &d->fd);
  }
  if (more) {
    char *fn;
    int isdrive, dotslash;
    Charbuf buf;  /* 2.39.12 */
    charbuf_init(L, &buf, 0, PATH_MAX < 513 ? PATH_MAX : 512, 1);  /* 2.39.12/13 */
    const char *objecttype = NULL;
    if (tools_streq(d->fd.cFileName, ".")) {  /* 2.39.7 improvement */
      readnextentry(L, d);
    }
    if (tools_streq(d->fd.cFileName, "..")) {  /* 2.39.7 improvement, don't combine with check for "." ! */
      readnextentry(L, d);
    }
    /* At least in Debian Stretch (x86 Raspian), if previous readdir() resulted in "..", entry->d_name may now be "." ! */
    if (tools_streq(d->fd.cFileName, ".")) {  /* 2.39.7 improvement */
      readnextentry(L, d);
    }
    isdrive = (l == 4 && isdriveletter(path) && ispathsep(path[2]) && path[3] == '*');
    charbuf_append(L, &buf, path, l - (2 - isdrive));
    if (!isdrive)
      charbuf_append(L, &buf, "/", 1);
    charbuf_append(L, &buf, d->fd.cFileName, tools_strlen(d->fd.cFileName));
    charbuf_finish(&buf);
    fn = buf.data;
    dotslash = fn[0] == '.' && fn[1] == '/';
    lua_pushlstring(L, fn + 2*dotslash, charbuf_size(&buf) - 2*dotslash);
    if (getmode == 1) {
      /* we must pass the `semi-full` path, relative to cwd, including dir name, to getfsobjecttype, otherwise it cannot find the files */
      getfsobjecttype(fn, &mode, 0);
      objecttype = fsobjtype[mode];
      if (objecttype)
        lua_pushstring(L, objecttype);
      else
        lua_pushfail(L);
    }
    charbuf_free(&buf);
    return 1 + (getmode == 1);
  }
  lua_pushnil(L);  /* explicitly return `null` */
  return 1;
}

static int dir_gc (lua_State *L) {
  Dir *d = checkdiriterator(L, 1);  /* *(DIR **) cast already done in macro */
  if (d->closefile && d) { set_errno(0); FindClose(d->fh); }
  return 0;
}
#endif

static const struct luaL_Reg os_diriteratorlib [] = {  /* metamethods */
  {"__gc", dir_gc},  /* do not forget garbage collection */
  {NULL, NULL}
};

/* Creates a factory that when called traverses a directory and optionally also returns the type (file,
  directory, link, etc.). The first argument must be the path to the (sub)directory to be iterated - you can also use
  the '.' , '*' or '*.*' abbreviations instead. The iterator returns the name of the file, link, directory, etc.

  The optional boolean second argument `true` causes the iterator to additionally return the type, i.e.
  'FILE', 'DIR', 'LINK', 'CHARSPECFILE', 'BLOCKSPECFILE' or 'OTHER'. After the directory has been completely traversed,
  the function returns `null`.

  The iterator cannot recurse into a subdirectory, just create another factory instead.

  See also: os.list, os.listcore. */

#ifndef _WIN32
static int os_iterate (lua_State *L) {  /* 2.17.1 */
  size_t l;
  const char *path = luaL_optlstring(L, 1, ".", &l);
  int getmode = agnL_optboolean(L, 2, 0);
  DIR **d = (DIR **)lua_newuserdata(L, sizeof(DIR *));
  if (tools_streq(path, "*") || tools_streq(path, "*.*")) path = ".";  /* opendir would not work otherwise */
  /* drive letter without trailing slash ? -> we must add a slash to avoid confusing opendir, 2.39.8 */
  char *path0 = (l == 2 && isdriveletter(path)) ? str_concat(path, "/", NULL) : tools_strndup(path, l);
  set_errno(0);  /* 2.39.8 reset */
  *d = opendir(path0);
  if (*d == NULL) {
    /* 4.12.10 extension: avoid error if directory exists but permissions to access it are missing */
    int mm, hnd;
    hnd = 0;
    getfsobjecttype(path0, &mm, hnd);
    if (mm == FS_ERROR) {  /* directory does not exist */
      xfree(path0);
      luaL_error(L, "Error in " LUA_QS ": " LUA_QS " does not exist.", "os.iterate", path);
    }
  }
  luaL_checkstack(L, 3, "not enough stack space");  /* 3.18.4 fix */
  lua_setmetatabletoobject(L, -1, "diriterator", 0);
  lua_pushstring(L, path0);
  lua_pushinteger(L, getmode);
  lua_pushcclosure(L, diriterator, 3);
  xfree(path0);
  return 1;
}
#else  /* Windows */
static int os_iterate (lua_State *L) {  /* 2.17.1 */
  size_t l;
  char *path0;
  const char *path = luaL_optlstring(L, 1, ".", &l);
  int getmode = agnL_optboolean(L, 2, 0);
  Dir *d = (Dir *)lua_newuserdata(L, sizeof(Dir));
  tools_bzero(&d->fd, sizeof(WIN32_FIND_DATA));
  d->firstcall = 1;
  d->closefile = !issysdir(path);
  if (tools_streq(path, "*") || tools_streq(path, "*.*")) path = ".";  /* opendir would not work otherwise */
  path0 = str_concat(path, (l == 3 && isdriveletter(path) && (path[2] == '/' || path[2] == '\\')) ? "*" : "/*", NULL);
  if (d->closefile) {  /* no system directory ? -> iterate, else push null */
    /* drive letter without trailing slash ? -> we must add a slash to avoid confusing opendir, 2.39.8 */
    int en;
    set_errno(0);  /* 2.39.8 reset */
    /* check whether reading the directory would succeed; although available in Windows XP and later, FindFirstFile also works in W2K */
    d->fh = FindFirst(path0, &d->fd);
    d->closefile = 1;
    en = GetLastError();
    if (d->fh == INVALID_HANDLE_VALUE) {  /* validate path */
      xfree(path0);
      luaL_error(L, "Error in " LUA_QS ": " LUA_QS " does not exist (%s).", "os.iterate", path, my_ioerror(en));
    }
    FindClose(d->fh);  /* 2.39.9 fix; we open the directory again with the very first call to the factory */
  }
  luaL_checkstack(L, 3, "not enough stack space");  /* 3.18.4 fix */
  lua_setmetatabletoobject(L, -1, "diriterator", 0);
  lua_pushstring(L, path0);
  lua_pushinteger(L, getmode);
  lua_pushcclosure(L, diriterator, 3);
  xfree(path0);
  return 1;
}
#endif


#define setdate(what,seconds) { \
  lua_pushstring(L, what); \
  lua_createtable(L, 6, 0); \
  stm = localtime64(&seconds); \
  if (stm != NULL) { \
    agn_setinumber(L, -1, 1, stm->tm_year + 1900); \
    agn_setinumber(L, -1, 2, stm->tm_mon + 1); \
    agn_setinumber(L, -1, 3, stm->tm_mday); \
    agn_setinumber(L, -1, 4, stm->tm_hour); \
    agn_setinumber(L, -1, 5, stm->tm_min); \
    agn_setinumber(L, -1, 6, stm->tm_sec); \
    lua_settable(L, -3); \
  } \
}

static int os_fstat (lua_State *L) {  /* 0.12.2, October 11, 2008; modified 0.26.0, August 06, 2009; patched 0.31.3, extended 2.12.0 RC 4 */
  struct stat entry;  /* MinGW does not know stat64 */
  const char *fn;
  mode_t mode;
  struct TM *stm;
  Time64_T seconds;
  int mm, hnd;
  char bits[18] = "----------:-----\0";
#ifdef _WIN32
  DWORD winmodet, compressedsize, shortlength, binarytype;
#elif defined(__OS2__)
  static FILESTATUS3 fstat, *fs;
  APIRET rc;
  uint32_t hx, lx;
#endif
  if (luaL_isudata(L, 1, AGN_FILEHANDLE) || luaL_isudata(L, 1, LUA_FILEHANDLE)) {
#ifdef __OS2__
    luaL_error(L, "Error in " LUA_QS ": file handles are not supported on this platform.", "os.fstat");
#endif
    hnd = agn_tofileno(L, 1, 1); fn = NULL;
    if (hnd < 3) luaL_error(L, "Error in " LUA_QS ": file handle is invalid.", "os.fstat");
  } else if (agn_isstring(L, 1)) {
    fn = luaL_checkstring(L, 1); hnd = 0;
  } else {
    fn = NULL, hnd = -1;
    luaL_error(L, "Error in " LUA_QS ": expected a binio file handle or file name, a string.", "os.fstat");
  }
  entry = getfsobjecttype(fn, &mm, hnd);  /* shortened 2.17.1, 2.37.5 */
  if (mm != FS_ERROR) {
    luaL_checkstack(L, 2, "not enough stack space");  /* 3.5.5 */
    lua_newtable(L);
    mode = entry.st_mode;
    lua_pushstring(L, "mode");
    if (mm == FS_DIR) {
      bits[0] = 'd';
#if defined(_WIN32) || defined(__OS2__) || defined (__DJGPP__)
      bits[11] = 'd';
#endif
    }
    if (mm == FS_LINK)
      bits[0] = 'l';
    lua_pushstring(L, fsobjtype[mm]);
    lua_settable(L, -3);
    lua_pushstring(L, "length");
#ifdef _WIN32
    if (hnd != 0)
      lua_pushnumber(L, _filelengthi64(hnd));
    else
      lua_pushnumber(L, entry.st_size);
#else
    lua_pushnumber(L, entry.st_size);
#endif
    lua_settable(L, -3);
    seconds = entry.st_mtime;  /* pre-0.31.3 */
    setdate("date", seconds);
    seconds = entry.st_atime;  /* 2.12.0 RC 4 */
    setdate("lastaccess", seconds);
    seconds = entry.st_ctime;  /* 2.12.0 RC 4 */
    setdate("attribchange", seconds);
    /* 0.25.1 file attributes additions */
    /* determine owner rights */
    lua_pushstring(L, "user");
    lua_createtable(L, 0, 3);
    lua_rawsetstringboolean(L, -1, "read",    mode & S_IRUSR);
    lua_rawsetstringboolean(L, -1, "write",   mode & S_IWUSR);
    lua_rawsetstringboolean(L, -1, "execute", mode & S_IXUSR);
    if (mode & S_IRUSR) bits[1] = 'r';
    if (mode & S_IWUSR) bits[2] = 'w';
    if (mode & S_IXUSR) bits[3] = 'x';
#ifdef __OS2__
    rc = DosQueryPathInfo((PCSZ)fn, FIL_STANDARD, &fstat, sizeof(fstat));
    if (rc == 0) {
      fs = &fstat;  /* do not change this, otherwise Agena will crash */
      unsigned int attribs = fs->attrFile;
      lua_rawsetstringboolean(L, -1, "hidden",    attribs & FILE_HIDDEN);
      lua_rawsetstringboolean(L, -1, "readonly",  attribs & FILE_READONLY);
      lua_rawsetstringboolean(L, -1, "system",    issysdir(fn) || (attribs & FILE_SYSTEM));  /* 2.39.8 fix */
      lua_rawsetstringboolean(L, -1, "archived",  attribs & FILE_ARCHIVED);
      lua_rawsetstringboolean(L, -1, "directory", attribs & FILE_DIRECTORY);
      if (attribs & FILE_DIRECTORY) bits[11] = 'd';
      if (attribs & FILE_READONLY)  bits[12] = 'r';
      if (attribs & FILE_HIDDEN)    bits[13] = 'h';
      if (attribs & FILE_ARCHIVED)  bits[14] = 'a';
      if (attribs & FILE_SYSTEM)    bits[15] = 's';
    }
#elif defined(_WIN32)
    winmodet = GetFileAttributes(fn);
    lua_rawsetstringboolean(L, -1, "hidden",   winmodet & FILE_ATTRIBUTE_HIDDEN);
    lua_rawsetstringboolean(L, -1, "readonly", winmodet & FILE_ATTRIBUTE_READONLY);
    lua_rawsetstringboolean(L, -1, "system",   issysdir(fn) || (winmodet & FILE_ATTRIBUTE_SYSTEM));  /* 2.39.8 fix */
    lua_rawsetstringboolean(L, -1, "archived", winmodet & FILE_ATTRIBUTE_ARCHIVE);
    if (winmodet & FILE_ATTRIBUTE_READONLY) bits[12] = 'r';
    if (winmodet & FILE_ATTRIBUTE_HIDDEN)   bits[13] = 'h';
    if (winmodet & FILE_ATTRIBUTE_ARCHIVE)  bits[14] = 'a';
    if (winmodet & FILE_ATTRIBUTE_SYSTEM)   bits[15] = 's';
#endif
    lua_settable(L, -3);
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
    /* determine group rights */
    lua_pushstring(L, "group");
    lua_createtable(L, 0, 3);
    lua_rawsetstringboolean(L, -1, "read",    mode & S_IRGRP);
    lua_rawsetstringboolean(L, -1, "write",   mode & S_IWGRP);
    lua_rawsetstringboolean(L, -1, "execute", mode & S_IXGRP);
    lua_settable(L, -3);
    if (mode & S_IRGRP) bits[4] = 'r';
    if (mode & S_IWGRP) bits[5] = 'w';
    if (mode & S_IXGRP) bits[6] = 'x';
    /* determine other users' rights */
    lua_pushstring(L, "other");
    lua_createtable(L, 0, 3);
    lua_rawsetstringboolean(L, -1, "read",    mode & S_IROTH);
    lua_rawsetstringboolean(L, -1, "write",   mode & S_IWOTH);
    lua_rawsetstringboolean(L, -1, "execute", mode & S_IXOTH);
    lua_settable(L, -3);
    if (mode & S_IROTH) bits[7] = 'r';
    if (mode & S_IWOTH) bits[8] = 'w';
    if (mode & S_IXOTH) bits[9] = 'x';
#endif
    /* 0.26.0, push file mode (mode_t) and permission `bits` */
    lua_rawsetstringnumber(L, -1, "perms", mode);
    lua_rawsetstringstring(L, -1, "bits", bits);
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__)
    lua_rawsetstringnumber(L, -1, "blocks",    entry.st_blocks);
    lua_rawsetstringnumber(L, -1, "blocksize", entry.st_blksize);
    lua_rawsetstringnumber(L, -1, "gid",       entry.st_gid);
    lua_rawsetstringnumber(L, -1, "uid",       entry.st_uid);
#endif
#if defined(__DJGPP__)
    lua_rawsetstringnumber(L, -1, "blocksize", entry.st_blksize);  /* 2.37.0 */
#endif
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__DJGPP__)
    luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
    agn_createpairnumbers(L, 0, entry.st_ino);  /* changed 2.35.4 */
    lua_setfield(L, -2, "inode");
    lua_rawsetstringnumber(L, -1, "device", entry.st_dev);
#elif defined(__OS2__)  /* fixed 2.35.4 */
    hx = tools_uint64touint32(entry.st_ino, &lx);  /* 2.35.5 fix */
    luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
    agn_createpairnumbers(L, hx, lx);
    lua_setfield(L, -2, "inode");
    lua_rawsetstringnumber(L, -1, "device", entry.st_dev);
#elif defined(_WIN32)
    /* taken from: https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/winbase/io/extendedfileapis/ExtendedFileAPIs.cpp, 2.34.10 */
    if (hnd == 0 && tools_getwinver() >= MS_WINXP) {
      HANDLE hFile;
      hFile = CreateFileA(fn, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
      if (hFile != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION basicInfo;
        if (GetFileInformationByHandle(hFile, &basicInfo)) {
          luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
          lua_rawsetstringnumber(L, -1, "device", basicInfo.dwVolumeSerialNumber);
          agn_createpairnumbers(L, basicInfo.nFileIndexHigh, basicInfo.nFileIndexLow);
          lua_setfield(L, -2, "inode");
        }
        CloseHandle(hFile);
      }
    }
#endif
#ifdef _WIN32
    /* 2.17.0: see: https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getshortpathnamew */
    compressedsize = GetCompressedFileSize(fn, NULL);
    if (compressedsize != INVALID_FILE_SIZE)  /* != 0xFFFFFFFF */
      lua_rawsetstringnumber(L, -1, "compressed", compressedsize);
    shortlength = GetShortPathName(fn, NULL, 0);
    if (shortlength != 0) {
      TCHAR *buffer = tools_stralloc(shortlength);
      shortlength = GetShortPathName(fn, buffer, shortlength);
      if (shortlength != 0)
        lua_rawsetstringstring(L, -1, "dosname", buffer);
      xfree(buffer);
    }
    /* https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getbinarytypea, 2.27.1 */
    if (GetBinaryType(fn, &binarytype)) {
      switch(binarytype) {
        case SCS_WOW_BINARY:
          lua_rawsetstringstring(L, -1, "binarytype", "Win16");
          break;
        case SCS_32BIT_BINARY:
          lua_rawsetstringstring(L, -1, "binarytype", "Win32");
          break;
        case SCS_64BIT_BINARY:
          lua_rawsetstringstring(L, -1, "binarytype", "Win64");
          break;
        case SCS_DOS_BINARY:
          lua_rawsetstringstring(L, -1, "binarytype", "DOS");
          break;
        case SCS_OS216_BINARY:
          lua_rawsetstringstring(L, -1, "binarytype", "OS2/16");
          break;
        case SCS_PIF_BINARY:
          lua_rawsetstringstring(L, -1, "binarytype", "PIF");
          break;
        case SCS_POSIX_BINARY:
          lua_rawsetstringstring(L, -1, "binarytype", "POSIX");
          break;
        default:
          lua_rawsetstringstring(L, -1, "binarytype", "unknown");
          break;
      }
    }
#endif
  } else
    lua_pushfail(L);  /* file does not exist */
  return 1;
}


static int os_exists (lua_State *L) {
  lua_pushboolean(L, tools_exists(luaL_checkstring(L, 1)));  /* 2.16.11 change */
  return 1;
}


/* The following routines were taken from the lposix.c file in the
*  POSIX library for Lua 5.0. Based on original by Claudio Terra for Lua 3.x.
*  Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br>
*  05 Nov 2003 22:09:10
*/

/** mkdir(path), modified for Agena; extended in 0.13.3 to support OS/2; improved 2.16.11 */
static int os_mkdir (lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  int noerror = agnL_optboolean(L, 2, 0);  /* 2.16.11 */
  if (noerror && tools_exists(path)) {
    lua_pushfail(L);
    return 1;
  }
#ifdef _WIN32
  return os_pushresult(L, mkdir(path), path, "os.mkdir", 1);
#elif defined(__unix__) || defined(__OS2__) || defined(__APPLE__) || defined(__HAIKU__)
  return os_pushresult(L, mkdir(path, 0777), path, "os.mkdir", 1);
#else
  lua_pushfail(L);
  return 1;
#endif
}


/* chdir(path [, any]), includes ex-os.curdir code:
   print working directory; July 07, 2008; modified 0.26.0, 04.08.2009 (PATH_MAX),
   extended Agena 1.1, tuned 1.6.0, extended 2.21.11
   if any is given, do not issue an error but return `fail` */
static int os_chdir (lua_State *L) {
  int nargs = lua_gettop(L);
  if (agn_isstring(L, 1)) {
    const char *path = agn_tostring(L, 1);  /* Agena 1.6.0 */
    if (nargs == 2 && !tools_exists(path))  /* 2.21.11 extension */
      lua_pushfalse(L);
    else
      return os_pushresult(L, chdir(path), path, "os.chdir", nargs == 1);  /* 2.21.11 extension */
  } else if (lua_isnil(L, 1) || nargs == 0) {  /* Agena 1.1.0 */
    char *buffer = agn_stralloc(L, PATH_MAX, "os.chdir", NULL);  /* 2.16.5 change */
    if (getcwd(buffer, PATH_MAX) == NULL) { /* 1.6.4 to avoid compiler warnings and unfreed memory */
      xfree(buffer);
      luaL_error(L, "Error in " LUA_QS ": internal error.", "os.chdir");  /* Agena 1.2.0 */
    }
    else {
      str_charreplace(buffer, '\\', '/', 1);
      lua_pushstring(L, buffer);
    }
    xfree(buffer);
  } else
    luaL_error(L, "Error in " LUA_QS ": expected a string or null.", "os.chdir");
  return 1;
}


static int os_curdir (lua_State *L) {  /* re-introduced with 2.17.3 */
  char *buffer = agn_stralloc(L, PATH_MAX, "os.curdir", NULL);
  if (getcwd(buffer, PATH_MAX) == NULL) {
    xfree(buffer);
    luaL_error(L, "Error in " LUA_QS ": internal error.", "os.curdir");
  }
  else {
    str_charreplace(buffer, '\\', '/', 1);
    lua_pushstring(L, buffer);
  }
  xfree(buffer);
  return 1;
}


/* rmdir(path), improved 2.16.11 */
static int os_rmdir (lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  int noerror = agnL_optboolean(L, 2, 0);    /* 2.16.11 */
  if (!noerror && !tools_exists(path))
    luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": directory does not exist.", "os.rmdir", path);
  if (tools_exists(path))
    return os_pushresult(L, rmdir(path), path, "os.rmdir", 1);
  else {
    if (!noerror)
      luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": directory does not exist.", "os.rmdir", path);  /* Agena 1.2 */
    else
      lua_pushfail(L);
    return 1;
  }
}


/* beep: sound the loudspeaker with a given frequency (first arg, a posint) and
   a duration (second arg, in seconds, a float), April 01, 2007 */

#if defined(__OS2__)
#define MYINT    ULONG
#define BEEP     DosBeep
#define MYERROR  (!NO_ERROR)
#elif defined(_WIN32)
#define MYINT    int
#define BEEP     Beep
#define MYERROR  0
#endif

static int os_beep (lua_State *L) {
  int nargs;
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
  int i;
#endif
  lua_Number duration;
  nargs = lua_gettop(L);
#if defined(__OS2__) || defined(_WIN32)
  if (nargs == 0) {
    putc('\007', stderr);
    lua_pushnil(L);
  } else {  /* rewritten 2.17.3 */
    MYINT frequency = tools_clip(agn_checknumber(L, 1), 37, 32767);  /* range corrected from 0 to 37 Hz in 2.17.4 */
    duration = agn_checknumber(L, 2);  /* check changed again in 2.17.4 */
    if ((duration < 0) || (BEEP(frequency, (MYINT)(duration*1000)) == MYERROR))
      lua_pushfail(L);
    else
      lua_pushnil(L);
  }
#elif defined(__DJGPP__)
  if (nargs == 0) {
    putc('\007', stderr);
    lua_pushnil(L);
  } else {  /* new 2.17.3, may be DJGPP-specific */
    int frequency = tools_clip(agn_checknumber(L, 1), 0, 32767);
    duration = agn_checknonnegative(L, 2);
    sound(frequency);  /* sound and nosound return void */
    tools_wait(duration);
    nosound();
    lua_pushnil(L);
  }
#elif defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
  duration = 1;  /* 2.17.3 change */
  if (nargs == 2)
    duration = agn_checknonnegint(L, 2);
  for (i=0; i < duration; i++)
    putc('\007', stderr);
  lua_pushnil(L);
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* wait: wait for given seconds; March 31, 2007, improved 2.12.4, July 24, 2018 */
static int os_wait (lua_State *L) {  /* modified 2.4.0 & 2.12.4 */
  lua_Number secs = agn_checknumber(L, 1);  /* 3.17.3 */
  if (secs >= 0.0 && tools_wait(secs))
    lua_pushnil(L);
  else
    lua_pushfail(L);
  return 1;
}


static int os_pause (lua_State *L) {  /* 2.12.4 */
  const char *msg;
  lua_Number secs, waitperiod, t;
  int keypressed = 0;
  secs = agn_checknonnegative(L, 1);
  msg = agnL_optstring(L, 2, NULL);
  if (msg) printf("%s", msg);
  t = tools_time();  /* 2.39.1 change */
  waitperiod = 0.05;
  while (tools_time() - t < secs) {
    tools_wait(waitperiod);  /* none-busy wait */
    if (tools_anykey() == 1) { keypressed = 1; break; }
  }
  lua_pushnumber(L, keypressed ? tools_time() - t : secs);
  return 1;
}


/* CLOCKS_PER_SEC is set to 91 in DJGPP, and 100 in OS/2, so do not use it, but use the constant 1000; see for example:
   https://groups.google.com/g/comp.os.msdos.djgpp/c/kWboKeWzpPY */
/* Approximation of processor time used by Agena. To get number of seconds, divide by environ.kernel().clockspersec. */
static int os_clock (lua_State *L) {  /* 2.12.4 */
#ifdef __OS2__
  lua_pushnumber(L, (ULONGLONG)(1000*(tools_time() - agn_getstarttime(L))));  /* 2.39.1 change: count beyond 24.8 days */
#else
  lua_pushnumber(L, (uint64_t)(1000*(tools_time() - agn_getstarttime(L))));  /* 2.39.1 change: count beyond 24.8 days */
#endif
  return 1;
}


/* Novell DOS 7 = 'IBMPcDos', '6', '00', 'pc'
   OS/2 Warp 4 Fixpack 15 = 'OS/2', '1', '2.45', 'i386'
   Windows = 'Windows', etc. */

#ifdef __DJGPP__
#define PC_DOS   0x00
#define PTS_DOS 0x66
#define RX_DOS  0x5e
#define DR_DOS  0xee
#define NOVELL  0xef
#define FREEDOS 0xfd
#define MS_DOS  0xff

static char *tools_getdosname (int *oemno) {
  union REGS r;
  r.x.ax = 0x3000;
  intdos(&r, &r);
  *oemno = r.h.bh;
  switch (*oemno) {
    case PC_DOS:
      return "PC-DOS";
    case PTS_DOS:
      return "PTS-DOS";
    case RX_DOS:
      return "RxDOS";
    case DR_DOS:
      return "DR-DOS";
    case NOVELL:
      return "Novell";
    case FREEDOS:
      return "FreeDOS";
    case MS_DOS:
      return "MS-DOS";
    default:
      return NULL;
  }
}
#endif


#ifdef _WIN32
/* The code is based on NTLua 3.0, file src/ntlua.c, taken from http://ntlua.sourceforge.net/, 2.27.1 */

static HKEY KEYMAP[5] = {HKEY_CLASSES_ROOT, HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, HKEY_USERS, HKEY_CURRENT_CONFIG};

static int utlReadStringKey (int bkey, char* key_name, char* value_name, char* value) {
  HKEY key;
  HKEY base_key = KEYMAP[bkey];
  long unsigned int max_size = 1024;
  if (RegOpenKeyEx(base_key, key_name, 0, KEY_READ, &key) != ERROR_SUCCESS) return 0;
  if (RegQueryValueEx(key, value_name, NULL, NULL, (LPBYTE)value, &max_size) != ERROR_SUCCESS) {
    RegCloseKey(key);
    return 0;
  }
  RegCloseKey(key);
  return 1;
}

enum {KEY_CLASSES_ROOT, KEY_CURRENT_USER, KEY_LOCAL_MACHINE, KEY_USERS, KEY_CURRENT_CONFIG};

static char *utlWinverStr (const char *system, int major, int minor, int build, char *maint) {
  char *type;
  char szProductType[80] = { 0 };
  utlReadStringKey(KEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\ProductOptions", "ProductType", szProductType);
  if (lstrcmpi("WINNT", szProductType) == 0) {
    type = (major <= 4) ? "Workstation" : "Professional";
  } else {
    type = "Server";
    if (major == 5 && minor == 1)
      system = ".NET";
  }
  {
    char ver[180] = { 0 };  /* if you CHANGE this, change nulling below, as well; 2.34.11 MinGW GCC 9.2.0 migration patch */
    if (tools_streq(system, "11")) major = 10;  /* 2.39.1, show same number as in NT shell */
    if (tools_strlen(maint) == 0)  /* 2.31.2 fix */
      sprintf(ver, "Microsoft Windows %s %s [Version %d.%d.%d]", system, type, major, minor, build);
    else
      sprintf(ver, "Microsoft Windows %s %s [Version %d.%d.%d] %s", system, type, major, minor, build, maint);
    ver[179] = 0;
    return tools_strdup(ver);  /* FREE ME ! */
  }
}
#endif

static int os_system (lua_State *L) {
#ifdef _WIN32
  /* grep "getWindowsVersion (struct WinVer \*winver)" * when extending the list of Windows versions

   dwMajorVersion
   Major version number of the operating system. This member can be one of the following values. Value Meaning
   4 Windows NT 4.0, Windows Me, Windows 98, or Windows 95.
   5 Windows Server 2003, Windows XP, or Windows 2000
   6 Windows Vista, Windows Server 2008, Windows 7, 8, 8.1

   dwMinorVersion
   Minor version number of the operating system. This member can be one of the following values. Value Meaning
   0 Windows 2000, Windows NT 4.0, Windows 95, or Windows Vista
   1 Windows XP, Windows 2008 server
   2 Windows Server 2003, Windows 7, Windows Server 2012
   3 Windows Server 2012 RC 2, Windows 8, 8.1
   10 Windows 98
   90 Windows Me

   dwPlatformId
   Operating system platform. This member can be one of the following values. Value Meaning
   VER_PLATFORM_WIN32_NT
   2 Windows Server 2003, Windows XP, Windows 2000, or Windows NT.
   VER_PLATFORM_WIN32_WINDOWS
   1 Windows Me, Windows 98, or Windows 95.
   VER_PLATFORM_WIN32s
   0 Win32s on Windows 3.1. */

  int winver;
  struct WinVer winversion;
  typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
  const char *versionnumber;
  char *versionstr = NULL;
  static const char *versions[] = {  /* extended 0.13.4, switched "ME" and "2000" in 1.9.3 */
    "unknown", "32s", "95", "NT 4.0", "98", "ME", "2000", "XP", "Server 2003", "Vista", "Server 2008",
    "7", "8", "Server 2012", "8.1", "Server 2012 R2", "10", "10 Server", "post-10"
    /* if you add future Windows releases, extend getWindowsVersion above, as well */
  };
  winver = getWindowsVersion(&winversion);
  /* for Win11, see https://stackoverflow.com/questions/69038560/detect-windows-11-with-net-framework-or-windows-api */
  versionnumber = ((winver == 16 || winver == 17) && winversion.BuildNumber >= 22000) ? "11" : versions[winver];  /* 2.39.1 */
  lua_newtable(L);
  lua_rawsetistring(L, -1, 1, "Windows");
  lua_rawsetistring(L, -1, 2, versionnumber);
  agn_setinumber(L, -1, 3, winversion.BuildNumber);
  agn_setinumber(L, -1, 4, winversion.PlatformId);
  agn_setinumber(L, -1, 5, winversion.MajorVersion);  /* added 0.13.4 */
  agn_setinumber(L, -1, 6, winversion.MinorVersion);  /* added 0.13.4 */
  agn_setinumber(L, -1, 7, winversion.ProductType);   /* added 0.13.4 */
  lua_rawsetistring(L, -1, 8, tools_streq(winversion.Maintenance, "") ? "no maintenance info" : winversion.Maintenance);  /* added 2.27.1 */
  versionstr =
    utlWinverStr(versionnumber, winversion.MajorVersion, winversion.MinorVersion, winversion.BuildNumber, winversion.Maintenance);
  lua_rawsetistring(L, -1, 9, versionstr ? versionstr : "unknown");  /* added 2.27.1 */
  xfree(versionstr);
  /* 2.16.6, see: https://stackoverflow.com/questions/7011071/detect-32-bit-or-64-bit-of-windows, answered by Survarium
     IsWow64Process is not available on all supported versions of Windows. Use GetModuleHandle to get a handle
     to the DLL that contains the function and GetProcAddress to get a pointer to the function if available. */
  LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
  if (fnIsWow64Process) {
    BOOL isWow64;
    if (fnIsWow64Process(GetCurrentProcess(), &isWow64)) {
      lua_rawsetstringnumber(L, -1, "bits", isWow64 ? 64 : 32);
    }
  }
#elif defined(__unix__) || defined(__OS2__) || defined(__APPLE__) || defined(__HAIKU__)
  /* applies to DJGPP compiled DOS code as well; on Apple returns Darwin version number */
  struct utsname info;
  if (uname(&info) != -1) {
#ifdef __OS2__
    APIRET result;
    ULONG a[3];
#elif defined(__DJGPP__)
    char *dosname;
    int oemno;
    dosname = tools_getdosname(&oemno);
#endif
    lua_newtable(L);
    lua_rawsetistring(L, -1, 1, info.sysname);
#ifdef __DJGPP__
    lua_rawsetistring(L, -1, 1, (dosname != NULL) ? dosname : info.sysname);
#else
    lua_rawsetistring(L, -1, 1, info.sysname);
#endif
    lua_rawsetistring(L, -1, 2, info.release);
    lua_rawsetistring(L, -1, 3, info.version);
    lua_rawsetistring(L, -1, 4, info.machine);
#ifdef __OS2__  /* 2.3.0 RC 2 eCS extension, QSV_VERSION_MINOR does not return different info */
    result = DosQuerySysInfo(QSV_VERSION_MAJOR, QSV_VERSION_REVISION, &a[0], sizeof(a));
    if (result == 0) {
      agn_setinumber(L, -1, 5, a[0]);
      agn_setinumber(L, -1, 6, a[1]);
      agn_setinumber(L, -1, 7, a[2]);
    }
#elif defined(__linux__)
    /* long int wordBits;
    wordBits = sysconf(_SC_WORD_BIT); Inquire about the number of bits in a variable of a register word. */
#ifdef IS32BIT
    lua_Number wordBits = 32;  /* 4.9.1 fix */
#elif defined(IS64BIT)
    lua_Number wordBits = 64;
#else
    lua_Number wordBits = AGN_NAN;
#endif
    lua_rawsetstringnumber(L, -1, "bits", wordBits);
    /* if (!(wordBits == -1 && errno == EINVAL)) {
      lua_rawsetstringnumber(L, -1, "bits", wordBits);
    } */
#endif
   } else  /* uname failure */
     lua_pushfail(L);
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* This function is an alternative to `os.system` and returns the Windows major release (a float). If any argument is given, it
   also returns the service pack major (an integer) and minor version (an integer), whether the operating system is a workstation
   (`true`) or server (`false`) and the build number (an integer), in this order.
   On all other platforms other than Windows, the function returns `undefined`, which, if used in a relation, always evaluates to
   `false`. 2.16.10 */
static int os_winver (lua_State *L) {
#ifdef _WIN32
  int minVer, build, platform, product, gotargs;
  double winver;
  uint8_t SPmaj, SPmin;
  getSysOpType(&minVer, &build, &platform, &product, &winver, &SPmaj, &SPmin);
  gotargs = lua_gettop(L) != 0;
  lua_pushnumber(L, winver);
  if (gotargs) {
    lua_pushinteger(L, SPmaj);
    lua_pushinteger(L, SPmin);
    lua_pushboolean(L, product == VER_NT_WORKSTATION);
    lua_pushinteger(L, build);
  }
  return 1 + 4*(gotargs);
#else
  lua_pushundefined(L);
  return 1;
#endif
}


/* Returns the IP address for a given domain. If no domain is given, the function tries to retrieve the IP of the host. 2.39.1 */
static int os_getip (lua_State *L) {
  int flag = 1;
  char *str = (char *)agnL_optstring(L, 1, NULL);
  char *computername = (str == NULL) ? tools_computername() : str;
#if defined(_WIN32)
  struct WinVer winversion;
  luaL_checkstack(L, 2, "not enough stack space");
  if (getWindowsVersion(&winversion) == MS_WIN2K) {  /* 7.5.17, restoration of functionality for W2K, patched */
    /* Taken from https://ntlua.sourceforge.net/, file tcp-ip.c, written by Antonio Escaño Scuri, MIT licence */
    struct hostent *phe;
    char ip[20];
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(1, 1);
    int pushfail = 1;
    if (WSAStartup(wVersionRequested, &wsaData) == 0) {
      phe = gethostbyname(computername);
      if (phe && phe->h_addr_list[0]) {
        /* CRITICAL FIX: Check if host_entry AND the first address entry exist */
        sprintf(ip, "%u.%u.%u.%u",
               (unsigned char) phe->h_addr_list[0][0],
               (unsigned char) phe->h_addr_list[0][1],
               (unsigned char) phe->h_addr_list[0][2],
               (unsigned char) phe->h_addr_list[0][3]);
        lua_pushstring(L, ip);
        lua_pushfail(L);
        pushfail = 0;
      }
      WSACleanup();
    }
    if (pushfail) {
      lua_pushfail(L);
      lua_pushfail(L);
    }
  } else {
    struct addrinfo hints = { 0 }, *res = NULL, *ptr = NULL;
    char ip_str[INET6_ADDRSTRLEN];
    char *ipv4_addr = NULL;
    char *ipv6_addr = NULL;
    init_network_api(L, "os.getip");
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
      hints.ai_family = AF_UNSPEC;    /* Support both IPv4 and IPv6 */
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_protocol = IPPROTO_TCP;
      if (safe_getaddrinfo(computername, NULL, &hints, &res) == 0) {
        for (ptr=res; ptr != NULL; ptr = ptr->ai_next) {  /* 7.5.16 fix for loopback IPv6 addresses */
          DWORD len = sizeof(ip_str);
          if (WSAAddressToStringA(ptr->ai_addr, (DWORD)ptr->ai_addrlen, NULL, ip_str, &len) == 0) {
            /* Only strip if we find a port (a colon that is NOT part of the IPv6 structure) */
            /* A quick check: If it contains a bracket, it likely has a port */
            char *bracket = strchr(ip_str, ']');
            if (bracket) {
              char *port = strrchr(bracket, ':');
              if (port) *port = '\0'; /* Remove port */
              /* Remove brackets */
              memmove(ip_str, ip_str + 1, strlen(ip_str) - 1);
              ip_str[strlen(ip_str) - 1] = '\0';
            }
            /* Capture the first instance */
            if (ptr->ai_family == AF_INET && !ipv4_addr) {
              ipv4_addr = strdup(ip_str);
              if (!ipv4_addr) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.getip");  /* 7.7.7 fix */
            } else if (ptr->ai_family == AF_INET6 && !ipv6_addr) {
              ipv6_addr = strdup(ip_str);
              if (!ipv6_addr) {
                luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.getip");  /* 7.7.7 fix */
              }
            }
          }
        }
        safe_freeaddrinfo(res);
      }
      WSACleanup();
    }
    if (ipv4_addr) { lua_pushstring(L, ipv4_addr); free(ipv4_addr); } else { lua_pushfail(L); }
    if (ipv6_addr) { lua_pushstring(L, ipv6_addr); free(ipv6_addr); } else { lua_pushfail(L); }
  }
#elif !defined(__DJGPP__) && (defined(__unix__) || defined(__OS2__) || defined(__APPLE__)) /* 7.5.16 */
  struct addrinfo hints = { 0 }, *res = NULL, *ptr = NULL;
  char *ipv4_addr = NULL;
  char *ipv6_addr = NULL;
  /* hints.ai_family = AF_UNSPEC allows both IPv4 and IPv6 */
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
getip_tryagain:
  if (safe_getaddrinfo(computername, NULL, &hints, &res) == 0) {
    for (ptr = res; ptr != NULL; ptr = ptr->ai_next) {
      char host[NI_MAXHOST];
      if (safe_getnameinfo(ptr->ai_addr, ptr->ai_addrlen, host, sizeof(host),
                      NULL, 0, NI_NUMERICHOST) == 0) {
        if (ptr->ai_family == AF_INET && !ipv4_addr) {
          ipv4_addr = strdup(host);
          if (!ipv4_addr) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.getip");
        } else if (ptr->ai_family == AF_INET6 && !ipv6_addr) {
          ipv6_addr = strdup(host);
          if (!ipv6_addr) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.getip");
        }
      }
      /* Optimization: Stop once we have both address types */
      if (ipv4_addr && ipv6_addr) break;
    }
    safe_freeaddrinfo(res);
  } else if (flag && !str) {  /* 7.5.16 extension, try localhost if default hostname is not resolvable */
    xfree(computername);
    computername = strdup("localhost");
    if (!computername) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.getip");
    flag = 0;
    goto getip_tryagain;
  }
  luaL_checkstack(L, 2, "not enough stack space");
  if (ipv4_addr) { lua_pushstring(L, ipv4_addr); free(ipv4_addr); } else { lua_pushfail(L); }
  if (ipv6_addr) { lua_pushstring(L, ipv6_addr); free(ipv6_addr); } else { lua_pushfail(L); }
#elif 0 && !defined(__DJGPP__) && (defined(__APPLE__))
  struct hostent *host_entry = gethostbyname(computername);
  /* CRITICAL FIX: Check if host_entry AND the first address entry exist */
  if (host_entry != NULL && host_entry->h_addr_list[0] != NULL) {
    lua_pushstring(L, inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])));
  } else {
    lua_pushfail(L);
  }
  lua_pushfail(L);
#else
  lua_pushnil(L);
  lua_pushnil(L);
#endif
  if (!str || !flag) { xfree(computername); }  /* 7.5.16 fix */
  return 2;
}

/* Retrieves network adapter information for the local host, for IPv4 addresses only; code taken from:
   https://gist.github.com/hash3liZer/4991c20ef3af15c69158b68723690a50, published by hash3liZer
   https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersinfo */
static int os_getadapter (lua_State *L) {
#if defined(_WIN32)
  char *mac_addr;
  struct WinVer winversion;
  PIP_ADAPTER_INFO AdapterInfo;
  DWORD dwBufLen = sizeof(IP_ADAPTER_INFO);
  if (getWindowsVersion(&winversion) < MS_WIN2K) {  /* does not work in NT 4.0 or earlier */
    lua_pushfail(L);
    return 1;
  }
  AdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
  if (AdapterInfo == NULL) {
    lua_pushfail(L);
    return 1;
  }
  /* Make an initial call to GetAdaptersInfo to get the necessary size into the dwBufLen variable */
  if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == ERROR_BUFFER_OVERFLOW) {
    xfree(AdapterInfo);
    AdapterInfo = (IP_ADAPTER_INFO *)malloc(dwBufLen);
    if (AdapterInfo == NULL) {
      lua_pushfail(L);
      return 1;
    }
  }
  if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == NO_ERROR) {
    int c = 0;
    PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;  /* Contains pointer to current adapter info */
    mac_addr = (char*)malloc(18);
    if (mac_addr == NULL) {  /* 4.11.5 fix */
      xfree(AdapterInfo);
      lua_pushfail(L);
      return 1;
    }
    lua_createtable(L, 8, 0);
    do {
      luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
      lua_createtable(L, 0, 9);
      if (pAdapterInfo->AddressLength == 6) {  /* 7.5.15 improvement */
        sprintf(mac_addr, "%02X:%02X:%02X:%02X:%02X:%02X",
          pAdapterInfo->Address[0], pAdapterInfo->Address[1],
          pAdapterInfo->Address[2], pAdapterInfo->Address[3],
          pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
        lua_rawsetstringstring(L, -1, "mac", mac_addr);
      }
      lua_rawsetstringnumber(L, -1, "comboindex", pAdapterInfo->Index);
      lua_rawsetstringstring(L, -1, "adapter", pAdapterInfo->AdapterName);
      lua_rawsetstringstring(L, -1, "desc", pAdapterInfo->Description);
      const char* ip = pAdapterInfo->IpAddressList.IpAddress.String;
      lua_rawsetstringstring(L, -1, "ip", (ip && ip[0] != '\0') ? ip : "0.0.0.0");  /* 7.5.15 security fix */
      lua_rawsetstringnumber(L, -1, "index", pAdapterInfo->Index);
      switch (pAdapterInfo->Type) {
        case MIB_IF_TYPE_OTHER:
          lua_rawsetstringstring(L, -1, "type", "other");
          break;
        case MIB_IF_TYPE_ETHERNET:
          lua_rawsetstringstring(L, -1, "type", "ethernet");
          break;
        case MIB_IF_TYPE_TOKENRING:
          lua_rawsetstringstring(L, -1, "type", "token ring");
          break;
        case MIB_IF_TYPE_FDDI:
          lua_rawsetstringstring(L, -1, "type", "FDDI");
          break;
        case MIB_IF_TYPE_PPP:
          lua_rawsetstringstring(L, -1, "type", "PPP");
          break;
        case MIB_IF_TYPE_LOOPBACK:
          lua_rawsetstringstring(L, -1, "type", "lookback");
          break;
        case MIB_IF_TYPE_SLIP:
          lua_rawsetstringstring(L, -1, "type", "slip");
          break;
        default:
          lua_rawsetistring(L, -1, 7, "unknown");
      }
      lua_rawsetstringstring(L, -1, "mask", pAdapterInfo->IpAddressList.IpMask.String);
      lua_rawsetstringstring(L, -1, "gateway", pAdapterInfo->GatewayList.IpAddress.String);
      lua_rawsetstringboolean(L, -1, "dhcp", pAdapterInfo->DhcpEnabled != 0);
      lua_rawsetstringboolean(L, -1, "wins", pAdapterInfo->HaveWins != 0);
      lua_rawseti(L, -2, ++c);
      /* printf("Address: %s, mac: %s\n", pAdapterInfo->IpAddressList.IpAddress.String, mac_addr); */
      pAdapterInfo = pAdapterInfo->Next;
    } while (pAdapterInfo);
    xfree(mac_addr);
  } else {
    lua_pushfail(L);
  }
  xfree(AdapterInfo);
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* retrieves MAC address for a given IP on the local host */
static int os_getmac (lua_State *L) {  /* 2.39.3 */
  const char *ip = agn_checkstring(L, 1);
#if defined(_WIN32)
  struct WinVer winversion;
  PIP_ADAPTER_INFO AdapterInfo;
  DWORD dwBufLen = sizeof(IP_ADAPTER_INFO);
  int rc = 0;
  char *mac_addr = NULL;
  if (getWindowsVersion(&winversion) < MS_WIN2K) {  /* does not work in NT 4.0 or earlier */
    lua_pushfail(L);
    return 1;
  }
  AdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
  if (AdapterInfo == NULL) {
    lua_pushfail(L);
    return 1;
  }
  /* Make an initial call to GetAdaptersInfo to get the necessary size into the dwBufLen variable */
  if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == ERROR_BUFFER_OVERFLOW) {
    xfree(AdapterInfo);
    AdapterInfo = (IP_ADAPTER_INFO *)malloc(dwBufLen);
    if (AdapterInfo == NULL) {
      lua_pushfail(L);
      return 1;
    }
  }
  if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == NO_ERROR) {
    PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;  /* Contains pointer to current adapter info */
    if (pAdapterInfo->AddressLength == 6) {
      mac_addr = (char*)malloc(18);
      if (mac_addr == NULL) {  /* 4.11.5 fix */
        xfree(AdapterInfo);
        lua_pushfail(L);
        return 1;
      }
      do {
        sprintf(mac_addr, "%02X:%02X:%02X:%02X:%02X:%02X",
          pAdapterInfo->Address[0], pAdapterInfo->Address[1],
          pAdapterInfo->Address[2], pAdapterInfo->Address[3],
          pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
        if (pAdapterInfo->IpAddressList.IpAddress.String &&  /* 7.5.15 security fix */
            tools_streq(ip, pAdapterInfo->IpAddressList.IpAddress.String)) {
          lua_pushstring(L, mac_addr);
          rc = 1;
          break;
        }
        pAdapterInfo = pAdapterInfo->Next;
      } while (pAdapterInfo);
      xfree(mac_addr);
    }
  }
  if (!rc) lua_pushfail(L);
  xfree(AdapterInfo);
#else
  (void)ip;
  lua_pushfail(L);
#endif
  return 1;
}


static int os_ismac (lua_State *L) {  /* 3.4.1 */
#ifdef __APPLE__
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


static int os_islinux (lua_State *L) {  /* 3.4.1 */
#if defined(__OS2__) || defined(__APPLE__) || defined(__HAIKU__) /* 3.17.3 fix */
  lua_pushfalse(L);
#elif defined(LUA_USE_LINUX) || defined(__linux__) || defined(OPENSUSE) || defined(LUA_RASPI_STRETCH) || defined(DEBIAN)
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


static int os_islinux386 (lua_State *L) {  /* 3.4.2 */
#if (defined(LUA_USE_LINUX) || defined(__linux__) || defined(OPENSUSE) || defined(LUA_RASPI_STRETCH) || defined(DEBIAN)) && \
  (defined(__i386__) || defined(__i386) || defined(__X86__))
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


static int os_is386 (lua_State *L) {  /* 3.4.2 */
#if (defined(__i386__) || defined(__i386) || defined(__X86__))
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


/* 2.4.0. Checks whether Agena is being run on OS/2, eComStation which would be very fine. */
static int os_isos2 (lua_State *L) {
#ifdef __OS2__
  struct utsname info;
  if (uname(&info) != -1) {
    lua_pushboolean(L, tools_streq(info.sysname, "OS/2"));  /* 2.16.12 tweak */
  } else
    lua_pushfail(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


static int os_isopensuse (lua_State *L) {  /* 6.3.7, UNDOC */
#if defined(OPENSUSE) && (defined(__i386__) || defined(__i386) || defined(__X86__))
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


/* 2.16.14. Checks whether the Agena version for Windows is being run and returns `true` or `false`.
   See also: `os.isos2`, `os.islinux`, `os.isunix`, `os.isdos`, `os.isansi`. */
static int os_iswindows (lua_State *L) {
#ifdef _WIN32
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


/* 3.17.3. Checks whether the Agena version for Solaris is being run and returns `true` or `false`.
   See also: `os.iswindows`, `os.isos2`, `os.islinux`, `os.isunix`, `os.isdos`, `os.isansi`. */
static int os_issolaris (lua_State *L) {
#ifdef __SOLARIS
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


/* 3.17.3. Checks whether the Agena version for Raspberry Pi is being run and returns `true` or `false`. */
static int os_israspi (lua_State *L) {
#ifdef RASPIPI
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


/* Checks whether Agena is run in the DOS environment and returns `true` or `false`. The procedure is quite dumb: if you are running the DOS version
   of Agena, it will always return `true` regardless whether it is actually being run in DOS, OS/2 or Windows.

   If you pass any argument, the function returns additional information in the following order: the name of the DOS edition, the official major and
   minor version if available, and the internal major and minor version from which you may deduce the actual DOS version. Furthermore, the internal
   OEM version number if returned. In FreeDOS, the kernel version number will also be given as the last return.

   See also: `os.isos2`, `os.islinux`, `os.isunix`, `os.isansi`, `os.iswindows`. */

static int os_isdos (lua_State *L) {  /* 2.16.14 */
#ifdef __DJGPP__
  int major, minor, onlyoneret, unamesuccess, oemno, nrets;
  struct utsname info;
  union REGS r;
  char *dosname;
  nrets = 0;
  onlyoneret = lua_gettop(L) == 0;
  lua_pushtrue(L);
  if (onlyoneret) return 1;
  nrets++;
  dosname = tools_getdosname(&oemno);
  r.h.ah = 0x30;
  r.h.al = 0x00;
  intdos(&r, &r);
  major = r.h.ah; minor = r.h.al;  /* these usually are internal version numbers that do not match the official ones */
  if (dosname != NULL) {  /* 2.17.1 patch */
    lua_pushstring(L, dosname); nrets++;
  }
  unamesuccess = uname(&info) != -1;
  if (dosname == NULL) {  /* 2.17.1 patch */
    lua_pushstring(L, unamesuccess ? info.sysname : "DOS"); nrets++;
  }
  if (unamesuccess) {
    lua_pushstring(L, info.release);
    lua_pushstring(L, info.version);
    nrets += 2;
  }
  lua_pushinteger(L, major);
  lua_pushinteger(L, minor);
  lua_pushinteger(L, oemno);
  nrets += 3;
  if (oemno == FREEDOS) {
    lua_pushinteger(L, r.h.bl); nrets++; /* kernel version */
  }
  return nrets;
#else
  lua_pushfalse(L);
  return 1;
#endif
}


static int os_isdow (lua_State *L) {  /* 2.39.12 */
#if defined(__DJGPP__) || defined(__OS2__) || defined(_WIN32)
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


/* 2.21.5. Check whether the Agena version for PPC/ARM/x86 is being run and returns `true` or `false`.
   See also: `os.isansi`, `os.isdos`, `os.islinux`, `os.isos2`, `os.isunix`, `os.iswindows`. */
static int os_isppc (lua_State *L) {
#if (defined(__POWERPC__) || defined(__ppc__) || defined(__powerpc) || \
     defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc64__))
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


static int os_isarm (lua_State *L) {  /* new 5.5.9, by Gemini AI */
  FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
  if (cpuinfo == NULL) {
#ifdef __ARMCPU
    lua_pushfail(L);
#else
    lua_pushfalse(L);
#endif
  } else {
    char line[256];
    tools_bzero(line, 256);
    int rc = 0;
    while (fgets(line, sizeof(line), cpuinfo)) {
      /* Look for the "vendor_id" or "CPU implementer" line */
      if (strstr(line, "CPU implementer")) {
        if (strstr(line, "0x41")) {  /* 0x41 is the hexadecimal code for ARM */
          rc = 1;
          break;
        }
      }
    }
    lua_pushboolean(L, rc);
    fclose(cpuinfo);  /* only close if cpuinfo != NULL, avoid crashes in OS/2 */
  }
  return 1;
}


static int os_isarm32 (lua_State *L) {  /* 3.10.5 */
#if defined(__ARMCPU) && defined(__linux__)
  struct utsname info;
  if (uname(&info) != -1) {
    long int wordBits;
    wordBits = sysconf(_SC_WORD_BIT);
    if (!(wordBits == -1 && errno == EINVAL)) {
      lua_pushboolean(L, wordBits == 32);
      return 1;
    }
  }
  lua_pushfail(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


static int os_isarm64 (lua_State *L) {  /* 3.10.5 */
#if defined(__ARMCPU) && defined(__linux__)
  struct utsname info;
  if (uname(&info) != -1) {
    long int wordBits;
    wordBits = sysconf(_SC_WORD_BIT);
    if (!(wordBits == -1 && errno == EINVAL)) {
      lua_pushboolean(L, wordBits == 64);
      return 1;
    }
  }
  lua_pushfail(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


static int os_isx86 (lua_State *L) {
#if (defined(__i386__) || defined(__x86_64__))
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


static int os_endian (lua_State *L) {
  int r = tools_endian();
  if (r == -1)
    lua_pushfail(L);
  else
    lua_pushinteger(L, r);
  return 1;
}


static int os_isANSI (lua_State *L) {  /* 0.28.2 */
#ifdef LUA_ANSI
  lua_pushtrue(L);
#else
  lua_pushfalse(L);
#endif
  return 1;
}


/* return status of battery on laptops, works in Windows 2000 and higher. 0.14.0, March 13, 2009 */
static int os_battery (lua_State *L) {
#ifdef _WIN32
  BYTE bACLineStatus, status, bBatteryLifePercent;
  DWORD bBatteryLifeTime, bBatteryFullLifeTime;
  SYSTEM_POWER_STATUS battstat;
  int BatteryCharging, winver;
  static const char *ACstatus[] = {"off", "on", "unknown"};
  static const char *BatteryStatusMapping[] =
    {"medium", "high", "low", "critical", "charging", "no battery", "unknown"};
  winver = tools_getwinver();  /* 2.17.4 change */
  if (winver < MS_WIN2K) {  /* Windows NT 4.0 and earlier are not supported */
    lua_pushfail(L);
    return 1;
  }
  /* get battery information by calling the Win32 API */
  GetSystemPowerStatus(&battstat);
  /* BYTE bReserved1 = battstat.Reserved1; */
  bACLineStatus = battstat.ACLineStatus;
  BatteryCharging = 0; /* not charging */
  if (bACLineStatus == 255)
    bACLineStatus = 2;  /* return 'unknown' */
  switch (battstat.BatteryFlag) {
    case 0: /* >= 33 and <= 66 % */
      status = 0;
      break;
    case 1: /* high */
      status = 1;
      break;
    case 2: /* low */
      status = 2;
      break;
    case 4: /* critical */
      status = 3;
      break;
    case 8: case 9: case 10:  /* charging */
      status = battstat.BatteryFlag - 8;
      BatteryCharging = 1;
      break;
    case 12:  /* charging & critical */
      status = 3;
      BatteryCharging = 1;
      break;  /* 7.5.15 fix */
    case 128: /* No system battery */
      status = 5;
      break;
    case 255: /* unknown status */
      status = 6;
      break;
    default:  /* got value 0 at around 45% on Acer, 10 right after reconnecting AC line  */
      status = 6;
      break;
  }
  bBatteryLifePercent = battstat.BatteryLifePercent;
  bBatteryLifeTime = battstat.BatteryLifeTime;
  bBatteryFullLifeTime = battstat.BatteryFullLifeTime;
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
  lua_createtable(L, 0, 7);  /* 2.4.1 change */
  lua_rawsetstringstring(L, -1, "acline", ACstatus[bACLineStatus]);
  if (status == 4) {
    lua_rawsetstringboolean(L, -1, "installed", 0);
  } else {
    lua_rawsetstringboolean(L, -1, "installed", 1);
    lua_rawsetstringnumber(L, -1, "life", bBatteryLifePercent);
    lua_rawsetstringstring(L, -1, "status", BatteryStatusMapping[status]);
    lua_rawsetstringboolean(L, -1, "charging", (BatteryCharging == 1));  /* 0.31.4 fix */
    lua_rawsetstringnumber(L, -1, "flag", battstat.BatteryFlag);
    lua_pushstring(L, "lifetime");
    if (bBatteryLifeTime != -1)
      lua_pushnumber(L, bBatteryLifeTime);
    else
      lua_pushundefined(L);
    lua_settable(L, -3);
    lua_pushstring(L, "fulllifetime");
    if (bBatteryFullLifeTime != -1)
      lua_pushnumber(L, bBatteryFullLifeTime);
    else
      lua_pushundefined(L);
    lua_settable(L, -3);
  }
#elif __OS2__
#define IOCTL_POWER 0x000c
#define POWER_GETPOWERSTATUS 0x0060
  int result;
  HFILE pAPI;
  ULONG call;
  struct _Battery {
    USHORT usParamLen;
    USHORT usPowerFlags;
    UCHAR  ucACStatus;
    UCHAR  ucBatteryStatus;
    UCHAR  ucBatteryLife;
  } Battery;
  struct _Data {
    USHORT usReturn;
  } Data;
  ULONG sizeBattery, sizeData;
  result = DosOpen((PCSZ)"\\DEV\\APM$", &pAPI, &call, 0,
    FILE_NORMAL, FILE_OPEN, OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE,
    NULL);
  if (result != 0) {
    lua_pushfail(L);
    return 1;
  }
  sizeBattery = Battery.usParamLen = sizeof(Battery);
  sizeData = sizeof(Data);
  result = DosDevIOCtl(pAPI,
    IOCTL_POWER, POWER_GETPOWERSTATUS, (PVOID)&Battery, sizeof(Battery),
    &sizeBattery, (PVOID)&Data, sizeof(Data), &sizeData);
  DosClose(pAPI);  /* 2.3.4 fix */
  if (result == ERROR_INVALID_PARAMETER) {
    lua_pushfail(L);
  } else {
    int acstatus, batterystatus;
    const char *ACstatus[] = {"off", "on", "unknown", "invalid"};
    const char *BatteryStatusMapping[] =
      {"high", "low", "critical", "charging", "unknown", "invalid"};
    luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
    lua_createtable(L, 0, 5);  /* 2.4.1 change */
    switch (Battery.ucACStatus) {
      case   0: acstatus = 0; break;
      case   1: acstatus = 1; break;
      case 255: acstatus = 2; break;
      default:  acstatus = 3; break;
    }
    switch (Battery.ucBatteryStatus) {
      case 0:   batterystatus = 0; break;
      case 1:   batterystatus = 1; break;
      case 2:   batterystatus = 2; break;
      case 3:   batterystatus = 3; break;
      case 255: batterystatus = 4; break;
      default:  batterystatus = 5; break;
    }
    lua_rawsetstringstring(L, -1, "acline", ACstatus[acstatus]);
    lua_rawsetstringstring(L, -1, "status", BatteryStatusMapping[batterystatus]);
    lua_rawsetstringnumber(L, -1, "flags", (lua_Number)Battery.usPowerFlags);  /* let us retain the OS/2 attribute name */
    lua_rawsetstringboolean(L, -1, "powermanagement", ((Battery.usPowerFlags & 0x0001) == 1));
    lua_pushstring(L, "life");
    if (Battery.ucACStatus != 1)  /* AC line is off */
      lua_pushnumber(L, (lua_Number)Battery.ucBatteryLife);
    else
      lua_pushundefined(L);
    lua_rawset(L, -3);
  }
#else
  lua_pushfail(L);
#endif
  return 1;
}


static void aux_getmodes (const char *fn, char *bits) {  /* 2.12.0 RC 2, based on os_fstat  */
  struct stat entry;
  mode_t mode;
  /* char bits[18] = "----------:-----\0"; */
#ifdef _WIN32
  DWORD winmodet;
#elif defined(__OS2__)
  static FILESTATUS3 fstat, *fs;
  APIRET rc;
#endif
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
  if (lstat(fn, &entry) == 0) {
#else
  if (stat(fn, &entry) == 0) {
#endif
    mode = entry.st_mode;
    if (S_ISDIR(mode)) {
      bits[0] = 'd';
#if defined(_WIN32) || defined(__OS2__) || defined (__DJGPP__)
      bits[11] = 'd';
#endif
    }
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
    else if (S_ISLNK(mode)) {
      bits[0] = 'l';
    }
#elif defined(_WIN32)
    else if (issymlink(fn)) {
      bits[0] = 'l';
    }
#endif
    if (mode & S_IRUSR) bits[1] = 'r';
    if (mode & S_IWUSR) bits[2] = 'w';
    if (mode & S_IXUSR) bits[3] = 'x';
#ifdef __OS2__
    rc = DosQueryPathInfo((PCSZ)fn, FIL_STANDARD, &fstat, sizeof(fstat));
    if (rc == 0) {
      fs = &fstat;  /* do not change this, otherwise Agena will crash */
      unsigned int attribs = fs->attrFile;
      if (attribs & FILE_DIRECTORY) bits[11] = 'd';
      if (attribs & FILE_READONLY) bits[12] = 'r';
      if (attribs & FILE_HIDDEN) bits[13] = 'h';
      if (attribs & FILE_ARCHIVED) bits[14] = 'a';
      if (attribs & FILE_SYSTEM) bits[15] = 's';
    }
#elif defined(_WIN32)
    winmodet = GetFileAttributes(fn);
    if (winmodet & FILE_ATTRIBUTE_READONLY) bits[12] = 'r';
    if (winmodet & FILE_ATTRIBUTE_HIDDEN) bits[13] = 'h';
    if (winmodet & FILE_ATTRIBUTE_ARCHIVE) bits[14] = 'a';
    if (winmodet & FILE_ATTRIBUTE_SYSTEM) bits[15] = 's';
#endif
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
    /* determine group rights */
    if (mode & S_IRGRP) bits[4] = 'r';
    if (mode & S_IWGRP) bits[5] = 'w';
    if (mode & S_IXGRP) bits[6] = 'x';
    /* determine other users' rights */
    if (mode & S_IROTH) bits[7] = 'r';
    if (mode & S_IWOTH) bits[8] = 'w';
    if (mode & S_IXOTH) bits[9] = 'x';
#endif
  }
}


/* 0.25.0, July 19, 2009; extended 26.07.2009, 0.25.1; patched 0.26.0, August 06, 2009 */
static int os_fcopy (lua_State *L) {
  const char *fin, *fouttemp0;
  char *buffer, *fout, *template, *fouttemp;
  char bits[17];  /* including NULL */
  int in, out, bufsize, result, nr, bytes, overwrite;  /* 2.12.0 RC 2 fix with bytes */
  struct stat entry;
  size_t foutlen;
  template = "----------:-----\0";
  tools_memcpy(bits, template, 17);  /* 2.21.5 tweak */
  result = 0;
  fin = agn_checkstring(L, 1);
  fouttemp0 = agn_checklstring(L, 2, &foutlen);
  overwrite = agnL_optboolean(L, 3, 0);  /* 2.16.10 */
  /* is target a directory ?  2.12.0 RC 2 */
  aux_getmodes(fouttemp0, bits);
  fouttemp = tools_strndup(fouttemp0, foutlen);  /* 2.37.1 fix */
  if (!fouttemp)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.fcopy");
  str_charreplace(fouttemp, '\\', '/', 1);
  if (bits[0] == 'd') {
    const char *finold;
    int i;
    size_t s_fin = tools_strlen(fin);  /* 2.17.8 tweak */
    finold = fin;
    /* get filename, ignore path, to construct target file name (and path) */
    for (i=s_fin - 1; i >= 0 && fin[i] != '/'; i--);
    fin += i + 1;
    fout = str_concat(fouttemp, "/", fin, NULL);
    if (!fout) {
      xfree(fouttemp);
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.fcopy");
    }
    fin = finold;
    xfree(fouttemp);
  } else {  /* 2.17.1 fix */
    fout = fouttemp;  /* we'll free it below */
  }
  if (tools_exists(fout)) {  /* 2.12.0 RC 2, destination file already exists ? 2.16.11 change */
    if (!overwrite) {
      lua_pushstring(L, fout);
      xfree(fout);
      luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": file already exists.", "os.fcopy", agn_tostring(L, -1));  /* changed 2.16.11 */
    } else {
      xfree(fout);
      lua_pushfail(L);
    }
    return 1;
  }
  if (tools_streq(fin, fout)) {  /* 1.8.0, prevent destroying the file to be copied if source = target, 2.16.12 tweak */
    lua_pushstring(L, fout);
    xfree(fout);
    luaL_error(L, "Error in " LUA_QS ": source and target for " LUA_QS " are identical.", "os.fcopy", agn_tostring(L, -1));
    return 1;
  }
  if ( (in = my_roopen(fin)) == -1) {
    lua_pushstring(L, fin);  /* 1.7.6 */
    xfree(fout);
    luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": file could not be opened.", "os.fcopy", agn_tostring(L, -1));
    return 1;
  }
  if ( (out = my_create(fout)) == -1) {
    close(in);
    lua_pushstring(L, fout);  /* 1.7.6 */
    xfree(fout);
    luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": file could not be created.", "os.fcopy", agn_tostring(L, -1));
    return 1;
  }
  bufsize = agn_getbuffersize(L);  /* 2.2.0 */
  buffer = tools_stralloc(bufsize);  /* 2.2.0 RC 5, 2.16.5 change */
  if (buffer == NULL)
    luaL_error(L, "Error in " LUA_QS ": internal memory allocation failed.", "os.fcopy");
  /* copy contents */
  while ( (bytes = read(in, buffer, bufsize)) ) {
    if (bytes == -1) {  /* 1.6.4 */
      xfree(buffer); xfree(fout);
      luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": could not read file.", "os.fcopy", fin);
    }
    if (write(out, buffer, bytes) == -1) {  /* 1.6.4 to prevent compiler warnings */
      xfree(buffer); xfree(fout);
      luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": could not copy file.", "os.fcopy", fin);
    }
  }
  close(in);
  close(out);
  nr = 1;
  if (bytes < 0) {
    lua_pushfail(L); lua_pushstring(L, fin); nr = 2;  /* 1.7.6 */
  } else {
    int en;
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if (stat(fin, &entry) == 0) {
      /* set file permission attributes */
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
      mode_t mode = entry.st_mode;
      result = chmod(fout, (mode & S_IRUSR)|(mode & S_IWUSR)|(mode & S_IXUSR)|(mode & S_IRGRP)|(mode & S_IWGRP)|(mode & S_IXGRP)|(mode & S_IROTH)|(mode & S_IWOTH)|(mode & S_IXOTH));
#elif defined(_WIN32)
      /* added 0.26.0, 07.08.2009 */
      result = !(SetFileAttributes(fout, GetFileAttributes(fin)));
#elif defined(__OS2__)
      static FILESTATUS3 fstat, *fs;
      APIRET rc = DosQueryPathInfo((PCSZ)fin, FIL_STANDARD, &fstat, sizeof(fstat));
      if (rc == 0) {
        fs = &fstat;  /* do not change this, otherwise Agena will crash */
        DosSetPathInfo((PCSZ)fout, FIL_STANDARD, fs, sizeof(FILESTATUS3), 0);
      }
#else
      mode_t mode = entry.st_mode;
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      result = chmod(fout, (mode & S_IRUSR)|(mode & S_IWUSR)|(mode & S_IXUSR));
#endif
#if (defined(_WIN32) || defined(__unix__) || defined(__APPLE__) || defined(__DJGPP__))
      /* file date and time, Agena 1.8.0 */
      struct utimbuf times;
      times.modtime = times.actime = entry.st_mtime;
      result = result || utime(fout, &times);
#endif
      if (result == 0)
        lua_pushtrue(L);
      else {
        en = errno;
        lua_pushfail(L);
        lua_pushstring(L, fin);
        lua_pushstring(L, ": successfully copied but attributes not set, ");
        lua_pushstring(L, my_ioerror(en));  /* 2.12.0 RC 2 */
        lua_concat(L, 3);
        nr = 2;  /* 1.7.6 */
      }
    } else {
      en = errno;
      lua_pushfail(L);
      lua_pushstring(L, fin);
      lua_pushstring(L, ": successfully copied but attributes not set, ");
      lua_pushstring(L, my_ioerror(en));  /* 2.12.0 RC 2 */
      lua_concat(L, 3);
      nr = 2;  /* 1.7.6 */
    }
  }
  xfree(buffer); xfree(fout);
  return nr;
}


#ifdef _WIN32
BOOL IsValidDrive (TCHAR cDriveLetter);
#endif


void aux_checkvaliddrive (lua_State *L, const char *what, size_t n, const char *procname) {  /* 2.39.11 */
#if defined (_WIN32) || defined(__OS2__) || defined(__DJGPP__)
  if (!(n == 1 || (n == 2 && isdriveletter(what)) ||
     (n == 3 && isdriveletter(what) && ispathsep(what[2]))))
    luaL_error(L, "Error in " LUA_QS ": string does not represent a drive letter.", procname);
#endif
  return;
}

static int os_isvaliddrive (lua_State *L) {
  size_t n;
  const char *what = agn_checklstring(L, 1, &n);
  aux_checkvaliddrive(L, what, n, "os.isvaliddrive");  /* 2.39.10 */
#ifdef _WIN32
  lua_pushboolean(L, IsValidDrive(*what));
#elif defined(__OS2__)
  ULONG buff, mask, curDrive;
  APIRET rc;
  DosError(FERR_DISABLEHARDERR);  /* disable hard-error popups */
  rc = DosQueryCurrentDisk(&curDrive, &buff);
  DosError(FERR_ENABLEHARDERR);  /* enable hard-error popups */
  if (rc != 0)
    luaL_error(L, "Error in " LUA_QS ": could not determine drives.", "os.isvaliddrive");
  mask = 1 << (toupper((int)*what) - 65);  /* uppercase table does not work */
  lua_pushboolean(L, (buff & mask) > 0);
#elif (__DJGPP__)  /* 2.39.11 */
  struct diskfree_t d;
  lua_pushboolean(L, !_dos_getdiskfree(toupper((int)*what) - 64, &d));  /* A: = 1, B: = 2, ... */
#else
  (void)what;
  lua_pushfail(L);
#endif
  return 1;
}


static int os_isdriveletter (lua_State *L) {  /* 2.39.12 */
  size_t n;
  const char *what = agn_checklstring(L, 1, &n);
  lua_pushboolean(L,
    (n == 2 && isdriveletter(what)) ||
    (n == 3 && isdriveletter(what) && ispathsep(what[2]))
  );
  return 1;
}


static int os_drives (lua_State *L) {
#if defined(_WIN32) || defined(__OS2__)
  unsigned int i, c;
  int anydrivefound;
  char drive[1];
  #ifdef _WIN32
  long buff, mask;
  buff = GetLogicalDrives();
  if (buff == 0)  /* 2.39.11, https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getlogicaldrives */
    luaL_error(L, "Error in " LUA_QS ": could not determine drives.", "os.drives");
  #else
  ULONG buff, mask, curDrive;
  APIRET rc;
  DosError(FERR_DISABLEHARDERR);  /* disable hard-error popups */
  rc = DosQueryCurrentDisk(&curDrive, &buff);
  DosError(FERR_ENABLEHARDERR);  /* enable hard-error popups */
  if (rc != 0)
    luaL_error(L, "Error in " LUA_QS ": could not determine drives.", "os.drives");
  #endif
  anydrivefound = c = i = 0;
  agn_createseq(L, 26);
  while( i < 26 ) {
    mask = (1 << i);
    if( (buff & mask) > 0 ) {
      drive[0] = 'A' + i;
      lua_pushlstring(L, drive, 1);
      lua_pushlstring(L, ":", 1);  /* 2.17.2 improvement */
      lua_concat(L, 2);
      lua_seqrawseti(L, -2, ++c);
      /* prevent error messages if no media are inserted in floppy drives or CD-ROMs */
      anydrivefound = 1;
    }
    i++;
  }
  if (!anydrivefound) {
    agn_poptop(L);  /* delete sequence */
    lua_pushfail(L);
  }
#elif defined(__DJGPP__)  /* 2.17.2 improvement */
  char drive[1];
  struct diskfree_t d;
  int i, c, anydrivefound;
  c = anydrivefound = 0;
  agn_createseq(L, 26);
  for (i=3; i <= 26; i++) {  /* avoid `insert floppy into A:/B:` message */
    /* if (i == 0) continue; */
    if (!_dos_getdiskfree(i, &d)) {
      drive[0] = 'A' - 1 + i;
      lua_pushlstring(L, drive, 1);
      lua_pushlstring(L, ":", 1);  /* 2.17.2 */
      lua_concat(L, 2);
      lua_seqrawseti(L, -2, ++c);
      anydrivefound = 1;
    }
  }
  if (!anydrivefound) {
    agn_poptop(L);  /* delete sequence */
    lua_pushfail(L);
  }
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* file systems

see: http://linux.die.net/man/2/statfs */

#if defined(__linux__)

#if __WORDSIZE == 32  /* System word size */
# define __SWORD_TYPE int
#else /* __WORDSIZE == 64 */
# define __SWORD_TYPE long int
#endif

typedef struct filesystemtypes {
  char *name;
  long number;
} filesystemtypes;

filesystemtypes filesystems[] = {  /* last time checked January 16, 2019 for completeness */
  {"ADFS", 0xadf5},
  {"AFFS", 0xADFF},
  {"BEFS", 0x42465331},
  {"BFS", 0x1BADFACE},
  {"CIFS", 0xFF534D42},
  {"CODA", 0x73757245},
  {"COH", 0x012FF7B7},
  {"CRAMFS", 0x28cd3d45},
  {"DEVFS", 0x1373},
  {"EFS", 0x00414A53},
  {"EXT FS", 0x137D},
  {"Old EXT-2 FS", 0xEF51},
  {"EXT-2 FS", 0xEF53},
  {"EXT-3 FS", 0xEF53},
  {"EXT-4 FS", 0xEF53},
  {"HFS", 0x4244},
  {"HPFS", 0xF995E849},
  {"HUGETLBFS", 0x958458f6},
  {"ISO-CD FS", 0x9660},
  {"JFFS2", 0x72b6},
  {"JFS", 0x3153464a},
  {"Original Minix FS", 0x137F},  /* orig. minix */
  {"Minix v2", 0x138F},  /* 30 char minix */
  {"Minix v2", 0x2468},  /* minix V2 */
  {"Minix v2, 30 char", 0x2478},  /* minix V2, 30 char names */
  {"MSDOS", 0x4d44},
  {"NCP", 0x564c},
  {"NFS", 0x6969},
  {"NTFS", 0x5346544e},
  {"OPENPROM", 0x9fa1},
  {"Proc", 0x9fa0},
  {"QNX4", 0x002f},
  {"REISERFS", 0x52654973},
  {"ROMFS", 0x7275},
  {"SMB", 0x517B},
  {"SysV.2", 0x012FF7B6},
  {"SysV.4", 0x012FF7B5},
  {"TMPFS", 0x01021994},
  {"UDF", 0x15013346},
  {"UFS Solaris", 0x00011954},
  {"USBDEVICE", 0x9fa2},
  {"VXFS", 0xa501FCF5},
  {"Xenix SysV", 0x012FF7B4},
  {"XFS", 0x58465342},
  {"Xia FS", 0x012FD16D},
  {NULL, 0}
};

static char *findfsentry (long number) {
  filesystemtypes *fs;
  for (fs = filesystems; fs->name; ++fs) {
    if (fs->number == number) return fs->name;
  }
  return "unknown";
}
#endif

#ifdef _WIN32
#ifndef NTFS_EXTENDED_VOLUME_DATA
typedef struct {
  ULONG ByteCount;
  USHORT MajorVersion;
  USHORT MinorVersion;
} NTFS_EXTENDED_VOLUME_DATA;
#endif
#endif

#if defined(__DJGPP__)  /* 7.8.2 */
void dos_drive_label (int drno, char *buffer) {
  struct find_t fileinfo;
  char drive_path[7];
  drive_path[0] = drno + 'A' - 1;
  drive_path[1] = ':';
  drive_path[2] = '\\';
  drive_path[3] = '*';
  drive_path[4] = '.';
  drive_path[5] = '*';
  drive_path[6] = 0;
  /* Search the root directory for the volume label attribute flag. Example path: "C:\\*.*" */
  if (_dos_findfirst(drive_path, _A_VOLID, &fileinfo) == 0) {
    sprintf(buffer, "%s", fileinfo.name);
  } else {
    sprintf(buffer, "NO_LABEL");
  }
}
#endif

static int os_drivestat (lua_State *L) {
#if (defined(_WIN32) || defined(__DJGPP__))
  char default_drive[4] = { 0 };
  const char *drivename = NULL;
#elif defined(__OS2__)
  char default_drive[4] = { 0 };
  PCSZ drivename = NULL;
#endif
#ifdef _WIN32
  unsigned int drivetype;
  size_t l;
  int rc;
  long buff, mask;
  char drive[4];
  DWORD serial_number, max_component_length, file_system_flags;
  HANDLE hdev;
  char name[256];
  char file_system_name[256];
  char buf[PATH_MAX] = { 0 };  /* 7.5.15 security fix */
  unsigned long sectors_per_cluster, bytes_per_sector, number_of_free_clusters, total_number_of_clusters;
  double freespace, totalspace, usedspace;
  ULARGE_INTEGER dummybytes, totalbytes, freebytes;
  /* see: https://msdn.microsoft.com/en-gb/library/windows/desktop/aa364939%28v=vs.85%29.aspx */
  static const char *types[] = {
    "unknown", "no root dir", "Removable", "Fixed", "Remote", "CD-ROM", "RAMDISK"};
  if (lua_isnoneornil(L, 1)) {  /* 7.8.1 extension */
    char cwd_buf[PATH_MAX];
    if (getcwd(cwd_buf, sizeof(cwd_buf)) != NULL && cwd_buf[0] != '\0') {
      default_drive[0] = cwd_buf[0]; default_drive[1] = ':'; default_drive[2] = '\0'; drivename = default_drive;
      l = 2;  /* Length of "X:" */
    } else {
      drivename = "C:"; l = 2;
    }
  } else {
    drivename = agn_checklstring(L, 1, &l);
  }
  aux_checkvaliddrive(L, drivename, l, "os.drivestat");  /* 2.39.11 */
  drive[0] = toupper(*drivename); drive[1] = ':'; drive[2] = '\\'; drive[3] = '\0';
  int drno = toupper(drive[0]) - 'A' + 1;  /* 7.8.2 streamlining */
  if (drno < 0 || drno > 26)
    luaL_error(L, "Error in " LUA_QS ": drive letter is invalid.", "os.drivestat");
  buff = GetLogicalDrives();
  mask = (1 << (drno - 1));
  if( (buff & mask) < 1 ) {  /* no valid drive ? */
    lua_pushfail(L);
    return 1;
  }
  /* set defaults to prevent invalid values when calling GetDiskFreeSpace */
  sectors_per_cluster = 0, bytes_per_sector = 0, number_of_free_clusters = 0, total_number_of_clusters = 0;
  /* set defaults to prevent invalid values when calling GetVolumeInformation  */
  strcpy(name, ""); strcpy(file_system_name, "");
  serial_number = 0; max_component_length = 0; file_system_flags = 0;
  drivetype = GetDriveType(drive);
  luaL_checkstack(L, 3, "not enough stack space");  /* 5.4.0 */
  lua_createtable(L, 0, 9);
  /* for whatever reason, the return value of GetVolumeInformation is sometimes wrong, so we do not check it. */
  rc = GetVolumeInformation(drive, name,
    sizeof(name), &serial_number, &max_component_length, &file_system_flags,
    file_system_name, sizeof(file_system_name));
  if (!rc) {
    name[0] = 0;
    file_system_name[0] = 0;
  }
  if (name[0] == '\0' || tools_streq(name, ""))  /* 2.25.1 tweak; 3.3.2 change to prevent compiler warnings */
    strcpy(name, "<none>");
  if (tools_strlen(name) > 19) name[19] = '\0';  /* 2.17.8 tweak */
  if (file_system_name[0] == '\0' || tools_streq(file_system_name, ""))  /* 2.25.1 tweak; 3.3.2 change to prevent compiler warnings */
    strcpy(file_system_name, "<unknown>");
  if (tools_strlen(file_system_name) > 11) file_system_name[11] = '\0';  /* 2.17.8 tweak */
  /* prevent error messages if no media are inserted in floppy drives or CD-ROMs */
  SetErrorMode(0x0001);
  /* get info on free space on drive processed */
  sectors_per_cluster = 0; bytes_per_sector = 0;  /* 7.5.15 security fixes */
  number_of_free_clusters = 0; total_number_of_clusters = 0;
  rc = GetDiskFreeSpaceEx(drive, &dummybytes, &totalbytes, &freebytes);  /* 7.7.0 u1 change */
  if (!rc) {
    ZeroMemory(&dummybytes, sizeof(dummybytes));
    ZeroMemory(&totalbytes, sizeof(totalbytes));
    ZeroMemory(&freebytes, sizeof(freebytes));
    luaL_error(L, "Error in " LUA_QS ": could not read drive statistics.", "os.drivestat");
  }
  GetDiskFreeSpace(drive, &sectors_per_cluster, &bytes_per_sector,
    &number_of_free_clusters, &total_number_of_clusters);
  SetErrorMode(0);  /* set error mode back to normal (any exceptions printed again) */
  freespace = (double)freebytes.HighPart*4294967296.0 + (double)freebytes.LowPart;  /* 7.7.0 u1 change */
  totalspace = (double)totalbytes.HighPart*4294967296.0 + (double)totalbytes.LowPart;  /* 7.7.0 u1 change */
  usedspace = totalspace - freespace;  /* 7.7.0 u1 change */
  ZeroMemory(&dummybytes, sizeof(dummybytes));
  ZeroMemory(&totalbytes, sizeof(totalbytes));
  ZeroMemory(&freebytes, sizeof(freebytes));
  lua_rawsetstringstring(L, -1, "label", name);
  lua_rawsetstringnumber(L, -1, "serialnumber", serial_number);
  lua_rawsetstringstring(L, -1, "drivetype", (drivetype < 7) ? types[drivetype] : "unknown");
  lua_rawsetstringstring(L, -1, "filesystem",
    (tools_streq(file_system_name, "")) ? "<unknown>" : file_system_name);  /* 2.25.1 tweak */
  lua_rawsetstringboolean(L, -1, "isfat32", tools_streq(file_system_name, "FAT32"));  /* 7.8.2 */
  lua_rawsetstringnumber(L, -1, "freesize", freespace);
  lua_rawsetstringnumber(L, -1, "usedsize", usedspace);
  lua_rawsetstringnumber(L, -1, "totalsize", totalspace);
  lua_rawsetstringnumber(L, -1, "totalclusters", total_number_of_clusters);  /* 2.4.1 */
  lua_rawsetstringnumber(L, -1, "freeclusters", number_of_free_clusters);  /* 2.4.1 */
  lua_rawsetstringnumber(L, -1, "sectorspercluster", sectors_per_cluster);  /* 2.4.1 */
  lua_rawsetstringnumber(L, -1, "bytespersector", bytes_per_sector);  /* 2.4.1 */
  /* maximum number of characters allowed for a single folder name or filename within a file system, 7.8.1 */
  lua_rawsetstringnumber(L, -1, "maxnamelength", max_component_length);
  lua_rawsetstringnumber(L, -1, "maxpathlength", MAX_PATH);  /* 7.8.1 */
  hdev = OpenVolume(*drivename, 1);
  if (hdev != INVALID_HANDLE_VALUE) {
    lua_pushstring(L, "trim");
    agn_pushboolean(L, GetTrimFlag(hdev));
    CloseVolume(hdev);
    lua_settable(L, -3);
  }
  /* 2.17.2, taken from Nodir Temirkhodjaev's luasys package; works fine with W2K */
  rc = QueryDosDeviceA(drivename, buf, PATH_MAX);
  if (rc > 2) {
    lua_pushstring(L, "dosdevice");
    lua_pushlstring(L, buf, rc - 2);
    lua_settable(L, -3);
  }
  /* 7.8.1 extension; we already have 3 stack slots reserved, but we need a fourth */
  luaL_checkstack(L, 1, "not enough stack space");
  lua_pushstring(L, "flags");
  agn_createset(L, 20);
  #define SETFSFLAG(Flags, Flag, Desc) \
    if (Flags & Flag) { \
      lua_pushstring(L, Desc); \
      lua_sinsert(L, -2); \
    }
  SETFSFLAG(file_system_flags, FILE_CASE_SENSITIVE_SEARCH, "case-sensitive file names");
  SETFSFLAG(file_system_flags, FILE_CASE_PRESERVED_NAMES, "preserved case of file names");
  SETFSFLAG(file_system_flags, FILE_UNICODE_ON_DISK, "unicode file names");
  SETFSFLAG(file_system_flags, FILE_PERSISTENT_ACLS, "ACL support");
  SETFSFLAG(file_system_flags, FILE_FILE_COMPRESSION, "compression per file");
  SETFSFLAG(file_system_flags, FILE_VOLUME_QUOTAS, "disk quotas");
  SETFSFLAG(file_system_flags, FILE_SUPPORTS_SPARSE_FILES, "sparse files");
  SETFSFLAG(file_system_flags, FILE_SUPPORTS_REPARSE_POINTS, "reparse points");
  SETFSFLAG(file_system_flags, FILE_VOLUME_IS_COMPRESSED, "compressed volume");
  SETFSFLAG(file_system_flags, FILE_SUPPORTS_OBJECT_IDS, "object identifiers");
  SETFSFLAG(file_system_flags, FILE_SUPPORTS_ENCRYPTION, "Encrypted File System (EFS) support");
  SETFSFLAG(file_system_flags, FILE_NAMED_STREAMS, "named streams");
  SETFSFLAG(file_system_flags, FILE_READ_ONLY_VOLUME, "read-only volume");
  SETFSFLAG(file_system_flags, FILE_SEQUENTIAL_WRITE_ONCE, "single sequential write");
  SETFSFLAG(file_system_flags, FILE_SUPPORTS_TRANSACTIONS, "transactions");
  SETFSFLAG(file_system_flags, FILE_SUPPORTS_HARD_LINKS, "hard links");
  SETFSFLAG(file_system_flags, FILE_SUPPORTS_EXTENDED_ATTRIBUTES, "extended attributes");
  SETFSFLAG(file_system_flags, FILE_SUPPORTS_OPEN_BY_FILE_ID, "opening of files per file identifier");
  SETFSFLAG(file_system_flags, FILE_SUPPORTS_USN_JOURNAL, "Update Sequence Number (USN) journals");
  #ifndef FILE_DAX_VOLUME
  #define FILE_DAX_VOLUME 0x20000000
  #endif
  SETFSFLAG(file_system_flags, FILE_DAX_VOLUME, "direct access volume");
  #ifndef FILE_SUPPORTS_SPARSE_VDL
  #define FILE_SUPPORTS_SPARSE_VDL 0x10000000
  #endif
  SETFSFLAG(file_system_flags, FILE_SUPPORTS_SPARSE_VDL, "valid data length");
  #ifndef FILE_SUPPORTS_GHOSTING
  #define FILE_SUPPORTS_GHOSTING   0x40000000
  #endif
  SETFSFLAG(file_system_flags, FILE_SUPPORTS_GHOSTING, "ghosting");
  lua_rawset(L, -3);
  /* NTFS version, 7.8.1 */
  if (tools_streq(file_system_name, "NTFS")) {
    struct WinVer winversion;
    if (getWindowsVersion(&winversion) < MS_WINXP) {
      lua_pushstring(L, "ntfsver");
      lua_pushfstring(L, "%d.%d", 3, 0);
    } else {
      WCHAR wDrive[64] = { 0 };
      char singleDriveLetter = drivename[0];
      _snwprintf(wDrive, 64, L"\\\\.\\%hc:", singleDriveLetter);
      HANDLE hDir = CreateFileW(
        wDrive, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      DWORD major = 3, minor = 1;  /* Default fallback for XP through 11 */
      BOOL versionFound = FALSE;
      if (hDir == INVALID_HANDLE_VALUE) {
        int en = GetLastError();
        /* If Access Denied (5), fall back to checking the Registry */
        if (en == 5) {
          HKEY hKey;
          if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\FileSystem", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
            DWORD dwType, dwSize = sizeof(DWORD), ntfsVersionField = 0;
            /* NtfsVersion updates based on the mounted capabilities */
            if (RegQueryValueExW(hKey, L"NtfsVersion", NULL, &dwType, (LPBYTE)&ntfsVersionField, &dwSize) == ERROR_SUCCESS) {
              /* The registry field usually holds configuration flags, but defaults to 3.1 capabilities */
              major = 3;
              minor = 1;
              versionFound = TRUE;
            }
            RegCloseKey(hKey);
          }
          /* If registry failed or we just use the guaranteed NT6+ baseline: */
          if (!versionFound) {
            major = 3;
            minor = 1;
          }
        } else {
          /* Hard error for actual hardware or missing drive issues */
          luaL_error(L, "Error in " LUA_QS ": got an invalid handle, %s (%d).", "os.drivestat", my_ioerror(en), en);
          return 1;
        }
      } else {
        /* Administrative path succeeded */
        BYTE buffer[sizeof(NTFS_VOLUME_DATA_BUFFER) + sizeof(NTFS_EXTENDED_VOLUME_DATA)] = { 0 };
        DWORD bytesReturned = 0;
        BOOL result = DeviceIoControl(hDir, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0, buffer, sizeof(buffer), &bytesReturned, NULL);
        CloseHandle(hDir);
        if (!result) {
          int en = GetLastError();
          luaL_error(L, "Error in " LUA_QS ": could not get handle, %s (%d).", "os.drivestat", my_ioerror(en), en);
          return 1;
        }
        NTFS_EXTENDED_VOLUME_DATA* extData = (NTFS_EXTENDED_VOLUME_DATA*)(buffer + sizeof(NTFS_VOLUME_DATA_BUFFER));
        major = extData->MajorVersion;
        minor = extData->MinorVersion;
      }
      lua_pushstring(L, "ntfsver");
      lua_pushfstring(L, "%d.%d", major, minor);
    }
    lua_settable(L, -3);
    drive[2] = '\0';
    lua_rawsetstringstring(L, -1, "driveletter", drive);  /* 7.8.2 */
  }
#elif defined(__OS2__)  /* 2.17.2 */
  /* See: http://www.edm2.com/index.php/DosQueryFSAttach and
          http://www.edm2.com/index.php/DosQueryFSInfo and
          include/os2emx.h */
  APIRET       rc;
  size_t       l;
  unsigned int driveno;
  int          drno;
  BYTE         drive[3];
  double       freespace, totalspace, cbUnit;
  PBYTE        pszFSDName;
  PBYTE        prgFSAData;
  BYTE         fsqBuffer[sizeof(FSQBUFFER2) + (3 * CCHMAXPATH)] = { 0 };
  ULONG        idfs       = 0;
  ULONG        cbBuffer   = sizeof(fsqBuffer);  /* buffer length */
  PFSQBUFFER2  pfsqBuffer = (PFSQBUFFER2)fsqBuffer;
  FSINFO       fsBuffer   = { {0} };  /* file system info buffer */
  FSALLOCATE   fsaBuffer  = {  0  };  /* file system info buffer */
  if (lua_isnoneornil(L, 1)) {  /* 7.8.1 extension */
    char cwd_buf[PATH_MAX];
    if (getcwd(cwd_buf, sizeof(cwd_buf)) != NULL && cwd_buf[0] != '\0') {
      default_drive[0] = cwd_buf[0]; default_drive[1] = ':'; default_drive[2] = '\0'; drivename = (PCSZ)default_drive;
      l = 2;  /* Length of "X:" */
    } else {
      drivename = (PCSZ)"C:"; l = 2;
    }
  } else {
    drivename = (PCSZ)agn_checklstring(L, 1, &l);
  }
  pszFSDName = prgFSAData = NULL;
  aux_checkvaliddrive(L, (const char *)drivename, l, "os.drivestat");  /* 2.39.11 */
  drive[0] = toupper(drivename[0]);
  drive[1] = ':';
  drive[2] = '\0';  /* drive letter must explicitly be terminated */
  drno = drive[0] - 'A' + 1;
  if (drno < 1 || drno > 26)  /* 2.39.11 patch; better be sure than sorry */
    luaL_error(L, "Error in " LUA_QS ": drive letter is invalid.", "os.drivestat");
  driveno = (unsigned int)drno;
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
  lua_createtable(L, 0, 16);  /* count it through ! */
  {  /* check for assigned drive letter, 7.8.2 */
    ULONG buff, mask, curDrive;
    APIRET rc;
    DosError(FERR_DISABLEHARDERR);  /* disable hard-error popups */
    rc = DosQueryCurrentDisk(&curDrive, &buff);
    DosError(FERR_ENABLEHARDERR);  /* enable hard-error popups */
    if (rc != 0)
      luaL_error(L, "Error in " LUA_QS ": could not determine drives.", "os.drivestat");
    mask = (1 << (drno - 1));
    if ((buff & mask) < 1) {
      lua_pushfail(L);
      return 1;
    }
  }
  rc = DosQueryFSAttach(
         (PCSZ)drive,      /* logical drive of attached FS */
         0,                /* ignored for FSAIL_QUERYNAME */
         FSAIL_QUERYNAME,  /* return data for a Drive or Device */
         pfsqBuffer,       /* returned data */
         &cbBuffer);       /* returned data length */
  if (rc != NO_ERROR) {
    agn_poptop(L);  /* drop empty table */
    luaL_error(L, "Error in " LUA_QS ": could not query file system.", "os.drivestat");
  }
  pszFSDName = pfsqBuffer->szName + pfsqBuffer->cbName + 1;
  /* pfsqBuffer->iType: 1 = Resident character device, 2 = Pseudo-character device,
     3 = Local drive, 4 = Remote drive attached */
  lua_rawsetstringnumber(L, -1, "iType", pfsqBuffer->iType);
  /* 7.8.2 extension */
  static const char *types[] =
    {"unknown", "ResidentCharacter", "PseudoCharacter", "Fixed", "Remote", "CD-ROM", "RAMDISK"};
  int typeno = pfsqBuffer->iType;
  if (typeno == 3 && tools_streq((const char *)pszFSDName, "CDFS"))  typeno = 5;
  if (typeno == 3 && tools_streq((const char *)pszFSDName, "RAMFS")) typeno = 6;
  lua_rawsetstringstring(L, -1, "drivetype", types[typeno]);
  /* iType: 1 = Resident character device, 2 = Pseudo-character device, 3 = Local drive, 4 = Remote drive attached to FSD;
     taken from: https://archive.org/stream/IBMOS2TechnicalDocumentation/KGUIDE20_djvu.txt */
  lua_rawsetstringboolean(L, -1, "isfat32", tools_streq((const char *)pszFSDName, "FAT32"));  /* 7.8.2 */
  lua_rawsetstringstring(L, -1, "driveletter", (const char *)pfsqBuffer->szName);
  lua_rawsetstringstring(L, -1, "filesystem",  (const char *)pszFSDName);  /* HPFS, JFS, etc. */
  if (pfsqBuffer->cbFSAData >= (2 * sizeof(USHORT))) {  /* created by Gemini AI, 7.8.1 */
    /* Calculate the exact address where rgFSAData begins in the variable-length struct packet */
    PBYTE pFsaData = (PBYTE)pfsqBuffer + (3 * sizeof(USHORT)) + pfsqBuffer->cbName + pfsqBuffer->cbFSDName;
    /* Read the tightly packed limits directly out of the driver data block */
    USHORT max_component = *(USHORT*)(pFsaData);
    USHORT max_path      = *(USHORT*)(pFsaData + sizeof(USHORT));
    /* Assign the metrics into your existing Lua metrics table output */
    lua_rawsetstringnumber(L, -1, "maxnamelength", (lua_Number)max_component);
    lua_rawsetstringnumber(L, -1, "maxpathlength", (lua_Number)max_path);
  } else {
    /* Fallback default boundaries for classic OS/2 partitions if the FSD returns empty attached data */
    lua_rawsetstringnumber(L, -1, "maxnamelength", 254);
    lua_rawsetstringnumber(L, -1, "maxpathlength", 260);
  }
  if (pfsqBuffer->rgFSAData != NULL && pfsqBuffer->cbFSAData != 0) {       /* FSD Attach Data */
    lua_pushstring(L, "rgFSAData");
    lua_pushlstring(L, (const char*)pfsqBuffer->rgFSAData, pfsqBuffer->cbFSAData);
    lua_rawset(L, -3);
  }
  rc = DosQueryFSInfo(driveno,
                FSIL_VOLSER,      /* request volume information, level 2 information */
                &fsBuffer,        /* buffer for information */
                sizeof(FSINFO));  /* size of buffer */
  if (rc != NO_ERROR) return 1;   /* at least drop what we already have got */
  /* we assume that DosQueryFSInfo works a second time */
  lua_rawsetstringstring(L, -1, "label", fsBuffer.vol.szVolLabel);  /* volume name (label) */
  rc = DosQueryFSInfo(driveno,        /* drive number */
                FSIL_ALLOC,           /* level 1 allocation info */
                (PVOID)&fsaBuffer,    /* buffer */
                sizeof(FSALLOCATE));  /* size of buffer */
  if (rc == NO_ERROR) {  /* allocation unit = cluster */
    idfs = fsaBuffer.idFileSystem;
    lua_rawsetstringnumber(L, -1, "idFileSystem",      idfs);                   /* unknown meaning */
    lua_rawsetstringnumber(L, -1, "totalclusters",     fsaBuffer.cUnit);        /* total allocation units */
    lua_rawsetstringnumber(L, -1, "freeclusters",      fsaBuffer.cUnitAvail);   /* free allocation units */
    lua_rawsetstringnumber(L, -1, "bytespersector",    fsaBuffer.cbSector);     /* bytes per sector */
    lua_rawsetstringnumber(L, -1, "sectorspercluster", fsaBuffer.cSectorUnit);  /* sectors per allocation unit */
    cbUnit =     (double)fsaBuffer.cSectorUnit * (double)fsaBuffer.cbSector;    /* bytes per allocation unit */
    freespace =  (double)fsaBuffer.cUnitAvail * cbUnit;
    totalspace = (double)fsaBuffer.cUnit * cbUnit;
    lua_rawsetstringnumber(L, -1, "freesize", freespace);
    lua_rawsetstringnumber(L, -1, "totalsize", totalspace);
  }
#elif defined(__DJGPP__)  /* 2.17.2 */
  struct diskfree_t d;
  unsigned int driveno, rc;
  size_t l;
  if (lua_isnoneornil(L, 1)) {  /* 7.8.2 extension */
    char cwd_buf[PATH_MAX];
    if (getcwd(cwd_buf, sizeof(cwd_buf)) != NULL && cwd_buf[0] != '\0') {
      default_drive[0] = cwd_buf[0]; default_drive[1] = ':'; default_drive[2] = '\0'; drivename = default_drive;
      l = 2;  /* Length of "X:" */
    } else {
      drivename = "C:"; l = 2;
    }
    aux_checkvaliddrive(L, drivename, l, "os.drivestat");  /* 2.39.11 */
    int drno = toupper(drivename[0]) - 'A' + 1;
    if (drno < 0 || drno > 26)
      luaL_error(L, "Error in " LUA_QS ": drive letter is invalid.", "os.drivestat");
    driveno = (unsigned int)drno;
  } else if (agn_isstring(L, 1)) {
    const char *dno = lua_tolstring(L, 1, &l);
    aux_checkvaliddrive(L, dno, l, "os.drivestat");  /* 2.39.11 */
    int drno = toupper(dno[0]) - 'A' + 1;
    if (drno < 0 || drno > 26)
      luaL_error(L, "Error in " LUA_QS ": drive letter is invalid.", "os.drivestat");
    driveno = (unsigned int)drno;
  } else {
    driveno = agn_checknonnegint(L, 1);  /* 0 = default drive, 1 = A:, 2 = B:, etc. */
  }
  rc = _dos_getdiskfree(driveno, &d);
  if (!rc) {
    static const char *types[] = {
      "unknown", "no root dir", "Removable", "Fixed", "Remote", "CD-ROM", "RAMDISK"};
    int fatsize, typeno;
    unsigned int freebytes, totalbytes, bytespercluster, mediatype;
    char str[9];  /* see DJGPP source file src/libc/dos/dos/getfatsz for an example */
    str[0] = '\0';
    luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
    char label[13];
    dos_drive_label(driveno, label);  /* 7.8.2 */
    lua_createtable(L, 0, 13);
    lua_rawsetstringstring(L, -1, "label", label);
    lua_rawsetstringnumber(L, -1, "totalclusters",     d.total_clusters);
    lua_rawsetstringnumber(L, -1, "freeclusters",      d.avail_clusters);
    lua_rawsetstringnumber(L, -1, "sectorspercluster", d.sectors_per_cluster);
    lua_rawsetstringnumber(L, -1, "bytespersector",    d.bytes_per_sector);
    bytespercluster = d.bytes_per_sector * d.sectors_per_cluster;
    freebytes =  bytespercluster * d.avail_clusters;
    totalbytes = bytespercluster * d.total_clusters;
    lua_rawsetstringnumber(L, -1, "totalsize",   totalbytes);  /* in bytes */
    lua_rawsetstringnumber(L, -1, "freesize",    freebytes);   /* in bytes */
    fatsize = _get_fat_size(driveno);
    lua_rawsetstringnumber(L, -1, "fatsize", (fatsize < 0) ? AGN_NAN : fatsize);
    lua_rawsetstringboolean(L, -1, "isfat32", _is_fat32(driveno) != 0);
    mediatype = _media_type(driveno);
    typeno = 0;                                           /* unknown */
    if (_is_ram_drive(driveno) != 0)         typeno = 6;  /* RAM drive */
    else if (_is_cdrom_drive(driveno) != 0)  typeno = 5;  /* CD-ROM */
    /* else if (_is_remote_drive(driveno) != 0) typeno = 4; */ /* remote, header file missing */
    else if (typeno == 0 && mediatype == 0)  typeno = 2;  /* removable */
    else if (typeno == 0 && mediatype == 1)  typeno = 3;  /* fixed */
    char driveletter[3] = { 0 };
    driveletter[0] = driveno + 'A' - 1;
    driveletter[1] = ':';
    lua_rawsetstringstring(L, -1, "driveletter", driveletter);
    lua_rawsetstringstring(L, -1, "drivetype", types[typeno]);
    rc = _get_fs_type(driveno, str);
    if (!rc) {
      lua_rawsetstringstring(L, -1, "filesystem", str);
    }
    lua_rawsetstringnumber(L, -1, "maxnamelength", 12);  /* 7.8.1 */
    lua_rawsetstringnumber(L, -1, "maxpathlength", 64);  /* 7.8.1 */
  } else {  /* invalid drive, mostly */
    lua_pushfail(L);
  }
#elif defined(__linux__)
  /* This section has been taken from Nodir Temirkhodjaev's LuaSys package using the
     online explanation of statfs at http://linux.die.net/man/2/statfs. Agena 2.4.1 */
  const char *path = luaL_optstring(L, 1, ".");
  int64_t ntotal, nfree;
  int res;
  struct statfs buf;
  res = statfs(path, &buf);
  if (!res) {
    ntotal = buf.f_blocks * buf.f_frsize;
    nfree = buf.f_bfree * buf.f_bsize;
    luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
    lua_createtable(L, 0, 2);
    lua_rawsetstringnumber(L, -1, "totalsize", (lua_Number)ntotal);
    lua_rawsetstringnumber(L, -1, "freesize", (lua_Number)nfree);
    lua_rawsetstringstring(L, -1, "filesystem", findfsentry(buf.f_type));
    lua_rawsetstringnumber(L, -1, "totalclusters", buf.f_blocks);
    lua_rawsetstringnumber(L, -1, "freeclusters", buf.f_bfree);
    lua_rawsetstringnumber(L, -1, "freeuserclusters", buf.f_bavail);
    lua_rawsetstringnumber(L, -1, "bytespersector", buf.f_bsize);
    lua_rawsetstringnumber(L, -1, "fragmentsize", buf.f_frsize);  /* since Linux 2.6 */
    lua_rawsetstringnumber(L, -1, "totalnodes", buf.f_files);
    lua_rawsetstringnumber(L, -1, "freenodes", buf.f_ffree);
    lua_rawsetstringnumber(L, -1, "maxnamelength", buf.f_namelen);
    lua_rawsetstringnumber(L, -1, "maxpathlength", 4096);
  } else
    lua_pushfail(L);
#elif defined(__SOLARIS)
  const char *path = luaL_optstring(L, 1, ".");
  int64_t ntotal, nfree;
  int res;
  struct statvfs buf;
  res = statvfs(path, &buf);
  if (!res) {
    ntotal = buf.f_blocks * buf.f_frsize;
    nfree = buf.f_bfree * buf.f_bsize;
    luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
    lua_createtable(L, 0, 12);
    lua_rawsetstringnumber(L, -1, "totalsize", (lua_Number)ntotal);
    lua_rawsetstringnumber(L, -1, "freesize", (lua_Number)nfree);
    lua_rawsetstringstring(L, -1, "filesystem", buf.f_basetype);
    lua_rawsetstringnumber(L, -1, "totalclusters", buf.f_blocks);
    lua_rawsetstringnumber(L, -1, "freeclusters", buf.f_bfree);
    lua_rawsetstringnumber(L, -1, "freeuserclusters", buf.f_bavail);
    lua_rawsetstringnumber(L, -1, "bytespersector", buf.f_bsize);
    lua_rawsetstringnumber(L, -1, "fragmentsize", buf.f_frsize);
    lua_rawsetstringnumber(L, -1, "totalfilenodes", buf.f_files);
    lua_rawsetstringnumber(L, -1, "freenodes", buf.f_ffree);
    lua_rawsetstringnumber(L, -1, "freeusernodes", buf.f_favail);
    lua_rawsetstringnumber(L, -1, "maxnamelength", buf.f_namemax);
    lua_rawsetstringnumber(L, -1, "maxpathlength", 1024);
    /* f_fname and f_fpack return garbage in Sun Solaris 10 u8 */
  } else
    lua_pushfail(L);
#elif defined(__APPLE__)  /* created by Gamini AI, 7.8.1 */
  const char *path = luaL_optstring(L, 1, ".");
  int64_t ntotal, nfree;
  int res;
  struct statfs buf;
  res = statfs(path, &buf);
  if (!res) {
    /* On macOS, f_bsize represents the optimal transfer block size */
    ntotal = (int64_t)buf.f_blocks * buf.f_bsize;
    nfree = (int64_t)buf.f_bfree * buf.f_bsize;
    luaL_checkstack(L, 2, "not enough stack space");
    lua_createtable(L, 0, 2);
    lua_rawsetstringnumber(L, -1, "totalsize", (lua_Number)ntotal);
    lua_rawsetstringnumber(L, -1, "freesize", (lua_Number)nfree);
    /* macOS provides the literal filesystem name string directly */
    lua_rawsetstringstring(L, -1, "filesystem", buf.f_fstypename);
    lua_rawsetstringnumber(L, -1, "totalclusters", buf.f_blocks);
    lua_rawsetstringnumber(L, -1, "freeclusters", buf.f_bfree);
    lua_rawsetstringnumber(L, -1, "freeuserclusters", buf.f_bavail);
    lua_rawsetstringnumber(L, -1, "bytespersector", buf.f_bsize);
    /* lua_rawsetstringnumber(L, -1, "fragmentsize", buf.f_frsize); does not exist on Mac OS X */
    lua_rawsetstringnumber(L, -1, "totalnodes", buf.f_files);
    lua_rawsetstringnumber(L, -1, "freenodes", buf.f_ffree);
    long max_name = pathconf(path, _PC_NAME_MAX);
    if (max_name == -1) max_name = MAXNAMLEN;  /* fallback to 255 if pathconf fails */
    lua_rawsetstringnumber(L, -1, "maxnamelength", max_name);
    lua_rawsetstringnumber(L, -1, "maxpathlength", 1024);
  } else {
    lua_pushfail(L);
  }
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* 7.8.1, In Windows, the function reads out BIOS and mainboard information and returns them in a table with the following
   fields and associated string entries:
    'BIOSVendor',
    'BIOSVersion',
    'BIOSReleaseDate',
    'SystemManufacturer',
    'SystemProductName',
    'SystemSerialNumber',
    'SystemSKU',
    'BaseBoardManufacturer',
    'BaseBoardProduct',
    'BaseBoardVersion'.
   The fields
    'BiosMajorRelease',
    'BiosMinorRelease'
   have number entries.
   If the Windows registry could not be accessed or the aforementioned registry keys are all missing, or on all other platforms,
   the function returns `fail`.
   7.8.1; created by Gemini AI */
static int os_bios (lua_State *L) {
#ifdef _WIN32
  const char* valueNames[] = {
    "BIOSVendor",
    "BIOSVersion",
    "BIOSReleaseDate",
    "SystemManufacturer",
    "SystemProductName",
    "SystemSerialNumber",
    "SystemSKU",
    "BaseBoardManufacturer",
    "BaseBoardProduct",
    "BaseBoardVersion"
  };
  LPCWSTR wValueNames[] = {
    L"BIOSVendor",
    L"BIOSVersion",
    L"BIOSReleaseDate",
    L"SystemManufacturer",
    L"SystemProductName",
    L"SystemSerialNumber",
    L"SystemSKU",
    L"BaseBoardManufacturer",
    L"BaseBoardProduct",
    L"BaseBoardVersion"
  };
  const char* dwordNames[] = {
    "BiosMajorRelease",
    "BiosMinorRelease"
  };
  LPCWSTR wDwordNames[] = {
    L"BiosMajorRelease",
    L"BiosMinorRelease"
  };
  int i, success = 0;
  HKEY hKey;
  LPCWSTR biosPath = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
  LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, biosPath, 0, KEY_READ, &hKey);
  if (result != ERROR_SUCCESS) {
    lua_pushfail(L);
    return 1;
  }
  lua_newtable(L);
  for (i=0; i < 10; i++) {
    WCHAR wDataBuffer[256] = { 0 };
    DWORD dataSize = sizeof(wDataBuffer);
    DWORD type = 0;
    result = RegQueryValueExW(hKey, wValueNames[i], NULL, &type, (LPBYTE)wDataBuffer, &dataSize);
    if (result == ERROR_SUCCESS && type == REG_SZ) {
      success++;
      char ansiDataBuffer[256] = { 0 };
      WideCharToMultiByte(CP_ACP, 0, wDataBuffer, -1, ansiDataBuffer, sizeof(ansiDataBuffer), NULL, NULL);
      lua_rawsetstringstring(L, -1, valueNames[i], ansiDataBuffer);
    } else {
      lua_rawsetstringstring(L, -1, valueNames[i], "unknown");
    }
  }
  for (i=0; i < 2; i++) {
    DWORD dwValue = 0;
    DWORD dataSize = sizeof(DWORD);
    DWORD type = 0;
    result = RegQueryValueExW(hKey, wDwordNames[i], NULL, &type, (LPBYTE)&dwValue, &dataSize);
    if (result == ERROR_SUCCESS && type == REG_DWORD) {
      success++;
      lua_rawsetstringnumber(L, -1, dwordNames[i], (unsigned long)dwValue);
    } else {
      lua_rawsetstringnumber(L, -1, dwordNames[i], AGN_NAN);
    }
  }
  RegCloseKey(hKey);
  if (!success) {
    agn_poptop(L);  /* drop entire table */
    lua_pushfail(L);
  }
#else
  lua_pushfail(L);
#endif
  return 1;
}


static mode_t newmode (mode_t oldmode, mode_t modeflag, int add) {
  mode_t modet = oldmode;
  if (add)
    modet |= modeflag;
  else if (modet & modeflag)
    modet -= modeflag;
  return modet;
}

#ifdef _WIN32
static DWORD winmode (DWORD oldmode, DWORD modeflag, int add) {
  DWORD winmodet = oldmode;
  if (add)
    winmodet |= modeflag;
  else if (winmodet & modeflag)
    winmodet -= modeflag;
  return winmodet;
}
#elif defined(__OS2__)
static unsigned int os2mode (unsigned int oldmode, unsigned int modeflag, int add) {
  unsigned int os2modet = oldmode;
  if (add)
    os2modet |= modeflag;
  else if (os2modet & modeflag)
    os2modet -= modeflag;
  return os2modet;
}
#endif


static int os_fattrib (lua_State *L) {  /* 0.26.0 */
  const char *fn, *mode;
  mode_t modet;
  struct stat entry;
  int result, owner, add;
  size_t l, i;
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
  size_t j;
#elif defined(_WIN32)
  DWORD winmodet;
#elif defined(__OS2__)
  static FILESTATUS3 fstat, *fs;
  unsigned int os2modet;
  APIRET rc;
#endif
  fn = agn_checkstring(L, 1);
  if (lua_istable(L, 2)) {  /* change file access and modification date, added OS/2 in 3.0.0 */
#if (defined(__OS2__) || defined(_WIN32) || defined(__unix__) || defined(__APPLE__) || defined(__DJGPP__))
    Time64_T t;
    struct utimbuf times;
    int year, month, day, hour, minute, second, nops, rc;
    year = agn_getinumber(L, 2, 1);
    month = agn_getinumber(L, 2, 2);
    day = agn_getinumber(L, 2, 3);
    nops = agn_size(L, 2);
    hour = (nops > 3) ? agn_getinumber(L, 2, 4) : 0;
    minute = (nops > 4) ? agn_getinumber(L, 2, 5) : 0;
    second = (nops > 5) ? agn_getinumber(L, 2, 6) : 0;
    t = tools_maketime(year, month, day, hour, minute, second, &rc);  /* 2.9.8, imported from agnhlps.c */
    if (rc == -1)  /* 2.6.1 fix, usually for dates earlier than 1970/1/1 */
      luaL_error(L, "Error in " LUA_QS ": could not determine date, may be out-of-range.", "os.fattrib");
    else if (rc == -2)  /* 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": time component out of range.", "os.fattrib");
    times.modtime = times.actime = t;
    if (utime(fn, &times) == -1)
      lua_pushfail(L);
    else
      lua_pushtrue(L);
#else
    lua_pushfail(L);
#endif
    return 1;
  }
  if (lua_type(L, 2) == LUA_TNUMBER) {  /* 2.6.1, octal mode given ? */
    int r = chmod(fn, (mode_t)agn_checkinteger(L, 2));
    lua_pushboolean(L, (r == -1) ? -1 : 1);
    return 1;
  }
  modet = owner = add = 0;
#if defined(_WIN32)
  winmodet = GetFileAttributes(fn);
#elif defined(__OS2__)
  rc = DosQueryPathInfo((PCSZ)fn, FIL_STANDARD, &fstat, sizeof(fstat));
  if (rc != 0)
    luaL_error(L, "Error in " LUA_QS ": could not determine file attributes.", "os.fattrib");
  fs = &fstat;
  os2modet = fs->attrFile;
#endif
  if (stat(fn, &entry) == 0)
    modet = entry.st_mode;
  else
    luaL_error(L, "Error in " LUA_QS ": file does not exist.", "os.fattrib");
  mode = agn_checklstring(L, 2, &l);
  if (l < 3)
    luaL_error(L, "Error in " LUA_QS ": malformed settings string.", "os.fattrib");
#if !defined(__unix__) && !defined(__APPLE__) && !defined(__HAIKU__)
  mode_t modew[1] = {S_IWUSR};
  mode_t modex[1] = {S_IXUSR};
#else
#ifndef __DJGPP__
  mode_t moder[3] = {S_IRUSR, S_IRGRP, S_IROTH};
#else
  mode_t moder[1] = {S_IRUSR};
#endif
  mode_t modew[3] = {S_IWUSR, S_IWGRP, S_IWOTH};
  mode_t modex[3] = {S_IXUSR, S_IXGRP, S_IXOTH};
#endif
  switch (*mode++) {
    case 'u': case 'U': {
      owner = 0;  /* owner */
      break;
    }
    case 'g': case 'G': {
      owner = 1;  /* group */
      break;
    }
    case 'o': case 'O': {
      owner = 2;  /* other */
      break;
    }
    case 'a': case 'A': {
      owner = 3;  /* all */
      break;
    }
    default:
      luaL_error(L,
        "Error in " LUA_QS ": wrong user specification, must be `u` , `g`, `o` or `a`.",
        "os.fattrib");
  }
#if ((!defined(__unix__) && !defined(__APPLE__) &&  !defined(__HAIKU__)) || defined(__DJGPP__))
  if (owner > 0) owner = 0;
#endif
  switch (*mode++) {
    case '+': {
      add = 1;  /* owner */
      break;
    }
    case '-': {
      add = 0;  /* group */
      break;
    }
    default:
      luaL_error(L,
        "Error in " LUA_QS ": wrong add/delete specification, must be `+` or `-`.",
        "os.fattrib");
  }
  for (i=2; i < l; i++, mode++) {
    switch(*mode) {
#if defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
      case 'r': case 'R': {
        if (owner < 3)
          modet = newmode(modet, moder[owner], add);
#ifndef __DJGPP__
        else
          for (j=0; j < 3; j++) { modet = newmode(modet, moder[j], add); }
#endif
        break;
      }
#endif
      case 'w': case 'W': {
        if (owner < 3)
          modet = newmode(modet, modew[owner], add);
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
        else
          for (j=0; j < 3; j++) { modet = newmode(modet, modew[j], add); }
#endif
        break;
      }
      case 'x': case 'X': {
        if (owner < 3)
          modet = newmode(modet, modex[owner], add);
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
        else
          for (j=0; j < 3; j++) { modet = newmode(modet, modex[j], add); }
#endif
        break;
      }
#ifdef _WIN32
      case 'h': case 'H': {
        winmodet = winmode(winmodet, FILE_ATTRIBUTE_HIDDEN, add);
        break;
      }
      case 'a': case 'A': {
        winmodet = winmode(winmodet, FILE_ATTRIBUTE_ARCHIVE, add);
        break;
      }
      case 's': case 'S': {
        winmodet = winmode(winmodet, FILE_ATTRIBUTE_SYSTEM, add);
        break;
      }
      case 'r': case 'R': {
        winmodet = winmode(winmodet, FILE_ATTRIBUTE_READONLY, add);
        break;
      }
#elif defined(__OS2__)
      case 'h': case 'H': {
        os2modet = os2mode(os2modet, FILE_HIDDEN, add);
        break;
      }
      case 'a': case 'A': {
        os2modet = os2mode(os2modet, FILE_ARCHIVED, add);
        break;
      }
      case 's': case 'S': {
        os2modet = os2mode(os2modet, FILE_SYSTEM, add);
        break;
      }
      case 'r': case 'R': {
        os2modet = os2mode(os2modet, FILE_READONLY, add);
        break;
      }
#endif
      default: lua_assert(0);
    }
  }
  result = chmod(fn, modet);
#ifdef _WIN32
  result = result || !(SetFileAttributes(fn, winmodet));
#elif defined(__OS2__)
  fs->attrFile = os2modet;
  rc = DosSetPathInfo((PCSZ)fn, FIL_STANDARD, fs, sizeof(FILESTATUS3), 0);
  result = result || rc;
#endif
  if (result == 0)
     lua_pushtrue(L);
  else
     lua_pushfail(L);
  return 1;
}


/* for Windows colour depth see:
   http://stackoverflow.com/questions/1276687/how-can-i-find-out-the-current-color-depth-of-a-machine-running-vista-w7
   and http://stackoverflow.com/questions/7068620/mingw-libssl-linking-errors for proper linking of gdi32 in MinGW. */

#ifdef _WIN32
/* from MSDN: `
  int GetDeviceCaps(
    _In_  HDC hdc,
    _In_  int nIndex );` */
static int gdc (int nIndex) {  /* 2.4.0 */
  HDC dc = GetDC(NULL);
  int r = GetDeviceCaps(dc, nIndex);
  ReleaseDC(NULL, dc);
  return r;
}

#define gsm GetSystemMetrics
#endif

static int os_screensize (lua_State *L) {  /* 0.31.3 */
#ifdef _WIN32
  agn_createpairnumbers(L, gsm(SM_CXSCREEN), gsm(SM_CYSCREEN));  /* 2.34.1 optimisation */
#elif defined(__OS2__)   /* 2.4.0 */
  VIOMODEINFO data;
  data.cb = sizeof(data);
  VioGetMode(&data, 0);
  agn_createpairnumbers(L, data.hres, data.vres);  /* 2.34.1 optimisation */
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* In eComStation and Windows, the function returns a table with the following information on the display:

resolution = a pair with the horizontal and vertical number of pixels,
depth = the colour depth in bits
monitors = the number of monitors attached to the system (Windows only)
vrefresh = the vertical refresh rate in Hertz (Windows only). */
#ifdef _WIN32
static int get_primary_gpu_name (char* outBuffer, int maxLen) {
  DISPLAY_DEVICEW dd;
  dd.cb = sizeof(DISPLAY_DEVICEW);
  DWORD deviceIndex = 0;
  /* Loop through all active display devices on the hardware bus */
  while (EnumDisplayDevicesW(NULL, deviceIndex, &dd, 0)) {
    /* Look for the primary display controller card actively feeding a monitor */
    if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
      /* dd.DeviceString contains the real name (e.g. "NVIDIA GeForce RTX 3060") */
      WideCharToMultiByte(CP_ACP, 0, dd.DeviceString, -1, outBuffer, maxLen, NULL, NULL);
      return 1;  /* Success! Primary GPU found. */
    }
    deviceIndex++;
  }
  /* Fallback: If no primary flag is caught, return the very first card found */
  if (deviceIndex > 0) {
    EnumDisplayDevicesW(NULL, 0, &dd, 0);
    WideCharToMultiByte(CP_ACP, 0, dd.DeviceString, -1, outBuffer, maxLen, NULL, NULL);
    return 1;
  }
  return 0;  /* No display adapters found (headless server context) */
}
#elif defined(__linux__)
/* Unprivileged Linux implementation reading raw PCI bus system flags */
static int get_primary_gpu_name (char* outBuffer, int maxLen) {
  /* Execute lspci to grab the default hardware display adapter label string */
  FILE *fp = popen("lspci | grep -E 'VGA|3D|Display'", "r");
  if (!fp) return 0;
  char line[256] = {0};
  /* Grab the first matched primary display controller card layout */
  if (fgets(line, sizeof(line), fp)) {
    /* lspci returns: 01:00.0 VGA compatible controller: NVIDIA Corporation TU117 [GeForce GTX 1650]
       We isolate everything following the second colon marker */
    char *colon = strchr(line, ':');
    if (colon) colon = strchr(colon + 1, ':');  /* Step past the bus address colon */
    if (colon) {
      char *gpuStart = colon + 2;  /* Jump past the colon and space character.
      Strip off the trailing newline character byte safely */
      gpuStart[strcspn(gpuStart, "\r\n")] = '\0';
      strncpy(outBuffer, gpuStart, maxLen - 1);
      outBuffer[maxLen - 1] = '\0';
      pclose(fp);
      return 1;
    }
  }
  pclose(fp);
  return 0;
}
#elif !defined(__unix__) && !defined(__APPLE__) && !defined(__DJGPP__) && !defined(__OS2__)
static int get_primary_gpu_name (char* outBuffer, int maxLen) {
  strncpy(outBuffer, "Unknown Platform GPU", maxLen);
  return 0;
}
#endif

static int os_vga (lua_State *L) {  /* 2.4.0 */
#ifdef _WIN32
  char gpuBuffer[128] = { 0 };
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  lua_createtable(L, 0, 4);
  lua_rawsetstringpairnumbers(L, -1, "resolution", gsm(SM_CXSCREEN), gsm(SM_CYSCREEN));
  lua_rawsetstringnumber(L, -1, "depth", gdc(BITSPIXEL));
  lua_rawsetstringnumber(L, -1, "vrefresh", gdc(VREFRESH));
  lua_rawsetstringnumber(L, -1, "monitors", gsm(SM_CMONITORS));
  /* Hardened Console check, 7.5.15 */
  if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &csbi)) {
    int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    lua_rawsetstringpairnumbers(L, -1, "dimension", rows, cols);
  } else {
    lua_rawsetstringpairnumbers(L, -1, "dimension", 0, 0);
  }
  if (get_primary_gpu_name(gpuBuffer, sizeof(gpuBuffer))) {
    lua_rawsetstringstring(L, -1, "primarygpu", gpuBuffer);
  }
#elif __OS2__
  lua_createtable(L, 0, 4);
  VIOMODEINFO data;
  data.cb = sizeof(data);
  VioGetMode(&data, 0);
  lua_rawsetstringpairnumbers(L, -1, "resolution", data.hres, data.vres);
  lua_rawsetstringpairnumbers(L, -1, "dimension", data.row, data.col);  /* 2.17.3 */
  lua_rawsetstringnumber(L, -1, "colours", tools_intpow(2, data.color));  /* 2.17.3 */
  lua_rawsetstringnumber(L, -1, "depth", data.color);  /* 2.17.3 */
#elif defined(__DJGPP__)  /* DJGPP only, 2.17.3 */
  lua_createtable(L, 0, 2);
  lua_rawsetstringpairnumbers(L, -1, "dimension", _farpeekb(0x0040, 0x0084) + 1, _farpeekb(0x0040, 0x004A));  /* rows:columns */
  lua_rawsetstringnumber(L, -1, "mode", _farpeekb(0x0040, 0x0049));  /* screen mode */
#elif defined(__linux__)
  /* taken from:
     https://stackoverflow.com/questions/23369503/get-size-of-terminal-window-rows-columns, by herohuyongtao */
  char gpuBuffer[128] = { 0 };
  struct winsize w;
  lua_createtable(L, 0, 1);
  /* Hardened ioctl check */
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
    lua_rawsetstringpairnumbers(L, -1, "dimension", w.ws_row, w.ws_col);
  } else {  /* 7.5.15 fix */
    lua_rawsetstringpairnumbers(L, -1, "dimension", 0, 0);
  }
  if (get_primary_gpu_name(gpuBuffer, sizeof(gpuBuffer))) {
    lua_rawsetstringstring(L, -1, "primarygpu", gpuBuffer);
  }
  /* 7.8.1, created by Gemini AI */
  Display *dir = XOpenDisplay(NULL);
  if (dir) {
    int screen = DefaultScreen(dir);
    Window root = RootWindow(dir, screen);
    /* 1. Resolution */
    int width = DisplayWidth(dir, screen);
    int height = DisplayHeight(dir, screen);
    /* 2. Depth */
    int depth = DefaultDepth(dir, screen);
    /* 3. Refresh Rate (Requires XRandR extension) */
    short vrefresh = 0;
    XRRScreenConfiguration *sc = XRRGetScreenInfo(dir, root);
    if (sc) {
      vrefresh = XRRConfigCurrentRate(sc);
      XRRFreeScreenConfigInfo(sc);
    }
    /* 4. Monitors Count (Using Xinerama or XRandR) */
    int monitors = 1;
#if !defined(OPENSUSE)
    XRRMonitorInfo *monitors_info = XRRGetMonitors(dir, root, True, &monitors);
    if (monitors_info) {
      XRRFreeMonitors(monitors_info);  /* Explicitly free the 64-byte block */
    }
#endif
    lua_rawsetstringpairnumbers(L, -1, "resolution", width, height);
    lua_rawsetstringnumber(L, -1, "depth", depth);
    lua_rawsetstringnumber(L, -1, "vrefresh", (double)vrefresh);
    lua_rawsetstringnumber(L, -1, "monitors", monitors);
    XCloseDisplay(dir);
  }
#elif defined(__APPLE__)  /* new 7.8.1, created by Gemini AI */
  char gpuBuffer[128] = { "Unknown GPU" };
  struct winsize w;
  lua_createtable(L, 0, 1);
  /* 1. Terminal dimensions */
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
    lua_rawsetstringpairnumbers(L, -1, "dimension", w.ws_row, w.ws_col);
  } else {
    lua_rawsetstringpairnumbers(L, -1, "dimension", 0, 0);
  }
  /* 2. Primary GPU Name via Broad Layer Traversal */
  io_iterator_t iterator;
  int gpu_found = 0;
  /* Attempt 1: Core Graphics Framebuffer Layer Mapping */
  CGDirectDisplayID mainDisplay = CGMainDisplayID();
  io_service_t displayService = CGDisplayIOServicePort(mainDisplay);
  if (displayService) {
    io_registry_entry_t parentEntry;
    /* Walk up the driver registry tree to locate the parent chipset provider entry */
    if (IORegistryEntryGetParentEntry(displayService, kIOServicePlane, &parentEntry) == kIOReturnSuccess) {
      CFTypeRef model = IORegistryEntryCreateCFProperty(parentEntry, CFSTR("model"), kCFAllocatorDefault, 0);
      if (model && CFGetTypeID(model) == CFDataGetTypeID()) {
        size_t len = CFDataGetLength((CFDataRef)model);
        if (len < sizeof(gpuBuffer)) {
          memcpy(gpuBuffer, CFDataGetBytePtr((CFDataRef)model), len);
          gpuBuffer[len] = '\0';
          gpu_found = 1;
        }
      }
      if (model) CFRelease(model);
      IOObjectRelease(parentEntry);
    }
  }
  /* Attempt 2: Fallback to vintage IOAccelerator controller loops if Framebuffer is blank */
  if (!gpu_found) {
    if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("IOAccelerator"), &iterator) == kIOReturnSuccess) {
      io_registry_entry_t regEntry = IOIteratorNext(iterator);
      if (regEntry) {
        CFTypeRef model = IORegistryEntryCreateCFProperty(regEntry, CFSTR("model"), kCFAllocatorDefault, 0);
        if (model && CFGetTypeID(model) == CFDataGetTypeID()) {
          size_t len = CFDataGetLength((CFDataRef)model);
          if (len < sizeof(gpuBuffer)) {
            memcpy(gpuBuffer, CFDataGetBytePtr((CFDataRef)model), len);
            gpuBuffer[len] = '\0';
            gpu_found = 1;
          }
        }
        if (model) CFRelease(model);
        IOObjectRelease(regEntry);
      }
      IOObjectRelease(iterator);
    }
  }
  /* Attempt 3: Safe hardcoded fallback profile for early 2009 Mini architectures */
  if (!gpu_found) {
    snprintf(gpuBuffer, sizeof(gpuBuffer), "unknown");
  }
  /* Final trim to clear open-firmware space padding */
  size_t cleanLen = strlen(gpuBuffer);
  while (cleanLen > 0 && (gpuBuffer[cleanLen - 1] == ' ' || gpuBuffer[cleanLen - 1] == '\0')) {
    gpuBuffer[--cleanLen] = '\0';
  }
  lua_rawsetstringstring(L, -1, "primarygpu", gpuBuffer);
  /* 3. Query CoreGraphics for Video Metrics */
  mainDisplay = CGMainDisplayID();
  lua_rawsetstringpairnumbers(L, -1, "resolution", (double)CGDisplayPixelsWide(mainDisplay), (double)CGDisplayPixelsHigh(mainDisplay));
  lua_rawsetstringnumber(L, -1, "depth", (double)CGDisplayBitsPerPixel(mainDisplay));
  double vrefresh = 0.0;
  CFDictionaryRef mode = CGDisplayCurrentMode(mainDisplay);
  if (mode) {
    CFNumberRef refreshRef = (CFNumberRef)CFDictionaryGetValue(mode, kCGDisplayRefreshRate);
    if (refreshRef) {
      CFNumberGetValue(refreshRef, kCFNumberDoubleType, &vrefresh);
    }
  }
  lua_rawsetstringnumber(L, -1, "vrefresh", vrefresh);
  uint32_t monitorCount = 0;
  CGGetActiveDisplayList(0, NULL, &monitorCount);
  lua_rawsetstringnumber(L, -1, "monitors", (double)monitorCount);
#elif defined(__SOLARIS)
  char gpuBuffer[128] = "Unknown GPU";
  struct winsize w;
  lua_createtable(L, 0, 1);
  /* 1. Terminal Window Dimensions */
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
    lua_rawsetstringpairnumbers(L, -1, "dimension", w.ws_row, w.ws_col);
  } else {
    lua_rawsetstringpairnumbers(L, -1, "dimension", 0, 0);
  }
  /* 2. Primary GPU Name Retrieval via Solaris prtconf */
  FILE *fp = popen("/usr/sbin/prtconf -v 2>/dev/null | grep -i \"display\"", "r");
  if (fp) {
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
      /* Solaris prtconf reports display nodes like: 'display, instance #0'
         We scan for the specific device banner name bound to it */
      if (strstr(line, "display") || strstr(line, "VGA")) {
        /* Fallback or explicit parsing can extract names like "NVIDIA" or "Sun XVR" */
        snprintf(gpuBuffer, sizeof(gpuBuffer), "Solaris Display Adapter");
        break;
      }
    }
    pclose(fp);
  }
  lua_rawsetstringstring(L, -1, "primarygpu", gpuBuffer);
  /* 3. Query X11 for Video Metrics (Same as Linux) */
  Display *dir = XOpenDisplay(NULL);
  if (dir) {
    int screen = DefaultScreen(dir);
    /* Resolution & Depth */
    int width = DisplayWidth(dir, screen);
    int height = DisplayHeight(dir, screen);
    int depth = DefaultDepth(dir, screen);
    /* int monitors = 1; */
    lua_rawsetstringpairnumbers(L, -1, "resolution", width, height);
    lua_rawsetstringnumber(L, -1, "depth", depth);
    XCloseDisplay(dir);
  }
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* In Windows, the function returns various information on the attached mouse by returning a table with the following
   entries:

   'mousebuttons' = number of mouse buttons; if more than one mouse is attached, the sum of all mouse
                    buttons is computed.
   'hmousewheel'  = `true` if the mouse features a horizontal mouse wheel, and `false` if not.
   'mousewheel'   = `true` if the mouse features a vertical mouse wheel, and `false` if not.
   'swapbuttons'  = `true` if the left and right mouse buttons have been swapped.
   'speed'        = an integer between 1 (slowest) and 20 (fastest).
   'threshold'    = the two mouse threshold values, x and y coordinates, as a pair of two numbers. */
static int os_mouse (lua_State *L) {  /* 2.4.0 */
#ifdef _WIN32
#ifndef SM_MOUSEHORIZONTALWHEELPRESENT
#define SM_MOUSEHORIZONTALWHEELPRESENT 91
#endif
  int mouseinfo[3];
  int mousespeed;
  lua_createtable(L, 0, 6);
  lua_rawsetstringnumber(L, -1, "mousebuttons", gsm(SM_CMOUSEBUTTONS));  /* number of mouse buttons */
  lua_rawsetstringboolean(L, -1, "hmousewheel", gsm(SM_MOUSEHORIZONTALWHEELPRESENT));
  /* 2.4.0; MSDN: `Nonzero if a mouse with a horizontal scroll wheel is installed; otherwise 0.` */
  lua_rawsetstringboolean(L, -1, "mousewheel", gsm(SM_MOUSEWHEELPRESENT));
  /* 2.4.0; MSDN: `Nonzero if a mouse with a vertical scroll wheel is installed; otherwise 0.` */
  lua_rawsetstringboolean(L, -1, "swapbutton", gsm(SM_SWAPBUTTON));
  /* 2.4.0; MSDN: `Nonzero if the meanings of the left and right mouse buttons are swapped; otherwise, 0.` */
  /* see http://msdn.microsoft.com/en-us/library/windows/desktop/ms724385%28v=vs.85%29.aspx */
  if (SystemParametersInfo(SPI_GETMOUSESPEED, 0, &mousespeed, 0))
    lua_rawsetstringnumber(L, -1, "speed", mousespeed);  /* from MSDN: `Retrieves the current mouse speed.
      The mouse speed determines how far the pointer will move based on the distance the mouse moves. The
      pvParam parameter must point to an integer that receives a value which ranges between 1 (slowest) and
      20 (fastest). A value of 10 is the default.
      On Windows 7, the alternative SystemParametersInfo(SPI_GETMOUSESPEED, 0, &mousespeed, 0) call definitely
      retrieves a wrong result to mousespeed[2]. */
  if (SystemParametersInfo(SPI_GETMOUSE, 0, &mouseinfo, 0))
    lua_rawsetstringpairnumbers(L, -1, "threshold", mouseinfo[0], mouseinfo[1]);  /* the two mouse threshold
      values, x and y coordinates */
#elif defined(__OS2__)  /* 2.17.3 */
  USHORT nbuttons, state, NumberOfMickeys;
  APIRET rc;
  THRESHOLD threshold;
  SCALEFACT scale;
  HMOU mhd = agn_checkinteger(L, 1);  /* mouse handle */
  rc = MouGetNumButtons(&nbuttons, mhd);
  if (rc != NO_ERROR)  /* mouse not found ? */
    luaL_error(L, "Error in " LUA_QS ": could not find mouse (%u).", "os.mouse", rc);
  lua_createtable(L, 0, 6);
  lua_rawsetstringnumber(L, -1, "mousebuttons", (rc == NO_ERROR) ? nbuttons : AGN_NAN);
  rc = MouGetNumMickeys(&NumberOfMickeys, mhd);
  /* number of mickeys per centimeter; a mickey is the amount that a mouse has to move for it to report that it has moved. 2.17.4 */
  lua_rawsetstringnumber(L, -1, "mickeys", (rc == NO_ERROR)? NumberOfMickeys : AGN_NAN);
  rc = MouGetDevStatus(&state, mhd);
  lua_rawsetstringboolean(L, -1, "inmickeys", (rc == NO_ERROR) ? tools_getuint32bit(state, 9 + 1) : -1);  /* get bit 10, mouse data in mickeys, not "pels"; 2.17.4; MOUSE_MICKEYS */
  rc = MouGetThreshold(&threshold, mhd);
  lua_rawsetstringnumber(L, -1, "threshold", (rc == NO_ERROR)? threshold.Length : AGN_NAN);  /* 2.17.4 */
  rc = MouGetScaleFact(&scale, mhd);
  lua_rawsetstringnumber(L, -1, "rowscale",    (rc == NO_ERROR)? scale.rowScale : AGN_NAN);  /* row scaling factor, 2.17.4 */
  lua_rawsetstringnumber(L, -1, "columnscale", (rc == NO_ERROR)? scale.colScale : AGN_NAN);  /* column scaling factor, 2.17.4 */
#elif defined(__DJGPP__)  /* 2.17.3 */
  int mouseconnected, nbuttons;
  union REGS regs;
  nbuttons = -1;
  regs.w.ax = 0;
  int86xx(0x33, &regs);
  mouseconnected = regs.w.ax;
  if (mouseconnected) {
    nbuttons = regs.w.bx;
    if (nbuttons == 0) nbuttons = 3;
    if (nbuttons == 0xffff ) nbuttons = 2;
  }
  lua_createtable(L, 0, 1);
  lua_rawsetstringnumber(L, -1, "mousebuttons", (nbuttons != -1) ? nbuttons : AGN_NAN);
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* Flushes the mouse queue and returns true on success and false otherwise */
#ifdef __OS2__
static int os_mouseopen (lua_State *L) {  /* 2.17.4 */
  APIRET rc;
  HMOU mhd;
  rc = MouOpen(0L, &mhd);
  if (rc != NO_ERROR)  /* mouse not found ? */
    luaL_error(L, "Error in " LUA_QS ": could not find mouse (%u).", "os.mouseopen", rc);
  else {
    USHORT events = MOUSE_MOTION |
                    MOUSE_MOTION_WITH_BN1_DOWN | MOUSE_BN1_DOWN |
                    MOUSE_MOTION_WITH_BN2_DOWN | MOUSE_BN2_DOWN |
                    MOUSE_MOTION_WITH_BN3_DOWN | MOUSE_BN3_DOWN;
    rc = MouSetEventMask(&events, mhd);
    if (rc != NO_ERROR)
      luaL_error(L, "Error in " LUA_QS ": could not initialise mouse (%u).", "os.mouseopen", rc);
  }
  lua_pushinteger(L, mhd);
  return 1;
}


static int os_mouseclose (lua_State *L) {  /* 2.17.4 */
  APIRET rc;
  HMOU mhd = agn_checkinteger(L, 1);  /* mouse handle */
  rc = MouClose(mhd);
  if (rc != NO_ERROR)
    luaL_error(L, "Error in " LUA_QS ": could not close mouse (%u).", "os.mouseclose", rc);
  return 0;
}
#endif


static int os_mouseflush (lua_State *L) {  /* 2.17.4 */
#if defined(__OS2__)
  APIRET rc;
  HMOU mhd = agn_checkinteger(L, 1);  /* mouse handle */
  rc = MouFlushQue(mhd);
  lua_pushboolean(L, rc == NO_ERROR);
#elif defined(_WIN32)  /* 7.5.15 extension */
  /* Discards all pending mouse and keyboard events currently stored in the console's input buffer.
     This is useful for clearing "stale" input data (such as clicks or key presses that occurred
     before a specific action) to ensure that the next input read by the application is fresh. */
  INPUT_RECORD record;
  DWORD numRead;
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  if (hIn != INVALID_HANDLE_VALUE) {  /* drain the input buffer */
    while (PeekConsoleInput(hIn, &record, 1, &numRead) && numRead > 0) {
      ReadConsoleInput(hIn, &record, 1, &numRead);
    }
  }
  lua_pushboolean(L, hIn != INVALID_HANDLE_VALUE);
#else
  lua_pushfail(L);
#endif
  return 1;
}


#ifdef __OS2__
/* Returns the current paths to be searched before and after system LIBPATH, when trying to locate DLLs.
   The first return is the path to be searched before the LIBPATH, the second one after the LIBPATH.
   See also: os.setextlibpath. */
static int os_getextlibpath (lua_State *L) {  /* 2.17.4 */
  const unsigned char *extpath = NULL;
  APIRET rc;
  rc = DosQueryExtLIBPATH(extpath, BEGIN_LIBPATH);
  lua_pushstring(L, (rc == NO_ERROR) ? (const char *)extpath : "unknown");  /* path is searched before the LIBPATH */
  extpath = NULL;
  rc = DosQueryExtLIBPATH(extpath, END_LIBPATH);
  lua_pushstring(L, (rc == NO_ERROR) ? (const char *)extpath : "unknown");  /* path is searched after the LIBPATH */
  return 2;
}


/* Sets the path to be searched before and after system LIBPATH, when trying to locate DLLs. If option is 0,
   then the path is set to the beginning of LIBPATH; if it is non-zero, the path is set to its end.
   The function returns `true` on success and `false` otherwise. See also: os.getextlibpath. */
static int os_setextlibpath (lua_State *L) {  /* 2.17.4 */
  int option;
  PSZ extpath;
  APIRET rc;
  extpath = (PSZ)agn_checkstring(L, 1);
  option = agnL_optinteger(L, 2, 0);
  rc = DosSetExtLIBPATH(extpath, (option == 0) ? BEGIN_LIBPATH: END_LIBPATH);
  lua_pushboolean(L, rc == NO_ERROR);
  return 1;
}
#endif


#if defined(_WIN32) || defined(__OS2__) || defined(__DJGPP__)
static int os_mousestate (lua_State *L) {
  int mask, inmotion, toclient, offset;
  lua_Number waitdur;
#ifdef __OS2__
  offset = 1;  /* in OS/2, we need the mouse handle as first argument */
#else
  offset = 0;
#endif
  mask = agnL_optinteger(L, 1 + offset, 0);
  waitdur = agnL_optnonnegative(L, 2 + offset, 0.001);
  inmotion = tools_getbit(mask, 1);  /* return mouse-has-been moved `motion` indicator (default: false) */
  toclient = tools_getbit(mask, 2);  /* try to transform absolute to window coordinates (default: false) */
#if defined(__DJGPP__)
  union REGS r;
  int mouseconnected, row, col, nbuttons;
  (void)inmotion; (void)toclient; (void)waitdur;  /* to avoid compiler warnings */
  r.w.ax = 0;
  int86xx(0x33, &r);
  mouseconnected = r.w.ax;  /* mouse present */
  if (mouseconnected) {
    int i;
    lua_createtable(L, 0, 5);
    /* number of buttons */
    nbuttons = r.w.bx;
    if (nbuttons == 0) nbuttons = 3;
    if (nbuttons == 0xffff) nbuttons = 2;
    /* show mouse */
    r.w.ax = 1;
    int86xx(0x33, &r);
    /* position */
    r.w.ax = 3;
    int86xx(0x33, &r);
    row = r.w.dx >> 3;
    col = r.w.cx >> 3;
    lua_rawsetstringnumber(L, -1, "row", row);
    lua_rawsetstringnumber(L, -1, "column", col);
    /* state of the buttons */
    lua_pushstring(L, "states");
    lua_createtable(L, nbuttons, 0);
    for (i=0; i < nbuttons; i++)
      agn_setinumber(L, -1, i + 1, r.w.bx & (1 << i));
    lua_rawset(L, -3);
    /* buttons pressed ? */
    lua_pushstring(L, "pressed");
    lua_createtable(L, nbuttons, 0);
    for (i=0; i < nbuttons; i++) {
      r.w.ax = 5;
      r.w.bx = i;
      int86xx(0x33, &r);
      lua_pushboolean(L, r.w.bx);
      lua_rawseti(L, -2, i + 1);
    }
    lua_rawset(L, -3);
    /* buttons released ? */
    lua_pushstring(L, "released");
    lua_createtable(L, nbuttons, 0);
    for (i=0; i < nbuttons; i++) {
      r.w.ax = 6;
      r.w.bx = i;
      int86xx(0x33, &r);
      lua_pushboolean(L, r.w.bx);
      lua_rawseti(L, -2, i + 1);
    }
    lua_rawset(L, -3);
    /* hide mouse */
    r.w.ax = 2;
    int86xx(0x33, &r);
  } else
    lua_pushfail(L);
#elif defined(__OS2__)
  PTRLOC PtrPos;  /* pointer to the mouse pointer data structure. */
  USHORT EventMask, State;
  APIRET  rc;
  (void)toclient; (void)waitdur;  /* to avoid compiler warnings */
  HMOU mhd = agn_checkinteger(L, 1);  /* mouse handle */
  lua_createtable(L, 0, 5);
  /* see: http://www.edm2.com/index.php/MouGetPtrPos */
  rc = MouGetPtrPos(&PtrPos, mhd);
  if (rc != NO_ERROR)
    luaL_error(L, "Error in " LUA_QS ": could not find mouse (%u).", "os.mousestate", rc);
  lua_rawsetstringnumber(L, -1, "row",    (rc == NO_ERROR) ? PtrPos.row : AGN_NAN);
  lua_rawsetstringnumber(L, -1, "column", (rc == NO_ERROR) ? PtrPos.col : AGN_NAN);
  /* See: http://www.edm2.com/index.php/MouGetEventMask:
  bit   description
  7-31  reserved (0)
  6   Report button-3 press/release events, without mouse motion
  5   Report button-3 press/release events, with mouse motion
  4   Report button-2 press/release events, without mouse motion
  3   Report button-2 press/release events, with mouse motion
  2   Report button-1 press/release events, without mouse motion
  1   Report button-1 press/release events, with mouse motion
  0   Report mouse motion events with no button press/release events */
  EventMask = 0;
  USHORT events = MOUSE_MOTION |
                  MOUSE_MOTION_WITH_BN1_DOWN | MOUSE_BN1_DOWN |
                  MOUSE_MOTION_WITH_BN2_DOWN | MOUSE_BN2_DOWN |
                  MOUSE_MOTION_WITH_BN3_DOWN | MOUSE_BN3_DOWN;
  rc = MouSetEventMask(&events, mhd);
  if (rc != NO_ERROR)
    luaL_error(L, "Error in " LUA_QS ": could not initialise mouse (%u).", "os.mousestate", rc);
  rc = MouGetEventMask(&EventMask, mhd);
  if (rc == NO_ERROR) {
    if (inmotion) {
      lua_rawsetstringboolean(L, -1, "motion",
        tools_getbit(EventMask, 1) ||  /* get same result as in Windows */
        tools_getbit(EventMask, 2) ||
        tools_getbit(EventMask, 4) ||
        tools_getbit(EventMask, 6)
      );
    }
    lua_rawsetstringnumber(L, -1, "eventmask", EventMask);
    lua_pushstring(L, "buttons");
    lua_createtable(L, 3, 0);
    lua_rawsetiboolean(L, -1, 1, tools_getbit(EventMask, 2) | tools_getbit(EventMask, 3));
    lua_rawsetiboolean(L, -1, 2, tools_getbit(EventMask, 4) | tools_getbit(EventMask, 5));
    lua_rawsetiboolean(L, -1, 3, tools_getbit(EventMask, 6) | tools_getbit(EventMask, 7));
    lua_rawset(L, -3);
  } else {
    int i;
    if (inmotion) {
      lua_rawsetstringboolean(L, -1, "motion", -1);  /* set fail */
    }
    lua_rawsetstringnumber(L, -1, "eventmask", AGN_NAN);
    lua_pushstring(L, "buttons");
    lua_createtable(L, 3, 0);
    for (i=0; i < 3; i++) {
      lua_rawsetiboolean(L, -1, 1, -1);
    }
    lua_rawset(L, -3);
  }
  rc = MouGetDevStatus(&State, mhd);
  if (rc == NO_ERROR) {
    lua_rawsetstringboolean(L, -1, "flush", tools_getuint32bit(State, 2 + 1));  /* flush is in progress; MOUSE_FLUSH */
    lua_rawsetstringboolean(L, -1, "blockread", tools_getuint32bit(State, 1 + 1));  /* block read is in progress; MOUSE_BLOCKREAD */
    lua_rawsetstringboolean(L, -1, "eventqueuebusywithio", tools_getuint32bit(State, 0 + 1));  /* MOUSE_QUEUEBUSY */
  }
#elif defined(_WIN32)
  BOOL rc;
  POINT lpPoint, lpPoint1;
  int winver = tools_getwinver();
  if (winver < MS_WIN2K) {  /* Windows NT 4.0 and earlier are not supported */
    lua_pushfail(L);
    return 1;
  }
  lua_createtable(L, 0, 5);
  /* See: https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorpos &
          https://docs.microsoft.com/en-gb/windows/win32/menurc/using-cursors */
  rc = GetCursorPos(&lpPoint);
  if (toclient) { ScreenToClient(NULL, &lpPoint); }  /* convert to client coordinates, does not work */
  lua_rawsetstringnumber(L, -1, "row",    (rc != 0) ? lpPoint.x : AGN_NAN);
  lua_rawsetstringnumber(L, -1, "column", (rc != 0) ? lpPoint.y : AGN_NAN);
  if (rc != 0 && inmotion) {
    tools_wait(waitdur);  /* XXX I am too stupid to get the WM_MOUSEMOVE event, but this will do, as well */
    GetCursorPos(&lpPoint1);
    if (toclient) { ScreenToClient(NULL, &lpPoint); } /* convert to client coordinates, does not work */
    lua_rawsetstringboolean(L, -1, "motion", (lpPoint.x - lpPoint1.x != 0) || (lpPoint.y - lpPoint1.y != 0));
  } else if (inmotion) {
    lua_rawsetstringboolean(L, -1, "motion", -1);  /* set fail */
  }
  lua_pushstring(L, "buttons");
  lua_createtable(L, 3, 0);
  lua_rawsetiboolean(L, -1, 1, (GetKeyState(VK_LBUTTON) & 0x800) != 0);  /* left button */
  lua_rawsetiboolean(L, -1, 2, (GetKeyState(VK_MBUTTON) & 0x800) != 0);  /* middle button */
  lua_rawsetiboolean(L, -1, 3, (GetKeyState(VK_RBUTTON) & 0x800) != 0);  /* right button */
  lua_rawset(L, -3);
#else
  lua_pushfail(L);
#endif
  return 1;
}
#endif


/* The function returns `true` if the system is connected to any network, and `false` otherwise. The function
   is available in Windows, only. The result is usually `true`. */
static int os_hasnetwork (lua_State *L) {  /* 2.4.0 */
#ifdef _WIN32
  lua_pushboolean(L, tools_getbit(gsm(SM_NETWORK), 1));  /* bit #1 contains the necessary information */
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* The function returns `true` if the computer is in docking mode, and `false` otherwise. The function is available in
   Windows, only. */
static int os_isdocked (lua_State *L) {  /* 2.4.0 */
#ifdef _WIN32
  lua_pushboolean(L, gsm(0x2004) != 0);  /* 0x2004 = SM_SYSTEMDOCKED */
  /* from MSDN: `Reflects the state of the docking mode, 0 for Undocked Mode and non-zero otherwise.` */
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* The function switches the monitor on and off (Windows and Linux), and can also put it on stand-by if the monitor
   supports this feature (Windows only).

   Pass the string 'off' as the only argument to switch off the monitor; pass 'on' to switch it on, and 'standby'
   to put it into stand-by mode. If no argument is given, the Monitor is switched on (which has no effect, if the
   screen is already active).

   On success, the function returns `true`, and `false` and a string containing the error analysis otherwise. */

#ifdef _WIN32
#ifndef SC_MONITORPOWER
#define SC_MONITORPOWER 0xF170
#endif

#ifndef HWND_BROADCAST
#define HWND_BROADCAST 0xFFFF  /* or just call GetConsoleWindow() */
#endif

#ifndef WM_SYSCOMMAND
#define WM_SYSCOMMAND 0x0112
#endif
#endif

#ifdef _WIN32
typedef EXECUTION_STATE (WINAPI *pSetThreadExecutionState)(EXECUTION_STATE);
#endif

static int os_monitor (lua_State *L) {
#if defined(_WIN32)
  const char *command = luaL_optstring(L, 1, "on");
  int rc = 0, cmd = 0; /* to prevent compiler warnings */
  if (tools_streq(command, "on"))
    cmd = -1;
  /* standby seems to be supported by few monitors, only; 7.5.17 restoration fix */
  else if (tools_streq(command, "standby"))
    cmd = 1;
  else if (tools_streq(command, "off"))
    cmd = 2;
  else
    luaL_error(L, "Error in " LUA_QS ": unknown argument " LUA_QS ".", "os.monitor", command);
  /* SendMessage is universal across all Windows versions */
  SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, cmd);
  if (cmd != -1) {  /* 1 or 2: switch to standby or off */
    rc = 1;
  } else {  /* -1: switch on */
    /* On modern systems (XP+), also use this to ensure the OS knows we need the screen */
    struct WinVer winversion;
    if (getWindowsVersion(&winversion) == MS_WIN2K) {  /* 7.5.17, restoration of functionality for W2K, patched */
      /* work-around for Windows for wake-up, 2.39.1 fix, see:
      https://stackoverflow.com/questions/12572441/sendmessage-sc-monitorpower-wont-turn-monitor-on-when-running-windows-8/14068239#14068239 */
      /* try to emulate a mouse movement */
      mouse_event(MOUSEEVENTF_MOVE, 0, 1, 0, 0);   /* one pixel up ... */
      Sleep(40);  /* 40 milliseconds */
      mouse_event(MOUSEEVENTF_MOVE, 0, -1, 0, 0);  /* ... and one pixel down */
      rc = 1;
    } else {
      HMODULE hKernel32 = GetModuleHandle("kernel32.dll");
      pSetThreadExecutionState pSetState = (pSetThreadExecutionState)GetProcAddress(hKernel32, "SetThreadExecutionState");
      if (pSetState) {
        pSetState(ES_DISPLAY_REQUIRED | ES_CONTINUOUS); rc = 1;
      }
    }
  }
  lua_pushboolean(L, rc);
#else
  lua_pushfail(L);
#endif
  return 1;
}

/* The function halts, reboots, sleeps, or logoffs Windows or Mac OS X. In Windows it can also lock the current user
   session or hibernate the system.

   The function makes sure that no data loss occurs: if there is any unsaved data, the function does not start termination
   and just quits.

   To put the system into energy-saving sleep mode, pass the string 'sleep' as the only argument. To
   hibernate (save the whole system state and then shut off the PC), pass 'hibernate' (Windows only); to shut down
   the computer completely, pass 'halt'; to reboot the system, pass 'reboot'; to lock the current user session
   without logging the user off, pass 'lock'; to log-off the open session of the current user, pass
   'logoff'.

   By default, the function waits for 60 seconds before initiating the termination process. You can change this timeout
   period to another number of seconds by setting the optional second argument to any non-negative integer.

   On all other platforms, the function returns fail and does nothing. */
#if defined(XXX) && defined(__APPLE__)  /* 2.8.4: linking to Carbon framework causes memory leaks in Agena */
/* taken from Mac Developer Library, article:
   `Technical Q&A QA1134  Programmatically causing restart, shutdown and_or logout.htm`*/
OSStatus SendAppleEventToSystemProcess (AEEventID EventToSend) {
  AEAddressDesc targetDesc;
  static const ProcessSerialNumber kPSNOfSystemProcess = { 0, kSystemProcess };
  AppleEvent eventReply = {typeNull, NULL};
  AppleEvent appleEventToSend = {typeNull, NULL};
  OSStatus error = noErr;
  error = AECreateDesc(typeProcessSerialNumber, &kPSNOfSystemProcess, sizeof(kPSNOfSystemProcess),
                       &targetDesc);
  if (error != noErr) return(error);
  error = AECreateAppleEvent(kCoreEventClass, EventToSend, &targetDesc, kAutoGenerateReturnID,
                             kAnyTransactionID, &appleEventToSend);
  AEDisposeDesc(&targetDesc);
  if (error != noErr) return(error);
  error = AESend(&appleEventToSend, &eventReply, kAENoReply, kAENormalPriority, kAEDefaultTimeout, NULL, NULL);
  AEDisposeDesc(&appleEventToSend);
  if (error != noErr) return(error);
  AEDisposeDesc(&eventReply);
  return(error);
}
#endif

/* for the OS/2 specific part, see:
   http://www.programd.com/3_109a201674fc28b9_1.htm and
   Mark Kimes' implementation available at https://github.com/vonj/snippets/blob/master/os2_boot.c */

#ifdef __OS2__  /* settings for reboot option */
#define CATEGORY_DOSSYS  0xD5
#define FUNCTION_REBOOT  0xAB

static APIRET eCSshutdownall (lua_State *L) {
  APIRET rc;
  /* clear the caches */
  rc = DosShutdown(1);
  if (rc != 0)
    luaL_error(L, "Error in " LUA_QS ": could not flush caches, quitting.", "os.terminate");
  /* shut down the file system */
  rc = DosShutdown(1);
  if (rc != 0)
    luaL_error(L, "Error in " LUA_QS ": could not shut down file system, quitting.", "os.terminate");
  /* halt the system */
  return DosShutdown(0);
}
#endif

static int os_terminate (lua_State *L) {  /* 2.4.0 */
#if !(defined(_WIN32) || defined(__APPLE__) || defined(__OS2__))
  lua_pushfail(L);
#else
  int cmd, ver, r, action, wait;
  const char *command;
#if defined(_WIN32)
  TOKEN_PRIVILEGES tkp;  /* pointer to token structure */
  HANDLE hToken;         /* handle to process token */
  /* timeout = agnL_optinteger(L, 2, 360);  // default time-out in seconds */
  /* message = NULL; */
  ver = tools_getwinver();  /* returns 6 or less in Windows 2000 or earlier, since sleeps or
    hibernates are not supported here; 2.17.4 change */
#elif defined(__APPLE__)
  r = ver = action = 0;
#elif defined(__OS2__)
  HFILE h;
  ULONG ulAction;
  ver = 0;
#endif
  command = luaL_optstring(L, 1, "sleep");
  wait = luaL_optinteger(L, 2, 60);
  if (wait < 0)
    luaL_error(L, "Error in " LUA_QS ": timeout must be non-negative.", "os.terminate");
  action = r = cmd = 0;
  if (tools_streq(command, "sleep") && (ver == 0 || ver > MS_WIN2K))  /* sleep = suspend, 2.25.1 tweak */
    cmd = 1;
  else if (tools_streq(command, "hibernate") && (ver == 0 || ver > MS_WIN2K))  /* 2.25.1 tweak */
    cmd = 2;
  else if (tools_streq(command, "halt")) {  /* 2.25.1 tweak */
    cmd = 3;
    #ifdef _WIN32
    action = EWX_POWEROFF;
    #endif
    /* message = "shutting down system"; */
  }
  else if (tools_streq(command, "reboot")) {  /* 2.25.1 tweak */
    cmd = 4;
    #ifdef _WIN32
    action = EWX_REBOOT;
    #endif
    /* message = "rebooting system"; */
  }
  else if (tools_streq(command, "logoff")) {  /* 2.25.1 tweak */
    cmd = 5;
    #ifdef _WIN32
    action = EWX_LOGOFF;
    #endif
  }
  else if (tools_streq(command, "lock")) {  /* 2.25.1 tweak */
    cmd = 6;
  }
  else
    luaL_error(L, "Error in " LUA_QS ": unknown or unsupported option " LUA_QS ".",
                  "os.terminate", command);
#ifdef _WIN32
  Sleep(wait * 1000);
#elif defined(__APPLE__) || defined(__OS2__)
  sleep(wait);
#endif
#ifdef _WIN32
  /* get shutdown privileges, taken from:
     http://msdn.microsoft.com/en-us/library/windows/desktop/aa376861%28v=vs.85%29.aspx */
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    luaL_error(L, "Error in " LUA_QS ": could not retrieve current process token handle.", "os.terminate");
  LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
  tkp.PrivilegeCount = 1;  /* one privilege to set */
  tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  /* get shutdown privilege for this process */
  AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
  /* since the return value of AdjustTokenPrivileges cannot be properly tested: */
  if (GetLastError() != ERROR_SUCCESS)
    luaL_error(L, "Error in " LUA_QS ": could not change the necessary termination privilege.", "os.terminate");
  switch (cmd) {
    case 1: case 2: {  /* suspend or hibernate ? */
      r = SetSuspendState(
        cmd == 2,  /* TRUE ? -> hibernate, do not suspend/sleep */
        TRUE,      /* do not suspend immediately, instead ask for permission, only regarded in XP or earlier */
        FALSE      /* FALSE: any system wake events remain enabled */
      );
      break;
    }
    case 3: case 4: case 5: {  /* halt, reboot, or logoff ? */
      r = ExitWindowsEx(action, 0);
      break;
    }
    case 6: {  /* lock the user ? */
      r = LockWorkStation();
      break;
    }
    default:  /* should not happen */
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "os.terminate");
  }
  /* Maybe Agena never reaches this part, for the termination commands immediately close the Agena console window. */
  /* now disable shutdown privilege */
  tkp.Privileges[0].Attributes = 0;
  AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
  if (GetLastError() != ERROR_SUCCESS)
    luaL_error(L, "Error in " LUA_QS ": could not disable termination privilege.", "os.terminate");
#elif defined(XXX) && defined(__APPLE__)  /* 2.8.4: linking to Carbon framework causes memory leaks in Agena */
  switch (cmd) {
    case 1: {  /* sleep ? */
      r = (SendAppleEventToSystemProcess(kAESleep) == noErr);  /* 0 if error occurred */
      break;
    };
    case 3: {  /* halt ? */
      r = (SendAppleEventToSystemProcess(kAEShutDown) == noErr);
      break;
    }
    case 4: {  /* reboot ? */
      r = (SendAppleEventToSystemProcess(kAERestart) == noErr);
      break;
    }
    case 5: {  /* logoff ? */
      r = (SendAppleEventToSystemProcess(kAEReallyLogOut) == noErr);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": unknown option %s.", "os.terminate", command);
  }
#elif defined(__OS2__)
  switch (cmd) {
    case 3: {  /* halt ? */
      /* r = eCSshutdownall(L) == 0;  --- XXX why does not this work ? */
      luaL_error(L, "Error in " LUA_QS ": halting currently is not supported.", "os.terminate");
      break;
    }
    case 4: {  /* reboot ? */
      r = DosOpen((PCSZ)"\\DEV\\DOS$", &h, &ulAction,
                   0L, FILE_NORMAL, FILE_OPEN,
                   OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE, 0L);
      if (r != 0)
        luaL_error(L, "Error in " LUA_QS ": could not access operating system, quitting.",
                      "os.terminate");
      r = eCSshutdownall(L) == 0;  /* success ? */
      if (r)  /* now reboot */
        DosDevIOCtl(h, CATEGORY_DOSSYS, FUNCTION_REBOOT, NULL, 0L, NULL, NULL, 0L, NULL);
      DosClose(h);
      (void)ulAction;  /* to prevent compiler warnings */
      (void)action;    /* ditto */
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": unknown option %s.", "os.terminate", command);
  }
#endif
  if (r == 0)
    luaL_error(L, "Error in " LUA_QS ": %s failed.", "os.terminate", command);
  lua_pushtrue(L);
#endif
  return 1;
}


/* os.datetosec, Agena 1.8.0, 21.09.2012

   receives a date of the form [year, month, date [, hour [, minute [, second]]], with a table of all values
   being integers, and transforms it to the number of seconds elapsed since the start of an `epoch`.
   The time zone acknowledged may depend on your operating system.

   Returns -1 on error.

   See also: os.secstodate. */

static int os_datetosecs (lua_State *L) {  /* Agena 1.8.0, extended 1.12.3, rewritten and extended 2.9.8 */
  if (lua_gettop(L) == 0)  /* 2.9.8 */
    lua_pushnumber(L, time(NULL));
  else {
    int ms;
    lua_pushnumber(L, agnL_datetosecs(L, 1, "os.datetosecs", 1, 0, &ms));
  }
  return 1;
}


/* Checks whether Daylight Saving Time is active for the given date and returns true or false. The function issues
   an error if the date does not exist. 2.9.8 */
static int os_isdst (lua_State *L) {
  Time64_T t;
  struct TM *stm;
  if (lua_gettop(L) == 0)  /* no argument given -> current date and time */
    t = time(NULL);
  else {
    int ms;
    t = agnL_datetosecs(L, 1, "os.isdst", 1, 0, &ms);
    if (t < 0) luaL_error(L, "Error in " LUA_QS ": could not determine date, may be out-of-range.", "os.isdst");  /* 2.16.2 fix */
  }
  stm = localtime64(&t);
  if (stm == NULL)  /* invalid date? */
    luaL_error(L, "Error in " LUA_QS ": time could not be converted.", "os.isdst");
  lua_pushboolean(L, stm->tm_isdst == 1);
  return 1;
}


/* Computes the Excel Serial date, a number, for the given date or - if no argument is given - the current date and time.
   The Excel Serial represents the number of days that have elapsed since 31st December 1899, 00:00h, where midnight
   January 01, 1900 is day 1.

   The function implemented here takes no account of daylight saving time (which `os.lsd` does): at Winter time change,
   it returns the same values for (as an example) 02:00 a.m. before and after time change. Also, there is a `gap` in
   the values returned at Summer time change between 02:00 a.m. and 03:00 a.m.

   In case of a non-existing date, the function issues an error. Thus, the function never returns 60 for February 29, 1900,
   the bug in the original Lotus 1-2-3 formula.

   2.16.2: There is a nasty bug in MinGW/GCC with the latest versions: instead of compute dates earlier than 1970/1/1,
   mktemp() now issues an error (this is compliant to the C standard). Thus we call SOFA function iauJuliandate in these cases
   to compute the LSD, returned in variable `alternative'. */

#define aux_serialdate(L, fn, procname, nargs, dst, withLSDbug) { \
  if ((nargs) == 0) { \
    t = time(NULL); \
  } else if ((nargs) == 1 && agn_isnumber(L, 1)) { \
    int r, iy, im, id, deg, min; \
    lua_Number x, fd, sec; \
    x = agn_tonumber(L, 1); \
    r = tools_jd2cdate(x + (x < 60) + 2415018.5, &iy, &im, &id, &fd, &deg, &min, &sec); \
    if (r == -1) { \
      luaL_error(L, "Error in " LUA_QS ": Julian Date could not be determined.", procname); \
      return 1; \
    } else if (r == -2) { \
      iy = 1900; im = 2; id = 29; \
    } \
    lua_pushnumber(L, iy); \
    lua_pushnumber(L, im); \
    lua_pushnumber(L, id); \
    lua_pushnumber(L, x - sun_trunc(x)); \
    lua_pushnumber(L, deg); \
    lua_pushnumber(L, min); \
    lua_pushnumber(L, sec); \
    return 7; \
  } else { \
    int ms; \
    t = agnL_datetosecs(L, 1, procname, 1, withLSDbug, &ms); \
    if (t == -1 || t == -2) { \
      lua_pushnumber(L, AGN_NAN); \
      lua_pushfail(L); \
      return 2; \
    } \
  } \
  stm = fn(&t); \
  if (stm == NULL) \
    luaL_error(L, "Error in " LUA_QS ": time could not be converted.", procname); \
  lua_pushnumber(L, \
    tools_esd(stm->tm_year + 1900, stm->tm_mon + 1, stm->tm_mday, stm->tm_hour, stm->tm_min, stm->tm_sec) - \
    stm->tm_isdst*(dst)/24.0 \
  ); \
}

/* The function computes the Lotus 1-2-3 Serial Date, which is also used in Excel (known there as
   `Excel Serial Date`), where midnight January 01, 1900 is day 1. It returns a number.
   To compute the Julian date from the Lotus Serial Date, add 2415018.5. To convert the Julian date
   to Gregorian date, use `astro.cdate`.
   See http://www.decimaltime.hynes.net/p/dates.html#excel &
   https://support.microsoft.com/en-us/kb/214326; 2.7.1, extended and patched 2.9.8, patched 2.16.2, patched 5.5.2 */
static int os_lsd (lua_State *L) {  /* C port 2.10.0 */
  int nargs, dstoff, booloption;
  Time64_T t;
  struct TM *stm;
  nargs = lua_gettop(L);
  booloption = nargs != 0 && lua_isboolean(L, nargs);
  dstoff = !(booloption && agn_isfalse(L, nargs));
  aux_serialdate(L, localtime64, "os.lsd", nargs - booloption, dstoff, 1);
  lua_pushboolean(L, stm->tm_isdst);
  return 2;
}


static int os_esd (lua_State *L) {  /* 2.9.8 */
  Time64_T t;
  struct TM *stm;
  aux_serialdate(L, localtime64, "os.esd", lua_gettop(L), 0, 1);
  return 1;
}


/* Computes the UTC Serial date, a number, for the given date or - if no argument is given - the current date and time,
   where the time zone is assumed to be UTC.

   The UTC Serial represents the number of days that have elapsed since 31st December 1899, 00:00h, where midnight
   January 01, 1900 is day 1.

   If no argument is given, the UTC Serial Date for the current date and time is computed. Otherwise, at least year,
   month, and day - all numbers - must be given. Optionally, you may add an hour, minute, or second, where all three
   default to 0.

   Since the date and time is considered to be UTC, the function implemented here takes no account of daylight saving
   time: at Winter time change, it returns the same values for (as an example) 02:00 a.m. before and after time change.
   Also, there is a `gap` in the values returned at Summer time change between 02:00 a.m. and 03:00 a.m.

   In case of a non-existing date, the function issues an error. 2.9.8 */
static int os_usd (lua_State *L) {
  Time64_T t;
  struct TM *stm;
  aux_serialdate(L, gmtime64, "os.usd", lua_gettop(L), 0, 0);
  return 1;
}


/* os.secstodate, Agena 1.8.0, 21.09.2012

   takes the number of seconds elapsed since the start of an epoch, in your local time zone, and returns a
   table of integers in the order: year, month, day, hour, minute, second. In case of an error, `fail` is
   returned.

   See also: os.datetosec. */

static int os_secstodate (lua_State *L) {  /* Agena 1.8.0 */
  Time64_T t;
  struct TM *stm;
  t = agn_checknonnegint(L, 1);  /* 5.5.7 fix */
  stm = localtime64(&t);
  if (stm != NULL) {  /* 0.31.3 patch */
    lua_createtable(L, 6, 0);  /* 6 = number of entries */
    agn_setinumber(L, -1, 1, stm->tm_year + 1900);
    agn_setinumber(L, -1, 2, stm->tm_mon + 1);
    agn_setinumber(L, -1, 3, stm->tm_mday);
    agn_setinumber(L, -1, 4, stm->tm_hour);
    agn_setinumber(L, -1, 5, stm->tm_min);
    agn_setinumber(L, -1, 6, stm->tm_sec);
  } else
    lua_pushfail(L);
  return 1;
}


/* os.now, Agena 1.8.0, 21.09.2012

   returns rather low-level information on the current or given date and time in form of a dictionary: the `gmt` table
   represents the current date and time in GMT/UTC. The `localtime` table includes the same information for your local
   time zone. The `tz` entry represents the difference between your local time zone and GMT in minutes with daylight
   saving time cancelled out, and _east_ of Greenwich. The `seconds` entry is the number of seconds elapsed since
   some given start time (the `epoch`), which on most operating systems is January 01, 1970, 00:00:00.

   If no argument is passed, the function returns information on the current date and time. If a non-negative number
   is given which represents the amount of seconds elapsed since the start of the epoch, information on this date and time
   are determined (see os.datetosecs to convert a date to seconds).

   The `gmt` and `localtime` entries are of the same structure: it is a table of data of the following order:
   year, month, day, hour, minute, second, number of weekday (where 0 means Sunday, 1 is Monday, and so forth),
   the number of full days since the beginning of the year (in the range 0:365), whether daylight saving time
   is in effect at the time given (0: no, 1: yes), and the strings 'AM' or 'PM'.

   See also: os.datetosecs, os.secstodate, os.time.

*/

#define dia_ae "ae"
#define dia_ao "ao"
#define dia_CZ "cz"
#define dia_ue "ue"

/* New English calendar names */

static char *months[] = {
      "none", "January", "February", "March", "April", "May", "June", "July", "August",
      "September", "October", "November", "December" };

static char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

static int os_now (lua_State *L) {  /* Agena 1.8.0, extended 1.8.2 */
  Time64_T tlct, tgmt, gmttime, lcttime, gmtday, lctday;
  struct TM *stmgmt, *stmlct;
#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__)
  struct timeb tp;
#elif defined(__OS2__)
  DATETIME tp;
#endif
  char ampm[3];
  double djm0, djm, djt, t;
  int msecs, nargs, i;
  for (i=0; i < 3; i++) ampm[i] = 0;
  nargs = lua_gettop(L);
  if (nargs == 0) {
    tlct = time(NULL);
    tgmt = time(NULL);
    if (tlct == ((time_t)-1) || tgmt == ((time_t)-1))
      luaL_error(L, "Error in " LUA_QS ": could not determine current time.", "os.now");
  } else if (nargs == 1 && lua_isnumber(L, 1)) {
    Time64_T t = agn_tonumber(L, 1);
    if (t < 0) luaL_error(L, "Error in " LUA_QS ": given time is negative.", "os.now");
    tlct = tgmt = t;
  } else {  /* 2.9.8 extension */
    tlct = tgmt = agnL_datetosecs(L, 1, "os.now", 1, 0, &msecs);
    if (tlct < 0) luaL_error(L, "Error in " LUA_QS ": could not determine date, may be out-of-range.", "os.now");  /* 2.16.2 fix */
  }
  gmttime = lcttime = gmtday = lctday = 0;
  lua_createtable(L, 0, 8);
  lua_pushstring(L, "gmt");
  stmgmt = gmtime64(&tgmt);
  if (stmgmt != NULL) {  /* 5.5.5 to avoid undefined behaviour */
    str_strftime(ampm, 3, "%p", stmgmt, msecs);  /* 5.5.3 change */
    ampm[2] = '\0';  /* better sure than sorry */
    lua_createtable(L, 12, 0);
    agn_setinumber(L, -1,  1, stmgmt->tm_year + 1900);
    agn_setinumber(L, -1,  2, stmgmt->tm_mon + 1);
    agn_setinumber(L, -1,  3, stmgmt->tm_mday);
    gmtday = stmgmt->tm_mday;
    agn_setinumber(L, -1,  4, stmgmt->tm_hour);
    agn_setinumber(L, -1,  5, stmgmt->tm_min);
    agn_setinumber(L, -1,  6, stmgmt->tm_sec);
    agn_setinumber(L, -1,  7, stmgmt->tm_wday);
    agn_setinumber(L, -1,  8, stmgmt->tm_yday);
    agn_setinumber(L, -1,  9, stmgmt->tm_isdst);
    lua_rawsetistring(L, -1, 10, ampm);
    lua_rawsetistring(L, -1, 11, months[stmgmt->tm_mon + 1]);
    lua_rawsetistring(L, -1, 12, weekdays[stmgmt->tm_wday]);  /* Agena 1.8.2 fix */
    gmttime = stmgmt->tm_sec + stmgmt->tm_min*60 + stmgmt->tm_hour*3600;
  } else {
    lua_pushfail(L);
  }
  lua_rawset(L, -3);
  lua_pushstring(L, "localtime");
  stmlct = localtime64(&tlct);  /* put this _here_, otherwise the time would be overwritten */
  if (stmlct != NULL) {  /* 5.5.5 fix to avoid undefined behaviour, 7.3.0 fix */
    str_strftime(ampm, 3, "%p", stmlct, msecs);
    lua_createtable(L, 12, 0);
    agn_setinumber(L, -1,  1, stmlct->tm_year + 1900);
    agn_setinumber(L, -1,  2, stmlct->tm_mon + 1);
    agn_setinumber(L, -1,  3, stmlct->tm_mday);
    lctday = stmlct->tm_mday;
    agn_setinumber(L, -1,  4, stmlct->tm_hour);
    agn_setinumber(L, -1,  5, stmlct->tm_min);
    agn_setinumber(L, -1,  6, stmlct->tm_sec);
    agn_setinumber(L, -1,  7, stmlct->tm_wday);
    agn_setinumber(L, -1,  8, stmlct->tm_yday);
    agn_setinumber(L, -1,  9, stmlct->tm_isdst);
    lua_rawsetistring(L, -1, 10, ampm);
    lua_rawsetistring(L, -1, 11, months[stmlct->tm_mon + 1]);
    lua_rawsetistring(L, -1, 12, weekdays[stmlct->tm_wday]);  /* Agena 1.8.2 fix */
    lcttime = stmlct->tm_sec + stmlct->tm_min*60 + stmlct->tm_hour*3600;
  } else {
    lua_pushfail(L);
  }
  lua_rawset(L, -3);
  t = (lctday - gmtday == 0) ? (lcttime - gmttime - stmlct->tm_isdst*3600)/60 :
                               (lcttime - gmttime + 86400 - stmlct->tm_isdst*3600)/60;
  lua_rawsetstringnumber(L, -1, "tz", t);
  t = (lctday - gmtday == 0) ? (lcttime - gmttime)/60 : (lcttime - gmttime + 86400)/60;
  lua_rawsetstringnumber(L, -1, "td", t);
  /* set daylight saving time flag for local time zone, Agena 1.8.2 */
  lua_rawsetstringboolean(L, -1, "dst", stmlct->tm_isdst);
  lua_rawsetstringnumber(L, -1, "seconds", tgmt);  /* return time in seconds */
  msecs = -1;
  if (nargs == 0) {
    /* 1.11.7, get milliseconds */
    #ifdef _WIN32
    tools_ftime(&tp);
    msecs = tp.millitm;
    #elif defined(__unix__) || defined(__APPLE__)
    if (tools_ftime(&tp) == 0)  /* set milliseconds only when query has been successful */
      msecs = tp.millitm;
    #elif defined(__OS2__)  /* 2.3.0 RC 2 eCS extension */
    DosGetDateTime(&tp);
    msecs = tp.hundredths;
    #endif
  }
  lua_rawsetstringnumber(L, -1, "mseconds", msecs == -1 ? 0 : msecs);
  if (iauCal2jd(stmgmt->tm_year + 1900, stmgmt->tm_mon + 1, stmgmt->tm_mday, &djm0, &djm) >= 0 &&
      iauTf2d('+', stmgmt->tm_hour, stmgmt->tm_min, stmgmt->tm_sec, &djt) >= 0) {
    lua_rawsetstringnumber(L, -1, "jd", djm0 + djm + djt);
    t = djm0 + djm + djt - 2415018.5 - (stmgmt->tm_isdst == 1)*1.0/24.0;
    lua_rawsetstringnumber(L, -1, "lsd", t - (t < 61));  /* 5.5.5 fix */
  }
  return 1;
}


/* Computes the difference between the system's local time zone and UTC in minutes and a Boolean indicating whether
   daylight Saving Time is active for the given date. */
static int os_tzdiff (lua_State *L) {  /* 2.9.8, based on os.new and os.datetosecs */
  Time64_T tlct, tgmt, gmttime, lcttime, gmtday, lctday;
  struct TM *stmgmt, *stmlct;
  int nargs, ms;
  nargs = lua_gettop(L);
  if (nargs == 0) {
    tlct = time(NULL);
    tgmt = time(NULL);
    if (tlct == ((time_t)-1) || tgmt == ((time_t)-1))
      luaL_error(L, "Error in " LUA_QS ": could not determine current time.", "os.tzdiff");
  } else if (nargs == 1 && lua_isnumber(L, 1)) {
    Time64_T t = agn_tonumber(L, 1);
    if (t < 0) luaL_error(L, "Error in " LUA_QS ": given time is negative.", "os.tzdiff");
    tlct = tgmt = t;
  } else {
    tlct = tgmt = agnL_datetosecs(L, 1, "os.tzdiff", 1, 0, &ms);
    if (tlct < 0) luaL_error(L, "Error in " LUA_QS ": could not determine date, may be out-of-range.", "os.now");  /* 2.16.2 fix */
  }
  gmttime = lcttime = gmtday = lctday = 0;
  stmgmt = gmtime64(&tgmt);
  if (stmgmt != NULL) {
    gmtday = stmgmt->tm_mday;
    gmttime = stmgmt->tm_sec + stmgmt->tm_min*60 + stmgmt->tm_hour*3600;
  } else
    luaL_error(L, "Error in " LUA_QS ": could not determine current time.", "os.tzdiff");
  stmlct = localtime64(&tlct);  /* put this _here_, otherwise the time would be overwritten */
  if (stmlct != NULL) {  /* 7.3.0 fix */
    lctday = stmlct->tm_mday;
    lcttime = stmlct->tm_sec + stmlct->tm_min*60 + stmlct->tm_hour*3600;
  } else
    luaL_error(L, "Error in " LUA_QS ": could not determine current time.", "os.tzdiff");
  if (lctday-gmtday == 0)
    lua_pushnumber(L, (lcttime - gmttime)/60);
  else
    lua_pushnumber(L, (lcttime - gmttime + 86400)/60);
  lua_pushboolean(L, stmlct->tm_isdst);
  return 2;
}


/* os.settime, Agena 1.8.0, 24.09.2012

   takes the number of seconds elapsed since the start of an epoch, in your local time zone, and sets the
   system clock accordingly. Agena must be run in root mode in order to change the system time. In case of
   an error, `fail` is returned. The function is only available in the Windows and UNIX versions of Agena (Solaris,
   Linux). */

static int os_settime (lua_State *L) {  /* Agena 1.8.0, 1.9.1. */
#if defined(__unix__) && !defined(__DJGPP__)
  time_t timet;
#elif defined(_WIN32)
  SYSTEMTIME lpSystemTime;
#elif defined(__OS2__)
  DATETIME lpSystemTime;
#else
  lua_pushfail(L);
#endif
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(_WIN32) || defined(__OS2__)
  /* common part */
  Time64_T t;
  struct TM *stm;
  int nargs, ms;
  nargs = lua_gettop(L);
  if (nargs == 1 && lua_isnumber(L, 1)) {
    t = agn_tonumber(L, 1);
    if (t < 0) luaL_error(L, "Error in " LUA_QS ": given time is negative.", "os.now");
  }
  else { /* 2.9.8 */
    t = agnL_datetosecs(L, 1, "os.settime", 1, 0, &ms);
    if (t < 0) luaL_error(L, "Error in " LUA_QS ": could not determine date, may be out-of-range.", "os.settime");  /* 2.16.2 fix */
  }
  stm = localtime64(&t);
  if (stm == NULL) {
    lua_pushfail(L);
    return 1;
  }
#ifdef _WIN32
  /* Windows part */
  lpSystemTime.wYear = stm->tm_year + 1900;
  lpSystemTime.wMonth = stm->tm_mon + 1;
  lpSystemTime.wDay = stm->tm_mday;
  lpSystemTime.wHour = stm->tm_hour;
  lpSystemTime.wMinute = stm->tm_min;
  lpSystemTime.wSecond = stm->tm_sec;
  lpSystemTime.wMilliseconds = lpSystemTime.wDayOfWeek = 0;  /* defaults */
  agn_pushboolean(L, SetLocalTime(&lpSystemTime) == 0 ? -1 : 1);
#elif defined(__OS2__)  /* 2.3.0 RC 2 eCS extension */
  lpSystemTime.year = stm->tm_year + 1900;
  lpSystemTime.month = stm->tm_mon + 1;
  lpSystemTime.day = stm->tm_mday;
  lpSystemTime.hours = stm->tm_hour;
  lpSystemTime.minutes = stm->tm_min;
  lpSystemTime.seconds = stm->tm_sec;
  lpSystemTime.hundredths = lpSystemTime.weekday = lpSystemTime.timezone = 0;  /* defaults */
  agn_pushboolean(L, DosSetDateTime(&lpSystemTime) == 0 ? 1 : -1);
#else
  /* UNIX part */
  {
    int rc;
    timet = tools_maketime(stm->tm_year + 1900, stm->tm_mon + 1, stm->tm_mday,
                           stm->tm_hour, stm->tm_min, stm->tm_sec, &rc);  /* 2.9.8 fix */
    if (rc == -1)  /* 2.6.1, usually for dates earlier than 1970/1/1 */
      luaL_error(L, "Error in " LUA_QS ": could not determine date, may be out-of-range.", "os.settime");
    else if (rc == -2)  /* 2.9.8 */
      luaL_error(L, "Error in " LUA_QS ": time component out of range.", "os.settime");
    agn_pushboolean(L, tools_stime(&timet) == 0 ? 1 : -1);  /* 2.25.5, stime removed from Debian Bullseye */
  }
#endif
#endif
  return 1;
}


#ifdef _WIN32
/* The following Windows implementation has been taken from the LuaSys package, written by
   Nodir Temirkhodjaev; Agena 2.3.0 RC 2.
   See: https://github.com/tnodir/luasys
   The function crashes in W2K and XP.
   See also:
   http://www.windowsnetworking.com/articles-tutorials/windows-2003/Key-Performance-Monitor-Counters.html */

typedef struct {
  DWORD status;
  union {
    LONG vLong;
    double vDouble;
    LONGLONG vLongLong;
    void *vPtr;
  } u;
} PDH_VALUE;

typedef long (WINAPI *PPdhOpenQuery) (LPCSTR, DWORD_PTR, HANDLE *);
typedef long (WINAPI *PPdhAddEnglishCounter) (HANDLE, LPCSTR, DWORD_PTR, HANDLE *);
typedef long (WINAPI *PPdhCollectQueryData) (HANDLE);
typedef long (WINAPI *PPdhGetFormattedCounterValue) (HANDLE, DWORD, LPDWORD, PDH_VALUE *);
typedef long (WINAPI *PPdhCloseQuery) (HANDLE);

static HINSTANCE hdll;
static HANDLE hquery;
static HANDLE hcounter;
static HANDLE pcounter;
static HANDLE ucounter;
static HANDLE qlcounter;
static HANDLE cscounter;
static HANDLE intcounter;

static PPdhOpenQuery pPdhOpenQuery;
static PPdhAddEnglishCounter pPdhAddEnglishCounter;
static PPdhCollectQueryData pPdhCollectQueryData;
static PPdhGetFormattedCounterValue pPdhGetFormattedCounterValue;
/* static PPdhCloseQuery pPdhCloseQuery; */

static int getloadavg (double *loadavg, double *kernel, double *user,
                       double *queuelen, double *cswitches, double *interrupts) {
  PDH_VALUE value, pvalue, uvalue, qlvalue, csvalue, intvalue;
  int res;
  if (!hdll) {
    hdll = LoadLibrary("pdh.dll");
    if (!hdll) return -1;
    pPdhOpenQuery = (PPdhOpenQuery)
     GetProcAddress(hdll, "PdhOpenQueryA");
    pPdhAddEnglishCounter = (PPdhAddEnglishCounter)
     GetProcAddress(hdll, "PdhAddEnglishCounterA");
    pPdhCollectQueryData = (PPdhCollectQueryData)
     GetProcAddress(hdll, "PdhCollectQueryData");
    pPdhGetFormattedCounterValue = (PPdhGetFormattedCounterValue)
     GetProcAddress(hdll, "PdhGetFormattedCounterValue");
    res = pPdhOpenQuery(NULL, 0, &hquery);
    if (res) return -1;
    /* total utilisation of processor by all running processes (average processor utilisation of machine) */
    res = pPdhAddEnglishCounter(hquery, "\\Processor(_Total)\\% Processor Time", 0, &hcounter);
    if (res) return -1;
    /* processor utilisation for kernel-mode processes */
    res = pPdhAddEnglishCounter(hquery, "\\Processor(_Total)\\% Privileged Time", 0, &pcounter);
    if (res) return -1;
    /* processor utilisation for user-mode processes */
    res = pPdhAddEnglishCounter(hquery, "\\Processor(_Total)\\% User Time", 0, &ucounter);
    if (res) return -1;
    if (!hcounter || !pcounter || !ucounter) return -1;
    /* number of threads waiting for execution */
    res = pPdhAddEnglishCounter(hquery, "\\System\\Processor Queue Length", 0, &qlcounter);
    if (res) return -1;
    /* number of processor switches per second from user- to kernel-mode to handle a request from a thread running in user mode */
    res = pPdhAddEnglishCounter(hquery, "\\System\\Context Switches/sec", 0, &cscounter);
    if (res) return -1;
    /* number of interrupts per second */
    res = pPdhAddEnglishCounter(hquery, "\\Processor(_Total)\\Interrupts/sec", 0, &intcounter);
    if (res) return -1;
    if (!qlcounter || !cscounter || !intcounter) return -1;
    pPdhCollectQueryData(hquery);  /* to avoid PDH_INVALID_DATA result */
  }
  res = pPdhCollectQueryData(hquery);
  if (res) return -1;
  res = pPdhGetFormattedCounterValue(hcounter, 0x8200, NULL, &value);
  if (res) return -1;
  *loadavg = value.u.vDouble / 100.0;
  res = pPdhGetFormattedCounterValue(pcounter, 0x8200, NULL, &pvalue);
  if (res) return -1;
  *kernel = pvalue.u.vDouble / 100.0;
  res = pPdhGetFormattedCounterValue(ucounter, 0x8200, NULL, &uvalue);
  if (res) return -1;
  *user = uvalue.u.vDouble / 100.0;
  res = pPdhGetFormattedCounterValue(qlcounter, 0x8200, NULL, &qlvalue);
  if (res) return -1;
  *queuelen = qlvalue.u.vDouble;
  res = pPdhGetFormattedCounterValue(cscounter, 0x8200, NULL, &csvalue);
  if (res) return -1;
  *cswitches = csvalue.u.vDouble;
  res = pPdhGetFormattedCounterValue(intcounter, 0x8200, NULL, &intvalue);
  if (res) return -1;
  *interrupts = intvalue.u.vDouble;
  return 0;
}
#endif

static int os_cpuload (lua_State *L) {  /* Agena 1.9.1, 2.3.0 RC 2 eCS extension */
#if !(defined _WIN32) && !defined (__DJGPP__) && !(defined(__SOLARIS) && !defined(__HAIKU__))
  double loadavg[3];
  int i;
  if (getloadavg(loadavg, 3) == -1) {
    lua_pushfail(L);  /* 2.17.5 fix */
    return 1;
  }
  agn_createseq(L, 3);
  for (i=0; i < 3; i++)
    agn_seqsetinumber(L, -1, i + 1, loadavg[i]);
#elif _WIN32
  double loadavg, kernel, user, queuelength, contextswitches, interrupts;
  int nargs = lua_gettop(L);
  /* Windows XP or earlier ? GCC's built-in getloadavg() crashes in W2K & XP, 2.17.4 change, 2.18.1 fix */
  if (tools_getwinver() <= MS_WIN2003) {  /* 2.25.0 fix */
    lua_pushfail(L);
    return 1;
  }
  loadavg = kernel = user = queuelength = contextswitches = interrupts = 0;
  if (getloadavg(&loadavg, &kernel, &user, &queuelength, &contextswitches, &interrupts) != 0) {
    lua_pushfail(L);  /* 2.17.5 fix */
    return 1;
  }
  agn_createseq(L, 6);
  agn_seqsetinumber(L, -1, 1, (lua_Number)loadavg);
  agn_seqsetinumber(L, -1, 2, (lua_Number)kernel);
  agn_seqsetinumber(L, -1, 3, (lua_Number)user);
  if (nargs) {
    agn_seqsetinumber(L, -1, 4, (lua_Number)queuelength);
    agn_seqsetinumber(L, -1, 5, (lua_Number)contextswitches);
    agn_seqsetinumber(L, -1, 6, (lua_Number)interrupts);
  }
#else
  lua_pushfail(L);
#endif
  return 1;
}


static int os_pid (lua_State *L) {  /* Agena 1.9.1. */
  luaL_checkstack(L, lua_gettop(L), "too many arguments");
  lua_pushnumber(L, getpid());
  return 1;
}


/* Taken from http://www.codeproject.com/Articles/7340/Get-the-Processor-Speed-in-two-simple-ways,
   originally written by Thomas Latuske in 2004; modified for Agena by alex walz */
#ifdef _WIN32
DWORD ProcSpeedRead () {
  DWORD BufSize, dwMHz;
  HKEY hKey;
  long keyError, valueError;
  BufSize = dwMHz = _MAX_PATH;
  keyError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey);
  if (keyError == ERROR_SUCCESS) {
    valueError = RegQueryValueEx(hKey, "~MHz", NULL, NULL, (LPBYTE)&dwMHz, &BufSize);  /* better sure than sorry */
    if (valueError != ERROR_SUCCESS) dwMHz = -1;  /* info could not be read; 2.10.0 fix */
    RegCloseKey(hKey);
  } else
    dwMHz = -1;  /* info could not be read, 2.10.0 fix */
  return dwMHz;
}

#define TOTALBYTES 1024  /* better be sure than sorry */

char *BrandRead () {
  DWORD BufSize;
  HKEY hKey;
  long keyError, valueError;
  char cpubrand[TOTALBYTES];  /* malloc will not work ! Brand info seems to be 48 bytes long, however, RegQueryValueEx
    returns the actual number of chars in the brand info. */
  cpubrand[0] = '\0';
  BufSize = sizeof(cpubrand);
  keyError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey);
  if (keyError == ERROR_SUCCESS) {
    valueError = RegQueryValueEx(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)cpubrand, &BufSize);  /* better sure than sorry */
    RegCloseKey(hKey);
    /* 3.3.2 patch to prevent compiler warnings */
    if (valueError == ERROR_SUCCESS && BufSize > 0 && BufSize <= TOTALBYTES && cpubrand[0] != '\0') {  /* 3.3.2 patch ('\0' instead of NULL) */
      cpubrand[BufSize - 1] = '\0';
      return tools_strdup(cpubrand);  /* FREE return after usage ! */
    }
  }
  return NULL;
}

char *VendorRead () {
  DWORD BufSize;
  HKEY hKey;
  long keyError, valueError;
  char vendor[TOTALBYTES];  /* malloc will not work ! Brand info seems to be 48 bytes long, however, RegQueryValueEx
    returns the actual number of chars in the brand info. */
  vendor[0] = '\0';
  BufSize = sizeof(vendor);
  keyError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey);
  if (keyError == ERROR_SUCCESS) {
    valueError = RegQueryValueEx(hKey, "VendorIdentifier", NULL, NULL, (LPBYTE)vendor, &BufSize);  /* better sure than sorry */
    RegCloseKey(hKey);
    /* 3.3.2 patch to prevent compiler warnings */
    if (valueError == ERROR_SUCCESS && BufSize > 0 && BufSize <= TOTALBYTES && vendor[0] != '\0') {  /* 3.3.2 patch ('\0' instead of NULL) */
      vendor[BufSize - 1] = '\0';
      return tools_strdup(vendor);  /* FREE return after usage ! */
    }
  }
  return NULL;
}
#endif

#ifdef __APPLE__

#define TOTALBYTES 1024
/* See: http://stackoverflow.com/questions/5782012/packagemaker-differentiate-between-ppc-and-intel */
const char *cputype[] = {
  "unknown", "unknown", "unknown", "unknown", "unknown", "unknown",  /* 0 to 5 */
  "MC680x0",  /* CPU_TYPE_MC680x0   (6) */
  "x86",      /* CPU_TYPE_X86       (7) */
  "unknown", "unknown",  /* 8 to 9 */
  "MC98000"   /* CPU_TYPE_MC98000   (10) */
  "HPPA",     /* CPU_TYPE_HPPA      (11) */
  "ARM",      /* CPU_TYPE_ARM       (12) */
  "MC88000",  /* CPU_TYPE_MC88000   (13) */
  "sparc",    /* CPU_TYPE_SPARC     (14) */
  "i860",     /* CPU_TYPE_I860      (15) */
  "unknown", "unknown",  /* 16 to 17 */
  "ppc"};     /* CPU_TYPE_POWERPC  (18) */
#endif


/* resulttype:
     0 = long int, 1 = kptr->value.c, 2 = kptr->value.str.addr.ptr

   char result: (FREE IT !)      long int result:
     "state", 1                    "state_begin", 0
     "cpu_type", 1                 "clock_MHz", 0
     "fpu_type", 1                 "chip_id", 0
     "brand", 2                    "core_id", 0
     "socket_type", 2
     "implementation", 2  */

#if defined(__SOLARIS)
#include <kstat.h>
#define CPUI "cpu_info"

/* compile with -lkstat switch */
static char *kstat_cpuinfo (char *what, int resulttype, long int *result, int *error) {  /* new 2.18.2 */
  char *r;
  kstat_ctl_t *kctl;
  kstat_t *ksp;
  kstat_named_t  *kptr;
  *error = 0;
  *result = -1;
  r = NULL;
  if ((kctl = kstat_open()) == NULL || kstat_lookup(kctl, CPUI, -1, NULL) == NULL) {
    *error = 1;
    return NULL;
  }
  for (ksp=kctl->kc_chain; ksp && tools_strneq(ksp->ks_module, CPUI); ksp = ksp->ks_next) { };  /* 2.25.1 tweak */
  /* we will read the data of the first CPU only */
  if ((kstat_read(kctl, ksp, NULL) != -1) && (kptr = kstat_data_lookup(ksp, what)) != NULL) {
    switch (resulttype) {
      case 0:  /* long int */
        *result = kptr->value.l;
        break;
      case 1:  /* char* */
        r = (kptr->value.c);
        break;
      case 2:  /* str.addr.ptr* */
        r = (kptr->value.str.addr.ptr);
        break;
      default:  /* should not happen */
        *error = 1;
    }
  } else
    *error = 1;
  kstat_close(kctl);
  return r;
}
#endif


/*   "state", 1                    "state_begin", 0
     "cpu_type", 1                 "clock_MHz", 0
     "fpu_type", 1                 "chip_id", 0
     "brand", 2                    "core_id", 0
     "socket_type", 2
     "implementation", 2 */
#if defined(__SOLARIS) || defined(__powerpc__) || defined(__DJGPP__) || defined(__OS2__) || defined(__EMX__) || defined(__linux__)
void otherarch (lua_State *L, const char *str) {  /* 1.9.5 */
#if __OS2__  /* 2.3.0 RC 2 eCS extension */
  ULONG a[1];
#elif defined(__SOLARIS)
  char *s;
  long int r;
  int error;
#endif
#if __OS2__
  if (DosQuerySysInfo(QSV_NUMPROCESSORS, QSV_NUMPROCESSORS, &a[0], sizeof(a)) == 0) {
    lua_rawsetstringinteger(L, -1, "ncpu", a[0]);
  }
#endif
#if defined(__DJGPP__) || defined(__OS2__) || (defined(__linux__) && defined(__INTEL))  /* works on Intel-compatible CPUs only */
  lua_rawsetstringinteger(L, -1, "frequency", tools_roundf(tools_getcyclesperusec()/10, -3, 4));  /* 5.6.2 */
#elif defined(__linux__) && (defined(__arm__) || defined(__aarch64__))
  {
    int64_t armfreq = tools_getArmFrequencyInKHz();  /* current frequency in KHz */
    if (armfreq == -1) {
       lua_rawsetstringnumber(L, -1, "frequency", AGN_NAN);
    } else {
      lua_rawsetstringinteger(L, -1, "frequency", armfreq/1000);  /* return MHz */
    }
  }
#endif
#if defined(__DJGPP__) || defined(__OS2__) || defined(__SOLARIS)
  lua_rawsetstringstring(L, -1, "brand", SDL_GetCPUName());  /* new 5.5.9 */
  lua_rawsetstringstring(L, -1, "vendor", SDL_GetCPUType());  /* new 5.5.9 */
#endif
  lua_rawsetstringboolean(L, -1, "bigendian", tools_endian());
  lua_rawsetstringstring(L, -1, "arch", str);
#if defined(__SOLARIS)  /* new 2.18.2 */
  lua_rawsetstringnumber(L, -1, "ncpu", sysconf(_SC_NPROCESSORS_ONLN));
  kstat_cpuinfo("clock_MHz", 0, &r, &error);
  lua_rawsetstringnumber(L, -1, "frequency", error ? AGN_NAN : r);
  /* s = kstat_cpuinfo("implementation", 2, &r, &error);
  lua_rawsetstringstring(L, -1, "vendor", error ? "unknown" : s); */
  s = kstat_cpuinfo("socket_type", 2, &r, &error);
  lua_rawsetstringstring(L, -1, "socket_type", error ? "unknown" : s);
  /* s = kstat_cpuinfo("brand", 2, &r, &error);
  lua_rawsetstringstring(L, -1, "brand", error ? "unknown" : s); */
  s = kstat_cpuinfo("cpu_type", 1, &r, &error);
  lua_rawsetstringstring(L, -1, "type", error ? "unknown" : s);
  s = kstat_cpuinfo("fpu_type", 1, &r, &error);
  lua_rawsetstringstring(L, -1, "fpu_type", error ? "unknown" : s);
  s = kstat_cpuinfo("state", 1, &r, &error);
  lua_rawsetstringstring(L, -1, "fpu_type", error ? "unknown" : s);
  kstat_cpuinfo("chip_id", 0, &r, &error);
  lua_rawsetstringnumber(L, -1, "chip_id", error ? AGN_NAN : r);
  kstat_cpuinfo("core_id", 0, &r, &error);
  lua_rawsetstringnumber(L, -1, "core_id", error ? AGN_NAN : r);
#endif
}
#endif


#if defined(__OS2__)

#define __cpuid(level, a, b, c, d) \
  __asm__ __volatile__ ("cpuid\n\t" \
	   : "=a" (a), "=b" (b), "=c" (c), "=d" (d) \
	   : "0" (level))
#define __cpuid_count(level, count, a, b, c, d) \
  __asm__ __volatile__ ("cpuid\n\t" \
	   : "=a" (a), "=b" (b), "=c" (c), "=d" (d) \
	   : "0" (level), "2" (count))
#define __builtin_cpu_init() do {} while(0)
#define __builtin_cpu_is(x) (0)
#define __builtin_cpu_supports(x) (0)

#elif defined(__APPLE__) || defined(OPENSUSE)

/* generated by Gemini AI, 5.5.10 */
static void __cpuid_alt (uint32_t info[4], int l) {
  uint32_t a, b, c, d;
  a = b = c = d = 0;  /* avoid compiler warnings */
  __asm__ __volatile__ (
    "pushl %%ebx\n"      /* 1. Manually save EBX */
    "movl %4, %%ebx\n"   /* 2. Move input value to EBX */
    "cpuid\n"            /* 3. Execute cpuid */
    "movl %%ebx, %1\n"   /* 4. Move EBX output to a C variable */
    "popl %%ebx\n"       /* 5. Restore EBX */
    : "=a" (a), "=r" (b), "=c" (c), "=d" (d) /* Outputs: a, b, c, d (b is not specific to ebx) */
    : "a" (l), "r" (b)   /* Inputs: a (value 0), and b (the register you want to use for the EBX output) */
    : "cc"               /* Clobbers: tells the compiler that the condition code flags are modified */
  );
  info[0] = a; info[1] = b; info[2] = c; info[3] = d;
}

#define __builtin_cpu_init() do {} while(0)
#define __builtin_cpu_is(x) (0)
#define __builtin_cpu_supports(x) (0)

#endif

static int cpuid (uint32_t info[4], int InfoType) {
  int i;
  for (i=0; i < 4; i++) info[i] = 0;
#if defined(__APPLE__) || defined(OPENSUSE)
  __cpuid_alt(info, InfoType);
  return 1;
#elif defined(CPUINFO)
  __cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
  return 1;  /* 5.5.10 patch */
#else
  return 0;
#endif
}

/* retrieve the maximum function callable using cpuid, source: http://newbiz.github.io/cpp/2010/12/20/Playing-with-cpuid.html */
#ifdef CPUID
static uint32_t cpuid_maxcall () {
  uint32_t info[4];  /* changed 5.5.10 */
  cpuid(info, 0);
  return info[0];  /* return eax */
}
#endif

/* Author: Nick Strupat
   Date: October 29, 2010
   Returns the cache line size (in bytes) of the processor, or 0 on failure
   MIT-licenced, see https://github.com/NickStrupat/CacheLineSize/blob/master/ */

#if defined(_WIN32) && defined(__MINGW32__)
/* generated by Gemini AI, 5.5.8 */
static size_t CacheLineSize () {
  unsigned int eax, ebx, ecx, edx;
  unsigned int cache_line_size;
#if defined(__GNUC__) || defined(__clang__)
  /* This function is for GCC and Clang compilers */
  __cpuid(0x80000006, eax, ebx, ecx, edx);
#else
  /* This is for Visual Studio compilers */
  int cpuInfo[4];
  __cpuid(cpuInfo, 0x80000006);
  eax = cpuInfo[0];
  ebx = cpuInfo[1];
  ecx = cpuInfo[2];
  edx = cpuInfo[3];
#endif
  /* The cache line size is in the lower 8 bits of the ECX register for extended function 0x80000006 */
  cache_line_size = ecx & 0xFF;
  /* else: A safe assumption for most modern x86 architectures is 64 bytes */
  return cache_line_size;
}
#elif defined(_WIN32) && !defined(__MINGW32__)
static size_t CacheLineSize () {
  size_t lineSize = 0;
  DWORD bufferSize = 0;
  DWORD i = 0;
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buffer = 0;
  GetLogicalProcessorInformation(0, &bufferSize);
  buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)malloc(bufferSize);
  if (buffer == NULL) return 0;  /* 4.11.5 fix */
  GetLogicalProcessorInformation(&buffer[0], &bufferSize);
  for (i=0; i != bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
    if (buffer[i].Relationship == RelationCache && buffer[i].Cache.Level == 1) {
      lineSize = buffer[i].Cache.LineSize;
      break;
    }
  }
  xfree(buffer);
  return lineSize;
}
#elif defined(CPUINFO)
static size_t CacheLineSize () {
  uint32_t info[4];
  if (cpuid(info, 0x80000006)) {
    unsigned int cache_line_size = info[2] & 0xff;
    return cache_line_size;
  } else {
    return 0;
  }
}
#else
#define CacheLineSize() (0)
#endif

static int os_cachelinesize (lua_State *L) {
  lua_pushinteger(L, CacheLineSize());
  return 1;
}


/* does not work in Stretch 32-bit, Solaris 32-bit */
#define CPUINFOEXCLUDE ((defined(__linux__) || defined(__SOLARIS)) && defined(__INTEL32))

/* For Linux, check library.agn */
static int os_cpuinfo (lua_State *L) {  /* Agena 1.9.3 */
#ifdef CPUID
  uint32_t i, maxcall, info[4];  /* 2.14.3 */
#endif
  luaL_checkstack(L, 3, "not enough stack space");  /* 5.4.0 */
  lua_createtable(L, 0, 10);
#ifdef CPUID
  /* https://en.wikipedia.org/wiki/CPUID, 2.14.3 */
  maxcall = cpuid_maxcall();
  if (maxcall == 0)
    luaL_error(L, "Error in " LUA_QS ": cannot read out CPU registers.", "os.cpuinfo");
  lua_pushstring(L, "cpuid");  /* 5.5.10, now also for non-Windows platforms */
  lua_createtable(L, maxcall - 1, 1);
  /* CPUID(0) - basic CPU information,
     CPUID(1) - additional CPU information,
     CPUID(2) - cache and TLB information */
  for (i=0; i <= maxcall; i++) {
    luaL_checkstack(L, 3, "not enough stack space");  /* 5.4.0 */
    lua_pushinteger(L, i);
    agn_createreg(L, 4);
    if (cpuid(info, i)) {
      agn_regsetinumber(L, -1, 1, info[0]);  /* eax */
      agn_regsetinumber(L, -1, 2, info[1]);  /* ebx */
      agn_regsetinumber(L, -1, 3, info[2]);  /* ecx */
      agn_regsetinumber(L, -1, 4, info[3]);  /* edx */
    }
    lua_rawset(L, -3);
  }
  lua_rawset(L, -3);
  /* Source: http://newbiz.github.io/cpp/2010/12/20/Playing-with-cpuid.html
  edx, AND this with os.cpuinfo().cpuid[1, 4], e.g. os.cpuinfo().cpuid1 <<< `FPU':
  FPU   = 1<< 0, Floating-Point Unit on-chip
  VME   = 1<< 1, Virtual Mode Extension
  DE    = 1<< 2, Debugging Extension
  PSE   = 1<< 3, Page Size Extension
  TSC   = 1<< 4, Time Stamp Counter
  MSR   = 1<< 5, Model Specific Registers
  PAE   = 1<< 6, Physical Address Extension
  MCE   = 1<< 7, Machine Check Exception
  CX8   = 1<< 8, CMPXCHG8 Instruction
  APIC  = 1<< 9, On-chip APIC hardware
  SEP   = 1<<11, Fast System Call
  MTRR  = 1<<12, Memory type Range Registers
  PGE   = 1<<13, Page Global Enable
  MCA   = 1<<14, Machine Check Architecture
  CMOV  = 1<<15, Conditional MOVe Instruction
  PAT   = 1<<16, Page Attribute Table
  PSE36 = 1<<17, 36bit Page Size Extension
  PSN   = 1<<18, Processor Serial Number
  CLFSH = 1<<19, CFLUSH Instruction
  DS    = 1<<21, Debug Store
  ACPI  = 1<<22, Thermal Monitor & Software Controlled Clock
  MMX   = 1<<23, MultiMedia eXtension
  FXSR  = 1<<24, Fast Floating Point Save & Restore
  SSE   = 1<<25, Streaming SIMD Extension 1
  SSE2  = 1<<26, Streaming SIMD Extension 2
  SS    = 1<<27, Self Snoop
  HTT   = 1<<28, Hyper Threading Technology
  TM    = 1<<29, Thermal Monitor
  PBE   = 1<<31, Pend Break Enabled

  ecx, AND this with os.cpuinfo().cpuid[1, 3], e.g. os.cpuinfo().cpuid1 <<< SSE3, see also: lib/gcc/mingw32/6.3.0/include/cpuid.h
  SSE3  = 1<< 0, Streaming SIMD Extension 3
  PCLMUL= 1<< 1, cpuid.h
  MW    = 1<< 3, Mwait instruction
  CPL   = 1<< 4, CPL-qualified Debug Store
  VMX   = 1<< 5, VMX (cpuid.h: LZCNT)
  EST   = 1<< 7, Enhanced Speed Test
  TM2   = 1<< 8, Thermal Monitor 2
  SSSE3 = 1<< 9, cpuid.h
  L1    = 1<<10, L1 Context ID
  FMA   = 1<<12, cpuid.h
  CAE   = 1<<13, CompareAndExchange 16B = CMPXCHG16B
  SSE4_1= 1<<19, cpuid.h
  SSE4_2= 1<<20, cpuid.h
  MOVBE=  1<<22, cpuid.h
  POPCNT= 1<<23, cpuid.h
  AES=    1<<25, cpuid.h
  XSAVE=  1<<26, cpuid.h
  OSXSAVE=1<<27, cpuid.h
  AVX=    1<<28, cpuid.h
  F16C=   1<<29, cpuid.h
  RDRND=  1<<30, cpuid.h */
#endif
#ifdef CPUINFO
  __builtin_cpu_init();
  /* cputype, see: https://gcc.gnu.org/onlinedocs/gcc/x86-Built-in-Functions.html */
  lua_pushstring(L, "cputype");
  agn_createset(L, 8);
  if (__builtin_cpu_is("intel") != 0)
    { lua_srawsetstring(L, -1, "intel"); }  /* Intel CPU */
  if (__builtin_cpu_is("atom") != 0)
    { lua_srawsetstring(L, -1, "atom"); }  /* Intel Atom CPU */
  if (__builtin_cpu_is("core2") != 0)
    { lua_srawsetstring(L, -1, "core2"); }  /* Intel Core 2 CPU */
  if (__builtin_cpu_is("corei7") != 0)
    { lua_srawsetstring(L, -1, "corei7"); }  /* Intel Core i7 CPU */
  if (__builtin_cpu_is("nehalem") != 0)
    { lua_srawsetstring(L, -1, "nehalem"); }  /* Intel Core i7 Nehalem CPU */
  if (__builtin_cpu_is("westmere") != 0)
    { lua_srawsetstring(L, -1, "westmere"); }  /* Intel Core i7 Westmere CPU */
  if (__builtin_cpu_is("sandybridge") != 0)
    { lua_srawsetstring(L, -1, "sandybridge"); }  /* Intel Core i7 Sandy Bridge CPU */
  if (__builtin_cpu_is("ivybridge") != 0)
    { lua_srawsetstring(L, -1, "ivybridge"); }  /* Intel Core i7 Ivy Bridge CPU */
  if (__builtin_cpu_is("haswell") != 0)
    { lua_srawsetstring(L, -1, "haswell"); }  /* Intel Core i7 Haswell CPU */
  if (__builtin_cpu_is("broadwell") != 0)
    { lua_srawsetstring(L, -1, "broadwell"); }  /* Intel Core i7 Broadwell CPU */
  if (__builtin_cpu_is("skylake") != 0)
    { lua_srawsetstring(L, -1, "skylake"); }  /* Intel Core i7 Skylake CPU */
  if (__builtin_cpu_is("skylake-avx512") != 0)
    { lua_srawsetstring(L, -1, "skylake-avx512"); }  /* Intel Core i7 Skylake AVX512 CPU */
  if (__builtin_cpu_is("bonnell") != 0)
    { lua_srawsetstring(L, -1, "bonnell"); }  /* Intel Atom Bonnell CPU */
  if (__builtin_cpu_is("silvermont") != 0)
    { lua_srawsetstring(L, -1, "silvermont"); }  /* Intel Atom Silvermont CPU */
#if __GNUC__ < 15
  if (__builtin_cpu_is("knl") != 0)
    { lua_srawsetstring(L, -1, "knl"); }  /* Intel Knights Landing CPU */
#endif
  if (__builtin_cpu_is("amd") != 0)
    { lua_srawsetstring(L, -1, "amd"); }  /* AMD CPU */
  if (__builtin_cpu_is("amdfam10h") != 0)
    { lua_srawsetstring(L, -1, "amdfam10h"); }  /* AMD Family 10h CPU */
  if (__builtin_cpu_is("barcelona") != 0)
    { lua_srawsetstring(L, -1, "barcelona"); }  /* AMD Family 10h Barcelona CPU */
  if (__builtin_cpu_is("shanghai") != 0)
    { lua_srawsetstring(L, -1, "shanghai"); }  /* AMD Family 10h Shanghai CPU */
  if (__builtin_cpu_is("istanbul") != 0)
    { lua_srawsetstring(L, -1, "istanbul"); }  /* AMD Family 10h Istanbul CPU */
  if (__builtin_cpu_is("btver1") != 0)
    { lua_srawsetstring(L, -1, "amdfam14h"); }  /* AMD Family 14h CPU */
  if (__builtin_cpu_is("amdfam15h") != 0)
    { lua_srawsetstring(L, -1, "amdfam15h"); }  /* AMD Family 15h CPU */
  if (__builtin_cpu_is("bdver1") != 0)
    { lua_srawsetstring(L, -1, "bdver1"); }  /* AMD Family 15h Bulldozer version 1 CPU */
  if (__builtin_cpu_is("bdver2") != 0)
    { lua_srawsetstring(L, -1, "bdver2"); }  /* AMD Family 15h Bulldozer version 2 CPU */
  if (__builtin_cpu_is("bdver3") != 0)
    { lua_srawsetstring(L, -1, "bdver3"); }  /* AMD Family 15h Bulldozer version 3 CPU */
  if (__builtin_cpu_is("bdver4") != 0)
    { lua_srawsetstring(L, -1, "bdver4"); }  /* AMD Family 15h Bulldozer version 4 CPU */
  if (__builtin_cpu_is("btver2") != 0)
    { lua_srawsetstring(L, -1, "amdfam16h"); }  /* AMD Family 16h CPU */
  if (__builtin_cpu_is("slm") != 0)  /* Intel Silvermont CPU. 5.5.9 */
    { lua_srawsetstring(L, -1, "slm"); }
  if (__builtin_cpu_is("bonnell") != 0)
    { lua_srawsetstring(L, -1, "bonnell"); }  /* Intel Intel Atom Bonnell CPU. 5.5.9 */
  if (__builtin_cpu_is("silvermont") != 0)
    { lua_srawsetstring(L, -1, "silvermont"); }  /* Intel Atom Silvermont CPU. 5.5.9 */
  if (__builtin_cpu_is("znver1") != 0)
    { lua_srawsetstring(L, -1, "znver1"); }  /* AMD Family 17h Zen version 1 CPU */
#if !(defined(__linux__) && (defined(__INTEL32) || defined(__ARMCPU)) || defined(__SOLARIS))
  if (__builtin_cpu_is("cannonlake") != 0)
    { lua_srawsetstring(L, -1, "cannonlake"); }  /* Intel Core i7 Cannon Lake CPU. 5.5.9 */
  if (__builtin_cpu_is("icelake-client") != 0)
    { lua_srawsetstring(L, -1, "icelake-client"); }  /* Intel Core i7 Ice Lake Client CPU. 5.5.9 */
  if (__builtin_cpu_is("icelake-server") != 0)
    { lua_srawsetstring(L, -1, "icelake-server"); }  /* Intel Core i7 Ice Lake Client CPU. 5.5.9 */
  if (__builtin_cpu_is("goldmont") != 0)
    { lua_srawsetstring(L, -1, "goldmont"); }  /* Intel Atom Goldmont CPU. 5.5.9 */
  if (__builtin_cpu_is("goldmont-plus") != 0)
    { lua_srawsetstring(L, -1, "goldmont-plus"); }  /* Intel Atom Goldmont Plus CPU. 5.5.9 */
  if (__builtin_cpu_is("tremont") != 0)
    { lua_srawsetstring(L, -1, "tremont"); }  /* Intel Atom Tremont CPU */
#if __GNUC__ < 15
  if (__builtin_cpu_is("knm") != 0)
    { lua_srawsetstring(L, -1, "knm"); }  /* Intel Knights Mill CPU */
#endif
  if (__builtin_cpu_is("amdfam17h") != 0)
    { lua_srawsetstring(L, -1, "amdfam17h"); }  /* AMD Family 17h CPU */
  if (__builtin_cpu_is("znver2") != 0)
    { lua_srawsetstring(L, -1, "znver2"); }  /* AMD Family 17h Zen version 2 CPU */
#ifndef __OS2__
  if (__builtin_cpu_is("cascadelake") != 0)
    { lua_srawsetstring(L, -1, "cascadelake"); }  /* Intel Core i7 Cascadelake CPU. 5.5.9 */
#endif
#endif
#ifdef NOTTOBECOMPILED
  if (__builtin_cpu_is("tigerlake") != 0)
    { lua_srawsetstring(L, -1, "tigerlake"); }  /* Intel Core i7 Tigerlake CPU. 5.5.9 */
  if (__builtin_cpu_is("cooperlake") != 0)
    { lua_srawsetstring(L, -1, "cooperlake"); }  /* Intel Core i7 Cooperlake CPU. 5.5.9 */
  if (__builtin_cpu_is("sapphirerapids") != 0)
    { lua_srawsetstring(L, -1, "sapphirerapids"); }  /* Intel Core i7 sapphirerapids CPU. 5.5.9 */
  if (__builtin_cpu_is("alderlake") != 0)
    { lua_srawsetstring(L, -1, "alderlake"); }  /* Intel Core i7 Alderlake CPU. 5.5.9 */
  if (__builtin_cpu_is("rocketlake") != 0)
    { lua_srawsetstring(L, -1, "rocketlake"); }  /* Intel Core i7 Rocketlake CPU. 5.5.9 */
  if (__builtin_cpu_is("graniterapids") != 0)
    { lua_srawsetstring(L, -1, "graniterapids"); }  /* Intel Core i7 graniterapids CPU. 5.5.9 */
  if (__builtin_cpu_is("graniterapids-d") != 0)
    { lua_srawsetstring(L, -1, "graniterapids-d"); }  /* Intel Core i7 graniterapids D CPU. 5.5.9 */
  if (__builtin_cpu_is("arrowlake") != 0)
    { lua_srawsetstring(L, -1, "arrowlake"); }  /* Intel Core i7 Arrow Lake CPU. 5.5.9 */
  if (__builtin_cpu_is("arrowlake-s") != 0)
    { lua_srawsetstring(L, -1, "arrowlake-s"); }  /* Intel Core i7 Arrow Lake S CPU. 5.5.9 */
  if (__builtin_cpu_is("pantherlake") != 0)
    { lua_srawsetstring(L, -1, "pantherlake"); }  /* Intel Core i7 Panther Lake CPU. 5.5.9 */
  if (__builtin_cpu_is("diamondrapids") != 0)
    { lua_srawsetstring(L, -1, "diamondrapids"); }  /* Intel Core i7 Diamond Rapids CPU. 5.5.9 */
  if (__builtin_cpu_is("sierraforest") != 0)
    { lua_srawsetstring(L, -1, "sierraforest"); }  /* Intel Atom Sierra Forest CPU */
  if (__builtin_cpu_is("clearwaterforest") != 0)
    { lua_srawsetstring(L, -1, "clearwaterforest"); }  /* Intel Atom Clearwater Forest CPU */
  if (__builtin_cpu_is("amdfam19h") != 0)
    { lua_srawsetstring(L, -1, "amdfam19h"); }  /* AMD Family 19h CPU */
  if (__builtin_cpu_is("znver3") != 0)
    { lua_srawsetstring(L, -1, "znver3"); }  /* AMD Family 17h Zen version 3 CPU */
  if (__builtin_cpu_is("znver4") != 0)
    { lua_srawsetstring(L, -1, "znver4"); }  /* AMD Family 17h Zen version 4 CPU */
  if (__builtin_cpu_is("znver5") != 0)
    { lua_srawsetstring(L, -1, "znver5"); }  /* AMD Family 17h Zen version 5 CPU */
  if (__builtin_cpu_is("penryn") != 0)
    { lua_srawsetstring(L, -1, "penryn"); }  /* Intel Core 2 CPU */
  if (__builtin_cpu_is("k6") != 0)
    { lua_srawsetstring(L, -1, "k6"); }  /* AMD K6 */
  if (__builtin_cpu_is("k6-2") != 0)
    { lua_srawsetstring(L, -1, "k6-2"); }  /* AMD K62 */
  if (__builtin_cpu_is("k6-3") != 0)
    { lua_srawsetstring(L, -1, "k6-3"); }  /* AMD K63 */
  if (__builtin_cpu_is("geode") != 0)
    { lua_srawsetstring(L, -1, "geode"); }  /* Geode */
#endif
  if (agn_ssize(L, -1) > 0) {  /* 5.5.10 */
    lua_rawset(L, -3);
  } else {
    agn_poptoptwo(L);
  }
  /* cpu support */
  lua_pushstring(L, "support");
  /* see: https://github.com/llvm-mirror/clang/blob/release_50/include/clang/Basic/BuiltinsX86.def */
  agn_createset(L, 8);
#if !CPUINFOEXCLUDE && (__GNUC__ < 15)
  if (__builtin_cpu_supports("avx5124vnniw") != 0)
    { lua_srawsetstring(L, -1, "avx5124vnniw"); }  /* AVX5124VNNIW instructions */
  if (__builtin_cpu_supports("avx5124fmaps") != 0)
    { lua_srawsetstring(L, -1, "avx5124fmaps"); }  /* AVX5124FMAPS instructions */
#endif
  {  /* 5.5.9 detect further chipsets */
    int call;
    uint32_t eax, nexids, info[4];
    if (cpuid(info, 0)) {
      eax = info[0];
      call = 0x00000001;  /* get chipset info */
      if (eax > call) {
        cpuid(info, call);
        if ((info[3] & ((int)1 << 31)) != 0) { lua_srawsetstring(L, -1, "3dnow"); }  /* 5.5.10 */
        if ((info[3] & ((int)1 << 30)) != 0) { lua_srawsetstring(L, -1, "3dnowp"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 << 25)) != 0) { lua_srawsetstring(L, -1, "aes"); }
        if ((info[2] & ((int)1 << 28)) != 0) { lua_srawsetstring(L, -1, "avx"); }
        if ((info[3] & ((int)1 << 15)) != 0) { lua_srawsetstring(L, -1, "cmov"); }  /* 5.5.10 */
        if ((info[3] & ((int)1 << 15)) != 0) { lua_srawsetstring(L, -1, "cmpxchg8b"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 << 29)) != 0) { lua_srawsetstring(L, -1, "f16c"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 << 12)) != 0) { lua_srawsetstring(L, -1, "fma"); }  /* = fma 3 ? */
        if ((info[2] & ((int)1 << 16)) != 0) { lua_srawsetstring(L, -1, "fma4"); }  /* Gemini AI */
        if ((info[3] & ((int)1 << 29)) != 0) { lua_srawsetstring(L, -1, "lm"); }
        if ((info[2] & ((int)1 <<  5)) != 0) { lua_srawsetstring(L, -1, "lzcnt"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 << 22)) != 0) { lua_srawsetstring(L, -1, "movb"); }  /* 5.5.10 */
        if ((info[3] & ((int)1 << 23)) != 0) { lua_srawsetstring(L, -1, "mmx"); }
        if ((info[3] & ((int)1 << 22)) != 0) { lua_srawsetstring(L, -1, "mmxext"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 << 27)) != 0) { lua_srawsetstring(L, -1, "osxsave"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 <<  1)) != 0) { lua_srawsetstring(L, -1, "pclmul"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 << 23)) != 0) { lua_srawsetstring(L, -1, "popcnt"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 << 30)) != 0) { lua_srawsetstring(L, -1, "rdrand"); }
        if ((info[3] & ((int)1 << 25)) != 0) { lua_srawsetstring(L, -1, "sse"); }
        if ((info[3] & ((int)1 << 25)) != 0) { lua_srawsetstring(L, -1, "sse2"); }
        if ((info[2] & ((int)1 <<  0)) != 0) { lua_srawsetstring(L, -1, "sse3"); }
        if ((info[2] & ((int)1 << 19)) != 0) { lua_srawsetstring(L, -1, "sse4.1"); }
        if ((info[2] & ((int)1 << 20)) != 0) { lua_srawsetstring(L, -1, "sse4.2"); }
        if ((info[2] & ((int)1 <<  9)) != 0) { lua_srawsetstring(L, -1, "ssse3"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 << 26)) != 0) { lua_srawsetstring(L, -1, "xsave"); }  /* 5.5.10 */
      }
      call = 0x00000007;
      if (eax >= call) {
        cpuid(info, call);
        if ((info[1] & ((int)1 << 19)) != 0) { lua_srawsetstring(L, -1, "adx"); }
        if ((info[1] & ((int)1 <<  5)) != 0) { lua_srawsetstring(L, -1, "avx2"); }
        if ((info[1] & ((int)1 << 30)) != 0) { lua_srawsetstring(L, -1, "avx512bw"); }
        if ((info[1] & ((int)1 << 28)) != 0) { lua_srawsetstring(L, -1, "avx512cd"); }
        if ((info[1] & ((int)1 << 17)) != 0) { lua_srawsetstring(L, -1, "avx512dq"); }
        if ((info[1] & ((int)1 << 27)) != 0) { lua_srawsetstring(L, -1, "avx512er"); }
        if ((info[1] & ((int)1 << 16)) != 0) { lua_srawsetstring(L, -1, "avx512f"); }
        if ((info[1] & ((int)1 << 21)) != 0) { lua_srawsetstring(L, -1, "avx512ifma"); }
        if ((info[1] & ((int)1 << 26)) != 0) { lua_srawsetstring(L, -1, "avx512pf"); }
        if ((info[2] & ((int)1 <<  1)) != 0) { lua_srawsetstring(L, -1, "avx512vbmi"); }
        if ((info[1] & ((int)1 << 31)) != 0) { lua_srawsetstring(L, -1, "avx512vl"); }
        if ((info[1] & ((int)1 <<  3)) != 0) { lua_srawsetstring(L, -1, "bmi"); }
        if ((info[1] & ((int)1 <<  8)) != 0) { lua_srawsetstring(L, -1, "bmi2"); }
        if ((info[1] & ((int)1 << 23)) != 0) { lua_srawsetstring(L, -1, "clflushopt"); }  /* 5.5.10 */
        if ((info[1] & ((int)1 << 24)) != 0) { lua_srawsetstring(L, -1, "clwb"); }  /* 5.5.10 */
        if ((info[1] & ((int)1 <<  0)) != 0) { lua_srawsetstring(L, -1, "fsgsbase"); }  /* 5.5.10 */
        if ((info[1] & ((int)1 <<  4)) != 0) { lua_srawsetstring(L, -1, "hle"); }  /* 5.5.10 */
        if ((info[1] & ((int)1 << 14)) != 0) { lua_srawsetstring(L, -1, "mpx"); }  /* 5.5.10 */
        if ((info[1] & ((int)1 << 22)) != 0) { lua_srawsetstring(L, -1, "pcommit"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 << 29)) != 0) { lua_srawsetstring(L, -1, "pku"); }  /* with help of Gemini AI, 5.5.9 */
        if ((info[2] & ((int)1 <<  0)) != 0) { lua_srawsetstring(L, -1, "prefetchwt1"); }
        if ((info[1] & ((int)1 << 18)) != 0) { lua_srawsetstring(L, -1, "rdseed"); }
        if ((info[1] & ((int)1 << 11)) != 0) { lua_srawsetstring(L, -1, "rtm"); }  /* with help of Gemini AI, 5.5.9 */
        if ((info[1] & ((int)1 << 29)) != 0) { lua_srawsetstring(L, -1, "sha"); }
      }
      cpuid(info, 0x80000000);
      nexids = info[0];
      call = 0x80000001;
      if (nexids >= call) {
        cpuid(info, call);
        if ((info[2] & ((int)1 <<  5)) != 0) { lua_srawsetstring(L, -1, "abm"); }
        if ((info[2] & ((int)1 << 16)) != 0) { lua_srawsetstring(L, -1, "fma4"); }
        if ((info[2] & ((int)1 <<  0)) != 0) { lua_srawsetstring(L, -1, "lahf_lm"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 << 15)) != 0) { lua_srawsetstring(L, -1, "lwp"); }
        if ((info[2] & ((int)1 <<  8)) != 0) { lua_srawsetstring(L, -1, "prefchw"); }  /* 5.5.10 */
        if ((info[2] & ((int)1 <<  6)) != 0) { lua_srawsetstring(L, -1, "sse4a"); }
        if ((info[2] & ((int)1 << 21)) != 0) { lua_srawsetstring(L, -1, "tbm"); }
        if ((info[3] & ((int)1 << 29)) != 0) { lua_srawsetstring(L, -1, "x64"); }
        if ((info[2] & ((int)1 << 11)) != 0) { lua_srawsetstring(L, -1, "xop"); }
      }
    }
  }
#ifdef NOTTOBECOMPILED
  /* Gemini AI: The "x86-64" micro-architecture levels (v1, v2, v3, v4) were introduced as standard baselines
     for x86-64 CPUs. They are not individual instruction set extensions but rather collections of instruction
     sets. For instance:
       x86-64-v2 includes instruction sets like SSE4.2, SSSE3, and POPCNT.
       x86-64-v3 adds AVX, AVX2, and FMA.
       x86-64-v4 is still in development but includes more advanced extensions like AVX512. */
  if (__builtin_cpu_supports("x86-64") != 0)
    { lua_srawsetstring(L, -1, "x86-64"); }  /* Baseline x86-64 microarchitecture level (as defined in x86-64 psABI), 5.5.9 */
  if (__builtin_cpu_supports("x86-64-v2") != 0)
    { lua_srawsetstring(L, -1, "x86-64-v2"); }  /* x86-64-v2 microarchitecture level. 5.5.9 */
  if (__builtin_cpu_supports("x86-64-v3") != 0)
    { lua_srawsetstring(L, -1, "x86-64-v3"); }  /* x86-64-v3 microarchitecture level. 5.5.9 */
  if (__builtin_cpu_supports("x86-64-v4") != 0)
    { lua_srawsetstring(L, -1, "x86-64-v4"); }  /* x86-64-v4 microarchitecture level. 5.5.9 */
  if (__builtin_cpu_supports("avx5124fmaps") != 0)
    { lua_srawsetstring(L, -1, "avx5124fmaps"); }  /* AVX5124FMAPS instructions */
  if (__builtin_cpu_supports("avx512vpopcntdq") != 0)
    { lua_srawsetstring(L, -1, "avx512vpopcntdq"); }  /* AVX512VPOPCNTDQ instructions */
  if (__builtin_cpu_supports("avx512vbmi2") != 0)
    { lua_srawsetstring(L, -1, "avx512vbmi2"); }  /* AVX512VBMI2 instructions */
  if (__builtin_cpu_supports("gfni") != 0)
    { lua_srawsetstring(L, -1, "gfni"); }  /* GFNI instructions */
  if (__builtin_cpu_supports("vpclmulqdq") != 0)
    { lua_srawsetstring(L, -1, "vpclmulqdq"); }  /* VPCLMULQDQ instructions */
  if (__builtin_cpu_supports("avx512vnni") != 0)
    { lua_srawsetstring(L, -1, "avx512vnni"); }  /* AVX512VNNI instructions */
  if (__builtin_cpu_supports("avx512bitalg") != 0)
    { lua_srawsetstring(L, -1, "avx512bitalg"); }  /* AVX512BITALG instructions */
  if (__builtin_cpu_supports("fxsr") != 0)
    { lua_srawsetstring(L, -1, "fxsr"); }  /* FXSR instructions */
  if (__builtin_cpu_supports("xsaveopt") != 0)
    { lua_srawsetstring(L, -1, "xsaveopt"); }  /* XSAVE instructions */
  if (__builtin_cpu_supports("xsavec") != 0)
    { lua_srawsetstring(L, -1, "xsavec"); }  /* XSAVEC instructions */
  if (__builtin_cpu_supports("xsaves") != 0)
    { lua_srawsetstring(L, -1, "xsaves"); }  /* XSAVES instructions */
#endif
  if (agn_ssize(L, -1) > 0) {  /* 5.5.10 */
    lua_rawset(L, -3);
  } else {
    agn_poptoptwo(L);
  }
#endif
#if defined(__GNUC__) && defined(__INTEL) && !defined(__APPLE__)
  /* Taken from https://copyprogramming.com/howto/c-program-to-determine-levels-size-of-cache; 2.37.9 */
  {
    uint32_t eax, ebx, ecx, edx;
    int i, cache_type, cache_level, cache_is_self_initializing, cache_is_fully_associative;
    char *cache_type_string;
    unsigned int cache_sets, cache_coherency_line_size, cache_physical_line_partitions, cache_ways_of_associativity;
    size_t cache_total_size;
    luaL_checkstack(L, 3, "not enough stack space");  /* 5.4.0 */
    lua_createtable(L, 0, 0);
    for (i=0; i < 32; i++) {
      /* Variables to hold the contents of the 4 i386 legacy registers */
      eax = 4;  /* get cache info */
      ecx = i;  /* cache id */
      __asm__  __volatile__ (
        "cpuid"  /* call i386 cpuid instruction */
        : "+a" (eax)  /* contains the cpuid command code, 4 for cache query */
        , "=b" (ebx)
        , "+c" (ecx)  /* contains the cache id */
        , "=d" (edx)
      ); /* generates output in 4 registers eax, ebx, ecx and edx
         See the page 3-191 of the manual. */
      cache_type = eax & 0x1F;
      if (cache_type == 0)  /* end of valid cache identifiers */
        break;
      lua_createtable(L, 0, 9);
      switch (cache_type) {
        case 1: cache_type_string = "data"; break;
        case 2: cache_type_string = "instruction"; break;
        case 3: cache_type_string = "unified"; break;
        default: cache_type_string = "unknown"; break;
      }
      cache_level = (eax >>= 5) & 0x7;
      cache_is_self_initializing = (eax >>= 3) & 0x1; /* does not need SW initialization */
      cache_is_fully_associative = (eax >>= 1) & 0x1;
      /* See the page 3-192 of the manual.
         ebx contains 3 integers of 10, 10 and 12 bits respectively */
      cache_sets = ecx + 1;
      cache_coherency_line_size = (ebx & 0xFFF) + 1;
      cache_physical_line_partitions = ((ebx >>= 12) & 0x3FF) + 1;
      cache_ways_of_associativity = ((ebx >>= 10) & 0x3FF) + 1;
      /* Total cache size is the product */
      cache_total_size = cache_ways_of_associativity * cache_physical_line_partitions * cache_coherency_line_size * cache_sets;
      lua_rawsetstringinteger(L, -1, "level", cache_level);
      lua_rawsetstringstring(L,  -1, "cache type", cache_type_string);
      lua_rawsetstringinteger(L, -1, "sets", cache_sets);
      lua_rawsetstringinteger(L, -1, "coherency line size", cache_coherency_line_size);
      lua_rawsetstringinteger(L, -1, "physical line partitions", cache_physical_line_partitions);
      lua_rawsetstringinteger(L, -1, "ways of associativity", cache_ways_of_associativity);
      lua_rawsetstringinteger(L, -1, "size", cache_total_size);
      lua_rawsetstringboolean(L, -1, "fully associative", cache_is_fully_associative);
      lua_rawsetstringboolean(L, -1, "self initialising", cache_is_self_initializing);
      lua_rawseti(L, -2, i);
    }
    lua_setfield(L, -2, "cache");
  }
#endif
#ifdef _WIN32
  DWORD procspeed;
  SYSTEM_INFO sysinfo;
  char *brand, *vendor;
  if (tools_getwinver() < MS_WIN2K) {  /* NT 4.0 or earlier ?  2.17.4 change */
    lua_pushfail(L);
    return 1;
  }
  GetSystemInfo(&sysinfo);
  brand = vendor = NULL;
  lua_rawsetstringinteger(L, -1, "ncpu", sysinfo.dwNumberOfProcessors);
  lua_pushstring(L, "type");
  switch (sysinfo.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64: {  /* 9 */
      lua_pushstring(L, "x64");
      break;
    }
    case PROCESSOR_ARCHITECTURE_ARM: {  /* reserved */
      lua_pushstring(L, "ARM");
      break;
    }
    case PROCESSOR_ARCHITECTURE_IA64: {  /* 6 */
      lua_pushstring(L, "Itanium");
      break;
    }
    case PROCESSOR_ARCHITECTURE_INTEL: {  /* 0 */
      lua_pushstring(L, "x86");
      break;
    }
    case PROCESSOR_ARCHITECTURE_UNKNOWN: {   /* 0xffff */
      lua_pushstring(L, "unknown");
      break;
    }
    default: {
      lua_pushstring(L, "unknown");
    }
  }
  lua_rawset(L, -3);
  lua_rawsetstringnumber(L, -1, "level", sysinfo.wProcessorLevel);
  lua_rawsetstringnumber(L, -1, "revision", sysinfo.wProcessorRevision);
  lua_pushstring(L, "frequency");
  procspeed = ProcSpeedRead();
  lua_pushnumber(L, (procspeed == -1) ? AGN_NAN : procspeed);  /* 2.10.0 fix */
  lua_rawset(L, -3);
  brand = BrandRead();
  if (brand != NULL) {
    lua_rawsetstringstring(L, -1, "brand", brand);
    xfree(brand);
  }
  vendor = VendorRead();
  if (vendor != NULL) {
    lua_rawsetstringstring(L, -1, "vendor", vendor);
    xfree(vendor);
  }
  lua_pushstring(L, "bigendian");
  agn_pushboolean(L, tools_endian());
  lua_rawset(L, -3);
#elif __APPLE__
  uint64_t data;
  size_t size, brandlen, vendorlen;
  char brand[TOTALBYTES];
  char vendor[TOTALBYTES];
  brand[0] = '\0'; brandlen = TOTALBYTES;
  vendor[0] = '\0'; vendorlen = TOTALBYTES;
  data = 0;
  size = sizeof(data);
  if (sysctlbyname("hw.cpufrequency", &data, &size, NULL, 0) >= 0) {
    lua_rawsetstringnumber(L, -1, "frequency", data/1e6);
  }
  data = 0; size = sizeof(data);
  if (sysctlbyname("hw.ncpu", &data, &size, NULL, 0) >= 0) {
    lua_rawsetstringnumber(L, -1, "ncpu", data);
  }
  data = 0; size = sizeof(data);
  if (sysctlbyname("hw.cputype", &data, &size, NULL, 0) >= 0) {
    lua_pushstring(L, "type");
    if (data >= 0 && data < 19) {
      uint64_t is64 = 0;
      if (sysctlbyname("hw.cpu64bit_capable", &is64, &size, NULL, 0) >= 0) {
        if (is64 && (data == 7)) lua_pushstring(L, "x64");
        else if (is64 && (data == 18)) lua_pushstring(L, "ppc64");
        else lua_pushstring(L, cputype[data]);
      } else
        lua_pushstring(L, cputype[data]);
    } else
      lua_pushstring(L, "unknown");
    lua_rawset(L, -3);
  }
  data = 0; size = sizeof(data);
  /* http://cr.yp.to/hardware/ppc.html:
     MacOS sysctl reports "hw.cputype: 18" for PowerPC, and in particular "hw.cpusubtype":
     1 for 601, 2 for 602, 3 or 4 or 5 for 603, 6 or 7 for 604, 8 for 620, 9 for 750, 10 for 7400, 11 for 7450, 100 for 970. */
  if (sysctlbyname("hw.cpusubtype", &data, &size, NULL, 0) >= 0) {
    lua_rawsetstringnumber(L, -1, "level", data);
  }
  if (sysctlbyname("machdep.cpu.brand_string", &brand, &brandlen, NULL, 0) >= 0) {
    lua_rawsetstringstring(L, -1, "brand", brand);
  }
  if (sysctlbyname("machdep.cpu.vendor", &vendor, &vendorlen, NULL, 0) >= 0) {
    lua_rawsetstringstring(L, -1, "vendor", vendor);
  }
  data = 0; size = sizeof(data);
  if (sysctlbyname("machdep.cpu.model", &data, &size, NULL, 0) >= 0) {
    lua_rawsetstringnumber(L, -1, "model", data);
  }
  data = 0; size = sizeof(data);
  if (sysctlbyname("machdep.cpu.stepping", &data, &size, NULL, 0) >= 0) {
    lua_rawsetstringnumber(L, -1, "stepping", data);
  }
  lua_pushstring(L, "bigendian");
  agn_pushboolean(L, tools_endian());
  lua_rawset(L, -3);
#elif defined(__sparc)
  otherarch(L, "sparc");
#elif defined(__SOLARIS)  /* new 2.18.2 */
  otherarch(L, "i386pc");
#elif __powerpc__
  otherarch(L, "ppc");  /* 1.9.5 */
#elif defined(__DJGPP__) || defined(__OS2__) || defined(__EMX__) || (defined(__linux__) && defined(__INTEL32))  /* 1.9.5, 5.6.2 */
  otherarch(L, "x86");
#elif defined(__linux__) && defined(__INTEL64)  /* 5.6.2, 6.0.0 RC 2 fix */
  otherarch(L, "x64");
#elif defined(__linux__) && defined(__aarch64__)
  otherarch(L, "aarch64");
#elif defined(__linux__) && defined(__arm__)
  otherarch(L, "arm");
#endif
#ifdef __INTEL
  lua_rawsetstringnumber(L, -1, "cyclesperusec", tools_getcyclesperusec());  /* in microseconds */
#endif
  return 1;
}


/* Gemini AI:

__cpuidex (Windows): This intrinsic function, available in compilers like MSVC and GCC for Windows targets, populates four integer
pointers with the values from the CPUID instruction. The function's signature is typically:

void __cpuidex(int cpuInfo[4], int functionId, int subfunctionId);.

    cpuInfo[0] (EAX)
    cpuInfo[1] (EBX)
    cpuInfo[2] (ECX)
    cpuInfo[3] (EDX)

    It returns void, but the actual information is passed back through the cpuInfo array.

__cpuid_count (GCC): This is the generic GCC intrinsic for the CPUID instruction. Its signature is often seen as
__cpuid_count(unsigned int functionId, unsigned int subfunctionId, unsigned int *eax, unsigned int *ebx,
unsigned int *ecx, unsigned int *edx);.

    The return values are written to the pointer arguments: eax, ebx, ecx, and edx.

    Like __cpuidex, this function itself has a void return type, as the information is returned via the pointers.

Input EAX = 0 (Basic CPUID Info):

When you call __cpuid_count with functionId = 0, the EAX register (cpuInfo[0]) returns the highest basic CPUID
leaf number the processor supports. For example, if it returns 0x16, you know you can call CPUID with functionId
values from 0 to 0x16 to get valid information. The other registers (EBX, ECX, EDX) will contain the processor's
vendor ID string (e.g., "GenuineIntel" or "AuthenticAMD").

Input EAX = 1 (Processor Features)

When you call __cpuid_count with functionId = 1, the EAX register (cpuInfo[0]) returns the processor's signature.
This is a composite value containing information about the CPU's:

    Stepping ID
    Model
    Family
    Processor Type
    Extended Model
    Extended Family

These values are extracted by bitwise operations on the EAX return value. In this case, cpuInfo[1], cpuInfo[2],
and cpuInfo[3] contain the feature flags for the processor. */

/* Reads out the EAX, EBX, ECX and EDX registers of the x86-compatble CPU. You pass a function number (CPUID leaf)
   and optionally the number of the desired register, which may be 1 for EAX, 2 for EBX, 3 for ECX, 4 for EDX or
   0 for all four registers. See the descriptions in the Internet on GCC's __cpuid_count() and __cpuid() functions
   for further information. You might also check the Agena source code file `src/loslib.c` for examples, just grep
   for the two C functions mentioned above.

   > eax, ebx, ecx, edx := os.cpuid(1);

   For AVX:

   > ecx && (1 <<< 28) <> 0:
   true

   See also: `os.xgetbv`.  */
static int os_cpuid (lua_State *L) {  /* Agena 2.14.3/5.5.9 */
  uint32_t call, info[4];
  int i, reg, nrets = 1;
  call = agnL_optnonnegint(L, 1, 0);  /* CPUID leaf or function number, 5.6.2 change */
  reg = agnL_optnonnegint(L, 2, 0);
  if (cpuid(info, 0) == 0)  /* OpenSUSE, OS/2, Mac OS X, 5.5.10 fix */
    luaL_error(L, "Error in " LUA_QS ": could not determine CPU features.", "os.cpuid");
  if (call > info[0])
    luaL_error(L, "Error in " LUA_QS ": invalid first argument %d, must be in 0 .. %d.", "os.cpuid", call, info[0]);
  cpuid(info, call);
  switch (reg) {
    case 0:
      luaL_checkstack(L, 4, "not enough stack space");
      for (i=0; i < 4; i++)
        lua_pushnumber(L, info[i]);
      nrets = 4;
      break;
    case 1: lua_pushnumber(L, info[0]); break;
    case 2: lua_pushnumber(L, info[1]); break;
    case 3: lua_pushnumber(L, info[2]); break;
    case 4: lua_pushnumber(L, info[3]); break;
    default:
      luaL_error(L, "Error in " LUA_QS ": invalid second argument %d.", "os.cpuid", reg);
  }
  return nrets;
}


static int aux_getmanufacturer (uint32_t ebx, uint32_t ecx, uint32_t edx) {
  uint32_t info[4];
  return (cpuid(info, 0)) ? info[1] == ebx && info[2] == ecx && info[3] == edx : -1;
}

/* Checks at runtime whether Agena is running on a specific CPU and returns `true` or `false` in the various fields:

   On some platforms the values might be `fail`. See also: `os.isx86`;

   lua_rawsetstringboolean() returns fail with -1. 5.6.2 */
static int os_vendor (lua_State *L) {
  lua_createtable(L, 0, 13);
  /* Advanced Micro Devices, Inc. (AMD) */
  lua_rawsetstringboolean(L, -1, "amd",
    aux_getmanufacturer(0x68747541, 0x69746e65, 0x444d4163));
  /* Intel Corporation */
  lua_rawsetstringboolean(L, -1, "intel",
    aux_getmanufacturer(0x756e6547, 0x6c65746e, 0x49656e69));
  /* Cyrix Corporation */
  lua_rawsetstringboolean(L, -1, "cyrix",
    aux_getmanufacturer(0x69727943, 0x64616574, 0x736e4978));
  /* NexGen, Inc. */
  lua_rawsetstringboolean(L, -1, "nexgen",
    aux_getmanufacturer(0x4778654e, 0x6e657669, 0x72446e65));
  /* National Semiconductor vendor, 5.5.10 */
  lua_rawsetstringboolean(L, -1, "nsc",
    aux_getmanufacturer(0x646f6547, 0x43534e20, 0x79622065));
  /* VIA Technologies vendor, 5.5.10 */
  lua_rawsetstringboolean(L, -1, "via",
    aux_getmanufacturer(0x20414956, 0x20414956, 0x20414956));
  /* Centaur Technology designed CPUs, 5.6.2 */
  lua_rawsetstringboolean(L, -1, "centaur",
    aux_getmanufacturer(0x746e6543, 0x736c7561, 0x48727561));
  /* check for Transmeta Corporation  "TransmetaCPU" CPUs, 5.6.2 */
  lua_rawsetstringboolean(L, -1, "transmeta",
    aux_getmanufacturer(0x6e617254, 0x55504361, 0x74656d73));
  /* "Genuine x86 M6 Tine", signature belongs to NexGen, GenuineTMx86 */
  lua_rawsetstringboolean(L, -1, "tmx86",
    aux_getmanufacturer(0x756e6547, 0x3638784d, 0x54656e69));
  /* Rise Technology (sometimes stylized as Rise! or R!SE), 5.6.2 */
  lua_rawsetstringboolean(L, -1, "rise",
    aux_getmanufacturer(0x65736952, 0x65736952, 0x65736952));
  /* Silicon Integrated Systems (SiS, Taiwan), 5.6.2 */
  lua_rawsetstringboolean(L, -1, "sis",
    aux_getmanufacturer(0x20536953, 0x20536953, 0x20536953));
  /* UMC Green CPU by United Microelectronics Corporation, 5.6.2 */
  lua_rawsetstringboolean(L, -1, "umc",
    aux_getmanufacturer(0x20434d55, 0x20434d55, 0x20434d55));
  /* Vortex86 CPU, originally by Rise Technology, now DM&P Electronics, Taiwan, 5.6.2 */
  lua_rawsetstringboolean(L, -1, "vortex",
    aux_getmanufacturer(0x74726f56, 0x436f5320, 0x36387865));
  return 1;
}


/* The function runs the XGETBV (Get Extended Control Register) instruction safely and returns an unsigned 32-bit integer
   that can be used to check whether certain CPU features have been enabled by the operating system, like the modern
   instruction sets like AVX, AVX2, and AVX-512. The function returns `undefined` when run on non-Intel/AMD CPUs.

   Specifically, the function reads the contents of the extended control register XCR0.

   The function prevents #UD (Invalid Opcode) exceptions on older CPUs or low-power Atoms where XSAVE might
   be disabled by the OS/BIOS.

   Following is a complete list of the currently defined bits in XCR0, as detailed in Intel's architecture manuals:

     Bit 0 (1): x87 FPU State. This is always required to be set if the OS supports the XSAVE feature set.
     Bit 1 (2): SSE State (XMM registers).
     Bit 2 (4): AVX State (YMM registers).
     Bit 3 (8): MPX (BNDREGS) State. This bit is for the Bound Registers used by the Memory Protection Extensions (MPX).
     Bit 4 (16): MPX (BNDCSR) State. This bit is for the BNDCFGS and BNDSTATUS registers, also part of MPX.
     Bit 5 (32): AVX-512 Opmask State (K registers).
     Bit 6 (64): AVX-512 ZMM_Hi256 State (the upper 256 bits of ZMM0-ZMM15).
     Bit 7 (128): AVX-512 ZMM_Hi16 State (ZMM16-ZMM31).
     Bit 8 (256): PT State (Processor Trace).
     Bit 9 (512): PKRU State (Protection Keys for User-Mode pages).

   See also: `os.cpuid`.

   See also StackOverflow "How to check if a CPU supports the SSE3 instruction set?", answer by atlaste.

   7.3.8 patch for CPUs such like Intel Atom x5 Z8350 CPU created by Gemini AI. */
static int os_xgetbv (lua_State *L) {
#if !(defined(__INTEL32) || defined(__x86_64__) || defined(__i386__))
  lua_pushundefined(L);
  return 1;
#endif
  uint32_t arg;
  uint32_t eax, ebx, ecx, edx;
#ifdef _WIN32
  struct WinVer winversion;
#endif
  /* Get the optional argument (default to 0 for XCR0) */
  arg = agnL_optnonnegint(L, 1, 0);
#ifdef _WIN32
  if (getWindowsVersion(&winversion) < MS_WIN7) {  /* prevent crashes */
    lua_pushnil(L);  /* 7.3.9 change */
    return 1;
  }
#endif
  /* 1. Hardware/OS Support Check via CPUID */
  /* Using __cpuid_count is safer in MinGW32 to ensure register preservation */
  eax = 1; /* Leaf 1 */
  ecx = 0; /* Subleaf 0 */
#if defined(__GNUC__) || defined(__clang__)
  /* PIC-safe CPUID for 32-bit: Manually save/restore EBX, 7.3.9 Mac OS X fix to avoid clashes with the EBX register.
     Ensure ebx is preserved and not immediately written over, to prevent crashes; 7.3.8 fix */
  #if defined(__x86_64__)  /* 7.3.9 fix */
    uint64_t rbx_tmp;
    __asm__ volatile (
      "xchgq %%rbx, %1\n\t"
      "cpuid\n\t"
      "xchgq %%rbx, %1\n\t"
      : "=a" (eax), "=r" (rbx_tmp), "=c" (ecx), "=d" (edx)
      : "a" (eax), "c" (ecx)
    );
    (void)ebx;  /* ebx = (uint32_t)rbx_tmp; we don't need ebx now */
  #else
    __asm__ volatile (
      "xchgl %%ebx, %1\n\t"
      "cpuid\n\t"
      "xchgl %%ebx, %1\n\t"
      : "=a" (eax), "=r" (ebx), "=c" (ecx), "=d" (edx)
      : "a" (eax), "c" (ecx)
    );
  #endif
#else
  if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
    lua_pushnil(L);  /* 7.3.9 change */
    return 1;
  }
#endif
  /* Bit 27 of ECX is OSXSAVE. If not set, XGETBV is an illegal instruction. 7.3.8 fix. Also Argument Validation 7.3.9 */
  const uint32_t osxsave_mask = (1 << 27);
  if ((arg != 0) || (ecx & osxsave_mask) == 0) {
    lua_pushnil(L);  /* 7.3.9 change */
    return 1;
  }
  /* 3. Execute XGETBV */
  uint32_t xcr0_low, xcr0_high;
  __asm__ volatile (
    /* This is the machine code for xgetbv, in case the compiler should not know about the instruction word "xgetbv" 7.3.8 */
    ".byte 0x0f, 0x01, 0xd0"
    : "=a" (xcr0_low), "=d" (xcr0_high)
    : "c" (arg)
  );
  /* Return the lower 32-bit register as a number to Lua */
  lua_pushnumber(L, (lua_Number)xcr0_low);
  return 1;
}


static int os_sdlcpuinfo (lua_State *L) {  /* 5.5.9, UNDOC */
  lua_createtable(L, 0, 3);
  lua_rawsetstringinteger(L, -1, "cpucachelinesize", SDL_GetCPUCacheLineSize());
  lua_rawsetstringstring(L, -1, "cpuname", SDL_GetCPUName());  /* e.g. `Intel(R) Core(TM) i7-9700K CPU @ 3.60GHz` */
  lua_rawsetstringstring(L, -1, "cputype", SDL_GetCPUType());  /* e.g. `GenuineIntel`, `AuthenticAMD` */
  return 1;
}

/* Taken from Bionic libm/i387/fenv.c, MIT licence
 *
 * Test for SSE support on this processor.  We need to do this because
 * we need to use ldmxcsr/stmxcsr to get correct results if any part
 * of the program was compiled to use SSE floating-point, but we can't
 * use SSE on older processors.
 */
#if ((LONG_MAX == 2147483647L) && !defined(__APPLE__)) && ((defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__) )

#define getfl(x)    __asm volatile(".code32\npushfl\n\tpopl %0" : "=mr" (*(x)))
#define setfl(x)    __asm volatile(".code32\npushl %0\n\tpopfl" : : "g" (x))
#define cpuid_dx(x) __asm volatile(".code32\npushl %%ebx\n\tmovl $1, %%eax\n\t"  \
                    "cpuid\n\tpopl %%ebx"          \
                    : "=d" (*(x)) : : "eax", "ecx")

static int issse (uint32_t *dx) {
  int flag, nflag, dx_features;
  /* am I a 486? */
  *dx = -1;
  getfl(&flag);
  nflag = flag ^ 0x200000;
  setfl(nflag);
  getfl(&nflag);
  if (flag != nflag) {
    /* not a 486, so CPUID should work. */
    cpuid_dx(&dx_features);
    *dx = (uint32_t)dx_features;
    if (dx_features & 0x2000000) return 1;
  }
  return 0;
}

static int os_hassse (lua_State *L) {  /* 2.14.13 */
  uint32_t dx;
  lua_pushboolean(L, issse(&dx));
  lua_pushnumber(L, dx);
  return 2;
}
#endif

/*
* lalarm.c
* an alarm library for Lua 5.1 based on signal
* Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br>
* 03 May 2012 00:26:33
* This code is hereby placed in the public domain.

  The library [function] exports a single function, alarm([s, [f]]), which tells
  Lua to call f after s seconds have elapsed. This is done only once. If you want
  f to be called every s seconds, call alarm(s) inside f. Call alarm() without
  arguments or with s=0 to cancel any pending alarm.
*/

#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__)
#define NAME  "alarm handler"

static lua_State *LL = NULL;
static lua_Hook oldhook = NULL;
static int oldmask=0;
static int oldcount=0;

static void l_handler (lua_State *L, lua_Debug *ar) {
  (void)ar;
  L = LL;
  lua_sethook(L, oldhook, oldmask, oldcount);
  lua_getfield(L, LUA_REGISTRYINDEX, NAME);
  lua_call(L, 0, 0);
}

static void l_signal (int i) {  /* assert(i==SIGALRM); */
  signal(i, SIG_DFL);
  oldhook = lua_gethook(LL);
  oldmask = lua_gethookmask(LL);
  oldcount = lua_gethookcount(LL);
  lua_sethook(LL, l_handler, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

static int os_alarm (lua_State *L) {  /* alarm([secs, [func]]) */
  LL = L;
  switch (lua_gettop(L)) {
    case 0:
      break;
    case 1:
      lua_getfield(L, LUA_REGISTRYINDEX, NAME);
      if (lua_isnil(L, -1)) luaL_error(L, "no alarm handler set");
      break;
    default:
      lua_settop(L, 2);
      luaL_checktype(L, 2, LUA_TFUNCTION);
      lua_setfield(L, LUA_REGISTRYINDEX, NAME);
      break;
  }
  if (signal(SIGALRM, l_signal) == SIG_ERR)
    lua_pushnil(L);
  else
    lua_pushinteger(L, alarm(luaL_optinteger(L, 1, 0)));
  return 1;
}
#endif


/* CreateLink and ResolveLink have been taken from "Shell Links" in MSDN library, reproduced in parts
   by the dynamorio project; taken from sources available at http://code.google.com/p/dynamorio and
   largely modified for Agena */

#ifdef _WIN32
/* this modified version of CreateLink can also cope with file names with no absolute or relative paths. */
HRESULT CreateLink (LPCSTR lpszPathObj, LPSTR lpszPathLink, LPSTR lpszDesc) {
  int hres;
  IShellLink *psl = NULL;
  IPersistFile *ppf = NULL;
  WCHAR *wsz;
  char *ObjWorkingDir, *tmp, *cwdbuffer, *fullsourcepath;
  LPSTR link;
  cwdbuffer = fullsourcepath = NULL;
  hres = CoInitialize(NULL);
  if (hres != S_FALSE && hres != S_OK) goto error3;  /* CoUninitialize, checked */
  /* Get a pointer to the IShellLink interface */
  /* GCC 15.2 does not have support for IID_IShellLink, so define a pointer for our chosen ID */
  /* Logic: On Windows 2000, IID_IShellLink is the ANSI version.
     On XP and later, IID_IShellLinkA is the ANSI version.
     Crucially, they share the same GUID value:
     {000214EE-0000-0000-C000-000000000046}; created by Gemini AI 7.1.7 */
  /* Use the standard UUID if available, or define it manually to be safe */
  static const IID My_IID_IShellLinkA = { 0x000214EE, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
  hres = CoInitialize(NULL);
  if (hres != S_FALSE && hres != S_OK) goto error3;
  /* We pass My_IID_IShellLinkA directly.
     Because we defined the bits manually, the linker doesn't need to find
     IID_IShellLinkA in the system's .a or .lib files at startup.
  */
  hres = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &My_IID_IShellLinkA, (LPVOID *)&psl);
  if (!SUCCEEDED(hres)) goto error3;  /* CoUninitialize, checked */
  /* Set the path to the shortcut target, and add the description. */
  /* psl->lpVtbl->SetPath(psl, (LPCSTR)str_charreplace((char *)lpszPathObj, '/', '\\', 0)); */
  psl->lpVtbl->SetDescription(psl, lpszDesc);
  ObjWorkingDir = tools_strdup(lpszPathObj);
  if (ObjWorkingDir != NULL) {
    str_charreplace(ObjWorkingDir, '/', '\\', 0);
    tmp = strrchr(ObjWorkingDir, '\\');
    if (tmp != NULL) {  /* source path given ? */
      tmp[0] = '\0';
      if (*ObjWorkingDir == '\0') goto error2;  /* leading slash ? */
      psl->lpVtbl->SetWorkingDirectory(psl, ObjWorkingDir);
      psl->lpVtbl->SetPath(psl, (LPCSTR)str_charreplace((char *)lpszPathObj, '/', '\\', 0));
    } else {  /* no source path given */
      cwdbuffer = tools_stralloc(PATH_MAX);  /* 2.16.5 change */
      if (cwdbuffer == NULL || tools_cwd(cwdbuffer) != 0) goto error2; /* frees cwdbuffer automatically; CoUninitialize & xfree(ObjWorkingDir) */
      psl->lpVtbl->SetWorkingDirectory(psl, cwdbuffer);
      fullsourcepath = str_concat(cwdbuffer, "\\", (char *)lpszPathObj, NULL);  /* 2.39.10 fix */
      psl->lpVtbl->SetPath(psl, (LPCSTR)str_charreplace((char *)fullsourcepath, '/', '\\', 0));
    }
  } else goto error3;  /* strdup failed, no need to free ObjWorkingDir; conduct CoUninitialize */
  /* process link target now */
  str_charreplace(lpszPathLink, '/', '\\', 0);
  if (strrchr(lpszPathLink, '\\') == NULL) {  /* no path given ? */
    if (cwdbuffer == NULL) {  /* cwd not yet determined ? */
      cwdbuffer = tools_stralloc(PATH_MAX);  /* 2.16.5 change */
      if (cwdbuffer == NULL || tools_cwd(cwdbuffer) != 0) goto error2;  /* frees cwdbuffer automatically; CoUninitialize & xfree(ObjWorkingDir) */
      str_charreplace(cwdbuffer, '/', '\\', 0);
    }
    /* compose full link path, cwd at this point has already been determined */
    link = (LPSTR)str_concat(cwdbuffer, "\\", lpszPathLink, NULL);
    if (link == NULL) goto error1;  /* CoUninitialize & xfree(ObjWorkingDir) & xfree(cwdbuffer) */
  } else {  /* a path has been given, at this point CoUnIni, ObjWorkingDir, and possibly cwdbuffer */
    link = (LPSTR)str_concat(lpszPathLink, NULL);
    if (link == NULL) {
      if (cwdbuffer == NULL)
        goto error2;  /* xfree CoUnIni, and ObjWorkingDir */
      else
        goto error1;  /* xfree CoUnIni, cwdbuffer, and ObjWorkingDir */
    }
  }
  /* Query IShellLink for the IPersistFile interface for saving the shortcut in persistent storage. */
  /* at this point, CoIni, OWD, cwdbuffer, and link have been malloc'ed */
  hres = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void **)&ppf);
  if (!SUCCEEDED(hres)) goto error0;
  /* Ensure that the string is ANSI, first determine the length of the resulting string wsz: */
  /* hres = MultiByteToWideChar(AreFileApisANSI() ? CP_ACP : CP_OEMCP, 0, lpszPathLink, -1, NULL, 0); */
  hres = MultiByteToWideChar(CP_UTF8, 0, link, -1, NULL, 0);
  if (!SUCCEEDED(hres)) goto error0;
  /* allocate memory for the buffer, plus 1 to be absolutely sure, malloc needs the number of characters,
     not the number of bytes */
  wsz = (WCHAR*)malloc(hres + 1);
  if (wsz == NULL) goto error0;
  /* hres = MultiByteToWideChar(AreFileApisANSI() ? CP_ACP : CP_OEMCP, 0, lpszPathLink, -1, wsz, hres); */
  hres = MultiByteToWideChar(CP_UTF8, 0, link, -1, wsz, hres);
  if (!SUCCEEDED(hres)) goto error;
  /* Save the link by calling IPersistFile::Save. */
  hres = ppf->lpVtbl->Save(ppf, wsz, TRUE);
  ppf->lpVtbl->Release(ppf);
  psl->lpVtbl->Release(psl);
  CoUninitialize();
  xfreeall(ObjWorkingDir, link, cwdbuffer, wsz);  /* 2.9.8 */
  return hres;
error:  /* fall through */
  xfree(wsz);
error0:
  xfree(link);
error1:
  xfree(cwdbuffer);
error2:
  xfree(ObjWorkingDir);
  if (fullsourcepath != NULL) xfree(fullsourcepath);
error3:
  CoUninitialize();
  return -1;
}

LUALIB_API int symlink (const char *src, const char *dest) {
  HRESULT res;
  char *src0, *dest0;  /* 2.39.10 fix, prevent modification of original strings */
  src0 = tools_strdup(src);
  if (!src0) return -1;
  dest0 = tools_strdup(dest);
  if (!dest0) {
    xfree(src0);
    return -1;
  }
  res = CreateLink((LPCSTR)src, (LPSTR)dest, (LPSTR)src);
  xfreeall(src0, dest0);
  return (int)res;
}

#undef PATHLEN
#define PATHLEN 4*(PATH_MAX-1)
/* ONLY call this function if the file lpszLinkFile really EXIST, otherwise the function would crash ! */
HRESULT ResolveLink (LPSTR lpszLinkFile, LPSTR lpszPath) {
  HRESULT hres;
  IShellLink* psl;
  IPersistFile* ppf;
  WCHAR *wsz;
  char szGotPath[PATHLEN];
  *lpszPath = 0;  /* assume failure */
  hres = CoInitialize(NULL);
  if (hres != S_FALSE && hres != S_OK) {
  CoUninitialize();
    return -1;
  }
  /* GCC 15.2 does not have support for IID_IShellLink any longer, so get a pointer to the IShellLink interface. */
  /* Logic: On Windows 2000, IID_IShellLink is the ANSI version.
     On XP and later, IID_IShellLinkA is the ANSI version.
     Crucially, they share the same GUID value:
     {000214EE-0000-0000-C000-000000000046}; created by Gemini AI 7.1.7.
     Use the standard UUID if available, or define it manually to be safe */
  static const IID My_IID_IShellLinkA = { 0x000214EE, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
  hres = CoInitialize(NULL);
  if (hres != S_FALSE && hres != S_OK) goto error2;
  /* We pass My_IID_IShellLinkA directly.
     Because we defined the bits manually, the linker doesn't need to find
     IID_IShellLinkA in the system's .a or .lib files at startup.
     Use our manual GUID. We cast to (REFIID) to satisfy the function signature. */
  hres = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, (REFIID)&My_IID_IShellLinkA, (LPVOID *)&psl);
  if (!SUCCEEDED(hres)) goto error2; /* Ensure we call CoUninitialize on failure */
  hres = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void **)&ppf);
  /* Ensure that the string is Unicode. */
  hres = MultiByteToWideChar(CP_UTF8, 0, lpszLinkFile, -1, NULL, 0);
  if (!SUCCEEDED (hres)) goto error2;
  wsz = (WCHAR*)malloc(hres + 1);
  if (wsz == NULL) goto error2;
  hres = MultiByteToWideChar(CP_UTF8, 0, lpszLinkFile, -1, wsz, hres);
  if (!SUCCEEDED (hres)) goto error1;
  /* Load the shortcut. */
  hres = ppf->lpVtbl->Load(ppf, wsz, 0); /* STGM_READ); */
  if (!SUCCEEDED(hres)) goto error1;
  /* Resolve the link. */
  hres = psl->lpVtbl->Resolve(psl, NULL, 0); /* SLR_ANY_MATCH); */
  if (!SUCCEEDED(hres)) goto error1;
  /* Get the path to the link target. */
  hres = psl->lpVtbl->GetPath(psl, szGotPath, PATHLEN, NULL, SLGP_SHORTPATH);
  if (!SUCCEEDED(hres)) goto error1;
  lstrcpy(lpszPath, szGotPath);
  /* Release the pointer to the IPersistFile interface. */
  ppf->lpVtbl->Release(ppf);
  /* Release the pointer to the IShellLink interface. */
  psl->lpVtbl->Release(psl);
  CoUninitialize();
  xfree(wsz);
  return hres;
error1:
  xfree(wsz);
  /* fall through */
error2:
  CoUninitialize();
  return -1;
}

static int os_readlink (lua_State *L) {
  HRESULT res;
  char *buffer;
  const char *linkname;
  linkname = agn_checkstring(L, 1);
  if (!tools_exists(linkname)) {  /* 2.16.11 change */
    lua_pushfail(L);
    return 1;
  }
  buffer = malloc(PATHLEN*CHARSIZE);
  if (buffer == NULL) {  /* 4.11.5 fix */
    lua_pushfail(L);
    return 1;
  }
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
  res = ResolveLink((char *)linkname, (LPSTR)buffer);
  SetErrorMode(0);
  if (res == -1)
    lua_pushfail(L);
  else
    lua_pushstring(L, buffer);
  xfree(buffer);
  return 1;
}
#endif


#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(_WIN32)
static int os_symlink (lua_State *L) {
  int en, r;
  const char *oldname, *newname;
  oldname = agn_checkstring(L, 1);
  newname = agn_checkstring(L, 2);
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  r = symlink(oldname, newname);
  if (r == -1) {
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.symlink");  /* 7.7.7 fix */
  }
  en = errno;
  if (r == -1) {
    luaL_error(L, "Error in " LUA_QS ": %s.", "os.symlink", my_ioerror(en));  /* changed 2.10.4 */
  }
  lua_pushtrue(L);
  return 1;
}
#endif

#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__)
static int os_readlink (lua_State *L) {
  int bufsize, nchars, en;
  const char *filename;
  char *buffer;
  bufsize = 1128;
  buffer = NULL;
  filename = agn_checkstring(L, 1);
  if (!tools_exists(filename)) {  /* 2.16.11 change */
    lua_pushfail(L);
    return 1;
  }
  while (1) {
    char *temp = (char *)realloc(buffer, bufsize);
    if (temp == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.readlink");
    buffer = temp;
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    nchars = readlink(filename, buffer, bufsize);  /* with _no_ \0 included ! */
    en = errno;
    if (nchars < 0) {
      xfree(buffer);
      lua_pushfail(L);
      if (en == EINVAL)
        lua_pushstring(L, "file is not a symbolic link");
      else
        lua_pushstring(L, my_ioerror(en));  /* changed 2.10.4 */
      return 2;
    }
    if (nchars < bufsize) {
      lua_pushlstring(L, buffer, nchars);
      xfree(buffer);
      return 1;
    }
    bufsize *= 2;
  }
}
#endif


#ifdef _WIN32  /* GCC 4.5.2 does not feature GetTickCount64 */
   #ifndef GetTickCount64
      #define GetTickCount64  GetTickCount
   #endif
#endif


#ifdef __APPLE__
CFTimeInterval getSystemUptime (void) {
  enum { NANOSECONDS_IN_SEC = 1000 * 1000 * 1000 };
  static double multiply = 0;
  if (multiply == 0) {
      mach_timebase_info_data_t s_timebase_info;
      kern_return_t result = mach_timebase_info(&s_timebase_info);
      assert(result == noErr);
      /* multiply to get value in the nano seconds */
      multiply = (double)s_timebase_info.numer / (double)s_timebase_info.denom;
      /* multiply to get value in the seconds */
      multiply /= NANOSECONDS_IN_SEC;
  }
  return mach_absolute_time() * multiply;
}
#endif


static int os_uptime (lua_State *L) {
#if defined(__OS2__)
  /* see: http://www.edm2.com/os2api/Dos/DosQuerySysInfo.html */
  ULONG sysInfo[1];
  if (DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &sysInfo[0], sizeof(sysInfo)) != 0)
    lua_pushfail(L);
  else
    lua_pushnumber(L, sysInfo[0]/1000);  /* Value of a 32-bit, free-running counter (milliseconds/1k). Zero at boot time. */
#elif defined(_WIN32)
  /* see: http://msdn.microsoft.com/en-us/library/windows/desktop/ms724408%28v=vs.85%29.aspx */
  int winver, highres;
  highres = lua_gettop(L) != 0;
  if (highres) {
    /* retrieves the current value of the performance counter, a high resolution (<1us) time stamp that can be used for
       time-interval measurements, 2.39.1 */
    LARGE_INTEGER ticks;
    int rc = QueryPerformanceCounter(&ticks);
    lua_pushnumber(L, rc == 0 ? AGN_NAN : ticks.QuadPart);  /* ticks.QuadPart is of type LONGLONG */
  } else {
    winver = tools_getwinver();  /* 2.17.4 change */
    if (winver < MS_WIN2K)  /* Win NT 4 or earlier ? */
      lua_pushfail(L);
    else if (winver < MS_WIN2003)  /* XP or earlier ? */
      lua_pushnumber(L, GetTickCount()/1000);  /* overflow if system has been up for more than 49.7 days. */
    else
      lua_pushnumber(L, GetTickCount64()/1000);
  }
#elif defined(__linux__)
  /* see: http://stackoverflow.com/questions/1540627/what-api-do-i-call-to-get-the-system-uptime */
  struct sysinfo sinfo;
  if (sysinfo(&sinfo) != 0)
    lua_pushfail(L);
  else
    lua_pushnumber(L, sinfo.uptime);
#elif defined(__SOLARIS)
  /* 2.3.4, see StackOverflow article `Getting the uptime of a SunOS UNIX box in seconds only` */
  unsigned long int nBootTime, nCurrentTime;
  struct utmpx *ent;
  nBootTime = 0;
  nCurrentTime = time(NULL);
  while ( (ent = getutxent()) ) {
    if (tools_streq("system boot", ent->ut_line)) {
      nBootTime = ent->ut_tv.tv_sec; break;
    }
  }
  endutent();  /* do not remove this line ! */
  lua_pushnumber(L, nCurrentTime - nBootTime);
#elif defined(__APPLE__)
  /* see article OSX  programmatically get uptime at StackOverflow, advised by Speakus */
  lua_pushnumber(L, getSystemUptime());
#elif defined(__DJGPP__)  /* 5.6.2 */
  lua_pushnumber(L, tools_uptime());
#else
  lua_pushfail(L);
#endif
  return 1;
}


#ifdef _WIN32
/* The following code has been taken from: http://support.microsoft.com/kb/165721# */

LPTSTR szVolumeFormat = TEXT("\\\\.\\%c:");
LPTSTR szRootFormat = TEXT("%c:\\");
LPTSTR szErrorFormat = TEXT("Error %d: %s\n");

HANDLE OpenVolume (TCHAR cDriveLetter, int fixedvolume) {
  HANDLE hVolume;
  UINT uDriveType;
  TCHAR szVolumeName[8];
  TCHAR szRootName[5];
  DWORD dwAccessFlags;
  wsprintf(szRootName, szRootFormat, cDriveLetter);
  uDriveType = GetDriveType(szRootName);
  switch(uDriveType) {
    case DRIVE_REMOVABLE:
      dwAccessFlags = GENERIC_READ | GENERIC_WRITE;
      break;
    case DRIVE_CDROM:
      dwAccessFlags = GENERIC_READ;
      break;
    case DRIVE_FIXED:  /* 2.11.5 */
      if (fixedvolume) {
        dwAccessFlags = 0;  /* `trim' flag of os.drivestat does accept 0 only */
        break;
      }  /* fall through */
    default:
      return INVALID_HANDLE_VALUE;
  }
  wsprintf(szVolumeName, szVolumeFormat, cDriveLetter);
  hVolume = CreateFile(szVolumeName,
                       dwAccessFlags,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL,
                       OPEN_EXISTING,
                       0,
                       NULL );
  return hVolume;
}

BOOL CloseVolume (HANDLE hVolume) {
  return CloseHandle(hVolume);
}

#define LOCK_TIMEOUT        10000       /* 10 Seconds */
#define LOCK_RETRIES        20

BOOL LockVolume (HANDLE hVolume) {
  DWORD dwBytesReturned;
  DWORD dwSleepAmount;
  int nTryCount;
  dwSleepAmount = LOCK_TIMEOUT / LOCK_RETRIES;
  /* Do this in a loop until a timeout period has expired */
  for (nTryCount = 0; nTryCount < LOCK_RETRIES; nTryCount++) {
    if (DeviceIoControl(hVolume,
                        FSCTL_LOCK_VOLUME,
                        NULL, 0,
                        NULL, 0,
                        &dwBytesReturned,
                        NULL))
      return TRUE;
    Sleep(dwSleepAmount);
  }
  return FALSE;
}


/* 2.3.2; If the volume is currently mounted, 2.11.3 returns a nonzero value, Otherwise,
   DeviceIoControl returns zero (0). To get extended error information, call GetLastError. */

#ifndef FSCTL_IS_VOLUME_MOUNTED
#define FSCTL_IS_VOLUME_MOUNTED  CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 10, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

BOOL HasVolumeBeenMounted (HANDLE hVolume) {
  DWORD dwBytesReturned;
  return DeviceIoControl(hVolume,
                         FSCTL_IS_VOLUME_MOUNTED,
                         NULL, 0,
                         NULL, 0,
                         &dwBytesReturned,
                         NULL);
}

BOOL DismountVolume (HANDLE hVolume) {
  DWORD dwBytesReturned;
  return DeviceIoControl(hVolume,
                         FSCTL_DISMOUNT_VOLUME,
                         NULL, 0,
                         NULL, 0,
                         &dwBytesReturned,
                         NULL);
}

BOOL PreventRemovalOfVolume (HANDLE hVolume, BOOL fPreventRemoval) {
  DWORD dwBytesReturned;
  PREVENT_MEDIA_REMOVAL PMRBuffer;
  PMRBuffer.PreventMediaRemoval = fPreventRemoval;
  return DeviceIoControl(hVolume,
                         IOCTL_STORAGE_MEDIA_REMOVAL,
                         &PMRBuffer, sizeof(PREVENT_MEDIA_REMOVAL),
                         NULL, 0,
                         &dwBytesReturned,
                         NULL);
}

BOOL AutoEjectVolume (HANDLE hVolume) {
  DWORD dwBytesReturned;
  return DeviceIoControl(hVolume,
                         IOCTL_STORAGE_EJECT_MEDIA,  /* IOCTL_STORAGE_LOAD_MEDIA */
                         NULL, 0,
                         NULL, 0,
                         &dwBytesReturned,
                         NULL);
}

BOOL IsValidDrive (TCHAR cDriveLetter) {  /* 2.3.3 */
  UINT uDriveType;
  TCHAR szRootName[5];
  wsprintf(szRootName, szRootFormat, cDriveLetter);
  uDriveType = GetDriveType(szRootName);
  return uDriveType >= DRIVE_REMOVABLE && uDriveType <= DRIVE_RAMDISK;
}

/* 2.3.2; If the operation completes successfully, DeviceIoControl returns a nonzero value. If the operation
   fails or is pending, DeviceIoControl returns zero. To get extended error information, call GetLastError. */
BOOL LoadMedia (HANDLE hVolume) {
  DWORD dwBytesReturned;
  return DeviceIoControl(hVolume,
                         IOCTL_STORAGE_LOAD_MEDIA,
                         NULL, 0,
                         NULL, 0,
                         &dwBytesReturned,
                         NULL);
}

int EjectVolume (TCHAR cDriveLetter) {  /* extended for Agena 2.3.3 */
  HANDLE hVolume;
  int result = 4;  /* 0b100 */
  BOOL fRemoveSafely = FALSE;
  BOOL fAutoEject = FALSE;
  /* Open the volume. */
  hVolume = OpenVolume(cDriveLetter, 0);
  if (hVolume == INVALID_HANDLE_VALUE)
    tools_setbit(&result, 3, 0);
  else {
    /* Lock and dismount the volume. */
    if (LockVolume(hVolume) && DismountVolume(hVolume)) {
      fRemoveSafely = TRUE;
      /* Set prevent removal to false and eject the volume. */
      if (PreventRemovalOfVolume(hVolume, FALSE) && AutoEjectVolume(hVolume))
        fAutoEject = TRUE;
      tools_setbit(&result, 1, fRemoveSafely && fAutoEject);
    }
    /* Close the volume so other processes can use the drive. */
    tools_setbit(&result, 2, CloseVolume(hVolume));
    /* fAutoEject == 1 ? -> Media in Drive has been ejected safely.
    fRemoveSafely == 1 ? -> Media in Drive can be safely removed. */
  }
  return result;
}

/* closes the CD-ROM drive tray, written for Agena 2.3.3 */
BOOL LoadVolume (TCHAR cDriveLetter) {
  HANDLE hVolume;
  int result = 4;  /* 0b100 */
  /* Open the volume. */
  hVolume = OpenVolume(cDriveLetter, 0);
  if (hVolume == INVALID_HANDLE_VALUE) {
    tools_setbit(&result, 3, 0);
  } else {
    /* close the drive tray. */
    tools_setbit(&result, 1, LoadMedia(hVolume));
    /* Close the volume so other processes can use the drive. */
    tools_setbit(&result, 2, CloseVolume(hVolume));
  }
  return result;
}

/* checks whether a volume has been mounted, written for Agena 2.3.3 */
int IsVolumeMounted (TCHAR cDriveLetter) {
  HANDLE hVolume;
  int result = 4;  /* 0b100 */
  /* Open the volume. */
  hVolume = OpenVolume(cDriveLetter, 1);
  if (hVolume == INVALID_HANDLE_VALUE) {
    tools_setbit(&result, 3, 0);  /* invalid drive */
  } else {
    /* Check status */
    tools_setbit(&result, 1, HasVolumeBeenMounted(hVolume));
    /* Close the volume so other processes can use the drive. */
    tools_setbit(&result, 2, CloseVolume(hVolume));
  }
  return result;
}

BOOL IsRemovable (TCHAR cDriveLetter) {  /* 2.3.3, determines whether a drive is removable */
  TCHAR szRootName[5];
  wsprintf(szRootName, szRootFormat, cDriveLetter);
  return GetDriveType(szRootName) == DRIVE_REMOVABLE;
}

#ifndef StorageDeviceTrimProperty
#define StorageDeviceTrimProperty 8
#endif

/* taken from https://stackoverflow.com/questions/23363115/detecting-ssd-in-windows */
#ifdef _DIRENT_HAVE_D_TYPE  /* 2.25.0 adaption to be ignored by MinGW/GCC 10.2*/
#include <ddk/ntddstor.h>
typedef struct _DEVICE_TRIM_DESCRIPTOR {
  DWORD   Version;
  DWORD   Size;
  BOOLEAN TrimEnabled;
} DEVICE_TRIM_DESCRIPTOR, *PDEVICE_TRIM_DESCRIPTOR;
#endif


int GetTrimFlag (HANDLE hDevice) {
  DWORD bytesReturned;
  STORAGE_PROPERTY_QUERY spqTrim = {0};  /* 7.5.15 security fix */
  spqTrim.PropertyId = (STORAGE_PROPERTY_ID)StorageDeviceTrimProperty;
  spqTrim.QueryType = PropertyStandardQuery;
  bytesReturned = 0;
  DEVICE_TRIM_DESCRIPTOR dtd = {0};
  if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
        &spqTrim, sizeof(spqTrim), &dtd, sizeof(dtd), &bytesReturned, NULL) &&
        bytesReturned == sizeof(dtd)) {
    return dtd.TrimEnabled;
  } else
    return -1;
}
#endif


#if defined(_WIN32) || defined(__linux__)
#define EJECT      0
#define CLOSETRAY  1
#endif

#ifdef __linux__

/* See Jürgen Wolf, `Linux-UNIX-Programmierung, Das umfassende Handbuch`, Galileo Computing, 3rd Ed., p. 164 */
#define CDROM "/dev/cdrom"

static int open_cdrom (void) {
  int fd = open(CDROM, O_RDONLY | O_NONBLOCK);
  if (fd == -1 && fd != ENOMEDIUM) return -1;
  else return fd;
}

static int open_tray (int cdrom) {
  if (ioctl(cdrom, CDROMEJECT) == -1) return -1;
  else return 0;
}

static int close_tray (int cdrom) {
  if (ioctl(cdrom, CDROMCLOSETRAY) == -1) return -1;
  else return 0;
}

static int stop_cdrom (int cdrom) {
  if (ioctl(cdrom, CDROMSTOP) == -1) return -1;
  else return 0;
}

#elif defined(__OS2__)
/* modified OS/2 code, for the original one see:
   http://hobbes.nmsu.edu/download/pub/os2/dev/c/cdclose.zip */
#define INCL_DOSMICS
#define IOCTL_CDROMDISK  0x80
#define EJECT            0x44
#define CLOSETRAY        0x45  /* Bug in toolkit */

APIRET CDTray (HFILE hCDROM, ULONG akce) {  /* 2.4.0 */
  PCSZ Signature = (PCSZ)"CD01";  /* compared to libdvdcss 1.2.10, this default is correct */
  ULONG PLength, DLength;
  struct {
    UCHAR First;
    UCHAR Last;
    ULONG Leadout;
  } DAudioDisk;
  PLength = 4;
  DLength = sizeof(DAudioDisk);
  tools_bzero(&DAudioDisk, DLength);
  return DosDevIOCtl(hCDROM, IOCTL_CDROMDISK, akce, &Signature,
                     PLength, &PLength, &DAudioDisk, DLength, &DLength);
}
#endif

static int os_cdrom (lua_State *L) {
#if defined(_WIN32) || defined(__linux__) || defined(__OS2__)
  int result;
  unsigned int mode;
  size_t l;
  const char *devname, *option;
  l = result = 0;  /* to prevent compiler warnings */
#if defined(__OS2__) || defined(_WIN32)
  devname = agn_checklstring(L, 1, &l);
#else
  devname = NULL;
#endif
#if defined(__OS2__) || defined(_WIN32)
  option = agn_checkstring(L, 2);
#else
  option = agn_checkstring(L, 1);
#endif
  mode = -1;
  if ((tools_streq(option, "eject")) || (tools_streq(option, "open")))  /* 2.25.1 tweak */
    mode = EJECT;
  else if (tools_streq(option, "close"))  /* 2.25.1 tweak */
    mode = CLOSETRAY;
  else
    luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", "os.cdrom", option);
#if defined(__OS2__) || defined(_WIN32)
  if (l != 1)
    luaL_error(L, "Error in " LUA_QS ": drive name must be a single letter.", "os.cdrom");
#endif
#ifdef __OS2__  /* 2.4.0 */
  HFILE h;
  ULONG Action = 0;
  char *drive = str_concat(devname, ":", NULL);
  if (!drive) {
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.cdrom");
  }
  result = DosOpen((PCSZ)drive, &h, &Action, 0, 0,
    OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
    OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_DASD | OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READONLY,
    NULL);
  if (result != 0) {
    xfree(drive);
    luaL_error(L, "Error in " LUA_QS ": could not access CD-ROM drive.", "os.cdrom");
  }
  xfree(drive);
  result = CDTray(h, mode);  /* to prevent compiler warnings */
  if (result != 0) lua_pushfail(L);
  else lua_pushtrue(L);
  DosClose(h);
#elif defined(_WIN32)
  if (mode == EJECT) {
    result = EjectVolume(*devname);
    if (!tools_getbit(result, 3)) lua_pushfail(L);  /* invalid drive */
    else lua_pushboolean(L, result == 7);
  } else {  /* CLOSETRAY */
    result = LoadVolume(*devname);
    if (!tools_getbit(result, 3))
      lua_pushfail(L);
    else
      lua_pushboolean(L, result == 7);
  }
#elif defined(__linux__)
  int fd = open_cdrom();
  (void)devname;  /* to prevent compiler warnings */
  (void)l;
  if (fd == -1)
    luaL_error(L, "Error in " LUA_QS ": could not access CD-ROM drive.", "os.cdrom");
  if (stop_cdrom(fd) == -1)
    luaL_error(L, "Error in " LUA_QS ": could not stop CD-ROM drive.", "os.cdrom");
  if (mode == EJECT) {
    if (open_tray(fd) == -1) lua_pushfail(L);  /* 2.12.0 RC 3 fix */
    else lua_pushtrue(L);
  } else {  /* CLOSETRAY */
    if (close_tray(fd) == -1) lua_pushfail(L);  /* 2.12.0 RC 3 fix */
    else lua_pushtrue(L);
  }
  stop_cdrom(fd);
  if (close(fd) != 0) {
    agn_poptop(L);  /* pop result */
    luaL_error(L, "Error in " LUA_QS ": could not close CD-ROM drive handle.", "os.cdrom");
  }
#endif
#else
  lua_pushfail(L);
#endif
  return 1;
}


static int os_ismounted (lua_State *L) {  /* 2.3.3 */
#ifdef _WIN32
  int result;
  size_t l;
  const char *devname;
  l = 0;  /* to prevent compiler warnings */
  devname = agn_checklstring(L, 1, &l);
  aux_checkvaliddrive(L, devname, l, "os.ismounted");  /* 2.39.11 */
  result = IsVolumeMounted(*devname);
  if (!tools_getbit(result, 3))  /* invalid drive */
    lua_pushfail(L);  /* invalid drive */
  else
    lua_pushboolean(L, result == 7);
#else
  lua_pushfail(L);
#endif
  return 1;
}


static int os_isremovable (lua_State *L) {  /* 2.3.3 */
  size_t l = 0;
  const char *devname = agn_checklstring(L, 1, &l);
  aux_checkvaliddrive(L, devname, l, "os.isremovable");  /* 2.39.11 */
#ifdef _WIN32
  lua_pushboolean(L, IsRemovable(*devname));
#elif defined(__DJGPP__)  /* 2.39.11 */
  unsigned int drno = toupper(devname[0]) - 'A' + 1;
  lua_pushboolean(L, _media_type(drno) == 0);
#else
  (void)devname;
  lua_pushfail(L);
#endif
  return 1;
}


/* Returns the letter, a one character-string, of the current drive. 2.3.3, rewritten 2.17.2 */
static int os_curdrive (lua_State *L) {
#if defined(_WIN32) || defined(__DJGPP__) || defined(__OS2__)
  int curdrive;
  char drive[1];
#ifdef _WIN32
  curdrive = _getdrive();   /* 1 = A, 2 = B, ... */
#elif defined(__DJGPP__)
  unsigned int ulDrive;
  _dos_getdrive(&ulDrive);  /* 1 = A, 2 = B, ... */
  curdrive = (int)ulDrive;
#else
  ULONG ulDrive;    /* Pointer to the current drive number. (1=A, 2=B etc.) */
  ULONG ulLogical;  /* Pointer to a 32-bit bit area where each of the 26 lowest bits represents a drive (0=A, 1=B etc.)
                       If bit n is set(=1) then the drive corresponding to n exists. */
  APIRET rc;
  rc = DosQueryCurrentDisk(&ulDrive, &ulLogical);  /* 1 = A, 2 = B, ... */
  if (rc != 0) {
    lua_pushfail(L);
    return 1;
  }
  curdrive = (int)ulDrive;
#endif
  drive[0] = 'A' - 1 + curdrive;
  lua_pushlstring(L, drive, 1);
  lua_pushlstring(L, ":", 1);
  lua_concat(L, 2);
#else
  lua_pushfail(L);
#endif
  return 1;
}


/* The function unmounts the filesystem `fs`. If the option `true` is given for the second argument `force`,
   the function forces the disconnection even if the filesystem is in use by another process. The default is
   `false`. If your system cannot force a umount, this flag is simply ignored.

   The function works only if Agena is run with superuser rights. Depending on the operating system, it may
   only unmount filesystems that the UNIX kernel directly supports (in Linux, look into /proc/filesystems
   folder), e.g. ntfs-3g filesystems using the FUSE driver may not be unmounted.

   On success, `os.unmount` returns `true`, and `false` plus a string indicating the error reason, otherwise. */
static int os_unmount (lua_State *L) {  /* 2.4.0 */
  int r = 1;  /* one result */
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__)
  const char *mountpoint;
  int force, rc, en;
  mountpoint = agn_checkstring(L, 1);
  force = agnL_optboolean(L, 2, 0);
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
#if defined(__APPLE__)
  rc = unmount(mountpoint, force);
#else
  rc = umount2(mountpoint, force);
#endif
  en = errno;
  if (rc == 0)
    lua_pushtrue(L);
  else {
    lua_pushfalse(L);
    lua_pushstring(L, my_ioerror(en));  /* changed 2.10.4 */
    r++;
  }
#else
  lua_pushfail(L);
#endif
  return r;
}


/* 2.8.6, taken from lua_sys package written by Nodir Temirkhodjaev, <nodir.temir@gmail.com>, modified by Alexander Walz
   Converts each pathname argument to an absolute pathname, with symbolic links resolved to their actual targets and no .
   or .. directory entries. */
static int os_realpath (lua_State *L) {
  size_t l;
  const char *path = luaL_checklstring(L, 1, &l);
  int replace = agnL_optboolean(L, 2, 1);
#if defined(_WIN32) || defined(__OS2__) || defined(__DJGPP__)
  if ( (l == 2 && isdriveletter(path)) ||  /* 2.39.9 extension */
       (l == 3 && isdriveletter(path) && (path[2] == '/' || path[2] == '\\')) ) {  /* 2.39.9; drivenames are expanded to curdir, but we don't want this, so: */
    lua_pushlstring(L, path, 2);
    if (l == 2)
      lua_pushchar(L, (replace) ? '/' : '\\');
    else
      lua_pushchar(L, (replace && path[2] == '\\') ? '/' : path[2]);
    lua_concat(L, 2);
    return 1;
  }
#endif
#ifdef _WIN32
  WCHAR real[PATH_MAX];
  const int n = GetFullPathName(path, PATH_MAX, (char *)real, NULL);
  /* 2.10.0 patch to check whether returned string really points to a valid filesystem object */
  if (n != 0 && n < PATH_MAX && tools_exists((const char *)real)) {  /* 2.16.11 change */
    if (replace) str_charreplace((char *)real, '\\', '/', 1);  /* 2.9.8 */
    lua_pushlstring(L, (const char*)real, n);
  } else  /* 2.39.9 fix */
    lua_pushfail(L);
#elif defined(__DJGPP__)
  char *real = _truename(path, NULL);  /* 2.17.2 */
  if (!real)
    lua_pushfail(L);
  else {
    if (replace) str_charreplace(real, '\\', '/', 1);
    lua_pushstring(L, real);
    xfree(real);
  }
#else
  char real[PATH_MAX];
  if (realpath(path, real)) {
    if (replace) str_charreplace(real, '\\', '/', 1);  /* 2.9.8 */
    lua_pushstring(L, real);
  } else
    lua_pushfail(L);
#endif
  return 1;
}


/* Searches for a specified file fn in a specified path. The return is the absolute path of the file plus its
   relative path. fn can be an absolute or relative path. The function does not search subdirectories in the path.
   If the file could not be found, the return is `null`. 2.27.1 */
static int os_searchpath (lua_State *L) {
  int nrets = 1;
#ifndef _WIN32
  lua_pushnil(L);
#else
  if (tools_getwinver() <= MS_WINXP) {
    lua_pushnil(L);
  } else {
    char buf[MAX_PATH], *bufptr;
    const char *path = luaL_checkstring(L, 1);
    const char *fn = luaL_checkstring(L, 2);
    int replace = agnL_optboolean(L, 3, 1);
    DWORD n = SearchPathA(path, fn, NULL, MAX_PATH, buf, &bufptr);
    if (n != 0 && n < PATH_MAX && tools_exists((const char *)buf)) {
      if (replace) str_charreplace((char *)buf, '\\', '/', 1);
      lua_pushstring(L, (const char*)buf);
      lua_pushstring(L, (const char*)bufptr);
      nrets++;
    } else
      lua_pushnil(L);
  }
#endif
  return nrets;
}


#ifdef _WIN32
/* NTLua Licence, see http://ntlua.sourceforge.net/

This product is free software: it can be used for both academic and commercial purposes at absolutely no cost. There are no royalties
or GNU-like "copyleft" restrictions. It is licensed under the terms of the MIT license reproduced below, and so is compatible with GPL
and also qualifies as Open Source software. It is not in the public domain, Tecgraf and Petrobras keep its copyright. The legal details
are below.

The spirit of this license is that you are free to use the library for any purpose at no cost without having to ask us. The only
requirement is that if you do use it, then you should give us credit by including the copyright notice below somewhere in your product
or its documentation. A nice, but optional, way to give us further credit is to include a Tecgraf logo in a web page for your product.

The code is designed and implemented by Antonio Escaño Scuri at Tecgraf/PUC-Rio in Brazil. The implementation is not derived from
licensed software.

Copyright © 1999-2010 Antonio Escaño Scuri.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

static char *utlNetLastErrorMessage (void) {
  DWORD dwWNetResult, dwLastError;
  CHAR szError[4096];  /* 2.34.11 MinGW GCC 9.2.0 migration patch */
  CHAR szDescription[256];
  CHAR szProvider[256];
  DWORD dwErrorCode = GetLastError();
  if (dwErrorCode != ERROR_EXTENDED_ERROR) {
    /* The following code performs standard error-handling. */
    char *MsgBuf, *error;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
                  dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* default language */
                  (LPTSTR) &MsgBuf, 0, NULL );
    {
      int size = strlen(MsgBuf);
      while (MsgBuf[size - 1] == '\n' || MsgBuf[size-1] == '\r')
        size--;
      MsgBuf[size] = 0;
    }
    error = tools_strdup(MsgBuf);
    LocalFree(MsgBuf);
    return error;
  } else {
    /* The following code performs error-handling when the ERROR_EXTENDED_ERROR return value indicates that WNetGetLastError
       can retrieve additional information. */
    dwWNetResult = WNetGetLastError(&dwLastError, (LPSTR)szDescription, sizeof(szDescription), (LPSTR)szProvider, sizeof(szProvider));
    if (dwWNetResult != NO_ERROR) return NULL;
    sprintf((LPSTR)szError, "%s failed with code %ld;\n%s", (LPSTR)szProvider, dwLastError, (LPSTR)szDescription);
    return tools_strdup(szError);
  }
}

static void issueosneterrorforchar (lua_State *L, char *obj, const char *procname) {
  if (!obj) {
    char *errmsg = utlNetLastErrorMessage();
    if (errmsg) errmsg[0] = tolower(errmsg[0]);
    lua_pushstring(L, errmsg ? errmsg : "unknown fault");
    xfree(errmsg);
    luaL_error(L, "Error in " LUA_QS ": %s", procname, agn_tostring(L, -1));
  }
}

static void issueosneterrorforrc (lua_State *L, int rc, const char *procname) {
  if (!rc) {
    char *errmsg = utlNetLastErrorMessage();
    if (errmsg) errmsg[0] = tolower(errmsg[0]);
    lua_pushstring(L, errmsg ? errmsg : "unknown fault");
    xfree(errmsg);
    luaL_error(L, "Error in " LUA_QS ": %s", procname, agn_tostring(L, -1));
  }
}

static int utlNetDel (char* drive) {
  return (WNetCancelConnection2(drive, CONNECT_UPDATE_PROFILE, TRUE) == NO_ERROR);
}

static int utlNetUse (char* drive, char* path) {
  NETRESOURCE NetResource;
  NetResource.dwType = RESOURCETYPE_DISK;
  NetResource.lpLocalName = drive;
  NetResource.lpRemoteName = path;
  NetResource.lpProvider = NULL;
  return (WNetAddConnection2(&NetResource, NULL, NULL, 0) == NO_ERROR);
}

/* On Windows, connects or disconnects a drive letter to a network path. The drive letter must be followed by a colon. For drive letters already
   in use, see `os.drives`. Example:

   > os.netuse('z:', '\\\\TITANIA\\drive_c');  # connect drive with label 'drive_c' on computer TITANIA to drive letter z:

   > os.netuse('z:');  # disconnect

   The function returns true on success and issues an error otherwise. On non-Windows platforms, the function simply returns `fail`.

   The code is based on NTLua 3.0, created by Antonio Escaño Scuri, file src/net.c, taken from http://ntlua.sourceforge.net/, 2.27.1 */

static int os_netuse (lua_State *L) {
  if (tools_getwinver() < MS_WIN2K) {
    lua_pushnil(L);
  } else {
    int rc;
    size_t l_drive, l_path;
    char *drive, *path;
    drive = (char *)agn_checklstring(L, 1, &l_drive);
    if (l_drive == 0)
      luaL_error(L, "Error in " LUA_QS ": drive letter must not be the empty string.", "os.netuse");
    else if (!isalpha(drive[0]))
      luaL_error(L, "Error in " LUA_QS ": drive letter " LUA_QS " is invalid.", "os.netuse", drive);
    else if (l_drive == 1)
      luaL_error(L, "Error in " LUA_QS ": drive letter must be followed by a colon.", "os.netuse");
    path = lua_isnoneornil(L, 2) ? NULL : (char *)agn_checklstring(L, 2, &l_path);
    if (l_path == 0)
      luaL_error(L, "Error in " LUA_QS ": path must not be the empty string.", "os.netuse");
    if (path)  /* connect */
      rc = utlNetUse(drive, path);
    else
      rc = utlNetDel(drive);
    issueosneterrorforrc(L, rc, "os.netuse");
    lua_pushboolean(L, rc);
  }
  return 1;
}


#include <lmcons.h>
#include <lmshare.h>
#include <lmaccess.h>
#include <lmmsg.h>     /* for os.netsend */
#include <lmapibuf.h>
#include <lmserver.h>

static WCHAR *utlChar2Wide (char *str) {
  if (str) {
    int n = strlen(str) + 1;
    WCHAR *wstr = malloc(n * sizeof(WCHAR));
    if (wstr == NULL) return NULL;  /* 4.11.5 fix */
    MultiByteToWideChar(CP_ACP, 0, str, -1, wstr, n);
    return wstr;
  }
  return NULL;
}

static char *utlWide2Char (WCHAR *wstr) {
  if (wstr) {
    int n = wcslen(wstr) + 1;
    char *str = malloc(n);
    if (str == NULL) return NULL;  /* 4.11.5 fix */
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, n, NULL, NULL);
    return str;  /* FREE ME ! */
  }
  return NULL;
}

static WCHAR* utlWideServerName (char *servername) {
  WCHAR *wservername = NULL;
  if (servername) {
    char *full_servername = str_concat("\\\\", servername, NULL);
    if (!full_servername) return NULL;
    wservername = utlChar2Wide(full_servername);
    xfree(full_servername);
  }
  return wservername;
}

static int utlNetSend (char *servername, char *toname, char *msg) {
  DWORD err;
  WCHAR *wszToName, *wszMsg, *wszServerName;
  wszServerName = utlWideServerName(servername);
  if (!wszServerName) return 0;
  wszToName = utlChar2Wide(toname);
  wszMsg = utlChar2Wide(msg);
  err = NetMessageBufferSend(wszServerName, wszToName, NULL, (unsigned char *)wszMsg, 2*wcslen(wszMsg));
  SetLastError(err);
  xfreeall(wszServerName, wszToName, wszMsg);
  return (err == 0);
}

/* The code is based on NTLua 3.0, created by Antonio Escaño Scuri, file src/net.c, taken from http://ntlua.sourceforge.net/, 2.27.1 */
static int os_netsend (lua_State *L) {  /* 2.27.1 should work in Windows XP and earlier */
  int rc;
  char *servername, *toname, *msg;
  servername = (lua_isnoneornil(L, 1)) ? NULL : (char *)agn_checkstring(L, 1);
  toname = (char *)agn_checkstring(L, 2);
  msg = (char *)agn_checkstring(L, 3);
  rc = utlNetSend(servername, toname, msg);
  issueosneterrorforrc(L, rc, "os.netsend");
  return 0;
}

/* returns the domain name; Win XP and above; 2.27.1 */
static char *utlNetGetDomain (void) {
  HANDLE hProcess, hAccessToken;
  UCHAR InfoBuffer[1000];
  PTOKEN_USER pTokenUser = (PTOKEN_USER)InfoBuffer;
  DWORD dwInfoBufferSize;
  SID_NAME_USE snu;
  char UserName[50];
  char DomainName[50];
  DWORD dwSize1 = 50, dwSize2 = 50;
  BOOL err;
  /* Pega o nome do dominio. */
  hProcess = GetCurrentProcess();
  OpenProcessToken(hProcess, TOKEN_READ, &hAccessToken);
  GetTokenInformation(hAccessToken, TokenUser, InfoBuffer, 1000, &dwInfoBufferSize);
  err = LookupAccountSid(NULL, pTokenUser->User.Sid, UserName, &dwSize1, DomainName, &dwSize2, &snu);
  CloseHandle(hAccessToken);
  return (err == 0) ? NULL : tools_strdup(DomainName);  /* FREE ME ! */
}

/* returns the name of the primary domain controller (PDC); Win 2000 and above; 2.27.1 */
static char *utlNetGetPDC (char *servername, char *domainname) {
  DWORD err;
  WCHAR *wszDomainName, *wszServerName, *wszPrimaryDC;
  char* pdc;
  wszServerName = utlWideServerName(servername);
  wszDomainName = utlChar2Wide(domainname);
  err = NetGetDCName(wszServerName, wszDomainName, (LPBYTE *)&wszPrimaryDC);
  SetLastError(err);
  xfreeall(wszServerName, wszDomainName);
  if (err != 0) return NULL;
  pdc = utlWide2Char(wszPrimaryDC + 2);
  NetApiBufferFree(wszPrimaryDC);
  return pdc;  /* FREE ME ! */
}


/* The code is based on NTLua 3.0, created by Antonio Escaño Scuri, file src/net.c, taken from http://ntlua.sourceforge.net/, 2.27.1 */
static int os_netdomain (lua_State *L) {  /* 2.27.1 */
  int nrets = 1;
  char *servername, *domainname, *pdc;
  if (tools_getwinver() < MS_WINXP)
    lua_pushnil(L);
  else {
    servername = (lua_isnoneornil(L, 1)) ? NULL : (char *)agn_checkstring(L, 1);
    /* if sername is NULL; the local computer is used */
    domainname = utlNetGetDomain();
    issueosneterrorforchar(L, domainname, "os.netdomain");
    lua_pushstring(L, domainname);
    xfree(domainname);
    pdc = utlNetGetPDC(servername, domainname);
    lua_pushstring(L, !pdc ? "unknown" : pdc);
    xfree(pdc);
    nrets++;
  }
  return nrets;
}
#endif


#ifdef _WIN32
#define SYS_ERRNO    GetLastError()
#else
#define SYS_ERRNO    errno
#endif

/* 2.8.6, taken from lua_sys package written by Nodir Temirkhodjaev, <nodir.temir@gmail.com>, modified by Alexander Walz.
  With no argument, returns the message thrown by the operating system of the last occurring error */
static int os_strerror (lua_State *L) {
  int surplus = 0;
  const int err = luaL_optint(L, 1, SYS_ERRNO);
#ifndef _WIN32
#if defined(BSD) || (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600)
  char buf[512];
  if (!err) goto success;
  if (!my_ioerror(err))  /* changed 2.10.4 */
    lua_pushstring(L, buf);
#endif
  lua_pushstring(L, my_ioerror(err));  /* changed 2.10.4 */
#else
  const int flags = FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM;
  WCHAR buf[512];
  if (!err) goto success;
  lua_pushstring(L, my_ioerror(err));  /* changed 2.10.4 */
#endif
  lua_pushinteger(L, err);
#ifdef _WIN32
  if (FormatMessageA(flags, NULL, err, 0, (char *)buf, sizeof(buf), NULL)) {
    lua_pushstring(L, (const char*)buf);  /* return translation of error message, too */
    surplus = 1;
  }
#endif
  return 2 + surplus;
#if defined(BSD) || (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) || defined(_WIN32)
 success:
#endif
  lua_pushliteral(L, "OK");
  lua_pushinteger(L, err);
  return 2;
}


#if defined(_WIN32) || defined(__OS2__) || defined(__DJGPP__)
/* See: https://msdn.microsoft.com/en-us/library/windows/desktop/ms682064%28v=vs.85%29.aspx */

/* Reads and changes the code page for both input and output. See also: os.getlocale, os.setlocale.
   OS/2 & DOS support added with 2.17.3.

   Linux has no codepages, and uses the locale to control associated functionality, see:
   https://stackoverflow.com/questions/45808888/how-to-get-local-codepage-in-linux */
static int os_codepage (lua_State *L) {
  size_t nargs = lua_gettop(L);
  int input = 0;  /* 7.5.15 security fixes */
  int output = 0;
  int value = 0;
  int i = 0;
  const char *option = NULL;
  if (nargs == 0) {
#ifdef _WIN32
    CPINFOEX lpCPInfoEx = {0};  /* 7.5.15 Dr. Memory fix */
    luaL_checkstack(L, 4, "not enough stack space");  /* 4.7.1 fix */
    input = GetConsoleCP();  /* error handling introduced with 2.17.3 */
    output = GetConsoleOutputCP();
    lua_pushnumber(L, (input != 0) ? input : AGN_NAN);  /* get the input code page used by the console */
    lua_pushnumber(L, (output != 0) ? output : AGN_NAN);  /* retrieves the output code page used by the console */
    if (GetCPInfoEx(input, 0, &lpCPInfoEx) != 0)   /* returns a nonzero value if successful, or 0 otherwise.  */
      lua_pushstring(L, lpCPInfoEx.CodePageName);
    else
      lua_pushfail(L);
    if (GetCPInfoEx(output, 0, &lpCPInfoEx) != 0)  /* ditto */
      lua_pushstring(L, lpCPInfoEx.CodePageName);
    else
      lua_pushfail(L);
    return 4;
#elif defined(__DJGPP__)
    (void)input; (void)output; (void)value; (void)i;
    /* See: https://groups.google.com/forum/#!topic/comp.os.msdos.djgpp/BgKJ9zGbdp0 */
    __dpmi_regs regs;
    short param_block[2] = {0, 437};
    regs.d.eax = 0x440C;                /* GENERIC IO FOR HANDLES */
    regs.d.ebx = 1;                     /* STDOUT */
    regs.d.ecx = 0x036A;                /* 3 = CON, 0x6A = QUERY SELECTED CP */
    regs.x.ds = __tb >> 4;              /* using transfer buffer for low mem. */
    regs.x.dx = __tb & 0x0F;            /* (suggested by DJGPP FAQ, hi Eli!) */
    regs.x.flags |= 1;                  /* preset carry for potential failure */
    __dpmi_int(0x21, &regs);
    if (!(regs.x.flags & 1))            /* if succeed (carry flag set) ... */
      dosmemget(__tb, 4, param_block);
    else {                              /* (undocumented method) */
      regs.x.ax = 0xAD02;               /* 440C -> MS-DOS or DR-DOS only */
      regs.x.bx = 0xFFFE;               /* AD02 -> MS-DOS or FreeDOS only */
      regs.x.flags |= 1;
      __dpmi_int(0x2F, &regs);
      if ((!(regs.x.flags & 1)) && (regs.x.bx < 0xFFFE))
        param_block[1] = regs.x.bx;
   }
   lua_pushinteger(L, param_block[1]);
   return 1;
#else
    (void)input; (void)output; (void)value; (void)i;
    /* input code page */
    ULONG  aulCpList[8]  = {0},                /* Code page list        */
           ulBufSize     = 8 * sizeof(ULONG),  /* Size of output list   */
           ulListSize    = 0;                  /* Size of list returned */
    USHORT pidCP;                              /* input code page */
    APIRET rc            = NO_ERROR;           /* Return code           */
    rc = KbdGetCp(0, &pidCP, 0);
    lua_pushnumber(L, (rc == NO_ERROR) ? pidCP : AGN_NAN);  /* current input code page */
    /* output code page */
    rc = DosQueryCp(ulBufSize,      /* Length of output code page list  */
                    aulCpList,      /* List of code pages               */
                    &ulListSize);   /* Length of list returned          */
    lua_pushnumber(L, (rc == NO_ERROR) ? aulCpList[0] : AGN_NAN);  /* current output code page */
    return 2;
#endif
  } else {
#if defined(__DJGPP__)
    luaL_error(L, "Error in " LUA_QS ": codepage cannot be set in DOS.", "os.codepage");
    return 1;
  }
#elif defined(_WIN32)
    int rc;
#else
    APIRET rc;
#endif
#if defined(_WIN32) || defined(__OS2__)
    luaL_checkstack(L, nargs, "not enough stack space");  /* 4.7.1 fix */
    input = rc = value = 0;
    option = NULL;
    for (i=0; i < nargs; i++) {
      if (lua_ispair(L, i + 1)) {
        agn_pairgeti(L, i + 1, 1);  /* get lhs */
        if (lua_isstring(L, -1))
          option = lua_tostring(L, -1);
        else
          luaL_error(L, "Error in " LUA_QS ": left-hand side must be a string, got %s.", "os.codepage", luaL_typename(L, -1));
        if (tools_streq(option, "input"))  /* 2.16.12 tweak */
          input = 1;
        else if (tools_streq(option, "output"))
          input = 0;
        else
          luaL_error(L, "Error in " LUA_QS ": unknown option %s.", "os.codepage", option);
        agn_poptop(L);
        agn_pairgeti(L, i + 1, 2);  /* get rhs */
        if (lua_isnumber(L, -1))
          value = lua_tonumber(L, -1);
        else
          luaL_error(L, "Error in " LUA_QS ": right-hand side must be a number, got %s.", "os.codepage", luaL_typename(L, -1));
        agn_poptop(L);
        if (input)
#ifdef _WIN32
          rc = SetConsoleCP(value);
#else
          rc = KbdSetCp(0, (USHORT)value, 0) == NO_ERROR;
#endif
        else
#ifdef _WIN32
          rc = SetConsoleOutputCP(value);
#else
          rc = VioSetCp(0, (USHORT)value, 0) == NO_ERROR;
#endif
        lua_pushboolean(L, rc != 0);
      } else
        luaL_error(L, "Error in " LUA_QS ": argument #%d must be a pair, got %s.", "os.codepage", i + 1, luaL_typename(L, i + 1));
    }
    return i;
  }
#endif
}
#endif


/* Returns various information on the current locale including decimal point and thousands separators,
   currency, and monetary formatting suggestions. The return is a table of the key~value pairs listed
   below. A value of "" means `unspecified`. 2.10.0 */

/* #ifdef _WIN32
#define PRIMARYLANGID(lgid)    ((WORD)(lgid) & 0x3ff)
#define SUBLANGID(lgid)        ((WORD)(lgid) >> 10)
#endif */

#ifdef _WIN32
#define WINDOWSAPI_BUFLENGTH  (sizeof(LPWSTR)*(KL_NAMELENGTH > LOCALE_NAME_MAX_LENGTH ? KL_NAMELENGTH : LOCALE_NAME_MAX_LENGTH))
#endif

static int os_getlocale (lua_State *L) {
#ifdef _WIN32
  char kblang[KL_NAMELENGTH + 1] = { 0 };
  HKL kblangid;
  struct WinVer winversion;
#endif
  struct lconv *lc;
  lc = localeconv();
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
  lua_createtable(L, 0, 19);
  lua_rawsetstringstring(L,  -1, "locale", setlocale(LC_ALL, NULL));
  lua_rawsetstringstring(L,  -1, "charset", setlocale(LC_CTYPE, NULL));  /* 2.35.4 */
#ifdef _WIN32
  if (getWindowsVersion(&winversion) >= MS_WIN2K) {  /* 2.35.4, W2K and higher */
    char buf[WINDOWSAPI_BUFLENGTH];
    LANGID systemdefaultuilanguage;
    luaL_checkstack(L, 3, "not enough stack space");  /* 5.4.0 */
    lua_createtable(L, 0, 3);
    if (GetKeyboardLayoutNameA(kblang) != 0) {
      /* for a list see:
         https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-language-pack-default-values?view=windows-11 */
      lua_pushstring(L, "0x");
      lua_pushstring(L, kblang);
      lua_concat(L, 2);
      lua_rawsetstringstring(L,  -2, "hex", agn_tostring(L, -1));  /* a string representing a hexadecimal code */
      agn_poptop(L);
    }
    /* W2K and higher, does not return correct values on `mixed` systems: */
    kblangid = GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), 0));
    lua_rawsetstringnumber(L,  -1, "langid", MAKELCID(kblangid, SORT_DEFAULT));
    lua_rawsetstringnumber(L,  -1, "primarylangid", PRIMARYLANGID(kblangid));
    lua_rawsetstringnumber(L,  -1, "sublangid", SUBLANGID(kblangid));
    if (GetLocaleInfoA(MAKELCID(kblangid, SORT_DEFAULT), LOCALE_SLANGUAGE, buf, WINDOWSAPI_BUFLENGTH)) {
      lua_rawsetstringstring(L,  -1, "name", buf);
    }
    lua_setfield(L, -2, "keyboard");
    lua_rawsetstringnumber(L,  -1, "UserDefaultLangID", GetUserDefaultLangID());
    systemdefaultuilanguage = GetSystemDefaultUILanguage();
    lua_rawsetstringnumber(L,  -1, "SystemDefaultUILangID", systemdefaultuilanguage);
    if (GetLocaleInfoA(GetUserDefaultLCID(), LOCALE_SLANGUAGE, buf, WINDOWSAPI_BUFLENGTH)) {
      lua_rawsetstringstring(L,  -1, "UserDefaultLangName", buf);
    }
    if (GetLocaleInfoA(systemdefaultuilanguage, LOCALE_SLANGUAGE, buf, WINDOWSAPI_BUFLENGTH)) {
      lua_rawsetstringstring(L,  -1, "SystemDefaultUILangName", buf);
    }
  }
#endif
  lua_rawsetstringstring(L,  -1, "decimal_point", lc->decimal_point);  /* decimal-point separator */
  lua_rawsetstringstring(L,  -1, "thousands_sep", lc->thousands_sep);  /* thousands separator */
  lua_rawsetstringstring(L,  -1, "grouping", lc->grouping);            /* unknown meaning */
  lua_rawsetstringstring(L,  -1, "mon_grouping", lc->mon_grouping);    /* unknown meaning */
  /* international currency symbol according to international standard ISO 4217 "Codes for the Representation of Currency and Funds" */
  lua_rawsetstringstring(L,  -1, "int_curr_symbol", lc->int_curr_symbol);
  lua_rawsetstringstring(L,  -1, "currency_symbol", lc->currency_symbol);      /* local currency symbol */
  lua_rawsetstringstring(L,  -1, "mon_decimal_point", lc->mon_decimal_point);  /* decimal point separator for monetary amounts */
  lua_rawsetstringstring(L,  -1, "mon_thousands_sep", lc->mon_thousands_sep);  /* thousands separator for monetary amounts */
  lua_rawsetstringstring(L,  -1, "positive_sign", lc->positive_sign);
  lua_rawsetstringstring(L,  -1, "negative_sign", lc->negative_sign);
  lua_rawsetstringnumber(L,  -1, "int_frac_digits", lc->int_frac_digits);  /* recommended number of decimal places of monetary amounts, international standard */
  lua_rawsetstringnumber(L,  -1, "frac_digits", lc->frac_digits);  /* recommended number of decimal places of monetary amounts, local standard */
  lua_rawsetstringboolean(L, -1, "p_cs_precedes", lc->p_cs_precedes);  /* recommendation whether currency symbol precedes positive monetary amount */
  lua_rawsetstringboolean(L, -1, "n_cs_precedes", lc->p_cs_precedes);  /* recommendation whether currency symbol precedes negative monetary amount */
  lua_rawsetstringboolean(L, -1, "p_sep_by_space", lc->p_sep_by_space);  /* recommendation whether currency symbol and positive monetary amount are separated by a blank */
  lua_rawsetstringboolean(L, -1, "n_sep_by_space", lc->n_sep_by_space);  /* recommendation whether currency symbol and negative monetary amount are separated by a blank */
  /* indicator how to position the sign for nonnegative and negative monetary quantities */
  lua_rawsetstringnumber(L,  -1, "p_sign_posn", lc->p_sign_posn);
  lua_rawsetstringnumber(L,  -1, "n_sign_posn", lc->n_sign_posn);
  return 1;
}


#ifdef _WIN32
static int os_getlanguage (lua_State *L) {
  char buf[WINDOWSAPI_BUFLENGTH];
  if (GetLocaleInfoA(MAKELCID(agn_checkinteger(L, 1), SORT_DEFAULT), LOCALE_SLANGUAGE, buf, WINDOWSAPI_BUFLENGTH)) {
    lua_pushstring(L, buf);
  } else {
    lua_pushfail(L);
  }
  return 1;
}
#endif


#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__)
/* The function changes the owner of the file fn (a filename, thus a string) to owner o, and optionally to group g.

o and g may be numbers or strings. If a number is passed, it denotes a user id (uid) or group id (gid).
If a string is passed, it denotes a user or group name. If g is not given, the default group of user o is set.

The function returns true on success and issues an error message otherwise. It is available in the UNIX
versions of Agena, only. */
static int os_chown (lua_State *L) {  /* 2.10.0 */
  const char *path;
  struct passwd *p;
  struct group *q;
  int r, en;
  p = NULL; q = NULL;
  path = agn_checkstring(L, 1);
  if (!tools_exists(path))  /* 2.16.11 change */
    luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": file or directory does not exist.", "os.chown", path);
  if (agn_isnumber(L, 2))
    p = getpwuid((uid_t)agn_tonumber(L, 2));
  else if (agn_isstring(L, 2))
    p = getpwnam(agn_tostring(L, 2));
  else
    luaL_error(L, "Error in " LUA_QS ": argument #%d must be a number or string, got %s.", "os.chown", 2, luaL_typename(L, 2));
  if (p == NULL)
    luaL_error(L, "Error in " LUA_QS ": user does not exist.", "os.chown");
  if (agn_isnumber(L, 3))
    q = getgrgid((gid_t)agn_tonumber(L, 3));
  else if (agn_isstring(L, 3))
    q = getgrnam(agn_tostring(L, 3));
  else
    q = getgrgid(p->pw_gid);
  if (q == NULL)
    luaL_error(L, "Error in " LUA_QS ": group does not exist.", "os.chown");
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  r = chown(path, p->pw_uid, q->gr_gid);
  en = errno;
  if (r == -1) {
    luaL_error(L, "Error in " LUA_QS ": %s.", "os.chown", my_ioerror(en));  /* changed 2.10.4 */
  }
  lua_pushtrue(L);
  return 1;
}


/* Takes a file path fn (a filename, thus a string) and a mode m (an integer) denoting a three-digit
   octal number and changes the file permissions accordingly. Contrary to `os.fattrib`, mode m must
   _not_ be preceded by the `0o` token. The function returns `true` on success and issues an error
   otherwise. It is available in the UNIX versions of Agena, only.*/
static int os_chmod (lua_State *L) {  /* 2.10.0 */
  const char *path;
  int mode, r, c, d, e, en;
  path = agn_checkstring(L, 1);
  if (!tools_exists(path))  /* 2.16.11 change */
    luaL_error(L, "Error in " LUA_QS " with " LUA_QS ": file or directory does not exist.", "os.chmod", path);
  mode = agn_checkinteger(L, 2);
  if (mode < 0)
    luaL_error(L, "Error in " LUA_QS " with %d: mode must be non-negative.", "os.chmod", mode);
  r = 0;
  for (c=0; mode > 0; c++) {
    d = mode % 10;
    if (mode > 7)
      luaL_error(L, "Error in " LUA_QS ": mode includes invalid digit %d.", "os.chmod", mode);
    r += d * tools_intpow(8, c);  /* 2.14.13 change */
    mode /= 10;
  }
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  e = chmod(path, (mode_t)r);
  en = errno;
  if (e == -1) {
    luaL_error(L, "Error in " LUA_QS ": %s.", "os.chmod", my_ioerror(en));  /* changed 2.10.4 */
  }
  lua_pushtrue(L);
  return 1;
}
#endif


/* Returns the directory name of the given path, a string. If `path` has no separator, then the function returns '.'.
   If you would like to test relative paths, apply `os.realpath` to `path` before calling this function. */
static void dirorfilename (lua_State *L, int what) {  /* 2.10.0 */
  char *path, *cur, *subs;
  const char *path0;
  size_t l;
  struct stat buf;
  int mode, e;
  path0 = agn_checklstring(L, 1, &l);
  if (l == 0)
    luaL_error(L, "Error in " LUA_QS ": path is empty.", (what) ? "os.filename" : "os.dirname");
  else if (l == 1 && ispathsep(*path0))
    lua_pushstring(L, "/");
  else {
    int isdriveletter;
    path = (l == 2 && isdriveletter(path0)) ? (l++, str_concat(path0, "/", NULL)) : tools_strndup(path0, l);
    str_charreplace(path, '\\', '/', 0);
    isdriveletter = (l == 3 && isdriveletter(path) && path[2] == '/');  /* drive letter with trailing slash ? 2.39.8 */
    cur = path + l;  /* set cursor to '\0' */
    while (!isdriveletter && path < cur && ispathsep(*(cur - 1)))
      cur--; /* "remove" trailing slashes; don't touch if path = "/" or drive letter, 2.39.8 fix */
    subs = str_substr(path, 0, cur - path - 1, &e);
    if (e == -1) {
      xfree(path);
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", (what) ? "os.filename" : "os.dirname");
    }
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__)
    mode = lstat(subs, &buf);
#else
    mode = stat(subs, &buf);
#endif
    /* check whether passed path actually is a directory */
    if (mode != -1 && S_ISDIR(buf.st_mode)) {  /* with os.filename, check for files, links, sockets, and special files */
      if (what)  /*`get directory name */
        lua_pushlstring(L, path, cur - path);  /* 2.39.7 fix, do not return surplus trailing \0 */
      else  /* filename retrieval ? */
        lua_pushnil(L);
    } else {  /* ignore (l)stat error */
      while (path < cur && !ispathsep(*cur)) cur--;  /* skip filename */
      if (what) {  /* get directory name */
        if (path == cur && !ispathsep(*path)) {
          char first = tolower(*path);
          if (tools_strlen(path) == 2 && first >= 'a' && first <= 'z' && *(path + 1) == ':') { /* a drive letter in OS/2, Windows, and DOS ? */
            *path = toupper(*path);
            lua_pushstring(L, path);  /* absolute path given: return drive letter, else relative path */
          } else
            lua_pushstring(L, ".");
        } else
          lua_pushlstring(L, path, cur - path);
      } else {  /* push filename without leading slash */
        if (ispathsep(*cur)) cur++;
        lua_pushstring(L, cur);
      }
    }
    xfree(path); xfree(subs);
  }
}

static int os_dirname (lua_State *L) {
  dirorfilename(L, 1);
  return 1;
}

static int os_filename (lua_State *L) {  /* = UNIX basename command */
#ifdef _WIN32
  const char *filename = agn_checkstring(L, 1);
  int option = lua_gettop(L) > 1;
#else
  int option = 0;
#endif
  dirorfilename(L, 0);
#ifdef _WIN32
  if (option && agn_isstring(L, -1)) {  /* 2.27.1 */
    DWORD shortlength = GetShortPathName(filename, NULL, 0);
    if (shortlength != 0) {
      TCHAR *buffer = tools_stralloc(shortlength);
      shortlength = GetShortPathName(filename, buffer, shortlength);
      if (!buffer)
        lua_pushnil(L);
      else {
        lua_pushstring(L, buffer);
        xfree(buffer);
      }
    }
  }
#endif
  return 1 + option;
}

/* The function checks whether path p (a string) points to a directory. It returns `true` on success,
   `false` if the path points to another existing file system object, and `fail` if p does not exist
   on the file system at all.

   See also: os.fstat, os.isfile, os.islink. */
static int os_isdir (lua_State *L) {  /* 2.10.0, based on os_fstat */
  struct stat entry;
  const char *fn;
  fn = luaL_checkstring(L, 1);
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
  if (lstat(fn, &entry) == 0) {  /* do not follow symbolic links */
#else
  if (stat(fn, &entry) == 0) {
#endif
    lua_pushboolean(L, S_ISDIR(entry.st_mode));
  } else
    lua_pushfail(L);  /* file does not exist */
  return 1;
}


/* The function checks whether path p (a string) points to a file. It returns `true` on success,
   `false` if the path points to another existing file system object, and `fail` if p does not exist
   on the file system at all.

   See also: os.fstat, os.isdir, os.islink. */
static int os_isfile (lua_State *L) {  /* 2.10.0, based on os_fstat */
  struct stat entry;
  const char *fn;
  fn = luaL_checkstring(L, 1);
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
  if (lstat(fn, &entry) == 0) {
#else
  if (stat(fn, &entry) == 0) {
#endif
    lua_pushboolean(L, S_ISREG(entry.st_mode));
  } else
    lua_pushfail(L);  /* file does not exist */
  return 1;
}


/* The function checks whether path p (a string) points to a file. It returns `true` on success,
   `false` if the path points to another existing file system object, and `fail` if p does not exist
   on the file system at all.

   See also: os.fstat, os.isdir, os.isfile. */
static int os_islink (lua_State *L) {  /* 2.10.0, based on os_fstat */
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
  struct stat entry;
#endif
  const char *fn;
  fn = luaL_checkstring(L, 1);
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
  if (lstat(fn, &entry) == 0)
    lua_pushboolean(L, S_ISLNK(entry.st_mode));
  else
    lua_pushfail(L);  /* file does not exist */
#elif defined(_WIN32)
  if (tools_exists(fn))  /* 2.16.11 change */
    lua_pushboolean(L, issymlink(fn));
  else
    lua_pushfail(L);  /* file does not exist */
#else
  (void)fn;  /* to prevent compiler warnings */
  lua_pushfail(L);
#endif
  return 1;
}


static int os_isvalidpath (lua_State *L) {  /* 5.2.3 */
  const char *path = agn_checkstring(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushboolean(L, tools_iswinpath(path));
  lua_pushboolean(L, tools_isunixpath(path));
  return 2;
}


/* This function takes a string `path`, converts it to a wide string, calls the Win32 C API
   function GetFullPathNameW, and returns the absolute path. The function is available in
   the Windows edition only. Created by Gemini AI 7.6.3 */
#ifdef _WIN32
static int os_getfullpathname (lua_State *L) {
  size_t len;
  const char *path = luaL_checklstring(L, 1, &len);
  /* Convert UTF-8/ANSI path to wide char */
  int wlen = MultiByteToWideChar(CP_UTF8, 0, path, (int)len, NULL, 0);
  wchar_t *wpath = (wchar_t *)malloc((wlen + 1) * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, path, (int)len, wpath, wlen);
  wpath[wlen] = L'\0';
  /* Get the buffer size needed */
  DWORD buffer_size = GetFullPathNameW(wpath, 0, NULL, NULL);
  wchar_t *full_path = (wchar_t *)malloc(buffer_size * sizeof(wchar_t));
  /* Retrieve the absolute path, works on Windows 2000 SP4, too, contrary to the MSDN description */
  GetFullPathNameW(wpath, buffer_size, full_path, NULL);
  /* Convert back to UTF-8 for Lua */
  int utf8_len = WideCharToMultiByte(CP_UTF8, 0, full_path, -1, NULL, 0, NULL, NULL);
  char *result = (char *)malloc(utf8_len);
  WideCharToMultiByte(CP_UTF8, 0, full_path, -1, result, utf8_len, NULL, NULL);
  lua_pushstring(L, result);
  /* Free allocated memory */
  free(wpath);
  free(full_path);
  free(result);
  return 1;
}
#endif


/* Returns the absolute path to the currently executing programme, 2.16.1 */
static int os_getmodulefilename (lua_State *L) {
  char *apath = getModuleFileName();
  if (apath == NULL)
    lua_pushnil(L);
  else
    lua_pushstring(L, apath);
  /* Note that free() according to the C standard just quits when pointer is NULL. */
  xfree(apath);
  return 1;
}


/* Retrieves the path of the directory designated for temporary files, of type string. Note that on non-Windows systems,
   the function might just issue an error, if the environment variables TEMP, TMP and TMPDIR are all unassigned. 2.16.10 */
static int os_gettemppath (lua_State *L) {
#ifdef _WIN32
  /* see: https://docs.microsoft.com/en-us/windows/win32/fileio/creating-and-using-a-temporary-file */
  char buf[MAX_PATH + 1];;
  if (!GetTempPath(MAX_PATH + 1, buf))
    luaL_error(L, "Error in " LUA_QS ": internal error.", "os.gettemppath");
  /* Do NOT use pushlstring/strlen since there are embedded zeros in the buffer. */
  str_charreplace(buf, '\\', '/', 1);
  lua_pushstring(L, buf);  /* 2.16.11 improvement */ /* 2.17.8 tweak, 2.28.3 fix */
  return 1;
#else  /* defined(__DJGPP__) || defined(__OS2__) || defined(__unix__) || defined(__APPLE__), etc. */
  int i;
  char *path;
  const char *const envnames[] = {"TMPDIR", "TEMP", "TMP", "SMTMP", NULL};
  const char *const tmps[] = {"/tmp", "/var/tmp", "c:/temp", NULL};  /* XXX In OS/2 better query <QSV_BOOT_DRIVE>:\temp */
  for (i=0; envnames[i]; i++) {
    path = tools_getenv(envnames[i]);  /* 2.16.11 */
    if (path) {
      lua_pushstring(L, path);
      xfree(path);
      return 1;
    }
  }
  for (i=0; tmps[i]; i++) {
    path = (char *)tmps[i];
    if (tools_exists(path)) {  /* 2.16.11 change */
      lua_pushstring(L, path);
      return 1;
    }
  }
  luaL_error(L, "Error in " LUA_QS ": could not determine path.", "os.gettemppath");
  return 0;  /* to prevent compiler warnings */
#endif
}


/* Checks whether a directory or file can be accessed. `path` represents the path to the file, directory or symbolic link, and `mode`
   specifies the accessibility:
   os.f_ok = check for existence
   os.x_ok = file is executable
   os.w_ok = write access is granted
   os.r_ok = read access is permitted (the default)

   You can either specify 0 for `mode`, or a bitwise-OR mask of 1, 2 and/or 4 created by calling the `||` operator.

   If at least one bit in the mask asked for a permission is denied, the function returns `false`, and `true` otherwise. If the specified
   access is not granted, a string describing the kind of error is returned, too.

   Example:

   > os.faccess('myfolder');            # check for read access to a non-existent folder
   false   file or directory does not exist

   > os.faccess('myfile.txt', os.r_ok || os.w_ok);  # check for both read and write access to an existing file
   true

   See also: `os.fattrib`, `os.fstat`, `os.exists`. */
static int os_faccess (lua_State *L) {  /* 2.22.0 */
  int mode, en, rc;
  const char *path = agn_checkstring(L, 1);
  mode = R_OK;  /* to prevent compiler warnings */
  if (lua_gettop(L) == 2) {
    if (agn_isinteger(L, 2)) mode = lua_tointeger(L, 2);
    else if (agn_isstring(L, 2)) {
      const char *p;
      int flag, c;
      flag = 1; c = 0; mode = 0;
      for (p = agn_tostring(L, 2); *p != '\0' && flag; p++, c++) {
        if (c > 2)  /* explicitly avoid overflows */
          luaL_error(L, "Error in " LUA_QS ": up to three characters can be given for mode.", "os.faccess");
        switch (*p) {
          case 'r': mode |= R_OK; break;
          case 'w': mode |= W_OK; break;
          case 'x': mode |= X_OK; break;
          case 'f': {
            if (c != 0)
              luaL_error(L, "Error in " LUA_QS ": 'f' and other modes cannot be mixed.", "os.faccess");
            mode |= F_OK; flag = 0; break;
          }
          default:
            luaL_error(L, "Error in " LUA_QS ": unknown mode %c.", "os.faccess", *p);
        }
      }
      if (c == 0)
        luaL_error(L, "Error in " LUA_QS ": at least one character must be given for mode.", "os.faccess");
    } else
      luaL_error(L, "Error in " LUA_QS ": invalid option.", "os.faccess");
  }
  /* OS/2, Windows, Stretch, Solaris: R_OK=4 W_OK=2 X_OK=1 F_OK=0
     DJGPP/DOS:                       R_OK=2 W_OK=4 X_OK=8 F_OK=1
     printf("R_OK=%d W_OK=%d X_OK=%d F_OK=%d\n", R_OK, W_OK, X_OK, F_OK); */
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  rc = access(path, mode) == -1;  /* 1 if at least one of the mask bits was denied or an error occurred, 0 if not */
  en = errno;
  if (rc) {
    lua_pushfalse(L);
    lua_pushstring(L, my_ioerror(en));
  } else
    lua_pushtrue(L);
  return 1 + rc;
}


/*
The function uses the identity of the file, i.e. its inode (see `os.fstat`) named by the given pathname, a string,
and the least significant 8 bits of id (which must be nonzero) to generate a signed 4-byte integral System V IPC
(Inter Process Communications) key.

On UNIX platforms, the return is one integer, and on Windows two integers, the first for the higher part of the file index and
the second for its lower part. On all other systems, the function issues an error.

The result is the same for all pathnames that name the same file, when the same value of `id` is used.
The value returned should be different when the (simultaneously existing) files or the given `id`s differ.
*/

/* Taken from MUSL-1.2.3 src/ipc/ftok.c */
static int os_ftok (lua_State *L) {  /* 2.35.4 */
  const char *path = agn_checkstring(L, 1);
  int32_t id = luaL_checkint32_t(L, 2);
  if (!tools_exists(path))
    luaL_error(L, "Error in " LUA_QS ": file does not exist.", "os.ftok");
/* #if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) */
#if defined(__unix__) || defined(__DJGPP__) || defined(__APPLE__)  /* 2.35.5 for DOS */
  struct stat st;
  if (stat(path, &st) >= 0) {
    lua_pushnumber(L, tools_ftok(st.st_ino, st.st_dev, id));
    return 1;
  }
#elif defined(__OS2__)
  struct stat st;
  if (stat(path, &st) >= 0) {
    uint32_t hx, lx;
    hx = tools_uint64touint32(st.st_ino, &lx);
    lua_pushnumber(L, tools_ftok(hx, st.st_dev, id));
    lua_pushnumber(L, tools_ftok(lx, st.st_dev, id));
    return 2;
  }
#elif (defined(_WIN32))
  if (tools_getwinver() >= MS_WINXP) {
    HANDLE hFile;
    hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
      int rc;
      BY_HANDLE_FILE_INFORMATION basicInfo;
      if ( (rc = GetFileInformationByHandle(hFile, &basicInfo) ) ) {
        lua_pushnumber(L, tools_ftok(basicInfo.nFileIndexHigh, basicInfo.dwVolumeSerialNumber, id));
        lua_pushnumber(L, tools_ftok(basicInfo.nFileIndexLow, basicInfo.dwVolumeSerialNumber, id));
      }
      CloseHandle(hFile);
      if (rc != 0) return 2;
    }
  }
#endif
  luaL_error(L, "Error in " LUA_QS ": error fetching data.", "os.ftok");
  return 1;
}


static int os_inode (lua_State *L) {  /* 2.35.4 */
  const char *path = agn_checkstring(L, 1);
  if (!tools_exists(path))
    luaL_error(L, "Error in " LUA_QS ": file does not exist.", "os.inode");
/* #if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) */
#if defined(__unix__) || defined(__DJGPP__) || defined(__APPLE__)  /* 2.35.5 for DOS */
  struct stat st;
  if (stat(path, &st) >= 0) {
    lua_pushnumber(L, st.st_dev);
    lua_pushnumber(L, st.st_ino);
    return 2;
  }
#elif defined(__OS2__)
  struct stat st;
  if (stat(path, &st) >= 0) {
    uint32_t hx, lx;
    luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
    lua_pushnumber(L, st.st_dev);
    hx = tools_uint64touint32(st.st_ino, &lx);
    lua_pushnumber(L, hx);
    lua_pushnumber(L, lx);
    return 3;
  }
#elif (defined(_WIN32))
  if (tools_getwinver() >= MS_WINXP) {
    HANDLE hFile;
    hFile = CreateFileA(path,
      GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
      int rc;
      BY_HANDLE_FILE_INFORMATION basicInfo;
      if ( (rc = GetFileInformationByHandle(hFile, &basicInfo) ) ) {
        luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
        lua_pushnumber(L, basicInfo.dwVolumeSerialNumber);
        lua_pushnumber(L, basicInfo.nFileIndexHigh);
        lua_pushnumber(L, basicInfo.nFileIndexLow);
      }
      CloseHandle(hFile);
      if (rc != 0) return 3;
    }
  }
#endif
  luaL_error(L, "Error in " LUA_QS ": error fetching data.", "os.inode");
  return 1;
}


static int os_getdirpathsep (lua_State *L) {  /* 2.39.12 */
  if (AGN_DIRECTORY_SEPARATOR == '\0' || AGN_PATH_SEPARATOR == '\0')
    luaL_error(L, "Error in " LUA_QS ": error fetching data.", "os.getdirpathsep");
  lua_pushchar(L, AGN_DIRECTORY_SEPARATOR);
  lua_pushchar(L, AGN_PATH_SEPARATOR);
  return 2;
}


/* Returns the complete command-line string used to launch the current Agena process, including the
   executable name and all passed flags or arguments.

   In Windows, the UTF-8 narrow string returned preserves Unicode characters (e.g., diacritics,
   international scripts).

   On all other operating systems, the function dynamically reconstructs the command line by iterating
   through the global `argc` and `argv` C argument vectors, joining them sequentially with space
   separators.

   On all platforms, the programme name is always separated from all the other arguments by two spaces
   instead of just one, thus fully emulating the Windows C API function GetCommandLineW().
   7.8.1. Created with Gemini AI. */
static int os_getcommandline (lua_State *L) {
#ifdef _WIN32
  LPWSTR wstr = GetCommandLineW();  /* this works in W2K, too */
  /* convert the wide string to a UTF-8 narrow string */
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
  char *str = (char *)malloc(size_needed);
  if (str != NULL) {
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, size_needed, NULL, NULL);
    lua_pushstring(L, str);
    xfree(str);
  } else {  /* fallback */
    Charbuf buf;
    int i, local_argc = 0;
    /* 1. Use the Windows shell API to cleanly parse the string into a wide argument array */
    LPWSTR *w_argv = CommandLineToArgvW(GetCommandLineW(), &local_argc);
    if (w_argv != NULL) {
      charbuf_init(L, &buf, 0, PATH_MAX < 513 ? PATH_MAX : 512, 1);
      /* Loop through each parsed argument just like the POSIX loop does */
      for (i=0; i < local_argc; i++) {
        /* Convert this specific wide argument back to a temporary narrow string */
        int arg_size = WideCharToMultiByte(CP_UTF8, 0, w_argv[i], -1, NULL, 0, NULL, NULL);
        char *arg = (char *)malloc(arg_size);
        if (arg != NULL) {
          WideCharToMultiByte(CP_UTF8, 0, w_argv[i], -1, arg, arg_size, NULL, NULL);
          /* Append the argument data exactly like your POSIX fallback loop */
          charbuf_append(L, &buf, arg, tools_strlen(arg));
          free(arg);
        }
        /* If it's not the final argument, append the space separator */
        if (i == 0) {  /* work exactly like CommandLineToArgvW does */
          charbuf_appendchar(L, &buf, ' ');
        }
        if (i < local_argc - 1) {
          charbuf_appendchar(L, &buf, ' ');
        }
      }
      /* 2. Free the wide memory block allocated by the Windows shell */
      LocalFree(w_argv);
      charbuf_finish(&buf);
      lua_pushlstring(L, buf.data, buf.size);
      charbuf_free(&buf);
    } else {
      lua_pushliteral(L, "");
    }
  }
#else
  /* 1. Link to the global structure managed by agena.c */
  int local_argc;
  char **local_argv;
  /* 7.8.3: We cannot use a structure that has been assigned in agena.c = agena.exe and is used in loslib.c = agena.dll */
  local_argc = agn_get_argc(L);
  local_argv = agn_get_argv(L);
  if (local_argv == NULL || local_argc <= 0) {
    lua_pushliteral(L, "");
    return 1;
  }
  Charbuf buf;
  int flag = 1;
  charbuf_init(L, &buf, 0, PATH_MAX < 513 ? PATH_MAX : 512, 1);
  while (local_argc) {
    char *arg = *local_argv;
    charbuf_append(L, &buf, arg, tools_strlen(arg));
    local_argv++;
    if (flag) {  /* work exactly like CommandLineToArgvW does */
      charbuf_appendchar(L, &buf, ' ');
      flag = 0;
    }
    if (local_argc != 1) {
      charbuf_appendchar(L, &buf, ' ');
    }
    local_argc--;
  }
  charbuf_finish(&buf);
  lua_pushlstring(L, buf.data, buf.size);
  charbuf_free(&buf);
#endif
  return 1;
}


/*
** Creates a link.
** @param #1 Object to link to.
** @param #2 Name of link.
** @param #3 True if link is symbolic (optional).
**
** Taken from: LuaFileSystem
** Copyright Kepler Project 2003 - 2020
** (http://keplerproject.github.io/luafilesystem)
** Copyright © 2003-2014 Kepler Project.
**
** Permission is hereby granted, free of charge, to any person
** obtaining a copy of this software and associated documentation
** files (the "Software"), to deal in the Software without
** restriction, including without limitation the rights to use, copy,
** modify, merge, publish, distribute, sublicense, and/or sell copies
** of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:

** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.

** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
** NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
** BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
** ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**
** Completely rewritten to compile in MinGW by a walz.
*/
static int os_mklink (lua_State * L) {
  const char *oldpath, *newpath;
  oldpath = luaL_checkstring(L, 1);
  newpath = luaL_checkstring(L, 2);
  if (!tools_exists(oldpath))
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS ".", "os.mklink", oldpath);
#ifndef _WIN32
  if (tools_exists(newpath))
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " does already exist.", "os.mklink", newpath);
  return os_pushresult(L, (agnL_optboolean(L, 3, 1) ? symlink : link)(oldpath, newpath), newpath, "os.mklink", 1);
#else
  char *lnkpath = str_concat(newpath, ".lnk", NULL);
  if (tools_exists(lnkpath)) {
    xfree(lnkpath);
    luaL_error(L, "Error in " LUA_QS ": target already exist.", "os.mklink");
  }
  int rc = symlink(oldpath, lnkpath);
  if (rc == -1) {  /* 7.7.7 fix */
    xfree(lnkpath);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "os.mklink");
  }
  xfree(lnkpath);
  return os_pushresult(L, rc, newpath, "os.mklink", 1);
#endif
}


/*
With no argument given, the function returns the current colour code used in a Windows shell, a non-negative integer
usually in the range [0, 255]. If it could not determine the colour code, the function will throw an error. 5.2.4

With a non-negative integer k given, sets the given colour code. The function returns `true` on success or `false`
and the error code otherwise.

The function gives access to the underlying Win32 C API functions GetConsoleScreenBufferInfo() and SetConsoleTextAttribute(),
thus it is available in Windows only.

Example:

old := os.shellcolour();  # save current setting
for i from 0 to 255 do
   os.shellcolour(i);
   print('My text in colour')
od
os.shellcolour(old);  # reset colour
*/
#ifdef _WIN32
static int os_shellcolour (lua_State * L) {
  HANDLE hConsole;
  int rc, nrets = 1;
  hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hConsole == INVALID_HANDLE_VALUE) {
    int en = GetLastError();  /* 6.6.11 fix */
    luaL_error(L, "Error in " LUA_QS ": could not get handle, %s.", "os.shellcolour", my_ioerror(en));
  }
  if (lua_gettop(L) == 0) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    rc = GetConsoleScreenBufferInfo(hConsole, &info);
    if (!rc) {
      int en = GetLastError();  /* 6.6.11 fix */
      luaL_error(L, "Error in " LUA_QS ": %s.", "os.shellcolour", my_ioerror(en));
    }
    else
      lua_pushinteger(L, info.wAttributes);
  } else {
    int k = agn_checknonnegint(L, 1);
    rc = SetConsoleTextAttribute(hConsole, k) != 0;
    lua_pushboolean(L, rc);
    if (!rc) {
      lua_pushinteger(L, GetLastError());
      nrets++;
    }
  }
  return nrets;
}

static int os_getconsolemode (lua_State * L) {  /* 5.6.1 */
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwOriginalMode = 0;
  if (hConsole == INVALID_HANDLE_VALUE) {
    int en = GetLastError();  /* 6.6.11 fix */
    luaL_error(L, "Error in " LUA_QS ": could not get handle, %s.", "os.getconsolemode", my_ioerror(en));
  }
  if (!GetConsoleMode(hConsole, &dwOriginalMode)) {
    int en = GetLastError();  /* 6.6.11 fix */
    luaL_error(L, "Error in " LUA_QS ": %s.", "os.getconsolemode", my_ioerror(en));
  }
  lua_pushnumber(L, dwOriginalMode);  /* returns a uint32_t */
  return 1;
}

static int os_setconsolemode (lua_State * L) {
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwOriginalMode = 0;
  if (hConsole == INVALID_HANDLE_VALUE) {
    int en = GetLastError();  /* 6.6.11 fix */
    luaL_error(L, "Error in " LUA_QS ": could not get handle, %s.", "os.setconsolemode", my_ioerror(en));
  }
  if (!GetConsoleMode(hConsole, &dwOriginalMode)) {
    int en = GetLastError();  /* 6.6.11 fix */
    luaL_error(L, "Error in " LUA_QS ": %s.", "os.setconsolemode", my_ioerror(en));
  }
  lua_pushboolean(L, SetConsoleMode(hConsole, agn_checknumber(L, 1)));
  return 1;
}


#elif defined(__linux__) || defined(__SOLARIS) || defined(__APPLE__) || defined(__OS2__)

#ifdef getstr
#undef getstr
#endif

#include <curses.h>  /* or <ncurses.h> on some systems */
/* Created by gemini.goggle.com. Put into the public domain. 5.3.2 */
static int os_shellcolour (lua_State * L) {
  int max_colors = 0;
  initscr();
  /* check if the terminal supports colors, if it does, get the maximum number of colors max_colors = COLORS; */
  if (has_colors() == TRUE) max_colors = COLORS;  /* 5.4.1 fix */
  lua_pushinteger(L, max_colors);
  endwin();
  return 1;
}
#endif

/* Written mostly by Google's Gemini AI, available at gemini.google.com. 5.2.4

   The function reads out the current attributes of the Windows console and returns the data in the following fields:
   dwSize: size of the console screen buffer, in character columns and rows.
   dwCursorPosition: column and row coordinates of the cursor in the console screen buffer.
   wAttributes: attributes of the characters written to a screen buffer.
   srWindow: the console screen buffer coordinates of the upper-left and lower-right corners of the display window.
   dwMaximumWindowSize: maximum size of the console window, in character columns and rows.
   inCp: input codepage (0 on error)
   outCp: output codepage (0 on error)
   The function is a binding to the underlying Win32 C API functions GetConsoleScreenBufferInfo(), GetConsoleCP() and
   GetConsoleOutputCP(), thus it is available in Windows only.

   See also: os.codepage, os.getlocal, os.setlocale, os.vga. */

/* Function to push a COORD structure as a Lua table */
#ifdef _WIN32
static void push_coord (lua_State *L, COORD coord) {
  luaL_checkstack(L, 3, "not enough stack space");  /* 5.3.2 fix */
  lua_createtable(L, 0, 2);          /* Create a new table for COORD { } */
  lua_pushstring(L, "X");            /* Push key "X" */
  lua_pushinteger(L, coord.X);       /* Push value coord.X */
  lua_settable(L, -3);               /* table.X = coord.X */
  lua_pushstring(L, "Y");            /* Push key "Y" */
  lua_pushinteger(L, coord.Y);       /* Push value coord.Y */
  lua_settable(L, -3);               /* table.Y = coord.Y */
}
#else
static void push_coord (lua_State *L, int width, int height) {
  luaL_checkstack(L, 3, "not enough stack space");
  lua_createtable(L, 0, 2);          /* Create a new table for COORD { } */
  lua_pushstring(L, "X");            /* Push key "X" */
  lua_pushinteger(L, width);         /* Push value coord.X */
  lua_settable(L, -3);               /* table.X = coord.X */
  lua_pushstring(L, "Y");            /* Push key "Y" */
  lua_pushinteger(L, height);        /* Push value coord.Y */
  lua_settable(L, -3);               /* table.Y = coord.Y */
}
#endif

#ifdef _WIN32
/* Function to push a SMALL_RECT structure as a Lua table */
static void push_small_rect (lua_State *L, SMALL_RECT rect) {
  luaL_checkstack(L, 3, "not enough stack space");  /* 5.3.2 fix */
  lua_createtable(L, 0, 4);         /* Create a new table for SMALL_RECT { } */
  lua_pushstring(L, "Left");        /* Push key "Left" */
  lua_pushinteger(L, rect.Left);    /* Push value rect.Left */
  lua_settable(L, -3);              /* table.Left = rect.Left */
  lua_pushstring(L, "Right");       /* Push key "Right" */
  lua_pushinteger(L, rect.Right);   /* Push value rect.Right */
  lua_settable(L, -3);              /* table.Right = rect.Right */
  lua_pushstring(L, "Top");         /* Push key "Top" */
  lua_pushinteger(L, rect.Top);     /* Push value rect.Top */
  lua_settable(L, -3);              /* table.Top = rect.Top */
  lua_pushstring(L, "Bottom");      /* Push key "Bottom" */
  lua_pushinteger(L, rect.Bottom);  /* Push value rect.Bottom */
  lua_settable(L, -3);              /* table.Bottom = rect.Bottom */
}

static int os_shellinfo (lua_State *L) {
  HANDLE hConsole;
  CONSOLE_SCREEN_BUFFER_INFO info;
  hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hConsole == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(hConsole, &info)) {
    lua_pushnil(L);
    return 1;
  }
  luaL_checkstack(L, 3, "not enough stack space");  /* 5.3.3 fix */
  lua_createtable(L, 0, 7);
  lua_pushstring(L, "dwSize");
  push_coord(L, info.dwSize);
  lua_settable(L, -3);  /* table.dwSize = {x=..., x=...} */
  lua_pushstring(L, "dwCursorPosition");
  push_coord(L, info.dwCursorPosition);
  lua_settable(L, -3);  /* table.dwCursorPosition = {x=..., x=...} */
  lua_pushstring(L, "wAttributes");
  lua_pushinteger(L, info.wAttributes);
  lua_settable(L, -3);  /* table.wAttributes = number */
  lua_pushstring(L, "srWindow");
  push_small_rect(L, info.srWindow);
  lua_settable(L, -3);  /* table.srWindow = {left=..., top=..., right=..., bottom=...} */
  lua_pushstring(L, "dwMaximumWindowSize");
  push_coord(L, info.dwMaximumWindowSize);
  lua_settable(L, -3);  /* table.dwMaximumWindowSize = {x=..., x=...} */
  /* Retrieve the input codepage */
  lua_pushstring(L, "inCp");
  lua_pushinteger(L, GetConsoleCP());
  lua_settable(L, -3);
  /* Retrieve the output codepage */
  lua_pushstring(L, "outCp");
  lua_pushinteger(L, GetConsoleOutputCP());
  lua_settable(L, -3);
  return 1;
}
#elif defined(__linux__) || defined(__SOLARIS) || defined(__APPLE__) || defined(__DJGPP__) || defined(__OS2__)
/* Created by gemini.goggle.com. Put into the public domain. 5.3.2 */
static int os_shellinfo (lua_State *L) {
  int width, height;
  if (tools_shellgeom(&width, &height) == -1) {
    lua_pushnil(L);
    return 1;
  }
  luaL_checkstack(L, 3, "not enough stack space");  /* 5.3.3 security fix */
  lua_createtable(L, 0, 1);
  lua_pushstring(L, "dwMaximumWindowSize");
  push_coord(L, width, height);
  lua_settable(L, -3);
  return 1;
}
#endif


static int os_shellgeom (lua_State *L) {
  int width, height;
  luaL_checkstack(L, 2, "not enough stack space");
  if (tools_shellgeom(&width, &height) == -1) {
    lua_pushnil(L);
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, width);
    lua_pushinteger(L, height);
  }
  return 2;
}


static const luaL_Reg syslib[] = {
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__)
  {"alarm",     os_alarm},
#endif
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(_WIN32)
  {"symlink",   os_symlink},          /* 1.12.1, added on June 15, 2013 */
  {"readlink",  os_readlink},         /* 1.12.1, added on June 17, 2013 */
#endif
  {"battery",   os_battery},          /* 0.14.0, added on March 13, 2009 */
  {"beep",      os_beep},             /* added on March 31, 2007 */
  {"bios",      os_bios},             /* added on July 31, 2026 */
  {"cachelinesize", os_cachelinesize},  /* added September 01, 2025 */
  {"cdrom",     os_cdrom},            /* added 2.3.3, November 25, 2014 */
  {"chdir",     os_chdir},            /* added July 07, 2008 */
#if (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__)
  {"chmod",     os_chmod},            /* added December 27, 2016 */
  {"chown",     os_chown},            /* added December 27, 2016 */
#endif
  {"clock",     os_clock},            /* added July 23, 2018 */
#if defined(_WIN32) || defined(__DJGPP__) || defined(__OS2__)
  {"codepage",  os_codepage},         /* 2.9.7, January 07, 2016 */
#endif
  {"computername", os_computername},  /* 0.5.3  Sept 15, 2007 */
  {"countcore", os_countcore},        /* added on March 29, 2025 */
  {"cpuinfo",   os_cpuinfo},          /* added on February 07, 2013 */
  {"cpuid",     os_cpuid},            /* added on December 24, 2018 */
  {"cpuload",   os_cpuload},          /* added on January 31, 2013 */
  {"curdir",    os_curdir},           /* added 2.17.3, February 28, 2020 */
  {"curdrive",  os_curdrive},         /* added 2.3.3, November 30, 2014 */
  {"date",      os_date},
  {"datetosecs", os_datetosecs},      /* added September 20, 2012 */
  {"difftime",  os_difftime},
  {"dirname",   os_dirname},          /* added January 11, 2017 */
  {"drives",    os_drives},           /* 0.26.0, 05.08.2009 */
  {"drivestat", os_drivestat},        /* 0.26.0, 05.08.2009 */
  {"endian",    os_endian},           /* 0.8.0, added on December 11, 2007 */
  {"environ",   os_environ},
  {"esd",       os_esd},              /* 2.9.8  June 08, 2016 */
  {"execute",   os_execute},          /* changed in 0.5.4 */
  {"exists",    os_exists},           /* added on April 09, 2007 */
  {"exit",      os_exit},
  {"faccess",   os_faccess},          /* added on September 30, 2020 */
  {"fattrib",   os_fattrib},          /* 0.26.0, added August 06, 2009 */
  {"fcopy",     os_fcopy},            /* added on July 19, 2009 */
  {"filename",  os_filename},         /* added on January 16, 2017 */
  {"freemem",   os_freemem},          /* 0.5.3  Sept 15, 2007 */
  {"fstat",     os_fstat},            /* added on October 11, 2008 */
  {"ftok",      os_ftok},             /* added on January 17, 2023 */
  {"getadapter", os_getadapter},      /* 2.39.3, May 16, 2023 */
  {"getcommandline", os_getcommandline},  /* 7.8.1, July 31, 2026 */
#ifdef _WIN32
  {"getconsolemode", os_getconsolemode},  /* 5.6.1, September 18, 2025 */
#endif
  {"getdirpathsep", os_getdirpathsep},  /* 2.39.12, 08.06.2023 */
  {"getenv",    os_getenv},
  {"getip",     os_getip},            /* Added April 29, 2023 */
#ifdef _WIN32
  {"getfullpathname", os_getfullpathname}, /* 7.6.3, June 18, 2026 */
  {"getlanguage", os_getlanguage},     /* 2.35.4, January 17, 2016 */
  {"getprocesses", os_getprocesses},   /* 3.5.5, August 06, 2025 */
  {"getusbdevices", os_getusbdevices}, /* 3.5.5, August 06, 2025 */
#endif
  {"getloadeddlls", os_getloadeddlls}, /* 2.4.3, August 30, 2023 */
  {"getlocale",  os_getlocale},        /* 2.10.0, December 09, 2016 */
  {"getmac",     os_getmac},           /* Added April 29, 2023 */
  {"getmodulefilename", os_getmodulefilename},  /* September 01, 2019 */
  {"gettemppath", os_gettemppath},     /* November 25, 2019 */
  {"getthreadid", os_getthreadid},     /* September 21, 2025 */
  {"getthreadseed", os_getthreadseed}, /* September 21, 2025 */
  {"getwinsysdirs", os_getwinsysdirs}, /* August 30, 2023 */
  {"hasnetwork", os_hasnetwork},       /* added December 29, 2014 */
#if ((LONG_MAX == 2147483647L) && !defined(__APPLE__)) && ((defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__) )
  {"hassse",     os_hassse},           /* 2.14.13, May 04, 2019 */
#endif
  {"inode",      os_inode},            /* 2.21.5, January 17, 2023 */
  {"is386",      os_is386},            /* 3.4.2, August 28, 2023 */
  {"isansi",     os_isANSI},           /* added 0.28.2, 17.11.2009, renamed 2.16.14 */
  {"isarm",      os_isarm},            /* 2.21.5, July 08, 2020 */
  {"isarm32",    os_isarm32},          /* 3.10.5, February 17, 2024 */
  {"isarm64",    os_isarm64},          /* 3.10.5, February 17, 2024 */
  {"isdir",      os_isdir},            /* added January 11, 2017 */
  {"isdocked",   os_isdocked},         /* 2.4.0, December 29, 2014 */
  {"isdos",      os_isdos},            /* 2.16.14, January 29, 2020 */
  {"isdow",      os_isdow},            /* 2.39.12, June 08, 2023 */
  {"isdriveletter", os_isdriveletter}, /* 2.39.12, June 08, 2023 */
  {"isdst",      os_isdst},            /* 2.9.8, June 08, 2016 */
  {"isfile",     os_isfile},           /* added January 11, 2017 */
  {"islink",     os_islink},           /* added January 11, 2017 */
  {"islinux",    os_islinux},          /* 3.4.1, August 25, 2023 */
  {"islinux386", os_islinux386},       /* 3.4.2, August 28, 2023 */
  {"islocale",   os_islocale},         /* added April 27, 2019 */
  {"ismac",      os_ismac},            /* 3.4.1, August 24, 2023 */
  {"ismounted",  os_ismounted},        /* added 2.3.3, November 25, 2014 */
  {"isopensuse", os_isopensuse},       /* 6.3.7, November 04, 2025 */
  {"isos2",      os_isos2},            /* 2.4.0, January 09, 2015 */
  {"isppc",      os_isppc},            /* 2.21.5, July 08, 2020 */
  {"israspi",    os_israspi},          /* added 3.17.3, June 19, 2024 */
  {"isremovable", os_isremovable},     /* added 2.3.3, November 25, 2014 */
  {"issolaris",  os_issolaris},        /* 3.17.3, June 19, 2024 */
  {"issysdir",   os_issysdir},         /* added 2.39.8, May 24, 2023 */
  {"isvaliddrive", os_isvaliddrive},   /* added 2.3.3, November 25, 2014 */
  {"isvalidpath", os_isvalidpath},     /* 5.2.3, July 21, 2025 */
  {"iswindows",  os_iswindows},        /* 2.16.14, January 29, 2020 */
  {"isx86",      os_isx86},            /* 2.21.5, July 08, 2020 */
  {"iterate",    os_iterate},          /* 2.17.1, February 09, 2020 */
  {"listcore",   os_listcore},         /* added on August 01, 2007 */
  {"login",      os_login},            /* 0.5.3  Sept 15, 2007 */
  {"lsd",        os_lsd},              /* 2.10.0  September 01, 2016 */
#if defined(_WIN32) || defined(__unix__) || defined(__DJGPP__) || defined(__OS2__) || defined(__HAIKU__)  || defined(__APPLE__)
  {"memstate",   os_memstate},         /* 0.5.3  Sept 15, 2007 */
  {"meminfo",    os_memstate},         /* 2.26.2 alias */
#endif
  {"mkdir",      os_mkdir},            /* added July 07, 2008 */
  {"mklink",     os_mklink},           /* added July 04, 2023 */
  {"monitor",    os_monitor},          /* added December 29, 2014 */
  {"mouse",      os_mouse},            /* added December 29, 2014 */
#if defined(_WIN32) || defined(__DJGPP__) || defined(__OS2__)
  {"mousestate", os_mousestate},       /* 2.17.3, February 28, 2020 */
#endif
  {"move",       os_move},
#ifdef _WIN32
  {"netdomain",  os_netdomain},        /* added April 11, 2022 */
  {"netsend",    os_netsend},          /* added April 11, 2022 */
  {"netuse",     os_netuse},           /* added April 11, 2022 */
#endif
  {"now",        os_now},              /* added September 20, 2012 */
  {"mouseflush", os_mouseflush},       /* added March 2020 */
#if defined(__OS2__)
  {"mouseopen",  os_mouseopen},        /* added March 2020 */
  {"mouseclose", os_mouseclose},       /* added March 2020 */
  {"os2info",    os_os2info},          /* added August 22, 2019 */
  {"getextlibpath", os_getextlibpath}, /* added March 06, 2020 */
  {"setextlibpath", os_setextlibpath}, /* added March 06, 2020 */
#endif
  {"pause",      os_pause},            /* July 23, 2018 */
  {"period",     os_period},           /* February 08, 2025 */
  {"pid",        os_pid},              /* added February 02, 2012 */
  {"realpath",   os_realpath},         /* added September 22, 2015 */
  {"remove",     os_remove},
  {"rmdir",      os_rmdir},            /* added July 07, 2008 */
  {"screensize", os_screensize},       /* added March 28, 2010 */
  {"sdlcpuinfo", os_sdlcpuinfo},       /* added September 06, 2025 */
  {"searchpath", os_searchpath},       /* added April 11, 2022 */
  {"secstodate", os_secstodate},       /* added September 20, 2012 */
#ifdef _WIN32
  {"setconsolemode", os_setconsolemode},  /* 5.6.1, September 18, 2025 */
#endif
  {"setenv",     os_setenv},           /* added October 24, 2010 */
  {"setlocale",  os_setlocale},        /* added on August 14, 2025 */
#ifdef _WIN32
  {"setthreadlocale", os_setthreadlocale},  /* Added August 14, 2025 */
#endif
  {"settime",    os_settime},          /* added September 24, 2012 */
#if defined(_WIN32) || defined(__linux__) || defined(__SOLARIS) || defined(__APPLE__) || defined(__OS2__)
  {"shellcolour", os_shellcolour},     /* added July 22, 2025 */
#endif
  {"shellgeom",  os_shellgeom},        /* August 03, 2025 */
  {"shellinfo",  os_shellinfo},        /* added July 22, 2025 */
  {"strerror",   os_strerror},         /* 2.8.6, September 22, 2015 */
  {"system",     os_system},           /* 0.12.2  October 12, 2008 */
  {"terminate",  os_terminate},        /* 2.4.0, added December 29, 2014 */
  {"ticker",     os_ticker},           /* February 08, 2025 */
  {"time",       os_time},
  {"timestamp",  os_timestamp},        /* February 08, 2025 */
  {"tmpdir",     os_tmpdir},           /* added June 04, 2022 */
  {"tmpname",    os_tmpname},
  {"tzdiff",     os_tzdiff},           /* 2.9.8, August 08, 2016 */
  {"unmount",    os_unmount},          /* December 06, 2015 */
  {"uptime",     os_uptime},           /* November 25, 2014 */
  {"usd",        os_usd},              /* June 10, 2016 */
  {"vendor",     os_vendor},           /* added on September 21, 2025 */
  {"vga",        os_vga},              /* added on December 29, 2014 */
  {"winver",     os_winver},           /* added on November 26, 2019 */
  {"wait",       os_wait},             /* added on March 31, 2007 */
  {"xgetbv",     os_xgetbv},           /* 5.5.9, September 07, 2025 */
  {NULL, NULL}
};

/* }====================================================== */


LUALIB_API int luaopen_os (lua_State *L) {
  /* ticker must be initialised at startup, else the first call to os.ticker will always return zero in DOS */
#ifdef __DJGPP__
  aux_timestamp();
#endif
  luaL_newmetatable(L, "diriterator");        /* metatable for diriterator userdata */
  luaL_register(L, NULL, os_diriteratorlib);  /* assign C metamethods to this metatable */
  luaL_register(L, LUA_OSLIBNAME, syslib);
  /* The the following access() mode constants have different values over operating systems, so define them here */
  lua_pushinteger(L, R_OK);
  lua_setfield(L, -2, "r_ok");  /* read access */
  lua_pushinteger(L, W_OK);
  lua_setfield(L, -2, "w_ok");  /* write access */
  lua_pushinteger(L, X_OK);
  lua_setfield(L, -2, "x_ok");  /* execute permission */
  lua_pushinteger(L, F_OK);
  lua_setfield(L, -2, "f_ok");  /* existence of file */
  return 1;
}

