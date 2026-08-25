/*
** $Id: lua.c,v 1.160 2006/06/02 15:34:00 roberto Exp $
** Lua stand-alone interpreter
** See Copyright Notice in agena.h
*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>   /* for memory status */
#endif

#if defined(__unix__) || defined (__HAIKU__)
#include <unistd.h>   /* for memory status */
#endif

#ifdef __OS2__
#define INCL_DOS
#include <os2.h>     /* for memory status query */
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#endif

#include <limits.h>    /* for PATH_MAX length */

#define lua_c

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"
#include "agnconf.h"
#include "agnhlps.h"
#include "llex.h"
#include "lstate.h"

static lua_State *globalL = NULL;

static const char *progname = LUA_PROGNAME;


static void lstop (lua_State *L, lua_Debug *ar) {
  (void)ar;  /* unused arg. */
  lua_sethook(L, NULL, 0, 0);
  luaL_error(L, "Execution interrupted.");  /* 2.0.0 RC 2 */
}


static void laction (int i) {
  signal(i, SIG_IGN);
  lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
  signal(i, SIG_DFL);
}


static const char *lmemfind (const char *s1, size_t l1, const char *s2, size_t l2) {
  if (l2 == 0) return s1;  /* empty strings are everywhere */
  else if (l2 > l1 || s1 == NULL) return NULL;  /* avoids a negative `l1' or end of string, 2.28.5 patch */
  else {
    const char *init;  /* to search for a `*s2' inside `s1' */
    l2--;  /* 1st char will be checked by `memchr' */
    l1 = l1 - l2;  /* `s2' cannot be found after that */
    while (l1 > 0 && (init = (const char *)memchr(s1, *s2, l1)) != NULL) {
      init++;  /* 1st char is already checked */
      if (memcmp(init, s2 + 1, l2) == 0)  /* compare the rest of the characters */
        return init - 1;
      else {  /* correct `l1' and `s1' to try again */
        l1 -= init - s1;
        s1 = init;
      }
    }
    return NULL;  /* not found */
  }
}


static void print_usage (int symbol) {
  fprintf(stderr,
  "Usage: %s [options] [script [arguments]]\n\n"
  "Available options are:\n"
  "  %ca         ignore AGENAPATH environment variable\n"
  "  %cd         print debugging information during startup and within a session\n"
  "  %ce \"stat\"  execute statement \"stat\" (double quotes needed)\n"
/* At least in Windows with Clink 0.4.9, the question mark is being expanded by the shell before executing main(). */
  "  %ch, %c?     display this help\n"
  "  %ci         enter interactive mode after executing " LUA_QL("script") " or other options\n"
  "  %cl         print licence\n"
  "  %cm         print the amount of free RAM\n"
#if !defined(__unix__) && !defined(__APPLE__) && !defined(__HAIKU__)
  "  %cn         do not run initialisation file(s) `agena.ini`\n",
#else
  "  %cn         do not run initialisation file(s) `.agenainit`\n",
#endif
  progname, symbol, symbol, symbol, symbol, symbol, symbol, symbol, symbol, symbol);
  fprintf(stderr,
  "  %cp path    set libname to <path>, overriding default libname initialisation\n"
  "  %cb         print strings in backquotes\n"
  "  %cq         print strings in single quotes\n"
  "  %cQ         print strings in double quotes\n"
  "  %cr name    readlib library <name> (no quotes needed)\n"
  "  %cs \"text\"  print the slogan \"text\" at start-up (double quotes needed)\n"
  "  %cv         show version and compilation time information\n"
  "  %cx         do not run main library file lib/library.agn\n"
  "  %cB         throw syntax error when numeric constants are too big\n"
  "  %cC         allow constants to be overwritten\n"
  "  %cD number  set number of digits in output of floats to <number> (1 to 17)\n"
  "  %cS         do not print start-up copyright message\n"
  "  %cW         turn warnings on\n"
  "  --         stop handling options\n"
  "  -          execute stdin and stop handling options\n"
  "\n"
  "Type `bye` to quit or press CTRL+C.\n",
  symbol, symbol, symbol, symbol, symbol, symbol, symbol, symbol, symbol, symbol, symbol, symbol, symbol);
  fflush(stderr);
}


/*
** Prints an error message, adding the program name in front of it (if present).
*/
static void l_message (const char *pname, const char *msg) {
  if (pname) lua_writestringerror("%s: ", pname);
  lua_writestringerror("%s\n", msg);
}


static int report (lua_State *L, int status) {
  if (status && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    l_message(progname, msg);
    agn_poptop(L);
  }
  return status;
}


static int traceback (lua_State *L) {
  lua_getfield(L, LUA_GLOBALSINDEX, "debug");
  if (!lua_istable(L, -1)) {
    agn_poptop(L);
    return 1;
  }
  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    agn_poptoptwo(L);
    return 1;
  }
  lua_pushvalue(L, 1);  /* pass error message */
  lua_pushinteger(L, 2);  /* skip this function and traceback */
  lua_call(L, 2, 1);  /* call debug.traceback */
  return 1;
}


static int docall (lua_State *L, int narg, int clear) {
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, traceback);  /* push traceback function */
  lua_insert(L, base);  /* put it under chunk and args */
  signal(SIGINT, laction);
  status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
  signal(SIGINT, SIG_DFL);
  lua_remove(L, base);  /* remove traceback function */
  /* force a complete garbage collection in case of errors */
  if (status != 0) lua_gc(L, LUA_GCCOLLECT, 0);
  return status;
}

/* added 0.5.4 */

#define MAX_ITEM   512

/* print an `integer` without decimals with thousand separators; for positive numbers only */

static char *nformat (double dv, char thousands) {
  int i, c;
  unsigned long v;
  char digit, *result, *fmt;
  fmt = (char *)malloc(MAX_ITEM*sizeof(char));
  if (fmt == NULL) return NULL;
  v = (unsigned long) ((dv * 100.0) + .5);
  if (dv < 0) return NULL;
  i = -2; c = 0;
  do {
    if ((i > 0) && (!(i % 3))) {
      fmt[c] = thousands;
      c++;
    }
    digit = (v % 10) + '0';
    if (i > -1) {  /* skip decimals */
      fmt[c] = digit;
      c++;
    }
    v /= 10;
    i++;
  } while((v) || (i < 1));
  fmt[c] = '\0';
  result = (char *)malloc((c + 1)*sizeof(char));  /* Agena 1.6.0, Valgrind fix */
  if (result == NULL) return NULL;  /* Agena 1.0.4 */
  i = 0;
  /* the string is `vice versa`, so reverse the string */
  while (i < c) {
    result[i] = fmt[c - i - 1];
    i++;
  }
  result[i] = '\0';
  xfree(fmt);
  return result;
}


static void print_memorystatus (void) {
  char *s = NULL;
#if defined(_WIN32)
  MEMORYSTATUS memstat;
  GlobalMemoryStatus(&memstat);
  s = nformat(memstat.dwAvailPhys/1024, '\'');
  if (s == NULL) return;
  fprintf(stderr, "%s KBytes of physical RAM free.\n\n", s);
#elif defined(__unix__) && !defined(__DJGPP__) || defined(__HAIKU__)
  s = nformat(sysconf(_SC_AVPHYS_PAGES)*sysconf(_SC_PAGESIZE)/1024, '\'');
  if (s == NULL) return;
  fprintf(stderr, "%s KBytes of physical RAM free.\n\n", s);
#elif defined(__OS2__)
  APIRET result;
  ULONG memInfo[3];
  result = DosQuerySysInfo(QSV_TOTPHYSMEM, QSV_TOTAVAILMEM,
    &memInfo[0], sizeof(memInfo));
  if (result == 0) {
    s = nformat((lua_Number)memInfo[2]/1024, '\'');
    if (s == NULL) return;
    fprintf(stderr, "%s KBytes of virtual memory free.\n\n", s);
  }
#elif defined(__APPLE__)
  vm_size_t pagesize;
  vm_statistics_data_t vminfo;
  mach_port_t system;
  unsigned int c = HOST_VM_INFO_COUNT;
  system = mach_host_self();
  if (host_page_size(mach_host_self(), &pagesize) != KERN_SUCCESS) pagesize = 4096;
  if (host_statistics(system, HOST_VM_INFO, (host_info_t)(&vminfo), &c) == KERN_SUCCESS) {
    s = nformat((uint64_t)(vminfo.free_count - vminfo.speculative_count) * pagesize/1024, '\'');
    if (s == NULL) return;
    fprintf(stderr, "%s KBytes of physical RAM free.\n\n", s);
  }
#endif
  if (s) { xfree(s); }  /* 4.10.5 fix */
}


#define l_colourlogo() { \
  l_message(NULL, \
    lua_pushfstring(L, \
    "\n" "\033[31m" AGENA_NAME " \033[36m" AGENA_LOGO " \033[31m" AGENA_EDITION " \033[36m%s\033[00m %s %s " AGENA_LICENCE, \
    AGENA_MOTTO, pl, leg) \
  ); \
}

#define l_noncolourlogo() { \
  l_message(NULL, lua_pushfstring(L, "\n" AGENA_RELEASE " %s %s %s " AGENA_LICENCE, AGENA_MOTTO, pl, leg)); \
}

static void print_version (lua_State *L, int mode) {  /* Agena 1.6.12 */
  const char *pl = "\b";
  const char *leg = "\b";
  /* #endif */
  agnL_gettablefield(L, "environ", "libpatchlevel", NULL, 0);
  if (lua_isnumber(L, -1)) {
    lua_pushstring(L, "update ");
    lua_pushstring(L, lua_tostring(L, -2));
    lua_concat(L, 2);
    pl = agn_checkstring(L, -1);
    lua_remove(L, -2);  /* remove patchlevel number */
  }
  if (mode == 1) {
#ifdef _WIN32
    struct WinVer winversion = {0};  /* 7.7.11 invalid read fix */
    if (getWindowsVersion(&winversion) >= MS_WIN10) {
      /* Gemini AI, 5.6.1 extension for older Win 10 flavours such as 22H2:
      1. Get the console output handle */
      HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
      /* 2. Variable to store the original mode */
      DWORD dwOriginalMode = 0;
      /* 3. Save the original console mode */
      if (GetConsoleMode(hOut, &dwOriginalMode)) {
        /* 4. Calculate the new mode: original mode OR'd with the VT processing flag */
        DWORD dwNewMode = dwOriginalMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        /* 5. Set the new mode (Enable VT processing) */
        if (SetConsoleMode(hOut, dwNewMode)) {
          l_colourlogo();
          SetConsoleMode(hOut, dwOriginalMode);
          goto exitlabel;
        }
      }
    }
    /* consolemode failed or Windows version < Win 10 */
    l_noncolourlogo();
#elif defined(__linux__) || defined(__SOLARIS) || defined(__APPLE__)
    l_colourlogo();
#else
    l_noncolourlogo();
#endif
  } else {
    l_message(NULL,
      lua_pushfstring(L, AGENA_RELEASE " %s %s %s " AGENA_COPYRIGHT " at " AGENA_BUILDTIME, AGENA_MOTTO, pl, leg));
  }
#ifdef _WIN32  /* to avoid compiler warnings */
exitlabel:
#endif
  agn_poptoptwo(L);  /* drop fstring and (patchlevel string or anything else) */
}


static int getargs (lua_State *L, char **argv, int n) {
  int narg;
  int i;
  int argc = 0;
  while (argv[argc]) argc++;  /* count total number of arguments */
  narg = argc - (n + 1);  /* number of arguments to the script */
  luaL_checkstack(L, narg + 3, "too many arguments to script");
  for (i=n + 1; i < argc; i++)
    lua_pushstring(L, argv[i]);
  lua_createtable(L, narg, n + 1);
  for (i=0; i < argc; i++) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i - n);
  }
  return narg;
}


static int dofile (lua_State *L, const char *name) {
  int status;
  status = luaL_loadfile(L, name) || docall(L, 0, 1);
  return report(L, status);
}


static int dostring (lua_State *L, const char *s, const char *name) {
  int status = luaL_loadbuffer(L, s, strlen(s), name) || docall(L, 0, 1);
  return report(L, status);
}


static int dolibrary (lua_State *L, const char *name) {
  lua_getglobal(L, "readlib");
  lua_pushstring(L, name);
  return report(L, docall(L, 1, 1));  /* 5.1.3 patch */
}


static int setpath (lua_State *L, char *name) {
  str_charreplace(name, '\\', '/', 1);
  lua_pushstring(L, name);  /* Agena 1.6.7 */
  lua_setglobal(L, "libname");
  return 0;
}


/* ------------------------------------------------------------------------ */


static const char *get_prompt (lua_State *L, int firstline) {
  const char *p;
  lua_getfield(L, LUA_GLOBALSINDEX, firstline ? "_PROMPT" : "_PROMPT2");
  p = lua_tostring(L, -1);
  /* avoid printing `null' at the prompt, 4.10.0 */
  if (p == NULL || !strcmp(p, (agn_gettoken(L, TK_NULL)))) p = (firstline ? LUA_PROMPT : LUA_PROMPT2);
  agn_poptop(L);  /* remove global */
  return p;
}


static int incomplete (lua_State *L, int status) {
  if (status == LUA_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = lua_tolstring(L, -1, &lmsg);
    const char *tp = msg + lmsg - (sizeof(LUA_QL("<eof>")) - 1);
    if (strstr(msg, LUA_QL("<eof>")) == tp) {
      agn_poptop(L);
      return 1;
    }
  }
  return 0;  /* else... */
}


static int iscomplete (lua_State *L, int status) {  /* 3.4.10, just check w/o modifying the stack */
  if (status == LUA_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = lua_tolstring(L, -1, &lmsg);
    const char *tp = msg + lmsg - (sizeof(LUA_QL("<eof>")) - 1);
    if (strstr(msg, LUA_QL("<eof>")) == tp) {
      return 0;
    }
  }
  return 1;  /* else... */
}


static int pushline (lua_State *L, int firstline) {
  char buffer[LUA_MAXINPUT];
  char *b = buffer;
  size_t l, newl;
  const char *prmt = get_prompt(L, firstline);
  if (lua_readline(L, b, prmt) == 0)
    return 0;  /* no input */
  l = strlen(b);
  newl = l;
  if (l > 0 && b[l - 1] == '\n') newl--;  /* 1.6.0 Valgrind */
  while (newl > 0 && b[newl - 1] == ' ') newl--;  /* 1.6.0 Valgrind */
  if (l > 0 && b[l - 1] == '\n')  /* line ends with newline? */
    b[l - 1] = '\0';  /* remove it */
  lua_pushstring(L, b);
  lua_freeline(L, b);
  return 1;
}


#define ASSIGNTOKEN      (agn_gettoken(L, TK_ASSIGN))
#define ASSIGNTOKENSIZE  (strlen(ASSIGNTOKEN))

static int loadline (lua_State *L) {
  int status, assignfound, iscolon, isdoublecolon;
  size_t l;
  char *input;
  const char *pos;
  lua_settop(L, 0);
  if (!pushline(L, 1))
    return -1;  /* no input */
  for (;;) {  /* repeat until gets a complete line, extended to replace dumb `colon` feature, 3.4.9 */
    input = (char *)lua_tolstring(L, -1, &l);
    while (l > 0 && input[l - 1] == ' ') l--;
    isdoublecolon = (l > 1) ? input[l - 2] == ':' : 0;  /* make sure line does not end in `::` ... 3.4.10 */
    iscolon = !isdoublecolon && l > 0 && input[l - 1] == ':';
    if (iscolon && l > 1) {
      pos = lmemfind(input, --l, ASSIGNTOKEN, ASSIGNTOKENSIZE);
      assignfound = (pos != NULL);
      luaL_checkstack(L, 2 + assignfound, "not enough stack space");
      if (assignfound) {  /* we have an assignment statement */
        lua_pushlstring(L, input, l);
        l -= (char *)pos + 2 - input;
        lua_pushlstring(L, (char *)input, pos - input);  /* fetch the variable name of the assignment, 3.9.0 fix */
        lua_pushfstring(L, "%s; return %s", agn_tostring(L, -2), agn_tostring(L, -1));
        lua_remove(L, -2);  /* remove intermediate string */
        lua_remove(L, -2);  /* ditto */
      } else {  /* we have an expression or a statement */
        lua_pushstring(L, "return ");
        lua_pushlstring(L, input, l);
        lua_concat(L, 2);
      }
      lua_remove(L, -2);  /* remove original input */
    }
    status = luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
    if (iscolon && !iscomplete(L, status))  /* we are not yet complete */
      agn_poptop(L);  /* drop tentative, incomplete string, 3.4.10 */
    if (!incomplete(L, status)) {  /* we are complete, 3.4.10 */
      if (iscolon && status == LUA_ERRSYNTAX) {
        /* syntax error: prevent misleading error message in call to `report`, 3.4.10 */
        char *errmsg = (char *)lua_tolstring(L, -1, &l);
        if (lmemfind(errmsg, l, "Error at line", 13) && (pos = lmemfind(errmsg, l, ": ", 2)) ) {
          /* reuse part of the original error message */
          lua_pushlstring(L, errmsg, pos - errmsg + 1);
          lua_pushstring(L, " unexpected symbol near `:`");
          lua_concat(L, 2);
        } else
          lua_pushstring(L, "Error: invalid or ambiguous syntax near `:`");
      }
      if (agn_gethistory(L) && status != LUA_ERRSYNTAX) {
        FILE *fh = fopen(agn_gethistfile(L), "a+");
        if (fh) {
          fprintf(fh, "%s", agn_tostring(L, 1));  /* 5.2.5 fix */
          fprintf(fh, "\n\n");
          fflush(fh);
          if (fclose(fh)) {
            fprintf(stderr, "Warning, could not close history file.\n");
            agn_sethistory(L, 0);  /* 5.2.3 extension */
          }
        } else {
          fprintf(stderr, "Warning, the logfile name seems to be invalid.\n");
          agn_sethistory(L, 0);  /* 5.2.3 extension */
        }
      }
      break;  /* cannot try to add lines? */
    }
    if (!pushline(L, 0)) return -1;  /* no more input? */
    lua_pushliteral(L, "\n");  /* add a new line... */
    lua_insert(L, -2);  /* ...between the two lines */
    lua_concat(L, 3);  /* join them */
  }
  lua_saveline(L, 1);
  lua_remove(L, 1);  /* remove line */
  return status;
}


static void dotty (lua_State *L) {
  int status;
  const char *oldprogname = progname;
  progname = NULL;
  while ((status = loadline(L)) != -1) {
    if (status == 0) status = docall(L, 0, 0);
    report(L, status);
    if (status == 0 && lua_gettop(L) > 0) {  /* any result to print? */
      if (agn_getpromptnewline(L) != 0) {  /* 1.7.6 */
        /* DO NOT use lua_isboolean since it checks a value on the stack and not the return value of agn_gettablefield (a nonneg int) */
        /* a newline is printed at the console after entering a statement */
        fputs("\n", stdout);
      }
      /* save last value printed to `ans` */
      if (!lua_isnil(L, 1)) {  /* Agena 1.6.6, via but not due to Valgrind */
        lua_pushvalue(L, 1);
        lua_setglobal(L, "ans");  /* pops the value from the stack */
      }
      lua_getglobal(L, "print");
      lua_insert(L, 1);
      if (lua_pcall(L, lua_gettop(L)-1, 0, 0) != 0)
        l_message(progname, lua_pushfstring(L,
                            "Error calling " LUA_QL("print") ": %s.",
                            lua_tostring(L, -1)));
    }
    /* a newline is printed at the console after entering a statement */
    if (agn_getemptyline(L)) fputs("\n", stdout);
  }
  lua_settop(L, 0);  /* clear stack */
  fputs("\n", stdout);
  fflush(stdout);
  progname = oldprogname;
}


static int handle_script (lua_State *L, char **argv, int n) {
  int status;
  const char *fname;
  int narg = getargs(L, argv, n);  /* collect arguments */
  lua_setglobal(L, "args");
  fname = argv[n];
  if (strcmp(fname, "-") == 0 && strcmp(argv[n - 1], "--") != 0)
    fname = NULL;  /* stdin */
  status = luaL_loadfile(L, fname);
  lua_insert(L, -(narg + 1));
  if (status == 0)
    status = docall(L, narg, 0);
  else
    lua_pop(L, narg);
  return report(L, status);
}


/* check that argument has no extra characters at the end */
#define notail(x)   {if ((x)[2] != '\0') return -1;}

#define has_error  1  /* bad option */

#define proceed(argv,i) { \
  if (argv[i][2] == '\0') { \
    i++; \
    if (argv[i] == NULL) return -1; \
  } \
}

static int collectargs (lua_State *L, char **argv, int *pi, int *pv, int *pe,
                        int *pnoini, int *pl, int *pp, int *pd, int *pm, int *ps,
                        int *pS, int *px, int *pa, int *pB, int *pC,
                        int *pq, int *pQ, int *pb,
                        int *symbol) {
  int i, flag;
  flag = 0;
  *symbol = (unsigned char)('\0');
  for (i=1; argv[i] != NULL; i++) {
    if (argv[i][0] != '-' && argv[i][0] != '/') { /* not an option? 2.34.9 extension */
      return i;
    } else if (!flag) {  /* 2.35.0 extension, fetch slash or hyphen */
      flag = 1;
      *symbol = (unsigned char)(argv[i][0]);
    }
    switch (argv[i][1]) {  /* option */
      case '-':  /* execute stdin and stop handling options */
        notail(argv[i]);
        return (argv[i + 1] != NULL ? i + 1 : 0);
      case '\0':
        return i;
      case '?':  /* 1.12.9, display help */
        return -1;
      case 'i':  /* enter interactive mode after executing `script` or other options */
        notail(argv[i]);
        *pi = 1;  /* go through */
        break;
      case 'n':  /* do not run initialisation file(s) `agena.ini` */
        *pnoini = 1;
        break;
      case 'x':  /* do not run main library file lib/library.agn */
        *px = 1;
        break;
      case 'd':  /* print debugging information during startup and within a session */
        *pd = 1;
        break;
      case 'm':  /* also print the amount of free RAM */
        *pm = 1;
        break;
      case 'v':  /* show version and compilation time information */
        notail(argv[i]);
        *pv = 1;
        break;
      case 'l':  /* print licence */
        notail(argv[i]);
        *pl = 1;
        break;
      case 'W':  /*  turn warnings on */
        lua_warning(L, "@on", 0);  /* warnings on, 4.10.5 change */
        break;
      case 'e':  /* execute statement "stat" (double quotes needed) */
        *pe = 1;  /* go through */
        proceed(argv, i);
        break;
      case 'a':   /* ignore AGENAPATH environment variable, 2.35.4 */
        *pa = 1;  /* go through */
        break;
      case 's':   /*  print the slogan "text" at start-up (double quotes needed), 4.10.5 fix */
        *ps = 1;
        proceed(argv, i);
        if (argv[i]) printf("%s\n", argv[i]);  /* 7.8.10 fix */
        break;
      case 'S':  /*  do not print start-up copyright message */
        *pS = 1;
        break;
      case 'B':  /* throw syntax error when numeric constants are too big */
        *pB = 1;
        break;
      case 'C':  /* allow constants to be overwritten */
        *pC = 1;
        break;
      case 'r':  /* readlib library <name> (no quotes needed) */
        proceed(argv, i);
        break;
      case 'p': {  /* sets libname to path, overriding the standard initialisation procedure for libname */
        char *path;
        *pp = 1;
        proceed(argv, i);
        path = argv[i];
        if (*path == '\0') path = argv[++i];
        lua_assert(path != NULL);
        setpath(L, path);
        break;
      }
      case 'b':  /* print strings in backquotes, 3.10.2 */
        *pb = 1;
        agn_setenclosebackquotes(L, 1);
        break;
      case 'q':  /* print strings in single quotes, 3.10.2 */
        *pq = 1;
        agn_setenclose(L, 1);
        break;
      case 'Q':  /* print strings in double quotes, 3.10.2 */
        *pQ = 1;
        agn_setenclosedouble(L, 1);
        break;
      case 'D': {  /* 2.39.7 set digits in double output, 4.10.5 fix, leave it here, do not put it into runargs() */
        int ndigits;
        const char *digits = argv[i] + 2;
        if (*digits == '\0') digits = argv[++i];
        if (!digits) return -1;
        ndigits = atoi(digits);
        if (ndigits > 0) {
          agn_setdigits(L, ndigits < 18 ? ndigits : 17);  /* 4.10.5 change */
        }
        break;
      }
      default: return -1;  /* invalid option */
    }
  }
  return 0;
}


static int runargs (lua_State *L, char **argv, int n) {
  int i;
  for (i=1; i < n; i++) {
    if (argv[i] == NULL || (argv[i][0] != '-' && argv[i][0] != '/')) continue;  /* 4.10.5 fix */
    switch (argv[i][1]) {  /* option */
      case 'e': {  /* execute statement "stat" (double quotes needed) */
        const char *chunk = argv[i] + 2;
        if (*chunk == '\0') chunk = argv[++i];
        lua_assert(chunk != NULL);
        if (dostring(L, chunk, "=(command line)") != 0)
          return 1;
        break;
      }
      case 'r': {  /* readlib library <name> (no quotes needed) */
        const char *filename = argv[i] + 2;
        if (*filename == '\0') filename = argv[++i];
        lua_assert(filename != NULL);
        if (dolibrary(L, filename))
          return 1;  /* stop if file fails */
        break;
      }
      default: break;
    }
  }
  return 0;
}


/* 0.22.1: sets a true copy of the structure at stack index idx into the registry, with key `b'. Changed Agena 1.0.3 */

static int savestate (lua_State *L, const char *a, const char *b) {
  lua_newtable(L);
  lua_getfield(L, LUA_GLOBALSINDEX, a);  /* get _G on stack */
  lua_pushnil(L);
  while (lua_next(L, -2) != 0) {
    lua_pushvalue(L, -2);  /* key */
    lua_pushvalue(L, -2);  /* value */
    if (lua_istable(L, -1)) {
      lua_newtable(L);
      lua_pushnil(L);
      while (lua_next(L, -3) != 0) {  /* copy table */
        lua_pushvalue(L, -2);  /* key */
        lua_pushvalue(L, -2);  /* value */
        lua_settable(L, -5);
        agn_poptop(L);  /* remove value */
      }  /* newtable is at -1 */
      lua_pushvalue(L, -3);  /* push key */
      lua_pushvalue(L, -2);  /* push newtable */
      lua_settable(L, -9);
      lua_pop(L, 4); }  /* pop newtable, value, key value */
    else {
      lua_settable(L, -6);
      agn_poptop(L);
    }
  }
  agn_poptop(L);  /* pop _G */
  lua_setfield(L, LUA_REGISTRYINDEX, b);
  return 0;  /* Agena 1.0.3 */
}


static void print_licence (void) {
  fprintf(stderr,
    "agena Copyright 2006-2026 by Alexander Walz. All rights reserved.\n\n"
    "Portions Copyright 1994-2025 Lua.org, PUC-Rio. All rights reserved.\n\n"
    "Please see the Agena manual (doc/agena.pdf) and the `licence` file for all\n"
    "credits and licences.\n\n"
    "--------------------------------------------------------------------------\n\n"
    "The Agena source code is licenced under the terms of the MIT licence\n"
    "reproduced below. This means that Agena is free software and can be used\n"
    "for both private, academic, and commercial purposes at absolutely no cost.\n\n"
    "Permission is hereby granted, free of charge, to any person obtaining a\n"
    "copy of this software and associated documentation files (the \"Software\"),\n"
    "to deal in the Software without restriction, including without limitation\n"
    "the rights to use, copy, modify, merge, publish, distribute, sublicence,\n"
    "and/or sell copies of the Software, and to permit persons to whom the\n"
    "Software is furnished to do so, subject to the following conditions:\n\n"
    "The above copyright notices and this permission notice shall be included\n"
    "in all copies or portions of the Software.\n\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\' WITHOUT WARRANTY OF ANY KIND, EXPRESS\n"
    "OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF\n"
    "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.\n\n"
    "IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY\n"
    "CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,\n"
    "TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE\n"
    "SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n\n"
    "--------------------------------------------------------------------------\n\n"
    "The Agena binaries are licenced under the terms of the GPL v2 licence:\n\n"
    "This programme is free software; you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public Licence as published by\n"
    "the Free Software Foundation; either version 2 of the Licence, or\n"
    "(at your option) any later version.\n\n"
    "This programme is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "GNU General Public Licence for more details.\n\n"
    "You should have received a copy of the GNU General Public Licence along\n"
    "with this program; if not, write to the Free Software Foundation, Inc.,\n"
    "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.\n");
  exit(0);
}

struct Smain {  /* for os.getcommandline */
  int argc;
  char **argv;
  int status;
};

static int pmain (lua_State *L) {  /* extended 1.6.12 */
  struct Smain *s = (struct Smain *)lua_touserdata(L, 1);
  char **argv = s->argv;
  int script, put_version, symbol;
  int has_i, has_v, has_e, has_n, has_l, has_p, has_b, has_q, has_Q, has_d, has_m, has_s, has_S, has_x, has_a, has_B, has_C;
  has_i = has_v = has_e = has_n = has_l = has_p = has_q = has_Q = has_b = has_d = has_m = has_s = has_S = has_x = has_a = has_B = has_C = 0;
  globalL = L;
  put_version = 1;
  if (argv[0] && argv[0][0]) progname = argv[0];
  lua_gc(L, LUA_GCSTOP, 0);  /* stop collector during initialisation phase 1 */
  luaL_openlibs(L);  /* open libraries */
  lua_gc(L, LUA_GCRESTART, 0);   /* restart collector. savestate must be called with activated collector ! */
  savestate(L, "_G", "_origG");
  /* save all global names assigned so far for restart command, store them to the registry with key `_oldG',
    can be accessed by issuing:
    for i, j in debug.getregistry()['_origG'] do print(i, j) od;
  */
  script =
    collectargs(L, argv,
      &has_i, &has_v, &has_e, &has_n, &has_l, &has_p, &has_d, &has_m, &has_s, &has_S, &has_x, &has_a, &has_B, &has_C, &has_q, &has_Q, &has_b, &symbol);
  if (script < 0) {  /* invalid args? */
    print_usage(symbol);
    s->status = 1;  /* do not free memory in main() at exit, to prevent segmentation faults */
    return 0;
  }
  lua_gc(L, LUA_GCSTOP, 0);  /* stop collector again during initialisation phase 2, Agena 1.0.4 */
  if (!has_p) {
    agnL_setLibname(L, 1, has_d, has_a);  /* determine value of libname */
  }
  agnL_initialise(L, has_n, has_d, has_x, has_a);  /* load and run library.agn and optionally initialisation files */
  /* initialise cache stack #7 that stores all kinds of data */
  agnL_initvaluecache(L, argv[0]);
  /* reserve a given number of slots (usually 128) in cache stack #7. 5.3.4 */
  luaL_checkstack(L->C, LUA_CELLSTACKSIZE, "(Agena initialisation)");
  /* restart collector, changed 6.1.3 */
  lua_gc(L, LUA_GCRESTART, 0);
  if (has_v && !has_S) {
    print_version(L, 0);
    put_version = 0;
  }
  if (has_l) {
    print_licence();
    put_version = 0;
  }
  if (script == 0 && !has_e && !has_v && !has_l && !has_S && lua_stdin_is_tty() && put_version) {
    print_version(L, 1);
  }
  if (has_m) print_memorystatus();  /* 2.8.6 */
  if (has_B) agn_setconstanttoobig(L, 1);  /* 2.39.4/5 */
  if (has_C) agn_setconstants(L, 0);  /* 2.39.6 */
  /* new start-up sequence 0.25.5, 05.08.2009 */
  s->status = runargs(L, argv, (script > 0) ? script : s->argc);
  if (s->status != 0) return 0;
  if (script)
    s->status = handle_script(L, argv, script);
  if (s->status != 0) return 0;
  if (has_i)
    dotty(L);
  else if (script == 0 && !has_e && !has_v && !has_l) {
    if (lua_stdin_is_tty())
      dotty(L);
    else
      dofile(L, NULL);  /* executes stdin as a file */
  }
  return 0;
}

struct Smain s;

int main (int argc, char **argv) {
  int status;
#ifdef _WIN32
  SetConsoleTitle("Agena " AGENA_EDITION " Programming Language");
#endif
  /* 2.3.0 RC 2 eCS fix provided by Dave Yeo on OS/2 world.com to avoid
     segmentation faults with mathematical functions (SIGFPEs issued by
     libc065.dll with trunc, sinh, cosh, etc.) and also to speed up double
     arithmetic significantly.

     See: http://www.os2world.com/forum/index.php/topic,496.msg4679.html#msg4679 */
#ifdef __OS2__
  short fcr;
  __asm__ volatile ("fstcw           %0 \n"
                    "or         $63, %0 \n"
                    "fldcw           %0 \n"
                    : "=m"(fcr));
#endif
  lua_State *L;
  L = lua_open();  /* create state */
  if (L == NULL) {
    l_message(argv[0], "Error, cannot create state: not enough memory.");
    return EXIT_FAILURE;
  }
  s.argc = argc;
  s.argv = argv;
  s.status = -1;  /* just in case Valgrind should complain, 6.1.3 */
  /* we cannot share s between agena.exe and agena.dll/libagena.so */
  L->agn_argc = s.argc;
  L->agn_argv = s.argv;
  status = lua_cpcall(L, &pmain, &s);
  report(L, status);
  if (s.status != 1) agnL_onexit(L, 0);  /* 2.7.0, 2.39.4 segfault fix, closes L->C, too */
  if (L) lua_close(L);  /* patched 7.6.6 for srglue/sragena. */
  return (status || s.status) ? EXIT_FAILURE : EXIT_SUCCESS;
}

