#define ads_c
#define LUA_LIB

/* ////////////////////////////////////////////////////////////////////////
//                             DB VERSION 1.0                            //
//                          Author: Dr. F.H.Toor                         //
//                           Dated: 08/07/2003                           //
//                             Licence: FREE                             //
//                 modified and extended by Alexander Walz               //
/////////////////////////////////////////////////////////////////////////*/

/* initiated May 2007 */

#include <fcntl.h>   /* for open */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>    /* added */
#include <unistd.h>  /* for ftruncate and close */

#ifdef __DJGPP__
#include <io.h>      /* for _flush_disk_cache */
#include "liolib.h"  /* for tofile */
#endif

#include "agena.h"

#include "agenalib.h"
#include "agnxlib.h"
#include "vecoff64.h"

#if defined(__DJGPP__) || defined(OPENSUSE)
#include "agncmpt.h"
#endif


#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_ADSLIBNAME "ads"
LUALIB_API int (luaopen_ads) (lua_State *L);
#endif


#define STAMP                    0               /* start of stamp */
#define STAMP_TEXT               "AgenaBASE DATA SYSTEM  \0"   /* DB stamp */
#define STAMP_LEN                25              /* length of STAMP */
#define VERSION                  25L             /* start of db version */
#define VERSION_NUM              300L            /* DB version, 230L used for phonetiQs AGB version ! */
#define DESCRIPTION              29L             /* start of description section */
#define DESCRIPTION_LEN          75              /* length of description in bytes including terminal \0 */
#define DESCRIPTION_TEXT         "                                                                          \0"  /* 2.9.5 fix */
#define MAXNRECORDS              104L            /* start of number that stores the maximum number of records permitted */
#define MAXNRECORDS_NUM          20000           /* maximum number of records permitted */
#define ACTNRECORDS              108L            /* start of number that stores the current number of records stored to db */
#define KEYLENGTH                112L            /* start of number that stores the length of a key */
#define KEYLEN_NUM               30              /* maximum number of bytes of a key */
#define COLUMNS                  116L            /* start of number that stores the number of columns (including key) per dataset */
#define COLUMNS_NUM              2               /* Number of columns */
#define TYPE                     120L            /* position of db type, one byte */
#define DATEUPDATE               121L            /* XXX not used: start of last date of an update to db */
#define DATECREATION             125L            /* start of original date of creation of db */
#define COMMENT                  129L            /* position of comment */
#define ADS_OFFSET               256L            /* first position of an entry in the index section */
#define ADS_EXPANSION_SIZE       10L             /* number of records to be added in write op when db is full */


static int base_open (lua_State *L) {
  off64_t ver;
  int hnd, readwrite, i;
  const char *stamp, *file;
  stamp = STAMP_TEXT;
  file = luaL_checkstring(L, 1);
  agn_checkvalidpath(L, file, 0, "base.open");  /* 7.6.3 */
  /* no option given: open in readwrite mode, option given: open in readonly mode */
  readwrite = (lua_gettop(L) == 1);
  if (access(file, F_OK) == 0) {  /* file already exists ? */
    if (access(file, (readwrite) ? R_OK|W_OK : R_OK) == -1) {
      luaL_error(L, "Error in " LUA_QS ": lacking permissions for " LUA_QS ".", "ads.open", file);
    }
  }
  hnd = (readwrite) ? my_open(file) : my_roopen(file);
  if (hnd == -1)
    luaL_error(L, "Error in " LUA_QS ": cannot open file " LUA_QS ".", "ads.open", file);
  /* 1.6.7 fix */
  my_seek(hnd, STAMP);
  for (i=0; i < STAMP_LEN; i++) {
    if (my_readc(hnd) != *stamp++) {
      my_close(hnd);
      luaL_error(L, "Error in " LUA_QS ": %s is not an ADS base.", "ads.open", file);
    }
  }
  my_seek(hnd, VERSION);
  ver = my_readl(hnd);
  if (ver != VERSION_NUM) {
    my_close(hnd);  /* 1.6.7 */
    luaL_error(L, "Error, invalid base version %d of " LUA_QS ", need version %d.",
      (int)ver, file, (int)VERSION_NUM);  /* 6.6.11 fix, cannot use %ld in luaL_error */
  }
  /* enter new open file to global ads.openfiles table */
  if (agnL_gettablefield(L, "ads", "openfiles", "ads.open", 1) == LUA_TTABLE) {
    /* 0.20.2 & 1.6.4, avoid segmentation faults if openfiles does not exist */
    lua_pushinteger(L, hnd);
    lua_pushstring(L, file);
    lua_rawset(L, -3);
  }
  agn_poptop(L);  /* delete "openfiles" */
  lua_pushinteger(L, hnd);
  return 1;
}


/* usage: ads.create(filename::string,
                    max_num_records::long,
                    max_key_legth::long,
                    max_value_length::integer,
                    type::string,
                    description::string[max 24 chars]) */
static int base_createbase (lua_State *L) {
  off64_t i, columns, maxrec, maxkeylen;
  size_t l;
  int hnd, type, next, nargs, j;
  time_t seconds;
  char Desc[DESCRIPTION_LEN];
  const char *desc, *list, *file;
  nargs = lua_gettop(L);
  file = agn_checkstring(L, 1);  /* 0.13.4 patch */
  agn_checkvalidpath(L, file, 0, "ads.createbase");  /* 7.6.3 */
  if (nargs > 1 && agn_isnumber(L, 2)) {  /* 2.37.6 */
    maxrec = luaL_optoff64_t(L, 2, MAXNRECORDS_NUM);
    if (maxrec < 1)
      luaL_error(L, "Error in " LUA_QS ": " LUA_QS " must contain at least one record.", "ads.createbase", file);
    list = luaL_optstring(L, 3, "database");
    if (strcmp(list, "database") != 0 && strcmp(list, "list") != 0 && strcmp(list, "seq") != 0) {
      luaL_error(L, "Error in "LUA_QS ": unknown base type `%s` in file " LUA_QS ".", "ads.createbase", list, file);
    }
    if (strcmp(list, "database") == 0) {
      columns = luaL_optoff64_t(L, 4, COLUMNS_NUM);
      next = 5;
      type = 0;
    } else {
      columns = 1;
      next = 4;
      type = (strcmp(list, "list") == 0) ? 1 : 2;
    }
    maxkeylen = luaL_optoff64_t(L, next, KEYLEN_NUM) + 1;  /* including terminal \0 */
    if (columns < 1)
      luaL_error(L, "Error in " LUA_QS ": invalid number of columns in " LUA_QS ".", "ads.createbase", file);
    next++;
    desc = luaL_optlstring(L, next, DESCRIPTION_TEXT, &l);
    if (l > DESCRIPTION_LEN - 1)
      luaL_error(L, "Error in " LUA_QS ": description too long, pass up to %d characters.", "ads.createbase", DESCRIPTION_LEN - 1);
    strncpy(Desc, desc, l);
    for (j=l; j < DESCRIPTION_LEN; j++) Desc[j] = '\0';  /* prevent Valgrind exceptions, 2.37.6 */
  } else {  /* nargs is 1 or option pairs given; 2.37.6 extension */
    int checkoptions, descgiven;
    size_t len;
    /* set defaults */
    descgiven = 0;
    type = 0;  /* create database */
    columns = COLUMNS_NUM;
    maxrec = MAXNRECORDS_NUM;
    maxkeylen = KEYLEN_NUM + 1;  /* including terminal \0 */
    /* description is set after the loop if not given */
    checkoptions = 5;  /* check n options; CHANGE THIS if you add/delete options */
    if (nargs > 1 && lua_ispair(L, nargs))  /* 3.15.2 fix */
      luaL_checkstack(L, 2, "too many arguments");
    while (checkoptions-- && nargs > 1 && lua_ispair(L, nargs)) {
      agn_pairgeti(L, nargs, 1);  /* get left value, set to stack index -2 */
      agn_pairgeti(L, nargs, 2);  /* get right value, set to stack index  -1 */
      if (agn_isstring(L, -2)) {
        const char *option = agn_tostring(L, -2);
        if (tools_streqx(option, "basetype", "base", NULL)) {
          const char *str = lua_tolstring(L, -1, &len);
          if (!str)
            luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option requires a string, got %s.", "ads.createbase", "basetype", luaL_typename(L, -1));
          if (!tools_streqx(str, "database", "list", "seq", NULL))
            luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option must be \'database\', \'list\' or \'seq\', got " LUA_QS ".", "ads.createbase", "basetype", str);
          type = tools_streq(str, "database") ? 0 : (tools_streq(str, "list") ? 1 : 2);
        } else if (tools_streqx(option, "desc", "description", NULL)) {
          const char *str = lua_tolstring(L, -1, &len);
          if (!str)
            luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option requires a string, got %s.", "ads.createbase", "description", luaL_typename(L, -1));
          if (len > DESCRIPTION_LEN - 1)
            luaL_error(L, "Error in " LUA_QS ": description is too long, pass up to %d characters.", "ads.createbase", DESCRIPTION_LEN - 1);
          strncpy(Desc, str, len);
          for (j=len; j < DESCRIPTION_LEN; j++) Desc[j] = '\0';  /* prevent Valgrind exceptions, 2.37.6 */
          descgiven = 1;
        } else if (tools_streqx(option, "records", "recs", NULL)) {
          if (!agn_isnumber(L, -1) || !tools_isposint(agn_tonumber(L, -1)))
            luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option requires a positive integer.", "ads.createbase", "records");
          maxrec = agn_tonumber(L, -1);
        } else if (tools_streqx(option, "columns", "cols", NULL)) {
          if (!agn_isnumber(L, -1) || !tools_isposint(agn_tonumber(L, -1)))
            luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option requires a positive integer.", "ads.createbase", "columns");
          columns = (off64_t)((type == 0) ? agn_tonumber(L, -1) : 1L);
        } else if (tools_streqx(option, "keylen", "keylength", NULL)) {
          if (!agn_isnumber(L, -1) || !tools_isposint(agn_tonumber(L, -1)))
            luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option requires a positive integer.", "ads.createbase", "keylength");
          maxkeylen = (off64_t)agn_tonumber(L, -1) + 1L;  /* including terminal \0 */
        } else {
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", "ads.createbase", option);
        }
      }
      /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
      nargs--;
      agn_poptoptwo(L);
    }
    /* set defaults if option not explicitly given */
    if (type > 0) columns = 1;  /* list or sequence ? -> always one column only */
    if (!descgiven) {
      strncpy(Desc, DESCRIPTION_TEXT, DESCRIPTION_LEN - 1);
      Desc[DESCRIPTION_LEN - 1] = '\0';
    }
  }
  seconds = time(NULL);
  hnd = my_create(file);
  if (hnd == -1)  /* 2.37.6 fix */
    luaL_error(L, "Error in " LUA_QS ": could not create file.", "ads.createbase");
  my_write(hnd, STAMP_TEXT, STAMP_LEN);      /* stamp */
  my_writel(hnd, VERSION_NUM);               /* database version (4 bytes) */
  my_write(hnd, Desc, DESCRIPTION_LEN);      /* description */
  my_writel(hnd, maxrec);                    /* maximum number of entries allowed in base */
  my_writel(hnd, 0L);                        /* 0L, number of actual entries */
  my_writel(hnd, maxkeylen);                 /* maximum length of keys */
  my_writel(hnd, columns);                   /* number of columns */
  my_writec(hnd, type);                      /* sequence (2), list (1) or database (0) */
  my_writel(hnd, 0L);                        /* date of update */
  my_writel(hnd, (off64_t)seconds);          /* date of creation */
  my_writel(hnd, 0L);                        /* position of comment */
  my_writec(hnd, 1);                         /* endianness */
  for (i=0; i < 122; i++)
    my_writec(hnd, 0);                       /* reserved space: 17 fields, each 4 bytes long */
  if (type < 2)
    for (i=0; i < maxrec; i++) {
      my_writel(hnd, 0L);                    /* write zeros for key positions in index section */
    }
  my_close(hnd);
  return 0;
}


static int base_close (lua_State *L) {
  int nargs, hnd, i, result;
  result = 1;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments for " LUA_QS "ads.close");
  for (i=1; i <= nargs; i++) {
    hnd = agnL_checkinteger(L, i);
    if (hnd < 3) {  /* avoid standard streams to be closed */
      result = 0;
      continue;
    }
    if (!my_close(hnd) == 0)
      result = 0;
    else {
      /* delete file from global ads.openfiles table */
      if (agnL_gettablefield(L, "ads", "openfiles", "ads.close", 1) == LUA_TTABLE) {
        /* 0.20.2 & 1.6.4, avoid segmentation faults if openfiles does not exist */
        lua_pushinteger(L, hnd);
        lua_pushnil(L);
        lua_rawset(L, -3);
      }
      agn_poptop(L);  /* delete "openfiles" */
    }
  }
  lua_pushboolean(L, result);
  return 1;
}


#define STACKMAX   LUA_MINSTACK + 10

static int base_read (lua_State *L) {
  off64_t mid, low, high, pos, rcln, keylen, columns;
  size_t bufsize;
  int res, try, i, hnd;
  char listflag;
  const char *key;
  hnd = agnL_checkinteger(L, 1);
  key = luaL_checkstring(L, 2);
  low = 0;
  bufsize = agn_getbuffersize(L);
  try = my_seek(hnd, ACTNRECORDS);
  if (try == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.read", hnd);
  }
  high = my_readl(hnd) - 1L;
  if (high < 0) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    lua_pushnil(L);
    return 1;
  }
  keylen = my_readl(hnd);
  char tkey[keylen];
  columns = my_readl(hnd);
  listflag = my_readc(hnd);
  if (listflag == 2) {  /* read sequence; this is the fastest way known; storing the length for each item
    and reading it during file traversal - thus avoiding checking each char for \0 - is around 30 % slower */
    int i, j, k, oldj;
    char buffer[bufsize];  /* 2.34.9 adaption */
    char data[keylen];
    my_seek(hnd, ADS_OFFSET);
    while (1) {
      res = read(hnd, buffer, bufsize);  /* 2.34.9 adaption */
      if (res == -1) {  /* 1.6.11 */
        my_seek(hnd, 0L);  /* 2.11.0 RC2 */
        luaL_error(L, "Error in " LUA_QS ": could not read base.", "ads.read");
      }
      i = res - 1;  /* get number of characters read, adjust to index counting mode (indices start from 0 */
      /* now set pointer to end of the last item that is stored completely in current chunk */
      while (buffer[i] != '\0') i--;
      j = oldj = 0;
      while (j < i) {  /* from the beginning of the chunk read in item by item */
        k = 0;
        while (buffer[j] != '\0') {  /* assign item to data; memcpy is much slower  */
          data[k] = buffer[j];
          j++; k++;
        }
        data[k] = '\0';
        if (tools_streq(data, key)) {  /* compare item with key searched, 5.5.9 tweak */
          lua_pushtrue(L);
          return 1;
        }
        j++;  /* skip '\0' */
      }
      if (res != bufsize) break;  /* EOF reached, 2.34.9 adaption */
      lseek(hnd, i - bufsize + 1, SEEK_CUR);  /* 2.34.9 adaption */
    }  /* if end of buffer is reached, read in new chunk and parse it to the first \0 */
    lua_pushfalse(L);
    return 1;
  }
  while (low <= high) {
    mid = tools_midpoint(low, high);  /* 2.38.2 patch */
    my_seek(hnd, mid*4L + ADS_OFFSET);  /* move to index section */
    pos = my_readl(hnd);  /* read position in the entries section */
    my_seek(hnd, pos);    /* and change to it */
    my_read(hnd, tkey, my_readl(hnd));  /* read the first item, i.e. the key */
    res = strcmp(key, tkey);  /* is this the key we are searching for ? */
    if (res == 0) {
      if (listflag) {
        lua_pushtrue(L);
        return 1; }
      else {
        if (columns > STACKMAX) {
          if (lua_checkstack(L, columns - STACKMAX) == 0) { /* increase stack size */
            my_seek(hnd, 0L);  /* 2.11.0 RC2 */
            luaL_error(L, "Error in " LUA_QS ": could not increase stack size, too many items: %d.",
              "ads.read", (int)columns);  /* 6.6.11 fix, cannot use %ld in luaL_error */
          }
        }
        for (i=1; i < columns; i++) {  /* return the values, noot including the key */
          rcln = my_readl(hnd);
          char data[rcln];
          my_read(hnd, data, rcln);
          lua_pushstring(L, data);  /* pushlstring is not faster */
        }
        return columns - 1;
      }
    }
    if (res > 0)  /* search in upper half of index session ... */
      low = mid + 1;
    else  /* ... or in lower half */
      high = mid - 1;
  }
  if (listflag)
    lua_pushfalse(L);
  else
    lua_pushnil(L);
  return 1;
}


static int base_fastseek (lua_State *L) {
  off64_t mid, low, high, rcln, keylen, pos;
  int res, hnd, try;
  char listflag;
  const char *key;
  hnd = agnL_checkinteger(L, 1);
  key = luaL_checkstring(L, 2);
  luaL_checktype(L, 3, LUA_TTABLE);     /* a table containing all valid indices, see ads.indices */
  low = 0L;
  high = agnL_checkinteger(L, 4) - 1L;  /* actual number of records, must be > 0 */
  keylen = agnL_checkinteger(L, 5);     /* standard length of the key */
  listflag = agnL_checkinteger(L, 6);   /* 0 for database, 1 for list, 2 for sequence */
  char tkey[keylen];
  if (listflag > 1) {  /* got sequence */
    luaL_error(L, "Error in " LUA_QS ": function is not applicable to sequences.", "ads.fastseek");
  }
  try = my_seek(hnd, 0L);  /* 2.11.0 RC2 */
  if (try == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.fastseek", hnd);
  }
  while (low <= high) {
    mid = tools_midpoint(low, high);  /* 2.38.2 patch */
    pos = (off64_t)agn_getinumber(L, 3, mid);
    if (pos == 0) {  /* 2.37.7 */
      my_seek(hnd, 0L);  /* 2.37.7 */
      luaL_error(L, "Error in " LUA_QS ": invalid entry in table.", "ads.fastseek");
    }
    my_seek(hnd, pos);  /* 2nd arg: get value from table at idx 3 at given index mid */
    my_read(hnd, tkey, my_readl(hnd));
    res = strcmp(key, tkey);
    if (res == 0) {
      if (!listflag) {
        rcln = my_readl(hnd);
        char data[rcln];
        my_read(hnd, data, rcln);
        lua_pushlstring(L, data, rcln);  /* 2.37.7 change */
      } else
        lua_pushtrue(L);
      return 1;
    }
    if (res > 0)
      low = mid + 1;
    else
      high = mid - 1;
  }
  if (listflag)
    lua_pushfalse(L);
  else
    lua_pushnil(L);
  return 1;
}


/* searches for the given key (a string) and returns the index as a number */
static int base_index (lua_State *L) {
  off64_t mid, low, high, pos, keylen;
  int res, try, hnd;
  size_t bufsize;
  char listflag;
  const char *key;
  hnd = agnL_checkinteger(L, 1);
  key = luaL_checkstring(L, 2);
  low = 0;
  bufsize = agn_getbuffersize(L);
  try = my_seek(hnd, ACTNRECORDS);
  if (try == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.index", hnd);
  }
  high = my_readl(hnd) - 1L;
  if (high < 0) {
    my_seek(hnd, 0L);  /* 2.37.7 */
    lua_pushnil(L);
    return 1;
  }
  keylen = my_readl(hnd);
  char tkey[keylen];
  my_seek(hnd, TYPE);
  listflag = my_readc(hnd);
  if (listflag == 2) {  /* read sequence */
    int i, j, k;
    char buffer[bufsize];  /* 2.34.9 adaption */
    char data[keylen];
    my_seek(hnd, ADS_OFFSET);
    while (1) {
      res = read(hnd, buffer, bufsize);  /* 2.34.9 adaption */
      if (res == -1) {
      my_seek(hnd, 0L);  /* 2.37.7 */
        luaL_error(L, "Error in " LUA_QS ": could not read base.", "ads.index");
      }
      i = res - 1;
      while (buffer[i] != '\0') i--;
      j = 0;
      while (j < i) {
        k = 0;
        while (buffer[j] != '\0') {
          data[k] = buffer[j];
          j++; k++;
        }
        data[k] = '\0';
        if (strcmp(data, key) == 0) {
          lua_pushnumber(L, j - k);
          return 1;
        }
        j++;  /* skip '\0' */
      }
      if (res != bufsize) break;  /* EOF reached, 2.34.9 adaption */
      lseek(hnd, i - bufsize + 1, SEEK_CUR);  /* 2.34.9 adaption */
    }
    lua_pushfalse(L);
    return 1;
  }
  while (low <= high) {
    mid = tools_midpoint(low, high);  /* 2.38.2 patch */
    my_seek(hnd, mid*4L + ADS_OFFSET);
    pos = my_readl(hnd);
    my_seek(hnd, pos);
    my_read(hnd, tkey, my_readl(hnd));
    res = strcmp(key, tkey);
    if (res == 0) {
      lua_pushnumber(L, pos);
      return 1;
    }
    if (res > 0)
      low = mid + 1;
    else
      high = mid - 1;
  }
  if (listflag)
    lua_pushfalse(L);
  else
    lua_pushnil(L);
  return 1;
}


int seqiterator (lua_State *L) {  /* 2.9.5 */
  int hnd, keylen;
  ssize_t result, i;
  char *buffer;
  hnd = lua_tonumber(L, lua_upvalueindex(1));
  keylen = lua_tonumber(L, lua_upvalueindex(2));
  buffer = (char *)agn_malloc(L, (keylen + 1)*sizeof(char), "ads.iterate", NULL);
  result = read(hnd, buffer, keylen + 1);
  if (result == -1) {
    xfree(buffer);
    luaL_error(L, "Error in " LUA_QS ": error while reading sequence.", "ads.iterate");
  }
  if (result == 0)
    lua_pushnil(L);
  else {
    i = 0;
    while (buffer[i]) i++;
    lua_pushlstring(L, buffer, i);
    if (lseek(hnd, -result + i + 1, SEEK_CUR) == -1) { /* put pointer to next value to be read */
      xfree(buffer);
      my_seek(hnd, 0L);  /* 2.37.7 */
      luaL_error(L, "Error in " LUA_QS ": could not change file position.", "ads.iterate");
    }
  }
  xfree(buffer);
  return 1;
}

static int base_iterate (lua_State *L) {
  off64_t mid, low, high, pos, rcln, cnt, keylen, columns;
  size_t l;
  char listflag;
  const char *key1;
  int res, try, i, hnd;
  hnd = agnL_checkinteger(L, 1);
  key1 = luaL_optlstring(L, 2, "", &l);
  char key[l + 1];
  strcpy(key, key1);  /* 2.9.5 fix, copies terminating \0, as well */
  low = 0;
  try = my_seek(hnd, ACTNRECORDS);
  if (try == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.iterate", hnd);
  }
  cnt = my_readl(hnd);
  if (cnt < 1) {  /* base is empty ? */
    lua_pushnil(L);
    my_seek(hnd, 0L);  /* 2.37.7 */
    return 1;
  }
  high = cnt - 1;
  keylen = my_readl(hnd);
  char tkey[keylen];
  columns = my_readl(hnd);
  listflag = my_readc(hnd);
  if (listflag > 1) {  /* sequence ? */
    my_seek(hnd, ADS_OFFSET);   /* set file pointer to beginning of record section */
    luaL_checkstack(L, 2, "not enough stack space");  /* 3.18.4 fix */
    lua_pushinteger(L, hnd);  /* push file handle */
    lua_pushinteger(L, keylen);  /* push key length */
    lua_pushcclosure(L, &seqiterator, 2);
    return 1;
  }
  luaL_checkstack(L, listflag ? 1 : 3, "not enough stack space");
  if (l == 0) {  /* '' has been passed to get first record */
    my_seek(hnd, ADS_OFFSET);
    pos = my_readl(hnd);
    my_seek(hnd, pos);
    my_read(hnd, tkey, my_readl(hnd));
    lua_pushstring(L, tkey);
    if (listflag) return 1;
    lua_createtable(L, columns, 0);
    for (i=1; i < columns; i++) {
      rcln = my_readl(hnd);
      char data[rcln];
      my_read(hnd, data, rcln);
      lua_pushstring(L, data);
      lua_rawseti(L, -2, i);
    }
    return 2;
  }
  while (low <= high) {
    mid = tools_midpoint(low, high);  /* 2.38.2 patch */
    my_seek(hnd, mid*4L + ADS_OFFSET);
    pos = my_readl(hnd);
    my_seek(hnd, pos);
    my_read(hnd, tkey, my_readl(hnd));
    res = strcmp(key, tkey);
    if (res == 0) {
      if (mid == cnt - 1) {
        lua_pushnil(L);
        return 1;
      }
      mid++;
      my_seek(hnd, mid*4L + ADS_OFFSET);
      pos = my_readl(hnd);
      my_seek(hnd, pos);
      my_read(hnd, tkey, my_readl(hnd));
      if (listflag) {
        lua_pushstring(L, tkey);
        return 1;
      }
      lua_pushstring(L, tkey);
      lua_createtable(L, columns, 0);
      for (i=1; i < columns; i++) {
        rcln = my_readl(hnd);
        char data[rcln];
        my_read(hnd, data, rcln);
        lua_pushstring(L, data);
        lua_rawseti(L, -2, i);
      }
      return 2;
    }
    if (res > 0)
      low = mid + 1;
    else
      high = mid - 1;
  }
  lua_pushnil(L);
  return 1;
}


static int base_write (lua_State *L) {
  off64_t mid, low, high, pos, cnt, mrc, keylen, columns, cpos;
  size_t l, k, clen;
  int res, flag, i, hnd, rewritecomment, error, nargs;
  const char *key1;
  char listflag, *comment;
  comment = NULL;
  nargs = lua_gettop(L);  /* 2.37.6 fix to prevent IO errors */
  hnd = agnL_checkinteger(L, 1);
  if (my_seek(hnd, 0L) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.write", hnd);
  }
  key1 = luaL_checklstring(L, 2, &l);
  for (i=3; i <= nargs; i++) {  /* 2.37.6 fix, we do not check `columns` due to problems of off64_t's in loops in MinGW.
    We will apply the check here for otherwise we would have to roll back the changes, including restoring the old index section */
    if (!lua_stringconvertible(L, i))
      luaL_error(L, "Error in " LUA_QS " with argument #%d: expected a string or a value convertible to a string, got %s.",
        "ads.write", i, luaL_typename(L, i));
  }
  low = rewritecomment = 0;
  my_seek(hnd, MAXNRECORDS);
  mrc = my_readl(hnd);
  cnt = my_readl(hnd);
  high = cnt - 1;
  keylen = my_readl(hnd);
  columns = my_readl(hnd);
  listflag = my_readc(hnd);
  my_seek(hnd, COMMENT);  /* 2.11.0 RC2 fix */
  cpos = my_readl(hnd);
  clen = 0;
  if (cpos != 0) {  /* save comment; 2.11.0 RC2 fix */
    my_seek(hnd, cpos);
    clen = my_readl(hnd);
    comment = (char *)malloc(clen*sizeof(char));
    if (comment == NULL) {
      my_seek(hnd, 0L);
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ads.write");
    }
    my_read(hnd, comment, clen);
    rewritecomment = 1;
  }
  char key[l + 1];
  strcpy(key, key1);  /* copies terminating \0, as well */
  char tkey[keylen];
  if (listflag == 2) {  /* sequence */
    my_seek(hnd, my_lof(hnd));  /* write to end */
    if (l + 1 >= keylen) {      /* word plus \0 longer than maxkeylen ? */
      key[keylen - 1] = 0;      /* strings count from 0 */
      l = keylen - 1;           /* adjust length (i.e. cut string) */
    }
    my_write(hnd, key, l + 1);  /* l + 1: write string plus \0 */
    cnt++;
  } else {  /* base or list */
    flag = 0;
    if (mrc - cnt == 0) {  /* 0.32.0 */
      my_expand(hnd, mrc, cnt, ADS_EXPANSION_SIZE, &error);
      if (error) {  /* 2.11.0 RC2 fix */
        xfree(comment);  /* 5.5.8 fix */
        my_seek(hnd, 0L);  /* 2.37.6 fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ads.write");
      }
      mrc += ADS_EXPANSION_SIZE;
    }
    /* The index section contains the file positions of the keys of the actual data sets in the record section of the file.
       The references in the index section are sorted with respect to the actual keys to allow seek operations with O(log2). */
    mid = 0;  /* to avoid compiler warnings */
    while (low <= high) {
      mid = tools_midpoint(low, high);  /* 2.38.2 patch */
      my_seek(hnd, mid*4L + ADS_OFFSET);  /* seek index */
      pos = my_readl(hnd);                /* read index */
      my_seek(hnd, pos);                  /* set cursor to key position in record area */
      my_read(hnd, tkey, my_readl(hnd));  /* read key */
      res = strcmp(key, tkey);            /* compare found key with search key */
      if (res == 0) {
        cnt--;                            /* decrement number of actual entries */
        flag = 1;
        break;
      }
      if (res > 0)
        low = mid + 1;
      else
        high = mid - 1;
    }
    pos = (cpos == 0) ? my_lof(hnd) : cpos;
    if (flag && listflag) {  /* list and key already stored ? -> do nothing */
      my_seek(hnd, 0L);
      lua_pushtrue(L);
      return 1;
    }
    if (flag) {  /* does key already exist ? */
      /* prepare storing its new position at existing index position */
      my_seek(hnd, mid*4L + ADS_OFFSET);
    } else {
      if (low < cnt)  /* if new key */
        my_move(hnd, low*4L + ADS_OFFSET, low*4L + ADS_OFFSET + 4L, cnt*4L + ADS_OFFSET);  /* move keys upwards */
      my_seek(hnd, low*4L + ADS_OFFSET);
    }
    my_writel(hnd, pos);  /* insert new index position */
    my_seek(hnd, pos);    /* write key (and value) to end of file */
    if (l + 1 >= keylen) {  /* word plus \0 longer than maxkeylen ? */
      key[keylen - 1] = 0;
      l = keylen - 1;       /* adjust length (i.e. cut string) */
    }
    my_writel(hnd, l + 1);
    my_write(hnd, key, l + 1);
    pos += 4 + l + 1;
    if (!listflag) {
      /* write values */
      for (i=2; i <= columns; i++) {
        k = 0;
        const char *value = (i > nargs) ? NULL : lua_tolstring(L, i + 1, &k);  /* protect against converting non-existing arguments; 2.37.6 change */
        char data[k + 1];
        if (!value) value = "";  /* 2.37.6 */
        if (k > 0) strncpy(data, value, k);  /* 2.37.6 change */
        data[k] = '\0';
        my_writel(hnd, k + 1);
        my_write(hnd, data, k + 1);
        pos += 4 + k + 1;
      }
    }
    if (rewritecomment) {  /* re-write comment to eof */
      my_writel(hnd, clen);
      my_write(hnd, comment, clen);
      my_seek(hnd, COMMENT);
      my_writel(hnd, pos);
    }
    cnt++;
  }
  /* set number of entries */
  my_seek(hnd, ACTNRECORDS);
  my_writel(hnd, cnt);
  my_seek(hnd, 0L);
  lua_pushtrue(L);
  xfree(comment);
  return 1;
}


static int base_expand (lua_State *L) {
  off64_t mrc, cnt;
  int hnd, count, error;
  char listflag;
  hnd = agnL_checkinteger(L, 1);
  if (my_seek(hnd, 0L) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.expand", hnd);
  }
  count = agnL_optinteger(L, 2, 10);
  my_seek(hnd, MAXNRECORDS);
  mrc = my_readl(hnd);
  cnt = my_readl(hnd);
  my_seek(hnd, TYPE);
  listflag = my_readc(hnd);
  if (listflag > 1) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": function not applicable to sequences.", "ads.expand");
  }
  /* call expansion procedure;
     mrc: maximum number of records currently allowed,
     cnt: current number of actual records,
     count: number of records to be added */
  my_expand(hnd, mrc, cnt, count, &error);
  /* unlock file */
  my_seek(hnd, 0L);
  if (error) {
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ads.expand");
  }
  lua_pushtrue(L);
  return 1;
}


static int base_remove (lua_State *L) {
  off64_t mid, low, high, pos, cnt, keylen;
  int res, result, hnd;
  char listflag;
  const char *key = luaL_checkstring(L, 2);
  hnd = agnL_checkinteger(L, 1);
  result = low = 0;
  if (my_seek(hnd, 0L) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.remove", hnd);
  }
  my_seek(hnd, ACTNRECORDS);
  cnt = my_readl(hnd);
  high = cnt - 1;
  keylen = my_readl(hnd);
  my_seek(hnd, TYPE);
  listflag = my_readc(hnd);
  if (listflag > 1) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": function not applicable to sequences.", "ads.remove");
  }
  char tkey[keylen];
  while (low <= high) {
    mid = tools_midpoint(low, high);  /* 2.38.2 patch */
    my_seek(hnd, mid*4L + ADS_OFFSET);
    pos = my_readl(hnd);
    my_seek(hnd, pos);
    my_read(hnd, tkey, my_readl(hnd));
    res = strcmp(key, tkey);
    if (res == 0) {
      if (mid < cnt)
        my_move(hnd, mid*4L + ADS_OFFSET + 4L, mid*4L + ADS_OFFSET, cnt*4L + ADS_OFFSET);
      cnt--;
      my_seek(hnd, ACTNRECORDS);
      my_writel(hnd, cnt);
      result = 1;
      break;
    }
    if (res > 0)
      low = mid + 1;
    else
      high = mid - 1;
  }
  my_seek(hnd, 0L);
  lua_pushboolean(L, result);
  return 1;
}


/* assumes that table is on the top of the stack */
static void setintegerfield (lua_State *L, const char *key, int value) {
  lua_pushinteger(L, value);
  lua_setfield(L, -2, key);
}

/* assumes that table is on the top of the stack */
static void setstringfield (lua_State *L, const char *key, const char *value) {
  lua_pushstring(L, value);
  lua_setfield(L, -2, key);
}


static int base_attrib (lua_State *L) {
  off64_t columns, cnt, mrc, ver, keylen, cpos;
  time_t creationdate;
  char list, desc[DESCRIPTION_LEN], stamp[STAMP_LEN], datestring[72];  /* `2007/01/01-01:01:01` = 19 chars, changed 2.16.1 to 72 bytes due to GCC warning */
  struct tm *stm;  /* FIXME with agnt64.h */
  int hnd;
  hnd = agnL_checkinteger(L, 1);
  if (my_seek(hnd, 0L) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.attrib", hnd);
  }
  my_read(hnd, stamp, STAMP_LEN);
  ver = my_readl(hnd);
  my_read(hnd, desc, DESCRIPTION_LEN);
  mrc = my_readl(hnd);
  cnt = my_readl(hnd);
  keylen = my_readl(hnd);
  columns = my_readl(hnd);
  list = my_readc(hnd);
  my_seek(hnd, DATECREATION);
  creationdate = my_readl(hnd);
  stm = localtime(&creationdate);
  cpos = my_readl(hnd);
  lua_newtable(L);
  setstringfield(L, "stamp", stamp);
  setintegerfield(L, "version", ver);
  setintegerfield(L, "maxsize", mrc);
  setintegerfield(L, "size", cnt);
  setintegerfield(L, "keylength", keylen);
  setintegerfield(L, "columns", columns);
  /* setstringfield(L, "desc", desc); */
  setintegerfield(L, "type", list);
  setintegerfield(L, "indexstart", ADS_OFFSET);
  setintegerfield(L, "indexend", mrc*4L + ADS_OFFSET-1);
  setintegerfield(L, "commentpos", cpos);
  if (stm != NULL) {  /* 0.31.3 patch */
    sprintf(datestring, "%d/%02d/%02d-%02d:%02d:%02d",
      stm->tm_year+1900, stm->tm_mon + 1, stm->tm_mday, stm->tm_hour, stm->tm_min, stm->tm_sec);
  } else {
    sprintf(datestring, "%d/%02d/%02d-%02d:%02d:%02d", 0, 0, 0, 0, 0, 0);
  }
  setstringfield(L, "creation", datestring);
  setstringfield(L, "description", desc);  /* 2.11.0 RC2 */
  my_seek(hnd, 0L);
  return 1;
}


/* return the number of free entries */
static int base_free (lua_State *L) {
  off64_t cnt, mrc;
  int hnd;
  hnd = agnL_checkinteger(L, 1);
  if (my_seek(hnd, 0L) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.free", hnd);
  }
  my_seek(hnd, MAXNRECORDS);
  mrc = my_readl(hnd);
  cnt = my_readl(hnd);
  lua_pushinteger(L, mrc - cnt);
  my_seek(hnd, 0L);
  return 1;
}


/* base_getall: get all valid keys and entries of a database, list or sequence and return them in a new set.
   Argument: file handle (integer).

   Moving the cursor to the beginning of the dataset section and then sequentially reading all key-value pairs
   does not work properly since deleted sets are also returned. */
static int base_getall (lua_State *L) {
  off64_t i;
  char listflag;
  int hnd, n, option;  /* 1.12.9 */
  size_t bufsize = agn_getbuffersize(L);
  hnd = agnL_checkinteger(L, 1);
  option = lua_gettop(L) > 1;
  if (my_seek(hnd, COLUMNS) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.getall", hnd);
  }
  my_readl(hnd);  /* skip long, 1.12.9 */
  listflag = my_readc(hnd);
  my_seek(hnd, ACTNRECORDS);
  n = (int)my_readl(hnd);
  if (listflag > 1) {  /* sequence ? */
    int j, k, result, c;
    char *buffer, *data;
    c = 0;
    buffer = (char *)agn_malloc(L, (bufsize + 1)*sizeof(char), "ads.getall", NULL);  /* 2.34.9 adaption */
    my_seek(hnd, KEYLENGTH);
    int keylen = my_readl(hnd);
    data = (char *)agn_malloc(L, (keylen + 1)*sizeof(char), "ads.getall", buffer, NULL);  /* 1.9.1 */
    my_seek(hnd, ADS_OFFSET);
    if (option)
      agn_createseq(L, n);
    else
      agn_createset(L, n);
    do {
      result = read(hnd, buffer, bufsize);  /* 2.34.9 adaption */
      if (result == -1) {  /* 1.6.11 */
        xfreeall(buffer, data);  /* 2.9.8 */
        luaL_error(L, "Error in " LUA_QS ": error while reading sequence.", "ads.getall");
      }
      i = result - 1;
      while (buffer[i]) i--;  /* set buffer pointer back to last full entry read; short for (buffer[i] != '\0') */
      j = 0;
      while (j < i) {  /* process all entries completely read */
        k = 0;
        while (buffer[j]) {  /* short for (buffer[j] != '\0') */
          data[k] = buffer[j];
          j++; k++;
        }
        data[k] = '\0';  /* Agena 1.6.0 Valgrind */
        /* set values to set */
        if (option) {
          lua_seqrawsetistring(L, -1, ++c, data);
        } else
          lua_srawsetlstring(L, -1, data, k);
        j++;  /* skip '\0' */
      }
      if (lseek(hnd, i - bufsize + 1, SEEK_CUR) == -1) {  /* set will pointer to first word not fully read, 2.34.9 adaption */
        xfreeall(buffer, data);  /* 2.9.8 */
        luaL_error(L, "Error in " LUA_QS ": error moving file pointer.", "ads.getall");
      }
    } while (result == bufsize);  /* EOF not reached ?  2.34.9 adaption */
    xfreeall(buffer, data);  /* 2.9.8 */
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot process lists or bases.", "ads.getall");
    my_seek(hnd, 0L);
    return 0;
  }
  my_seek(hnd, 0L);
  return 1;
}


static int base_getkeys (lua_State *L) {
  off64_t cnt, keylen, i, j, columns;
  char listflag;
  int hnd;
  hnd = agnL_checkinteger(L, 1);
  if (my_seek(hnd, ACTNRECORDS) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.getkeys", hnd);
  }
  cnt = my_readl(hnd);
  if (cnt < 1) {
    lua_pushnil(L);
    return 1;
  }
  keylen = my_readl(hnd);
  char tkey[keylen];
  columns = my_readl(hnd);
  listflag = my_readc(hnd);
  if (listflag > 1) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": function not applicable to sequences.", "ads.getkeys");
  }
  my_seek(hnd, ADS_OFFSET);
  off64_t pos[cnt];
  /* read all indices */
  for (i=0; i < cnt; i++) {
    pos[i] = my_readl(hnd);
  }
  /* read keys from record section */
  lua_newtable(L);
  for (i = 0; i < cnt; i++) {
    my_seek(hnd, pos[i]);
    my_read(hnd, tkey, my_readl(hnd));
    lua_rawsetistring(L, -1, i + 1, tkey);
    if (!listflag) {
      for (j=1; j < columns; j++) {
        lseek(hnd, my_readl(hnd), SEEK_CUR);  /* skip values */
      }
    }
  }
  my_seek(hnd, 0L);
  return 1;
}


/* base_values: get all values from a table; May 12, 2007; changed July 20, 2007 */
static int base_getvalues (lua_State *L) {
  off64_t cnt, rcln, i, columns;
  int hnd, j, colnum;
  char listflag;
  hnd = agnL_checkinteger(L, 1);
  if (my_seek(hnd, ACTNRECORDS) == -1) {  /* file is not open ? */
    luaL_error(L,  "Error in " LUA_QS ": file #%d is not open.", "ads.getvalues", hnd);
  }
  colnum = agnL_optinteger(L, 2, 2);
  cnt = my_readl(hnd);
  if (cnt < 1) {
    lua_pushnil(L);
    return 1;
  }
  my_seek(hnd, COLUMNS);
  columns = my_readl(hnd);
  listflag = my_readc(hnd);
  if (listflag != 0) {
    lua_pushnil(L);
    return 1;
  }
  if (columns < colnum) {
    my_seek(hnd, 0L);  /* 2.37.6 fix */
    luaL_error(L, "Error in " LUA_QS ": column %d does not exist.", "ads.getvalues", colnum);
  }
  my_seek(hnd, ADS_OFFSET);
  off64_t pos[cnt];
  for (i=0; i < cnt; i++) {
    pos[i] = my_readl(hnd);
  }
  lua_newtable(L);
  for (i=0; i < cnt; i++) {
    my_seek(hnd, pos[i]);
    for (j=1; j < colnum; j++)
      lseek(hnd, my_readl(hnd), SEEK_CUR);  /* skip first n columns */
    rcln = my_readl(hnd);  /* read column */
    char data[rcln];
    my_read(hnd, data, rcln);
    lua_rawsetistring(L, -1, i + 1, data);
    for (j=colnum + 1; j <= columns; j++)
      lseek(hnd, my_readl(hnd), SEEK_CUR);
  }
  my_seek(hnd, 0L);
  return 1;
}


/* base_find: search a given column for a substring; July 21, 2007 */
static int base_find (lua_State *L) {
  off64_t cnt, rcln, i, j, columns, keylen;
  int hnd, colnum;
  size_t l;
  char listflag, *lookup;
  const char *pattern;
  hnd = agnL_checkinteger(L, 1);
  pattern = agn_checklstring(L, 2, &l);  /* 2.11.0 RC2 change */
  colnum = agnL_optnonnegint(L, 3, 2);   /* 2.37.7 */
  if (my_seek(hnd, ACTNRECORDS) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.find", hnd);
    lua_pushfail(L);
    return 1;
  }
  cnt = my_readl(hnd);  /* number of records */
  if (cnt < 1) {
    lua_pushnil(L);
    return 1;
  }
  my_seek(hnd, COLUMNS);
  columns = my_readl(hnd);
  listflag = my_readc(hnd);
  if (listflag != 0) {
    my_seek(hnd, 0L);  /* 2.37.7 */
    lua_pushnil(L);
    return 1;
  }
  if (columns < colnum) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": column %d does not exist.", "ads.find", colnum);
  }
  my_seek(hnd, ADS_OFFSET);
  /* read entire index section */
  off64_t pos[cnt];
  for (i = 0; i < cnt; i++) {
    pos[i] = my_readl(hnd);
  }
  luaL_checkstack(L, colnum == 0 ? 4 : 3, "not enough stack space");
  lua_newtable(L);
  if (colnum == 0) {  /* 2.37.7: search all columns but the first one */
    int first;
    for (i=0; i < cnt; i++) {
      first = 1;
      my_seek(hnd, pos[i]);
      keylen = my_readl(hnd);
      char indexkey[keylen];
      my_read(hnd, indexkey, keylen);
      for (j=2; j <= columns; j++) {
        rcln = my_readl(hnd);  /* read column */
        char data[rcln];
        my_read(hnd, data, rcln);
        lookup = agnL_strmatch(L, data, rcln, pattern, l);  /* 2.37.7 */
        if (lookup != NULL) {
          if (first) {
            first = 0;
            lua_pushstring(L, indexkey);
            lua_createtable(L, columns, 0);
          }
          lua_pushstring(L, data);
          lua_rawseti(L, -2, j);
        }
      }
      if (!first) lua_rawset(L, -3);
    }
  } else {
    /* in all records, for each key search specified column */
    for (i=0; i < cnt; i++) {
      my_seek(hnd, pos[i]);
      keylen = my_readl(hnd);
      char indexkey[keylen];
      my_read(hnd, indexkey, keylen);
      if (colnum == 1)
        lseek(hnd, -keylen - 4L, SEEK_CUR);  /* reset cursor to key length field */
      for (j=2; j < colnum; j++)
        lseek(hnd, my_readl(hnd), SEEK_CUR);  /* skip first n columns */
      rcln = my_readl(hnd);  /* read column */
      char data[rcln];
      my_read(hnd, data, rcln);
      lookup = agnL_strmatch(L, data, rcln, pattern, l);  /* 2.38.0 */
      if (lookup != NULL) {
        lua_pushstring(L, indexkey);
        lua_pushstring(L, data);
        lua_rawset(L, -3);
      }
      for (j=colnum + 1; colnum != 0 && j <= columns; j++)
        lseek(hnd, my_readl(hnd), SEEK_CUR);
    }
  }
  my_seek(hnd, 0L);
  return 1;
}


/* base_indices: return the file positions of all datasets as a table; May 12, 2007 */
static int base_indices (lua_State *L) {
  off64_t i, cnt;
  int hnd;
  char listflag;
  hnd = agnL_checkinteger(L, 1);
  if (my_seek(hnd, ACTNRECORDS) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.indices", hnd);
  }
  cnt = my_readl(hnd);
  if (cnt < 0) {  /* FIXME: can this even happen ? */
    my_seek(hnd, 0L);  /* 2.37.7 */
    lua_pushinteger(L, 0);
    return 1;
  }
  my_seek(hnd, TYPE);
  listflag = my_readc(hnd);
  if (listflag > 1) {
    my_seek(hnd, 0L);
    luaL_error(L, "Error in " LUA_QS ": function not applicable to sequences.", "ads.indices");
  }
  my_seek(hnd, ADS_OFFSET);  /* set file handler to beginning of index section */
  lua_newtable(L);
  for (i=0; i < cnt; i++) {
    agn_setinumber(L, -1, i + 1, my_readl(hnd));
  }
  my_seek(hnd, 0L);
  return 1;
}


/* puts a set with all valid and invalid indices on top of the stack */
int base_auxgetallindices (lua_State *L, int hnd, int mrc, int listflag) {
  off64_t dsbegin, dsend, cnt, i, k, c, z, cpos, columns;
  int flag;
  size_t lenkey, lenval;
  cnt = my_readl(hnd);
  my_seek(hnd, COLUMNS);
  columns = my_readl(hnd);
  my_seek(hnd, COMMENT);
  cpos = my_readl(hnd);
  off64_t indextbl[cnt];
  agn_createset(L, 0);
  c = 0;
  /* read all indices */
  my_seek(hnd, ADS_OFFSET);
  for (i=0; i < cnt; i++) {
    indextbl[i] = my_readl(hnd);
    c++;
    lua_srawsetnumber(L, -1, indextbl[i]);
  }
  /* sort them */
  tools_quicksort(indextbl, 0, cnt - 1);
  dsbegin = mrc*4L + ADS_OFFSET;  /* start of current dataset section */
  dsend = my_lof(hnd) - 1;        /* end of current dataset section */
  my_seek(hnd, dsbegin);
  z = dsbegin;
  while (z < dsend + 1) {
    /* binsearch indextbl for an entry of the current position */
    flag = !tools_binsearch(indextbl, cnt, z);
    if (flag && z != cpos) {  /* invalid key found that is not a comment ? */
      c++;
      lua_srawsetnumber(L, -1, z);
    }
    lenkey = my_readl(hnd) + 4L;
    /* set cursor to next entry */
    z += lenkey;
    my_seek(hnd, z);
    if (!listflag && z - lenkey != cpos) {
      lenval = 0;
      for (k=2; k <= columns; k++) {
        lenval = my_readl(hnd) + 4L;
        z += lenval;
        lseek(hnd, lenval - 4L, SEEK_CUR);
      }
    }
  }
  /* move back to beginning of file */
  my_seek(hnd, 0L);
  /* increase stack top */
  return 1;
}


/* base_retrieve: get key and value from a database (file handler, 1st arg) at the given file
   position (integer, 2nd arg). Returns are the respective key and its value. */
static int base_retrieve (lua_State *L) {
  off64_t high, pos, rcln, keylen, columns, mrc;
  int i, hnd;
  char listflag;
  hnd = agnL_checkinteger(L, 1);
  pos = agnL_checkinteger(L, 2);
  if (pos < ADS_OFFSET) {  /* otherwise there will be a crash with certain non-index file positions */
    luaL_error(L, "Error in " LUA_QS ": invalid position given for file with handle #%d.", "ads.retrieve", hnd);
  }
  if (my_seek(hnd, MAXNRECORDS) == -1) {  /* file is not open ? */
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.retrieve", hnd);
  }
  mrc = my_readl(hnd);
  high = my_readl(hnd) - 1L;
  if (high < 0) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    lua_pushnil(L);
    return 1;
  }
  keylen = my_readl(hnd);
  char tkey[keylen];
  columns = my_readl(hnd);
  listflag = my_readc(hnd);
  if (listflag > 1) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": function not applicable to sequences.", "ads.retrieve");
  }
  base_auxgetallindices(L, hnd, mrc, listflag);  /* get all valid and invalid indices as a set */
  lua_pushnumber(L, pos);
  lua_srawget(L, -2);
  if (lua_isfalse(L, -1)) {  /* 5.0.1 fix */
    agn_poptoptwo(L);  /* remove false and set */
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": invalid position given for file #%d.", "ads.retrieve", hnd);
  }
  agn_poptoptwo(L);  /* remove false and set */
  my_seek(hnd, pos);                      /* set cursor to key */
  my_read(hnd, tkey, my_readl(hnd));      /* read key */
  lua_newtable(L);
  lua_pushstring(L, tkey);
  lua_rawseti(L, -2, 1);
  if (!listflag) {
    for (i=1; i < columns; i++) {
      rcln = my_readl(hnd);
      char data[rcln];
      my_read(hnd, data, rcln);
      lua_pushstring(L, data);
      lua_rawseti(L, -2, i + 1);
    }
  }
  my_seek(hnd, 0L);
  return 1;
}


/* base.peek: get length and entry at given position */
static int base_peekin (lua_State *L) {
  off64_t mrc, pos, keylen, len;
  int hnd;
  char listflag;
  hnd = agnL_checkinteger(L, 1);
  pos = luaL_checkoff64_t(L, 2);
  if (my_seek(hnd, MAXNRECORDS) == -1) {  /* file is not open ? */
    luaL_error(L,  "Error in " LUA_QS ": file #%d is not open.", "ads.peekin", hnd);
  }
  mrc = my_readl(hnd);
  /* peeking the header or the index section is not allowed */
  if (pos < mrc*4L + ADS_OFFSET || pos >= my_lof(hnd)) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": invalid position given for file #%d.", "ads.peekin", hnd);
  }
  my_seek(hnd, KEYLENGTH);
  keylen = my_readl(hnd);
  char tkey[keylen];
  my_seek(hnd, TYPE);
  listflag = my_readc(hnd);
  if (listflag > 1) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": function not applicable to sequences.", "ads.peekin");
  }
  /* check whether pos is a valid index position */
  base_auxgetallindices(L, hnd, mrc, listflag);  /* get all valid and invalid indices as a set */
  lua_pushnumber(L, pos);
  lua_srawget(L, -2);
  if (lua_isfalse(L, -1)) {  /* 5.0.1 fix */
    agn_poptoptwo(L);  /* remove false and set */
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": invalid position given for file #%d.", "ads.peekin", hnd);
  }
  agn_poptoptwo(L);         /* remove false and set */
  my_seek(hnd, pos);        /* set cursor to key */
  len = my_readl(hnd);
  my_read(hnd, tkey, len);  /* read key */
  lua_pushnumber(L, len);
  lua_pushstring(L, tkey);
  my_seek(hnd, 0L);
  return 2;
}


/* base_size: number of valid entries; July 06, 2007 */
static int base_sizeof (lua_State *L) {
  int hnd;
  off64_t pos;
  hnd = agnL_checkinteger(L, 1);
  pos = my_seek(hnd, ACTNRECORDS);
  if (pos == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.sizeof", hnd);
  }
  lua_pushnumber(L, my_readl(hnd));
  my_seek(hnd, 0L);
  return 1;
}


/* set description field in database; May 12, 2007; extended July 21, 2007 */
static int base_desc (lua_State *L) {
  int hnd;
  size_t l;
  const char *desc;
  char Desc[DESCRIPTION_LEN];
  hnd = agnL_checkinteger(L, 1);
  if (my_seek(hnd, DESCRIPTION) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.desc", hnd);
  }
  if (lua_gettop(L) == 1) {
    my_read(hnd, Desc, DESCRIPTION_LEN);
    lua_pushstring(L, (tools_streq(Desc, DESCRIPTION_TEXT)) ? "": Desc);  /* 5.5.9 tweak */
    my_seek(hnd, 0L);  /* 2.37.7 */
    return 1;
  }
  desc = luaL_checklstringornil(L, 2, &l);  /* 2.38.0 extension */
  if (l > DESCRIPTION_LEN - 1) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": description too long; must be %d chars.", "ads.desc", DESCRIPTION_LEN - 1);
  }
  strcpy(Desc, desc);
  Desc[l] = '\0';
  my_write(hnd, Desc, l + 1);  /* 2.11.0 RC2 fix */
  lua_pushtrue(L);
  my_seek(hnd, 0L);
  return 1;
}


static int base_comment (lua_State *L) {
  off64_t pos, cpos, length, clen, cur, cnt, i, ind;
  size_t l, len, bufsize;
  int try, hnd;
  char listflag;  /* 2.34.9 adaption */
  const char *comment;
  bufsize = agn_getbuffersize(L);
  char buffer[bufsize];
  hnd = agnL_checkinteger(L, 1);
  try = my_seek(hnd, 0L);
  if (try == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.comment", hnd);
  }
  listflag = my_readc(hnd);
  if (listflag == 2) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": function not applicable to sequences.", "ads.comment");
  }
  my_seek(hnd, COMMENT);
  cpos = my_readl(hnd);
  if (lua_gettop(L) == 1) {
    if (cpos == 0) {
      lua_pushnil(L);
    } else {
      my_seek(hnd, cpos);
      len = my_readl(hnd);
      char comment[len];
      my_read(hnd, comment, len);
      lua_pushstring(L, comment);
    }
    my_seek(hnd, 0L);  /* 2.37.7 */
    return 1;
  }
  my_seek(hnd, 0L);
  my_seek(hnd, TYPE);
  comment = luaL_checklstringornil(L, 2, &l);  /* 2.38.0 */
  char value[l + 1];
  strcpy(value, comment);
  value[l] = '\0';
  if (cpos != 0) {
    /* delete comment and shift all records to the left */
    length = my_lof(hnd);
    my_seek(hnd, cpos);                    /* read it */
    clen = my_readl(hnd) + 4L;             /* length of comment including length info */
    cur = cpos + clen;
    while (length - cur + 1 >= bufsize) {  /* 2.34.9 adaption */
      my_seek(hnd, cur);                   /* set cursor to record following invalid one */
      my_read(hnd, buffer, bufsize);       /* read bufsize chars, 2.34.9 adaption */
      my_seek(hnd, cur - clen);            /* set cursor to begin of invalid record */
      my_write(hnd, buffer, bufsize);      /* shift valid sets to the left */
      cur += bufsize;
    }
    if (cur < length) {
      my_seek(hnd, cur);
      my_read(hnd, buffer, length - cur);  /* read rest */
      my_seek(hnd, cur - clen);
      my_write(hnd, buffer, length - cur);
    }
    /* truncate base */
    try = ftruncate(hnd, length - clen);
    if (try == -1) {
      /* unlock file */
      my_seek(hnd, 0L);
      luaL_error(L, "Error in " LUA_QS " while truncating file %d.", "ads.comment", hnd);
    }
    /* read indices */
    my_seek(hnd, MAXNRECORDS);
    my_readl(hnd);  /* skip long, 1.12.9 */
    cnt = my_readl(hnd);
    my_seek(hnd, ADS_OFFSET);
    lua_newtable(L);
    for (i=0; i < cnt; i++) {
      agn_setinumber(L, -1, i + 1, my_readl(hnd));
    }
    /* update indices */
    my_seek(hnd, ADS_OFFSET);
    for (i=0; i < cnt; i++) {
      ind = agn_getinumber(L, -1, i + 1);
      if (ind > cpos)
        my_writel(hnd, ind - clen);
      else
        lseek(hnd, 4L, SEEK_CUR);
    }
  }
  pos = (tools_streq(comment, "")) ? 0L : my_lof(hnd);  /* 5.5.9 tweak */
  /* prepare storing its new position at existing COMMENT position */
  my_seek(hnd, COMMENT);
  my_writel(hnd, pos);  /* insert comment position */
  /* write comment to end of file */
  if (pos != 0) {  /* new comment to be added ? */
    my_seek(hnd, pos);
    my_writel(hnd, l + 1);  /* including \0 */
    my_write(hnd, value, l + 1);
  }
  /* unlock file */
  my_seek(hnd, 0L);
  lua_pushtrue(L);
  return 1;
}


/* base_sync: flushes all unwritten content to the database file. */
static int base_sync (lua_State *L) {
  int hnd = agnL_checkinteger(L, 1);
  /* file is not open ? -> leave this here since there will be lock errors later with closed files*/
  if (my_seek(hnd, 0L) == -1) {
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.sync", hnd);
  }
  /* filepos is at position 0 already */
  if (!tools_fsync(hnd))
    lua_pushtrue(L);
  else
    lua_pushfail(L);
  my_seek(hnd, 0L);
  return 1;
}


/* get all invalid (old) entries in a database, June 25, 2007 */
static int base_invalids (lua_State *L) {
  off64_t dsbegin, dsend, mrc, cnt, i, k, c, z, cpos, columns, *indextbl;
  int hnd, flag;
  size_t lenkey, lenval;
  char listflag;
  hnd = agnL_checkinteger(L, 1);
  if (my_seek(hnd, 0L) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.invalids", hnd);
  }
  my_seek(hnd, MAXNRECORDS);
  mrc = my_readl(hnd);
  cnt = my_readl(hnd);
  my_seek(hnd, COLUMNS);
  columns = my_readl(hnd);
  listflag = my_readc(hnd);
  if (listflag > 1) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": function not applicable to sequences.", "ads.invalids");
  }
  my_seek(hnd, COMMENT);
  cpos = my_readl(hnd);
  indextbl = malloc(cnt * sizeof(off64_t));
  if (indextbl == NULL) {
    my_seek(hnd, 0L);  /* 2.11.0 RC2 */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ads.invalids");
  }
  /* read all indices */
  my_seek(hnd, ADS_OFFSET);
  for (i=0; i < cnt; i++)
    indextbl[i] = my_readl(hnd);
  /* sort them */
  tools_quicksort(indextbl, 0, cnt - 1);
  dsbegin = mrc*4L + ADS_OFFSET;  /* start of current dataset section */
  dsend = my_lof(hnd) - 1;        /* end of current dataset section */
  my_seek(hnd, dsbegin);
  lua_newtable(L);
  z = dsbegin;
  c = 0;
  while (z < dsend + 1) {
    /* binsearch indextbl for an entry of the current position */
    flag = !tools_binsearch(indextbl, cnt, z);
    if (flag && z != cpos) {  /* invalid key found that is not the comment ? */
      agn_setinumber(L, -1, ++c, z);
    }
    lenkey = my_readl(hnd) + 4L;
    /* set cursor to next entry */
    z += lenkey;
    my_seek(hnd, z);
    if (!listflag && z - lenkey != cpos) {
      lenval = 0;
      for (k=2; k <= columns; k++) {
        lenval = my_readl(hnd) + 4L;
        z += lenval;
        lseek(hnd, lenval - 4L, SEEK_CUR);
      }
    }
  }
  /* unlock file */
  my_seek(hnd, 0L);
  xfree(indextbl);
  return 1;
}


static int base_clean (lua_State *L) {
  off64_t dsend, mrc, cnt, i, j, k, z, ind, length, cur, num, cpos, columns, lenvalues;
  int hnd, flag, verbose, top, try;
  size_t lenkey, lenval, totlen, bufsize;
  char listflag;
  double time;
  off64_t tobeshortened;
  bufsize = agn_getbuffersize(L);
  char buffer[bufsize];  /* 2.34.9 adaption */
  /* _vector_init(indices, sizeof(off64_t)); */
  Vector indices, invalid;
  vector_init(&indices);
  if (indices.data == NULL)  /* 2.11.0 RC 2 modification, use C vector arrays instead of Agena tables */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ads.clean");
  /* _vector_init(invalid, sizeof(off64_t)); */
  vector_init(&invalid);
  if (invalid.data == NULL)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ads.clean");
  hnd = agnL_checkinteger(L, 1);
  verbose = (agnL_optinteger(L, 2, 0));
  if (my_seek(hnd, 0L) == -1) {  /* file is not open ? */
    luaL_error(L, "Error in " LUA_QS ": file #%d is not open.", "ads.clean", hnd);
  }
  time = clock();
  length = my_lof(hnd);
  my_seek(hnd, MAXNRECORDS);
  mrc = my_readl(hnd);  /* max number of records */
  cnt = (off64_t)my_readl(hnd);  /* max number of valid datasets */
  my_seek(hnd, COLUMNS);
  columns = my_readl(hnd);
  listflag = my_readc(hnd);
  if (listflag > 1) {
    my_seek(hnd, 0L);
    luaL_error(L, "Error in " LUA_QS ": function not applicable to sequences.", "ads.clean");
  }
  my_seek(hnd, COMMENT);
  cpos = (off64_t)my_readl(hnd);
  off64_t indextbl[cnt];
  /* read all indices */
  if (verbose) fprintf(stderr, "Reading index section ... ");
  my_seek(hnd, ADS_OFFSET);
  /* store valid index in a C array and in a Lua table. The C array gets sorted afterwards.
     Using a Lua table is necessary here since too large C arrays crash the system. <- XXX ??? */
  for (i=0; i < cnt; i++) {
    ind = (off64_t)my_readl(hnd);
    indextbl[i] = (off64_t)ind;
    /* _vector_add(indices, ind); */
    if (vector_add(&indices, ind)) {
      my_seek(hnd, 0L);  /* 2.37.6 fix */
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ads.clean");
    }
  }
  /* sort them */
  tools_quicksort(indextbl, 0, cnt - 1);
  if (verbose) {
    const char *str = (cnt == 1) ? "position" : "positions";
    fprintf(stderr, "got %ld ", (long int)cnt);  /* 2.11.0 RC2 fix, otherwise %s in the same format string would return (null) for str */
    fprintf(stderr, "%s.\n", str);
  }
  dsend = my_lof(hnd) - 1;       /* FIXME, is this necessary ? end of current dataset section */
  z = mrc*4L + ADS_OFFSET;       /* start of current dataset section */
  my_seek(hnd, z);
  if (verbose) fprintf(stderr, "Scanning invalid records ... ");
  /* z is cursor */
  while (z < dsend + 1) {
    /* binsearch indextbl for an entry of the current position */
    flag = !tools_binsearch(indextbl, cnt, z);
    if (flag && z != cpos) {  /* file position is not in index table */
      /* _vector_add(invalid, z); */
      if (vector_add(&invalid, z)) {
        my_seek(hnd, 0L);  /* 2.37.6 fix */
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ads.clean");
      }
    }
    lenkey = my_readl(hnd) + 4L;
    /* set cursor to next entry */
    z += lenkey;
    my_seek(hnd, z);
    if (!listflag && z - lenkey != cpos) {
      lenval = 0;
      for (k=2; k <= columns; k++) {
        lenval = my_readl(hnd) + 4L;
        z += lenval;
        lseek(hnd, lenval - 4L, SEEK_CUR);
      }
    }
  }
  /* top = _vector_size(invalid); */
  top = invalid.size;
  if (top == 0) {
    if (verbose) fprintf(stderr, "Nothing to be cleaned.\n");
    /* unlock file */
    my_seek(hnd, 0L);
    /*_vector_free(indices);
      _vector_free(invalid);*/
    vector_free(&indices);
    vector_free(&invalid);
    lua_pushfalse(L);
    return 1;
  }
  if (cpos != 0) {
    /* _vector_add(invalid, cpos); */
    if (vector_add(&invalid, cpos)) {
      my_seek(hnd, 0L);  /* 2.37.6 fix */
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ads.clean");
    }
  }
  if (verbose) {
    const char *str = (top == 1) ? "position" : "positions";
    fprintf(stderr, "got %d %s.\n", top, str);
  }
  tobeshortened = 0;  /* of type off64_t */
  if (verbose) fprintf(stderr, "Reshifting valid records ... ");
  /* for each invalid record do */
  totlen = 0;
  for (i=0; i < top; i++) {
    /* ind = _vector_getoff64_t(invalid, i); */    /* position of invalid item in record section */
    ind = vector_getoff64_t(&invalid, i);
    my_seek(hnd, ind);                       /* read it */
    lenkey = my_readl(hnd) + 4L;             /* length of key including length info */
    my_seek(hnd, ind + lenkey);              /* set cursor to value */
    lenval = 0;
    if (!listflag) {
      for (j=2; j <= columns; j++) {         /* determine entire length of second to last column */
        lenvalues = my_readl(hnd) + 4L;
        lenval += lenvalues;                 /* length of value including length info */
        lseek(hnd, lenvalues - 4L, SEEK_CUR);
      }
    }
    totlen = lenkey + lenval;                /* lengths of key and value plus length info */
    cur = ind + totlen;
    while (length - cur + 1 >= bufsize) {  /* 2.34.9 adaption */
      my_seek(hnd, cur);                       /* set cursor to record following invalid one */
      my_read(hnd, buffer, bufsize);     /* read bufsize chars, 2.34.9 adaption */
      my_seek(hnd, cur - totlen);              /* set cursor to begin of invalid record */
      my_write(hnd, buffer, bufsize);    /* shift valid sets to the left, 2.34.9 adaption */
      cur += bufsize;  /* 2.34.9 adaption */
    }
    if (cur < length) {                        /* read and move rest */
      my_seek(hnd, cur);
      my_read(hnd, buffer, length - cur);
      my_seek(hnd, cur - totlen);
      my_write(hnd, buffer, length - cur);
    }
    /* update valid indices Lua table and also position of comment */
    for (j=0; j < cnt; j++) {  /* for all datasets do */
      /* num = _vector_getoff64_t(indices, j); */
      num = vector_getoff64_t(&indices, j);
      if (num > ind) {
        /* _vector_set(indices, j, num - totlen); */
        vector_set(&indices, j, num - totlen);
      }
    }
    /* update invalid indices */
    for (j=i + 1; j < top; j++) {
      /* _vector_set(invalid, j, _vector_getoff64_t(invalid, j) - totlen); */
      vector_set(&invalid, j, vector_getoff64_t(&invalid, j) - totlen);
    }
    tobeshortened += totlen;
  }
  if (verbose) {
    fprintf(stderr, "Done.\n");
#ifndef __DJGPP__
    fprintf(stderr, "Truncating file by %ld bytes ... ", (long int)tobeshortened);
#else
    fprintf(stderr, "Truncating file by %lld bytes ... ", (long long int)tobeshortened);
#endif
  }
  try = ftruncate(hnd, length - tobeshortened);
  if (try == -1) {
    /* unlock file */
    my_seek(hnd, 0L);
    luaL_error(L, "Error while truncating file, length=%d\n", length - tobeshortened);
  }
  if (verbose) {
    fprintf(stderr, "Done.\n");
    fprintf(stderr, "Rebuilding index ... ");
  }
  /* rewriting index section */
  my_seek(hnd, ADS_OFFSET);
  for (i=0; i < cnt; i++) {
    /* my_writel(hnd, _vector_getoff64_t(indices, i)); */
    my_writel(hnd, vector_getoff64_t(&indices, i));
  }
  if (verbose) {
    fprintf(stderr, "Done.\n");
    time = (clock() - time)/CLOCKS_PER_SEC;
    fprintf(stderr, "All done in %1.2f seconds.\n", (float)time);
  }
  /* reset comment position or set it unchanged */
  if (cpos != 0) {  /* comment exists and is at end of file, 2.11.0 RC 2 optimisation */
    cpos -= tobeshortened;
    my_seek(hnd, COMMENT);
    my_writel(hnd, cpos);
  }
  /* unlock file */
  my_seek(hnd, 0L);
  /* _vector_free(indices);
  _vector_free(invalid); */
  vector_free(&indices);
  vector_free(&invalid);
  lua_pushtrue(L);
  return 1;
}


static int base_filepos (lua_State *L) {
  int hnd;
  off64_t fpos;
  hnd = agnL_checkinteger(L, 1);
  fpos = my_fpos(hnd);
  if (fpos == -1)
    lua_pushfalse(L);
  else
    lua_pushnumber(L, fpos);
  return 1;
}


static int base_lock (lua_State *L) {  /* 0.32.5 */
  int hnd;
  size_t nargs;
  off64_t start, size;
  hnd = agnL_checkinteger(L, 1);
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1) {  /* lock entire file (in Windows lock 2^63 bytes only) */
    start = 0;
    size = 0;
  } else {
    /* lock from current file position */
    start = my_fpos(hnd);
    size = agnL_checknumber(L, 2);
    if (size < 0) {
      my_seek(hnd, 0L);  /* 2.37.6 fix */
      luaL_error(L, "Error in " LUA_QS ": must lock at least one byte.", "ads.lock");
    }
  }
  lua_pushboolean(L, my_lock(hnd, start, size) == 0);
  return 1;
}


static int base_unlock (lua_State *L) {  /* 0.32.5 */
  int hnd;
  size_t nargs;
  off64_t start, size;
  hnd = agnL_checkinteger(L, 1);
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1) {  /* lock entire file */
    start = 0;
    size = 0;
  } else {
    /* lock from current file position */
    start = my_fpos(hnd);
    size = agnL_checknumber(L, 2);
    if (size < 0) {
      my_seek(hnd, 0L);  /* 2.37.6 fix */
      luaL_error(L, "Error in " LUA_QS ": must unlock at least one byte.", "ads.unlock");
    }
  }
  lua_pushboolean(L, my_unlock(hnd, start, size) == 0);
  return 1;
}


static const luaL_Reg ads[] = {
  {"attrib", base_attrib},                /* May 06, 2007 */
  {"clean", base_clean},                  /* July 06, 2007 */
  {"close", base_close},                  /* May 05, 2007 */
  {"comment", base_comment},              /* July 11, 2007 */
  {"createbase", base_createbase},        /* May 05, 2007 */
  {"desc", base_desc},                    /* May 12, 2007 */
  {"expand", base_expand},                /* June 25, 2007 */
  {"fastseek", base_fastseek},            /* July 12, 2007 */
  {"filepos", base_filepos},              /* June 28, 2007 */
  {"free", base_free},                    /* July 06, 2007 */
  {"getall", base_getall},                /* April 08, 2008; tweaked December 24, 2008 */
  {"getkeys", base_getkeys},              /* May 06, 2007 */
  {"getvalues", base_getvalues},          /* May 12, 2007 */
  {"index", base_index},                  /* July 12, 2007 */
  {"indices", base_indices},              /* May 12, 2007 */
  {"invalids", base_invalids},            /* June 25, 2007 */
  {"iterate", base_iterate},              /* May 06, 2007 */
  {"lock", base_lock},                    /* June 12, 2010 */
  {"open", base_open},                    /* May 05, 2007 */
  {"peekin", base_peekin},                /* July 12, 2007 */
  {"find", base_find},                    /* July 21, 2007 */
  {"read", base_read},                    /* May 06, 2007 */
  {"remove", base_remove},                /* May 06, 2007 */
  {"retrieve", base_retrieve},            /* May 06, 2007 */
  {"sizeof", base_sizeof},                /* May 06, 2007 */
  {"sync", base_sync},                    /* May 12, 2007 */
  {"unlock", base_unlock},                /* June 12, 2010 */
  {"write", base_write},                  /* May 06, 2007 */
  {NULL, NULL}
};


/*
** Open base library
*/
LUALIB_API int luaopen_ads (lua_State *L) {
  luaL_register(L, AGENA_ADSLIBNAME, ads);
  lua_newtable(L);
  lua_setfield(L, -2, "openfiles");  /* table ads.openfiles for information on all open files */
  return 1;
}

