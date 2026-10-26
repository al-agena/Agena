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
  nargs = lua_gettop(L);  /* 7.10.9 fix, newfile() obviously changes the stack */
  pblKeyFile_t **hnd = newfile(L);
  char *filename = (char *)agn_checkstring(L, 1);
  mode = 1;
  if (nargs > 1 && agn_isstring(L, 2)) {  /* read-only mode ?  */
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
  int isseq, rc;
  size_t klen, vallen, j;
  const char *key = luaL_checklstring(L, 2, &klen);  /* 7.10.4 extension for numbers */
  char *data = NULL;
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.write");
  }
  if (klen == 0 || klen > 255) {
    luaL_error(L, "Error in " LUA_QS ": key length must be in [1, 255].", "keyfile.write");
  }
  int has_value = (lua_gettop(L) >= 3 && !lua_isnil(L, 3));
  if (has_value) {
    if ( (isseq = lua_isseq(L, 3)) ) {  /* 7.10.4 extension */
      vallen = agn_seqsize(L, 3);
      if (vallen < 1)
        luaL_error(L, "Error in " LUA_QS ": sequence is empty.", "keyfile.write");
      data = (char *)agn_stralloc(L, vallen, "keyfile.write", NULL);
      for (j=0; j < vallen; j++) {
        data[j] = (unsigned char)agn_seqrawgetiinteger(L, 3, j + 1, &rc);
        if (!rc) {
          xfree(data);
          luaL_error(L, "Error in " LUA_QS ": sequence must consist of integers only.", "keyfile.write");
        }
      }
      vallen++;
    } else {
      data = (char *)luaL_checklstring(L, 3, &vallen);  /* ditto */
      vallen++;
    }
  } else {  /* no value to be stored, 7.11.6 */
    isseq = 0;
    data = NULL;
    vallen = 0; /* 0 length signals PBL not to allocate any data blocks */
  }
  if (vallen == 0) {
    char okey[256] = { 0 };
    size_t okeylen = sizeof(okey);
    if (pblKfFind(*pf, PBLEQ, (void *)key, klen + 1, (void *)okey, &okeylen) == 0) {
      if (isseq && data) { xfree(data); }
      lua_pushtrue(L);
      return 1;
    }
  }
  /* Assuming pblKfInsert signature */
  if (pblKfInsert(*pf, (void *)key, klen + 1, (void *)data, vallen) != 0) {
    /* If the file configuration rejected it because it already exists, it is a no-op! */
    if (pbl_errno == PBL_ERROR_EXISTS) goto finale;
    /* Real database error handling */
    if (isseq && data) { xfree(data); }
    luaL_error(L, "Error in " LUA_QS ": failed to insert record, pbl code %d.", "keyfile.write", pbl_errno);
  }
finale:
  if (isseq) { xfree(data); }
  lua_pushtrue(L);
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
  const char *key = luaL_checklstring(L, 2, &keylen);  /* 7.10.4 extension */
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
  } else if (reclen == 0) {
    lua_pushtrue(L);
    return 1;
  }
  char *buffer = calloc(reclen, sizeof(char));
  if (!buffer) {
    luaL_error(L, "Error in " LUA_QS ": failed to allocate %ld bytes.", "keyfile.read", reclen);
  }
  /* reclen will hold the record length, the return can be negative, indicating an error; we must
     subtract 1 for the return counts the trailing \0, too. */
  reclen = pblKfRead(*pf, buffer, reclen);
  if (reclen > 0) {
    lua_pushlstring(L, buffer, reclen - 1);
  } else if (reclen == 0) {
    lua_pushfalse(L);
  } else {  /* error */
    lua_pushnil(L);
  }
  xfree(buffer);
  return 1;
}


/* Checks whether the key file denoted by its handle fh contains the key `key` and returns `true` or
   `false`. */

static int aux_has (lua_State *L, int idx1, int idx2, const char *pn) {
  size_t keylen;
  pblKeyFile_t **pf = checkkeyfile(L, idx1);
  const char *key = agn_checklstring(L, idx2, &keylen);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", pn);
  }
  if (keylen == 0 || keylen > 255) {
    return 0;
  } else {
    return pblKfFind(*pf, PBLFI, (void *)key, keylen + 1, NULL, NULL) >= 0;
  }
}

static int keyfile_has (lua_State *L) {
  lua_pushboolean(L, aux_has(L, 1, 2, "keyfile.has"));
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
  luaL_checkstack(L, 1 + (reclen != 0), "not enough stack space");
  if (reclen < 0) {  /* reclen < 0 indicates either no more records or an error */
    lua_pushnil(L);
    lua_pushnil(L);
  } else if (reclen == 0) {
    lua_pushlstring(L, (const char *)key, keylen - 1);
  } else {
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
  }
  return 1 + (reclen != 0);
}


/* Returns all the keys in the key file denoted by its handle fh. The result is a table of strings. If
   a key is existing in fh n times, then the resulting table will also include the key n times.

   If you pass a second argument, a function f returning a boolean, then `keyfile.allkeys` only returns
   those keys where the call to f evaluates to `true`.

   See also: keyfile.has. */

#define aux_insertinto(L,k,kl) { \
  lua_pushlstring(L, (const char *)(k), (kl)); \
  agn_structinsert(L, -2, -1); \
  agn_poptop(L); \
}

static int keyfile_allkeys (lua_State *L) {
  int hasfunc, istrue, unique, nargs, structat, strmatch;
  size_t keylen;
  long int reclen;
  char key[256] = { 0 };
  char oldkey[256] = { 0 };
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.allkeys");
  }
  strmatch = unique = structat = 0;
  nargs = lua_gettop(L);
  hasfunc = nargs > 1 && lua_isfunction(L, 2);
  if (nargs > 1) {
    if (lua_istable(L, nargs) || lua_isseq(L, nargs) || lua_isreg(L, nargs) || lua_isset(L, nargs)) {
      structat = nargs--;
    }
    strmatch = lua_isstring(L, 2);
    unique = lua_istrue(L, nargs);  /* insert a key only once */
  }
  keylen = sizeof(key);
  luaL_checkstack(L, 2 + hasfunc, "not enough stack space");
  if (!structat) {
    lua_createtable(L, 8, 0);
  } else {
    lua_pushvalue(L, structat);
  }
  reclen = pblKfFirst(*pf, key, &keylen);  /* reclen < 0 indicates either no more records or an error */
  if (reclen < 0) return 1;
  if (unique) memcpy(oldkey, key, keylen);
  if (strmatch) {
    size_t l;
    char *lookup;
    const char *pattern = lua_tolstring(L, 2, &l);
    lookup = agnL_strmatch(L, key, keylen, pattern, l);
    if (lookup) {
      aux_insertinto(L, key, keylen - 1);
    }
    while (1) {
      reclen = pblKfNext(*pf, key, &keylen);
      if (reclen < 0) return 1;
      if (unique) {
        /* the keys are implicitly sorted in the B-Tree, so we can easily check for duplicates */
        if (!strcmp(oldkey, key)) continue;
        memcpy(oldkey, key, keylen);
      }
      lookup = agnL_strmatch(L, key, keylen, pattern, l);
      if (lookup) {
        aux_insertinto(L, key, keylen - 1);
      }
    }
  } else if (!hasfunc) {
    lua_pushlstring(L, (const char *)key, keylen - 1);
    agn_structinsert(L, -2, -1);
    agn_poptop(L);
    while (1) {
      reclen = pblKfNext(*pf, key, &keylen);
      if (reclen < 0) return 1;
      if (unique) {
        /* the keys are implicitly sorted in the B-Tree, so we can easily check for duplicates */
        if (!strcmp(oldkey, key)) continue;
        memcpy(oldkey, key, keylen);
      }
      aux_insertinto(L, key, keylen - 1);
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
      aux_insertinto(L, key, keylen - 1);
    }
    while (1) {
      reclen = pblKfNext(*pf, key, &keylen);
      if (reclen < 0) return 1;
      luaL_checkstack(L, 2, "not enough stack space");
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
        aux_insertinto(L, key, keylen - 1);
      }
    }
  }
  return 1;
}


static size_t aux_sizeof (lua_State *L, const char *pn) {
  int hasfunc, istrue, unique, nargs, i, strmatch;
  size_t keylen, c;
  long int reclen;
  char key[256] = { 0 };
  char oldkey[256] = { 0 };
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", pn);
  }
  nargs = lua_gettop(L);
  hasfunc = unique = strmatch = 0;
  if (nargs > 1) {
    hasfunc = lua_isfunction(L, 2);
    strmatch = lua_isstring(L, 2);
    unique  = lua_istrue(L, nargs);  /* insert a key only once */
    for (i=2; i <= nargs; i++) {
      if (!lua_isfunction(L, i) && !lua_isboolean(L, i) && !lua_isstring(L, i))
        luaL_error(L, "Error in " LUA_QS ": expected a function, string or boolean for argument #%d.", pn, i);
    }
  }
  keylen = sizeof(key);
  reclen = pblKfFirst(*pf, key, &keylen);  /* reclen < 0 indicates either no more records or an error */
  if (reclen < 0) return 0;
  c = 0;
  if (unique) memcpy(oldkey, key, keylen);
  if (strmatch) {
    size_t l;
    char *lookup;
    const char *pattern = lua_tolstring(L, 2, &l);
    lookup = agnL_strmatch(L, key, keylen, pattern, l);
    if (lookup) c++;
    while (1) {
      reclen = pblKfNext(*pf, key, &keylen);
      if (reclen < 0) return c;
      if (unique) {
        /* the keys are implicitly sorted in the B-Tree, so we can easily check for duplicates */
        if (!strcmp(oldkey, key)) continue;
        memcpy(oldkey, key, keylen);
      }
      lookup = agnL_strmatch(L, key, keylen, pattern, l);
      if (lookup) c++;
    }
  } else if (!hasfunc) {
    c++;
    if (unique) {
      while (1) {
        reclen = pblKfNext(*pf, key, &keylen);
        if (reclen < 0) return c;
        if (!strcmp(oldkey, key)) continue;
        memcpy(oldkey, key, keylen);
        c++;
      }
    } else {  /* 7.11.6 3.5 % tweak */
      while (pblKfNext(*pf, NULL, NULL) >= 0) c++;
    }
  } else {
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushvalue(L, 2);
    lua_pushlstring(L, (const char *)key, keylen - 1);
    lua_call(L, 1, 1);
    if (!lua_isboolean(L, -1)) {
      agn_poptoptwo(L);
      luaL_error(L, "Error in " LUA_QS ": function must return a boolean.", pn);
    }
    istrue = agn_istrue(L, -1);
    agn_poptop(L);
    c += istrue;
    while (1) {
      reclen = pblKfNext(*pf, key, &keylen);
      if (reclen < 0) return c;
      luaL_checkstack(L, 2, "not enough stack space");
      lua_pushvalue(L, 2);
      lua_pushlstring(L, (const char *)key, keylen - 1);
      lua_call(L, 1, 1);
      if (!lua_isboolean(L, -1)) {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": function must return a boolean.", pn);
      }
      istrue = agn_istrue(L, -1);
      agn_poptop(L);
      if (istrue) {
        if (unique) {
          if (!strcmp(oldkey, key)) continue;
          memcpy(oldkey, key, keylen);
        }
        c++;
      }
    }
  }
  return c;
}

static int keyfile_count (lua_State *L) {
  lua_pushinteger(L, aux_sizeof(L, "keyfile.count"));
  return 1;
}


/* Deletes the current record of the key file denoted by its handle fh. You may first want to
   search for a specific record by calling `keyfile.read` one or more times, and if the search
   is successful invoke `keyfile.purge`. */
static int keyfile_purge (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  size_t klen;
  int rc = 0;
  const char *key = luaL_optlstring(L, 2, NULL, &klen);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.purge");
  }
  if (key == NULL) {
    rc = (pblKfDelete(*pf) == 0);
  } else {  /* 7.11.6 extension */
    if (klen == 0 || klen > 255) {
      luaL_error(L, "Error in " LUA_QS ": key length must be in [1, 255].", "keyfile.purge");
    }
    char okey[256] = { 0 };
    size_t okeylen = sizeof(okey);
    /* Step 1: Use pblKfFind to seek the key and position the internal transaction cursor precisely
       onto the record we want to delete. */
    if (pblKfFind(*pf, PBLEQ, (void *)key, klen + 1, (void *)okey, &okeylen) == 0) {
      /* Step 2: The cursor is now locked onto the target key. Safe to delete! */
      if (pblKfDelete(*pf) == 0) rc = 1;  /* successfully deleted */
    }
  }
  lua_pushboolean(L, rc);  /* true: success, else: Key wasn't found or delete failed */
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
  char current_key[256] = { 0 };
  size_t klen = sizeof(current_key);
  long datalenatpos = pblKfThis(*pf, current_key, &klen);
  if (datalenatpos < 0)
    luaL_error(L, "Error in " LUA_QS ": no record currently selected.", "keyfile.update");
  else if (datalenatpos == 0)
    luaL_error(L, "Error in " LUA_QS ": cannot update a key-only entry.", "keyfile.update");
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


/* Positions the cursor on the very first alphabetical record in the file.
   Returns true on success, false if the file is empty. Created by Gemini AI 7.11.6 */
static int keyfile_first (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.first");
  }
  char okey[256] = { 0 };
  size_t okeylen = sizeof(okey);
  const char *dummy_search_key = "";
  /* Pass a valid string address and a length > 0.
     PBLFIRST will override the dummy key and drop straight onto the lowest key. */
  if (pblKfFind(*pf, PBLFIRST, (void *)dummy_search_key, 1, (void *)okey, &okeylen) == 0) {
    lua_pushtrue(L);
  } else {
    lua_pushfalse(L);
  }
  return 1;
}


/* Positions the cursor on the very last alphabetical record in the file. Returns true on success,
   false if the file is empty. Created by Gemini AI 7.11.6 */
static int keyfile_last (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.last");
  }
  char okey[256] = { 0 };
  size_t okeylen = sizeof(okey);
  lua_pushboolean(L, pblKfLast(*pf, okey, &okeylen) >= 0);
  return 1;
}


/* Advances the cursor to the next record slot in the keyfile. Returns true on success, or false if
   it hits the end of the file. Created by Gemini AI 7.11.6 */
static int keyfile_advance (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.advance");
  }
  /* pblKfNext reads the next sequential index entry on the B-Tree leaf page. If it returns >= 0,
     it successfully advanced the cursor. */
  lua_pushboolean(L, pblKfNext(*pf, NULL, NULL) >= 0);
  return 1;
}


/* Extracts the key and value strings at the current cursor position. Returns true, the string key
   and the value, a string or null if no value is associated with the key. If the position is
   invalid or empty, returns false. The function does not move the cursor.
   Created by Gemini AI 7.11.6 */
static int keyfile_current (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.current");
  }
  char current_key[256] = { 0 };
  size_t klen = sizeof(current_key);
  /* Step 1: Fetch the data length and key at the current cursor position.
     pblKfThis does not alter the B-Tree position context. */
  long datalenatpos = pblKfThis(*pf, current_key, &klen);
  /* If datalenatpos < 0, no valid record is currently selected */
  if (datalenatpos < 0) {
    lua_pushfalse(L);
    return 1;
  }
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushtrue(L);
  /* 1. Push the key string (stripping the trailing \0 overhead) */
  lua_pushlstring(L, current_key, klen - 1);
  /* 2. Handle the value payload based on your storage format */
  if (datalenatpos == 0) {
    /* wordlist / key-only mode: explicitly push null */
    lua_pushnil(L);
  } else {
    /* key-value mode: allocate a temporary memory buffer to read the value from disk */
    char *val_buffer = (char *)malloc(datalenatpos);
    if (!val_buffer) {
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "keyfile.current");
    }
    /* read the actual value payload out of the data block */
    long read_bytes = pblKfRead(*pf, val_buffer, datalenatpos);
    if (read_bytes >= 0) {
      /* safely push the string, stripping the trailing \0 byte (read_bytes - 1) */
      lua_pushlstring(L, val_buffer, read_bytes - 1);
    } else {
      lua_pushnil(L); /* disk read failed or data block mismatch */
    }
    free(val_buffer);
  }
  return 3; /* true, key, (value or null) */
}


/* Checks if a key exists in the file and returns true if found, false if not found. The function bypasses
   all data payload reading overhead and sets the cursor to the position of the key if it is found. 7.11.6,
   created by Gemini AI */
static int keyfile_exists (lua_State *L) {
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  size_t keylen;
  const char *key = luaL_checklstring(L, 2, &keylen);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "keyfile.exists");
  }
  if (keylen == 0 || keylen > 255) {
    luaL_error(L, "Error in " LUA_QS ": key length must be in [1, 255].", "keyfile.exists");
  }
  /* We pass NULL and 0 for the output key buffer. We use PBLFI (Find Exact) to search the index tree.
     The engine will locate the key but skip reading or loading any attached data blocks. */
  int found = (pblKfFind(*pf, PBLFI, (void *)key, keylen + 1, NULL, NULL) == 0);
  lua_pushboolean(L, found);
  return 1;
}


/* ---------------- metamethods ----------------------------------------------------- */

static int mt_size (lua_State *L) {
  lua_pushinteger(L, aux_sizeof(L, "(__size mt)"));
  return 1;
}


static int mt_empty (lua_State *L) {
  size_t keylen;
  long int reclen;
  char key[256] = { 0 };
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "(__empty mt)");
  }
  reclen = pblKfFirst(*pf, key, &keylen);  /* reclen < 0 indicates either no more records or an error */
  lua_pushboolean(L, (reclen < 0));
  return 1;
}


static int mt_filled (lua_State *L) {
  size_t keylen;
  long int reclen;
  char key[256] = { 0 };
  pblKeyFile_t **pf = checkkeyfile(L, 1);
  if (*pf == NULL) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a closed file.", "(__filled mt)");
  }
  reclen = pblKfFirst(*pf, key, &keylen);  /* reclen < 0 indicates either no more records or an error */
  lua_pushboolean(L, (reclen >= 0));
  return 1;
}


static int mt_in (lua_State *L) {
  lua_pushboolean(L, aux_has(L, 2, 1, "(__in mt)"));
  return 1;
}


static int mt_notin (lua_State *L) {
  lua_pushboolean(L, !aux_has(L, 2, 1, "(__notin mt)"));
  return 1;
}


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


static const struct luaL_Reg keyfile_meta[] = {
  {"__gc",            mt_gc},
  {"__tostring",      mt_tostring},
  {"__size",          mt_size},
  {"__empty",         mt_empty},
  {"__filled",        mt_filled},
  {"__in",            mt_in},
  {"__notin",         mt_notin},
  {NULL, NULL}
};


static const luaL_Reg keyfile[] = {
  {"allkeys",         keyfile_allkeys},
  {"close",           keyfile_close},
  {"commit",          keyfile_commit},
  {"count",           keyfile_count},
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
  /* iterator-specific functions */
  {"advance",         keyfile_advance},
  {"current",         keyfile_current},
  {"exists",          keyfile_exists},
  {"first",           keyfile_first},
  {"last",            keyfile_last},
  {NULL, NULL}
};


static void createmeta (lua_State *L) {  /* new 7.11.2 */
  luaL_newmetatable(L, AGENA_KEYFILELIBNAME);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_register(L, NULL, keyfile);
  luaL_register(L, NULL, keyfile_meta);
}

LUALIB_API int luaopen_keyfile (lua_State *L) {
  createmeta(L);
  lua_pop(L, 1);  /* 7.11.2 fix */
  luaL_register(L, AGENA_KEYFILELIBNAME, keyfile);
  return 1;
}
