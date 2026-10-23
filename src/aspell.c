/*
** $Id: aspell.c, initiated October 07, 2026 $
** aspell 0.60 binding
** See Copyright Notice in agena.h
** This library has mostly been created with the help of Gemini AI.
*/

#define _GNU_SOURCE

#define aspell_c
#define LUA_LIB

#include <aspell.h>
#include <string.h>
#include <unistd.h>


#if defined(_WIN32) || defined(__MSVCRT__)
#include <windows.h>
#elif defined(__OS2__) || defined(__EMX__)
#define INCL_DOS
#include <os2.h>
#include <stdlib.h>
#else
#include <dlfcn.h>
#endif

#include "agena.h"

#include "agenalib.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "prepdefs.h"

#if defined(__OS2__) || defined(__EMX__)
  /* Wir zwingen den OS/2-Compiler, die korrekte C-Standard-Aufrufkonvention
     für alle exportierten Aspell-Funktionen zu nutzen, um den SIGSEGV zu bannen */
  #if defined(__GNUC__)
    #define ASPELL_API __attribute__((cdecl))
  #else
    #define ASPELL_API _cdecl
  #endif
#else
  /* Für Windows, Linux, Solaris und macOS bleibt das Makro komplett leer */
  #define ASPELL_API
#endif

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_ASPELLLIBNAME "aspell"
LUALIB_API int (luaopen_aspell) (lua_State *L);
#endif

typedef struct {
  AspellSpeller *speller;
  char lang[16];
} LuaAspeller;

#ifdef IS32BIT
#define ASPELL_DIR_NAME "aspell"
#else
#define ASPELL_DIR_NAME "aspell64"
#endif

static char custom_dict_path[PATH_MAX] = {0};

/* Helper to check and return the speller userdata */
static LuaAspeller *get_speller (lua_State *L, int index) {
  LuaAspeller *la = (LuaAspeller *)luaL_checkudata(L, index, AGENA_ASPELLLIBNAME);
  if (la->speller == NULL) {
    luaL_error(L, "attempt to use a closed speller");
  }
  return la;
}

/* Frees the underlying memory and system resources allocated for an Aspell speller instance. */
static int l_speller_close (lua_State *L) {
  LuaAspeller *la = (LuaAspeller *)luaL_checkudata(L, 1, AGENA_ASPELLLIBNAME);
  if (la->speller != NULL) {
    delete_aspell_speller(la->speller);
    la->speller = NULL;
  }
  return 0;
}

/*
aspell.new
Creates and initializes a new Aspell speller instance for a specific language.

The function takes a string depicting the language to initialize (for example, 'en', 'en_UK', 'en_US',
'en_CA') and returns a newly created Aspell speller object if successful. In case of error, the function
returns `null` and an error message.

The binding automatically resolves its execution path to locate the dictionary files. On 32-bit Agena,
it looks for a `data/aspell` folder of your Agena installation, and `data/aspell64` on 64-bit Agena.

You may download additional Aspell 0.60-compliant dictionaries from the Internet, but be aware that
Aspell 32-bit dictionaries only work with the 32-bit edition of Agena and Aspell 64-bit dictionaries
only work with the 64-bit edition.

See also: aspell.close.
*/

static void sanitize_base_dir (char *base_dir, int check_backslash) {
  /* 1. Find the trailing separator to strip the executable name */
  char *last_slash = strrchr(base_dir, '/');
  if (check_backslash && !last_slash) {
    last_slash = strrchr(base_dir, '\\');
  }
  if (!last_slash) return;
  *last_slash = '\0'; /* Strip executable file name -> leaves "/usr/bin" */
#if defined(__unix__) || defined(__APPLE__)
  /* 2. Check explicitly for standard Unix installation layouts */
  if (strcmp(base_dir, "/usr/local/bin") == 0 ||
      strcmp(base_dir, "/usr/bin") == 0 ||
      strcmp(base_dir, "/usr/sfw/bin") == 0) {
    strcpy(base_dir, "/usr/agena"); /* Rewrite base directly to /usr/agena */
    return;
  }
#endif
  /* 4. Fallback: Step out one level if inside a standard /bin, /lib, or /src folder */
  char *sub_dir = strrchr(base_dir, '/');
  if (check_backslash && !sub_dir) {
    sub_dir = strrchr(base_dir, '\\');
  }
  if (sub_dir && (strcmp(sub_dir + 1, "lib") == 0 ||
                  strcmp(sub_dir + 1, "bin") == 0 ||
                  strcmp(sub_dir + 1, "src") == 0)) {
    *sub_dir = '\0'; /* Step out one level to the application root */
  }
#ifdef __OS2__
  /* In OS/2, some binary aspell libraries found on Hobbes may have big problems with a path
     preceded by a drive letter and colon, then internally replicating it thrice, and we do
     not want this. */
  if (strlen(base_dir) > 2 &&
      isalpha(uchar(base_dir[0])) &&
      base_dir[1] == ':' &&
      (base_dir[2] == '/' || base_dir[2] == '\\')) {
    memmove(base_dir, base_dir + 2, strlen(base_dir + 2) + 1);
  }
#endif
}

#ifdef __OS2__
#define isdriveletter(s)  (isalpha(uchar((s)[0])) && uchar((s)[1]) == ':')
#endif

/* aspell.new(lang) -> speller or (nil, err_msg) */
static int l_aspell_new (lua_State *L) {
  const char *lang = luaL_checkstring(L, 1);
  AspellConfig *config = new_aspell_config();
  if (!config) {
    lua_pushnil(L);
    lua_pushstring(L, "failed to create Aspell config allocation");
    return 2;
  }
  /* Sized strictly to the system's maximum path limit */
  char abs_aspell_dir[PATH_MAX] = { 0 };
  if (custom_dict_path[0] != '\0') {
    /* Use the custom path explicitly from our static variable */
    aspell_config_replace(config, "data-dir", custom_dict_path);
    aspell_config_replace(config, "dict-dir", custom_dict_path);
    /* strncpy(abs_aspell_dir, custom_dict_path, sizeof(abs_aspell_dir) - 1); */
    snprintf(abs_aspell_dir, sizeof(abs_aspell_dir), "%s", custom_dict_path);
  } else {
    /* Your standard fallback setup goes here */
    char *base_dir = getModuleFileName();
    if (base_dir) {
      char *r;
      size_t l;
      r = tools_findinpath(base_dir, "data/" ASPELL_DIR_NAME, &l);  /* 7.11.3 improvement */
      (void)l;
      if (r) {
        snprintf(abs_aspell_dir, sizeof(abs_aspell_dir), "%s", r);
        xfree(r);
      } else {
        sanitize_base_dir(base_dir, 1);
        snprintf(abs_aspell_dir, sizeof(abs_aspell_dir), "%s/data/" ASPELL_DIR_NAME, base_dir);
      }
      xfree(base_dir);
      char *path_to_use = abs_aspell_dir;
#if defined(__OS2__)
      /* 7.11.4: does path start with drive letter and a colon (e.g. "c:") ? */
      if (isdriveletter(abs_aspell_dir)) {
        /* skip them, for otherwise the dictionary files cannot be found in OS/2 Aspell, for whatever reason */
        path_to_use += 2;
      }
#endif
      aspell_config_replace(config, "data-dir", path_to_use);
      aspell_config_replace(config, "dict-dir", path_to_use);
    } else {
      aspell_config_replace(config, "data-dir", "data/" ASPELL_DIR_NAME);
      aspell_config_replace(config, "dict-dir", "data/" ASPELL_DIR_NAME);
    }
  }
  aspell_config_replace(config, "lang", lang);
  AspellCanHaveError *possible_err = new_aspell_speller(config);
  delete_aspell_config(config); /* speller retains its own copy */
  if (aspell_error_number(possible_err) != 0) {
    lua_pushnil(L);
    char debug_msg[4096];
    snprintf(debug_msg, sizeof(debug_msg), "%s\b, search path is: %.*s",
         aspell_error_message(possible_err),
         (int)(sizeof(debug_msg) - 256), abs_aspell_dir);
    lua_pushstring(L, debug_msg);
    delete_aspell_can_have_error(possible_err);
    return 2;
  }
  AspellSpeller *speller = to_aspell_speller(possible_err);
  LuaAspeller *la = (LuaAspeller *)lua_newuserdata(L, sizeof(LuaAspeller));
  la->speller = speller;
  /* Retain string copy in your extended tracking layout */
  strncpy(la->lang, lang, sizeof(la->lang) - 1);
  la->lang[sizeof(la->lang) - 1] = '\0';
  /* Set the metatable for our object */
  luaL_getmetatable(L, AGENA_ASPELLLIBNAME);
  lua_setmetatable(L, -2);
  return 1;
}


/* speller:check("word") -> boolean

Checks whether a specific word, a string, is spelled correctly according to the loaded language dictionary speller and returns `true` or `false`.

If internal dictionary processing fails (returning `null`), the second return contains the explicit Aspell error description. */
static int l_speller_check (lua_State *L) {
  LuaAspeller *la = get_speller(L, 1);
  size_t len;
  const char *word = luaL_checklstring(L, 2, &len);
  int result = aspell_speller_check(la->speller, word, len);
  if (result == -1) {
    lua_pushnil(L);
    lua_pushstring(L, aspell_speller_error_message(la->speller));
    return 2;
  }
  lua_pushboolean(L, result != 0);
  return 1;
}


/* speller:suggest("word") -> table of strings
Retrieves a list of spelling corrections and alternative suggestions for a given word, a string, in dictionary speller.
If no corrections are found, or if an internal calculation issue occurs, an empty table is returned.

The suggestions in the returned table are ordered by relevance, matching Aspell's internal scoring metric. The very first
entry represents the closest, most statistically accurate match calculated by the spelling engine, followed sequentially
by decreasingly likely alternatives. */
static int l_speller_suggest (lua_State *L) {
  LuaAspeller *la = get_speller(L, 1);
  size_t len;
  const char *word = luaL_checklstring(L, 2, &len);
  const AspellWordList *suggestions = aspell_speller_suggest(la->speller, word, len);
  if (!suggestions) {
    lua_newtable(L);
    return 1;
  }
  lua_newtable(L);
  AspellStringEnumeration *elements = aspell_word_list_elements(suggestions);
  const char *suggested_word;
  int index = 1;
  while ((suggested_word = aspell_string_enumeration_next(elements)) != NULL) {
    lua_pushstring(L, suggested_word);
    lua_rawseti(L, -2, index++);
  }
  delete_aspell_string_enumeration(elements);
  return 1;
}


/* speller:config("key") -> string or nil

1. Dictionary Selection
    "lang": The core language string (e.g., "en_US", "fr").
    "master": The base filename of the master dictionary.
    "variety": Distinguishes extra dictionary variations if size and language are identical.

2. Spelling & Suggestion Control
    "sug-mode": Adjusts suggestion speed vs. precision. Acceptable values include "ultra", "fast", "normal", and "bad-spellers".
    "ignore": Tells Aspell to ignore words below a certain length (takes an integer converted to string, e.g., "3").
    "ignore-case": Determines whether to ignore variations in capitalization ("true" or "false").
    "camel-case": Enables or disables checking split words inside camelCase tokens ("true" or "false").
    "run-together": Treats runtogetherwords as legal compounds ("true" or "false").
    "size": Size metric score rating of the active dictionary word list

3. File Path Configuration
    "dict-dir": The directory containing the primary dictionary binaries.
    "data-dir": The directory containing language prefix rules and .dat configuration files.
    "personal": The specific filename or absolute path for the user's personal wordlist database.

4. Text Content Processing (Filters)
    "mode": Adjusts document parsing type rules. Common structural modes include "url", "email", "html", "tex", and "markdown".

5. Encoding:
   "encoding": The encoding the input text is in. Valid values include, but not limited to, ‘iso-8859-*’, ‘utf-8’, ‘ucs-2’, ‘ucs-4’.
   "normalize": Perform Unicode normalization. Enabled by default.

All configurations can be read, but not all can be changed.

Please consult the Aspell 0.60 documentation on the Internet for more information, options and settings. */
static int l_speller_config (lua_State *L) {
  int nargs = lua_gettop(L);
  LuaAspeller *la = get_speller(L, 1);
  const char *key = luaL_checkstring(L, 2);
  if (nargs == 2) {
    /* Grab the speller's internal config context and fetch the key value string */
    struct AspellConfig *config = aspell_speller_config(la->speller);
    const char *value = aspell_config_retrieve(config, key);
    if (!value) {
      lua_pushnil(L);
    } else {
      lua_pushstring(L, value);
    }
  } else {
    const char *value = luaL_checkstring(L, 3);
    /* Grab the speller's internal config context */
    struct AspellConfig *config = aspell_speller_config(la->speller);
    /* Try to replace the configuration option on the fly */
    int success = aspell_config_replace(config, key, value);
    lua_pushboolean(L, success != 0);
  }
  return 1;
}


static int l_speller_list_dicts (lua_State *L) {
  LuaAspeller *la = get_speller(L, 1);
  AspellConfig *speller_config = aspell_speller_config(la->speller);
  if (!speller_config) {
    luaL_error(L, "Error in " LUA_QS ": could not retrieve speller configuration.", "aspell.getdictinfo");
  }
  AspellConfig *config = aspell_config_clone(speller_config);
  if (lua_gettop(L) >= 2 && lua_isstring(L, 2)) {
    const char *lang_filter = lua_tostring(L, 2);
    if (strlen(lang_filter) == 0) {
      aspell_config_replace(config, "lang", "");
    } else {
      aspell_config_replace(config, "lang", lang_filter);
    }
  } else {
    /* Default behavior: Clear language option to fetch all available dicts */
    aspell_config_replace(config, "lang", "");
  }
  AspellDictInfoList *dict_list = get_aspell_dict_info_list(config);
  if (!dict_list) {
    delete_aspell_config(config);
    luaL_error(L, "Error in " LUA_QS ": failed to create dictionary info list.", "aspell.getdictinfo");
  }
  lua_newtable(L);
  AspellDictInfoEnumeration *elements = aspell_dict_info_list_elements(dict_list);
  const AspellDictInfo *dict;
  int index = 1;
  while ((dict = aspell_dict_info_enumeration_next(elements)) != NULL) {
    lua_newtable(L);
    /* The "name" field is the exact identifier you must pass to aspell.new() to open that specific 
       dictionary file. It combines the language code with regional dialects, spelling reforms, 
       or vocabulary sizes. */
    lua_pushstring(L, dict->name);
    lua_setfield(L, -2, "name");
    /* The "code" field represents the root language using the standard ISO 639 code. */
    lua_pushstring(L, dict->code);
    lua_setfield(L, -2, "code");
    /* The jargon field represents extra descriptive information used to distinguish between different 
       variants or sub-types of dictionaries for the exact same language and size. */
    lua_pushstring(L, dict->jargon);
    lua_setfield(L, -2, "jargon");
    /* The "size" field represents the relative vocabulary size (or density) of that specific 
         dictionary variant:
       - 10 to 40 (Small): Contains only the most common words. Great for minimal resource 
         environments or specialized, small-vocabulary tasks.
       - 50 (Standard/Default): The normal, recommended size for daily spellchecking 
         (roughly equivalent to a standard desk dictionary).
       - 60 to 70 (Large): Includes specialized terms, less common variants, and 
         technical jargon.
       - 80+ (Huge/Insane): Contains archaic words, rare regional variants, and massive 
         permutations of word forms. */
    lua_pushinteger(L, dict->size);
    lua_setfield(L, -2, "size");
    /* The "module" field indicates the underlying spelling engine backend that is responsible for 
       loading and processing that specific dictionary. */
    lua_pushstring(L, dict->module->name);
    lua_setfield(L, -2, "module");
    lua_rawseti(L, -2, index++);
  }
  /* Free allocated enumeration iterator and cloned configuration context */
  delete_aspell_dict_info_enumeration(elements);
  delete_aspell_config(config);
  return 1;
}


/* speller:add_to_session("word") -> boolean or (nil, err_msg) */
static int l_speller_add_to_session (lua_State *L) {
  LuaAspeller *la = get_speller(L, 1);
  size_t len;
  const char *word = luaL_checklstring(L, 2, &len);
  /* Aspell C API accepts word and length (-1 can be used for null-terminated strings, but len is safer) */
  aspell_speller_add_to_session(la->speller, word, len);
  /* Check if an error occurred during adding */
  if (aspell_speller_error_number(la->speller) != 0) {
    lua_pushnil(L);
    lua_pushstring(L, aspell_speller_error_message(la->speller));
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}


/* speller:add_to_personal("word") -> boolean or (nil, err_msg) */
static int l_speller_add_to_personal (lua_State *L) {
  LuaAspeller *la = get_speller(L, 1);
  size_t len;
  const char *word = luaL_checklstring(L, 2, &len);
  aspell_speller_add_to_personal(la->speller, word, len);
  if (aspell_speller_error_number(la->speller) != 0) {
    lua_pushnil(L);
    lua_pushstring(L, aspell_speller_error_message(la->speller));
    return 2;
  }
  /* Important: Personal words are cached in memory. You must save them to disk. */
  aspell_speller_save_all_word_lists(la->speller);
  if (aspell_speller_error_number(la->speller) != 0) {
    lua_pushnil(L);
    lua_pushstring(L, aspell_speller_error_message(la->speller));
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}


static int l_aspell_path (lua_State *L) {
  if (lua_gettop(L) > 0) {
    size_t l;
    const char *path = luaL_checklstring(L, 1, &l);
    if (l == 0) {
      memset(custom_dict_path, 0, sizeof(custom_dict_path));
    } else if (l >= sizeof(custom_dict_path)) {
      luaL_error(L, "Error in " LUA_QS ": path is too short or too long.", "aspell.path");
    } else if (!tools_exists(path)) {
      luaL_error(L, "Error in " LUA_QS ": path does not exist or permissions are missing.", "aspell.path");
    } else {
      /* Copy directly into our safe static storage */
      strncpy(custom_dict_path, path, sizeof(custom_dict_path) - 1);
      custom_dict_path[sizeof(custom_dict_path) - 1] = '\0';
    }
  }
  if (custom_dict_path[0] == '\0') {
    /* Create a temporary config to extract the default compiled-in dict-dir */
    AspellConfig *config = new_aspell_config();
    if (config) {
      const char *default_path = aspell_config_retrieve(config, "dict-dir");
      if (default_path) {
        lua_pushstring(L, default_path);
      } else {
        lua_pushnil(L);
      }
      delete_aspell_config(config);
    } else {
      lua_pushnil(L);
    }
  } else {
    lua_pushstring(L, custom_dict_path);
  }
  return 1;
}


static int mt_tostring (lua_State *L) {
  LuaAspeller *la = get_speller(L, 1);
  lua_pushfstring(L, "%s(%s)", AGENA_ASPELLLIBNAME, la->lang);
  return 1;
}


/* Module entry points exposed directly on the package */
static const struct luaL_Reg aspell_funcs[] = {
  {"addtosession",  l_speller_add_to_session},
  {"addtopersonal", l_speller_add_to_personal},
  {"check",         l_speller_check},
  {"close",         l_speller_close},
  {"config",        l_speller_config},
  {"getdictinfo",   l_speller_list_dicts},
  {"new",           l_aspell_new},
  {"path",          l_aspell_path},
  {"suggest",       l_speller_suggest},
  {NULL, NULL}
};


/* Methods attached exclusively to the object instances */
static const struct luaL_Reg speller_meta[] = {
  {"__gc",          l_speller_close},
  {"__tostring",    mt_tostring},
  {NULL, NULL}
};


LUALIB_API int luaopen_aspell (lua_State *L) {
  /* 1. Setup the object metatable for OOP methods */
  luaL_newmetatable(L, AGENA_ASPELLLIBNAME);
  /* metatable.__index = metatable */
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  /* Register the shared actions and meta-rules into the metatable */
  luaL_register(L, NULL, aspell_funcs);  /* Skips "new" automatically if your methods check self types */
  luaL_register(L, NULL, speller_meta);
  lua_pop(L, 1); /* Pop metatable */
  /* 2. Setup the module package for ordinary functions: aspell.check(speller) */
  luaL_register(L, AGENA_ASPELLLIBNAME, aspell_funcs);
  return 1;
}


/* The following is needed for the Windows 32-bit aspell-0.60 binding to be successfully compiled ! */
#if defined(_WIN32) || defined(__MSVCRT__)
/* Dummy types to satisfy internally compiled headers */
typedef void *pthread_mutex_t;
typedef void *pthread_mutexattr_t;

int pthread_mutex_init (pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
  (void)mutex; (void)attr;
  return 0; /* Success */
}

int pthread_mutex_destroy (pthread_mutex_t *mutex) {
  (void)mutex;
  return 0;
}

int pthread_mutex_lock (pthread_mutex_t *mutex) {
  (void)mutex;
  return 0;
}

int pthread_mutex_unlock (pthread_mutex_t *mutex) {
  (void)mutex;
  return 0;
}
#endif
