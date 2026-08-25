/*
** $Id: keyfile.c,v 0.1 17.06.2026 $
** Library for reading and writing key files
** See Copyright Notice in agena.h
*/

#include <errno.h>      /* for set_errno */
#include <fcntl.h>      /* to prevent "previous definition of 'lseek' was here" GCC error */
#include <stdio.h>      /* also for BUFSIZ */
#include <string.h>
#include <unistd.h>     /* for access */

#define keyfile_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnxlib.h"

#include "pbl.h"


/* ---------------- utilities ------------------------------------------------------- */

#define checkkeyfile(L,idx) ((pblKeyFile_t **)luaL_checkudata(L, idx, AGENA_KEYFILELIBNAME))

#define iskeyfile(L,idx)    (luaL_isudata(L, idx, AGENA_KEYFILELIBNAME))

/*
** When creating file handles, always creates a `closed' file handle
** before opening the actual file; so, if there is a memory error, the
** file is not left opened. Adapted from liolib.c.
*/
static pblKeyFile_t **newfile (lua_State *L) {
  pblKeyFile_t **pf = (pblKeyFile_t **)lua_newuserdata(L, sizeof(pblKeyFile_t *));
  *pf = NULL;  /* file handle is currently `closed' */
  luaL_getmetatable(L, AGENA_KEYFILELIBNAME);
  lua_setmetatable(L, -2);
  return pf;
}


/* ---------------- main part ------------------------------------------------------- */


/* Creates a new key file. It returns nothing if successful and issues an error otherwise.

   See also: `keyfile.open`. */
static int keyfile_new (lua_State *L) {
  int en;
  pblKeyFile_t *hnd;
  char *filename = (char *)agn_checkstring(L, 1);
  agn_checkvalidpath(L, filename, 0, "keyfile.new");
  /* if the file already exists, check whether you have the proper file rights */
  if (access(filename, F_OK) == 0 && access(filename, R_OK|W_OK) == -1) {
    luaL_error(L, "Error in " LUA_QS ": missing permissions for " LUA_QS ".", "keyfile.new", filename);
  }
  set_errno(0);  /* Better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  hnd = pblKfCreate(filename, NULL);
  en = errno;
  if (hnd == NULL) {  /* file does not yet exist ? */
    luaL_error(L, "Error in " LUA_QS ": %s (pbl code %d).", "keyfile.new", my_ioerror(en), pbl_errno);
  }
  pblKfClose(hnd);  /* pblKfCreate leaves the file open when exiting */
  /* Surprisingly pblKfCreate does not complain about some invalid paths, so let's try to find the file
     just created. */
  if (access(filename, F_OK) != 0) {
    luaL_error(L, "Error in " LUA_QS ": %s could not be created.", "keyfile.new", filename);
  }
  return 0;
}


/* Opens the key file denoted by its filename, a string. The default mode is read-and-write mode. You can
   open a key file in read-only mode by passing one of the strings "read" or "r" as a second argument.

   The return is the file handle needed to conduct any operation on a key files.

   See also: `keyfile.new`, `keyfile.close`. */
static int keyfile_open (lua_State *L) {
  int en, nargs, mode;
  pblKeyFile_t **hnd = newfile(L);
  char *filename = (char *)agn_checkstring(L, 1);
  nargs = lua_gettop(L);
  mode = 1;
  if (nargs == 2 && agn_isstring(L, 2)) {  /* read-only mode ?  */
    const char *s = agn_tostring(L, 2);
    if (tools_streqx(s, "read", "r", NULL)) mode = 0;
  }
  /* check whether you have the proper file rights */
  if (access(filename, F_OK) == 0 && access(filename, (mode) ? R_OK|W_OK : R_OK) == -1)
    luaL_error(L, "Error in " LUA_QS ": missing permissions for " LUA_QS ".", "keyfile.open", filename);
  set_errno(0);  /* Better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  *hnd = pblKfOpen(filename, mode, NULL);
  en = errno;
  if (*hnd == NULL) {  /* file does not yet exist ? */
    luaL_error(L, "Error in " LUA_QS ": %s (pbl code %d).", "keyfile.open", my_ioerror(en), pbl_errno);
  }
  return 1;  /* return userdata */
}


/* Closes one or more key files denoted by their file handles fh, etc. The function returns `true` on success and
   issues an error otherwise. The function also returns `true` of at least one of the handles has already
   been closed.

   If you have started transaction mode before (see `keyfile.start`) and have not already committed
   a transaction (see `keyfile.commit`), then `keyfile.close` will not flush unwritten contents to the
   key file before closing the key file.

   If you are in the default non-transactional mode, however, closing a file will automatically flush any
   unwritten content to the file.

   See also: keyfile.open. */
static int keyfile_close (lua_State *L) {
  int i, nargs, en;
  nargs = lua_gettop(L);
  for (i=0; i < nargs; i++) {
    pblKeyFile_t **pf = checkkeyfile(L, i + 1);
    if (*pf == NULL) continue;  /* file already closed */
    set_errno(0);  /* Windows 2000 seems susceptible to uncleared errno's */
    if (pblKfClose(*pf) != 0) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with argument #%d: %s (pbl code %d).", "keyfile.close",
        i, my_ioerror(en), pbl_errno);
    }
    lua_setmetatabletoobject(L, i + 1, NULL, 1);
    *pf = NULL;
  }
  lua_pushtrue(L);
  return 1;
}


/* Invokes transaction mode for the key file denoted by its handle `fh` and returns `true` on success and
   `false` otherwise. You must call this function in order to commit or roll back transactions with
   `keyfile.commit` later on. */
static int keyfile_start (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  lua_pushboolean(L, pblKfStartTransaction(*pf) == 0);
  return 1;
}


/* Created with the help of Gemini AI. */
/* The function writes the record `data`, a string, along with its associated `key`, a string, to the key file
   denoted by its handle fh.

   The key must not be the empty string or consist of more than 255 characters. There are no limits on the
   length of string `data`.

   Usually the content is not immediately written to the file. Call `keyfile.sync` to force a flush. Alternatively,
   first invoke transaction mode for fh with a call to `keyfile.start`, then run `keyfile.write` operations and
   call keyfile.commit` plus `keyfile.sync`. */

static int keyfile_write (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  size_t klen, dlen;
  const char *key = agn_checklstring(L, 2, &klen);
  const char *data = agn_checklstring(L, 3, &dlen);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.write");
  }
  if (klen == 0 || klen > 255) {
    luaL_error(L, "Error in " LUA_QS ": key length must be in [1, 255].", "keyfile.write");
  }
  /* Assuming pblKfInsert signature */
  if (pblKfInsert(*pf, (void *)key, klen + 1, (void *)data, dlen + 1) != 0) {
    luaL_error(L, "Error in " LUA_QS ": failed to insert record, pbl code %d.", "keyfile.write", pbl_errno);
  }
  lua_pushboolean(L, 1);
  return 1;
}


/* Created with the help of Gemini AI */

/* The function searches for the record(s) stored with the given key, a string, in the key file denoted by its handle fh.
   If the key exists, the return is the record, a string. If key does not exist, or there are no more associated records
   to read, the function returns `null`.

   To return all records associated with a given key, flag must be `null` at first invocation, and non-null with every
   subsequent call.

   Example to return all the records with key 'Portland':

   > keyfile.new('test.key');

   > fh := keyfile.open('test.key');

   > keyfile.write(fh, 'Houston', 'TX');
   > keyfile.write(fh, 'Portland', 'ON');
   > keyfile.write(fh, 'Portland', 'ME');
   > keyfile.write(fh, 'Portland', 'NSW');
   > keyfile.write(fh, 'Chattanooga', 'TN');

   Flush the content to the key file:

   > keyfile.sync(fh);

   Return all three records associated with key 'Portland':

   > index := 'Portland';
   > flag := null;

   > while record := keyfile.read(fh, index, flag) do
   >    print(index, record);
   >    flag := true  # for succeeding records, flag must be non-null
   > od;
   Portland        ON
   Portland        ME
   Portland        NSW

   > keyfile.close(fh);

   > os.remove('test.key');
*/
static int keyfile_read (lua_State *L) {
  size_t keylen;
  long int reclen;
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  const char *key = agn_checklstring(L, 2, &keylen);
  int firstone = lua_isnoneornil(L, 3);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.read");
  }
  if (keylen == 0 || keylen > 255) {
    luaL_error(L, "Error in " LUA_QS ": key length must be in [1, 255].", "keyfile.read");
  }
  char okey[256] = { 0 };  /* PBL keys are max 255 bytes per documentation */
  size_t okeylen = sizeof(okey);
  if (firstone) {  /* we deliberately use a second if statement to prevent a "stale buffer" risk
    for `okey' later on. */
    if (pblKfFind(*pf, PBLFI, (void *)key, keylen + 1, (void *)okey, &okeylen) < 0) {
      lua_pushnil(L);  /* key not found */
      return 1;
    }
  }
  /* the stale buffer risk may exist with the following line: */
  reclen = (firstone) ? pblKfThis(*pf, (char *)okey, &okeylen) :
                        pblKfNext(*pf, (char *)okey, &okeylen);
  if (reclen < 0 || !tools_streq(key, okey)) {
    lua_pushnil(L);
    return 1;
  }
  char *buffer = calloc(reclen, sizeof(char));
  if (!buffer) {
    luaL_error(L, "Error in " LUA_QS ": failed to allocate %ld bytes.", "keyfile.read", reclen);
  }
  /* reclen will hold the record length, the return can be negative, indicating an error; we must
     subtract 1 for the return counts the trailing \0, too. */
  reclen = pblKfRead(*pf, buffer, reclen);
  if (reclen >= 0) {
    lua_pushlstring(L, buffer, reclen - 1);
  } else {
    lua_pushnil(L);
  }
  xfree(buffer);
  return 1;
}


/* Checks whether the key file denoted by its handle fh contains the key `key` and returns `true` or
   `false`. */
static int keyfile_has (lua_State *L) {
  size_t keylen;
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  const char *key = agn_checklstring(L, 2, &keylen);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.has");
  }
  if (keylen == 0 || keylen > 255) {
    lua_pushfalse(L);
  } else {
    lua_pushboolean(L, pblKfFind(*pf, PBLFI, (void *)key, keylen + 1, NULL, NULL) >= 0);
  }
  return 1;
}


/* If mode is `true`, flags all previous keyfile.write operations as to be actually written to the key file
   denoted by `fh` later on. Call `keyfile.sync` to actually flush unwritten content to the file.

   If mode is `false`, rolls back all previous unsaved `keyfile.write` operations so they will not be actually
   written later on.

   The function returns `true` on success and `false` otherwise.

   Before adding content with `keyfile.write` you must have invoked `keyfile.start` to put fh into transaction mode,
   otherwise the function may issue errors.

   Created by Gemini AI */
static int keyfile_commit (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  int commit = !agnL_optboolean(L, 2, 1);  /* false = rollback, true = commit */
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.commit");
  }
  int rc = pblKfCommit(*pf, commit);
  if (rc < 0) {
    luaL_error(L, "Error in " LUA_QS ": failed to %s changes, pbl code %d.", "keyfile.commit",
      (commit == 0) ? "commit" : "rollback", pbl_errno);
  }
  /* true = committed, false = rollback happened, either because the caller requested it or because an
     inner transaction resulted in a rollback */
  lua_pushboolean(L, rc == 0);
  return 1;
}


/* Writes any unwritten content to the key file denoted by its handle fh and returns `true` on success
   and `false` otherwise.

   If you have started transaction mode before (see `keyfile.start`) and have not already committed
   a transaction (see `keyfile.commit`), then `keyfile.sync` will not flush unwritten contents to the
   key file.

   See also: `keyfile.commit`. */
static int keyfile_sync (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.sync");
  }
  lua_pushboolean(L, pblKfFlush(*pf) == 0);
  return 1;
}


/* Iterates an entire key file denoted by its handle fh. To start the iteration, pass `null` for
   `flag`, and pass any non-null argument for `flag` in succeeding calls to traverse towards the end
   of fh.

   The function returns the key and corresponding record as two strings. If the end of file has
   been reached, that is there are no more keys left, the function returns `null` twice.

   Created with the help of Gemini AI. */
static int keyfile_iterate (lua_State *L) {
  long int reclen;
  char *buffer;
  char key[256] = { 0 };
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  size_t keylen = sizeof(key);
  int firstone = lua_isnoneornil(L, 2);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.iterate");
  }
  luaL_checkstack(L, 2, "not enough stack space");
  reclen = (firstone) ? pblKfFirst(*pf, key, &keylen) : pblKfNext(*pf, key, &keylen);
  if (reclen < 0) {  /* reclen < 0 indicates either no more records or an error */
    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
  }
  buffer = calloc(reclen, sizeof(char));
  if (!buffer)
    luaL_error(L, "Error in " LUA_QS ": failed to allocate %ld bytes.", "keyfile.iterate", reclen);
  if (pblKfRead(*pf, (void *)buffer, reclen) == 0) {
    xfree(buffer);
    luaL_error(L, "Error in " LUA_QS ": cannot read record with key %s.", "keyfile.iterate", key);
  }
  lua_pushlstring(L, (const char *)key, keylen - 1);
  lua_pushlstring(L, (const char *)buffer, (size_t)reclen - 1);
  xfree(buffer);
  return 2;
}


/* Returns all the keys in the key file denoted by its handle fh. The result is a table of strings. If
   a key is existing in fh n times, then the resulting table will also include the key n times.

   If you pass a second argument, a function f returning a boolean, then `keyfile.allkeys` only returns
   those keys where the call to f evaluates to `true`.

   See also: keyfile.has. */
static int keyfile_allkeys (lua_State *L) {
  int hasfunc, istrue, unique, nargs;
  size_t keylen, c;
  long int reclen;
  char key[256] = { 0 };
  char oldkey[256] = { 0 };
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.allkeys");
  }
  nargs = lua_gettop(L);
  hasfunc = nargs >= 2 && lua_isfunction(L, 2);
  unique = nargs > 1 && lua_istrue(L, nargs);  /* insert a key only once */
  keylen = sizeof(key);
  luaL_checkstack(L, 2 + hasfunc, "not enough stack space");
  lua_createtable(L, 8, 0);
  reclen = pblKfFirst(*pf, key, &keylen);  /* reclen < 0 indicates either no more records or an error */
  if (reclen < 0) return 1;
  c = 1;
  if (unique) memcpy(oldkey, key, keylen);
  if (!hasfunc) {
    lua_pushlstring(L, (const char *)key, keylen - 1);
    lua_rawseti(L, -2, c++);
    while (1) {
      reclen = pblKfNext(*pf, key, &keylen);
      if (reclen < 0) return 1;
      if (unique) {
        /* the keys are implicitly sorted in the B-Tree, so we can easily check for duplicates */
        if (!strcmp(oldkey, key)) continue;
        memcpy(oldkey, key, keylen);
      }
      lua_pushlstring(L, (const char *)key, keylen - 1);
      lua_rawseti(L, -2, c++);
    }
  } else {
    lua_pushvalue(L, 2);
    lua_pushlstring(L, (const char *)key, keylen - 1);
    lua_call(L, 1, 1);
    if (!lua_isboolean(L, -1)) {
      agn_poptoptwo(L);
      luaL_error(L, "Error in " LUA_QS ": function must return a boolean.", "keyfile.allkeys");
    }
    istrue = agn_istrue(L, -1);
    agn_poptop(L);
    if (istrue) {
      lua_pushlstring(L, (const char *)key, keylen - 1);
      lua_rawseti(L, -2, c++);
    }
    while (1) {
      reclen = pblKfNext(*pf, key, &keylen);
      if (reclen < 0) return 1;
      lua_pushvalue(L, 2);
      lua_pushlstring(L, (const char *)key, keylen - 1);
      lua_call(L, 1, 1);
      if (!lua_isboolean(L, -1)) {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": function must return a boolean.", "keyfile.allkeys");
      }
      istrue = agn_istrue(L, -1);
      agn_poptop(L);
      if (istrue) {
        if (unique) {
          if (!strcmp(oldkey, key)) continue;
          memcpy(oldkey, key, keylen);
        }
        lua_pushlstring(L, (const char *)key, keylen - 1);
        lua_rawseti(L, -2, c++);
      }
    }
  }
  return 1;
}


/* Deletes the current record of the key file denoted by its handle fh. You may first want to
   search for a specific record by calling `keyfile.read` one or more times, and if the search
   is successful invoke `keyfile.purge`. */
static int keyfile_purge (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.purge");
  }
  lua_pushboolean(L, pblKfDelete(*pf) == 0);
  return 1;
}


/* Updates the current record of the key file denoted by its handle fh. If `data' is the
   empty string, the record will be deleted.

   You may first want to search for a specific record by calling `keyfile.read` one or more times,
   and if the search is successful invoke `keyfile.update`. */
static int keyfile_update (lua_State *L) {
  size_t dlen;
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  const char *data = agn_checklstring(L, 2, &dlen);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.update");
  }
  if (dlen == 0) {
    lua_pushboolean(L, pblKfDelete(*pf) == 0);
  } else {
    lua_pushboolean(L, pblKfUpdate(*pf, (char *)data, dlen + 1) == 0);
  }
  return 1;
}


/* Checks whether the first argument fh, usually a handle, represents an open key file,
   and returns `true` or `false`. */
static int keyfile_isopen (lua_State *L) {
  if (!iskeyfile(L, 1)) {
    lua_pushfalse(L);
  } else {
    pblKeyFile_t **pf = (pblKeyFile_t **)lua_touserdata(L, 1);
    lua_pushboolean(L, *pf != NULL);
  }
  return 1;
}


/* ---------------- metamethods ----------------------------------------------------- */

static int mt_gc (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (pf != NULL && *pf != NULL) {
    pblKfClose(*pf);
    lua_setmetatabletoobject(L, 1, NULL, 1);
    *pf = NULL;
  }
  return 0;
}


static int mt_tostring (lua_State *L) {
  if (luaL_isudata(L, 1, AGENA_KEYFILELIBNAME))
    lua_pushfstring(L, AGENA_KEYFILELIBNAME "(%p)", lua_topointer(L, 1));
  else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}


static const luaL_Reg keyfilelib[] = {
  {"__gc",            mt_gc},
  {"__tostring",      mt_tostring},
  {"allkeys",         keyfile_allkeys},
  {"close",           keyfile_close},
  {"commit",          keyfile_commit},
  {"has",             keyfile_has},
  {"isopen",          keyfile_isopen},
  {"iterate",         keyfile_iterate},
  {"purge",           keyfile_purge},
  {"read",            keyfile_read},
  {"start",           keyfile_start},
  {"sync",            keyfile_sync},
  {"update",          keyfile_update},
  {"write",           keyfile_write},
  {NULL, NULL}
};


static const luaL_Reg keyfile[] = {
  {"allkeys",         keyfile_allkeys},
  {"close",           keyfile_close},
  {"commit",          keyfile_commit},
  {"has",             keyfile_has},
  {"isopen",          keyfile_isopen},
  {"iterate",         keyfile_iterate},
  {"new",             keyfile_new},
  {"open",            keyfile_open},
  {"purge",           keyfile_purge},
  {"read",            keyfile_read},
  {"start",           keyfile_start},
  {"sync",            keyfile_sync},
  {"update",          keyfile_update},
  {"write",           keyfile_write},
  {NULL, NULL}
};


static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_KEYFILELIBNAME);  /* create metatable for key files */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, keyfilelib);  /* file methods */
}

/*
** Open keyfile library
*/
LUALIB_API int luaopen_keyfile (lua_State *L) {
  createmeta(L);
  luaL_register(L, AGENA_KEYFILELIBNAME, keyfile);
  return 1;
}

