/* lua-miniz - Lua module for miniz support, written by leso-kn.
 *
 * Taken from: https://codeberg.org/leso-kn/lua-miniz, written by leso-kn, mirrored at:
 * https://luarocks.org/modules/leso-kn/lua-miniz
 *
 * This repository is a fork of https://github.com/richgel999/miniz
 *
 * Changes for Lua:
 *  - Reduced to miniz functionality without the xslx module
 *  - Luarocks packaging
 *  - Raw access to internal file-descriptor (`za:rawfd()/za@@rawfd()`)
 *
 * This module adds deflate/inflate and zip file operations support to the Lua language.
 * Some code has been taken from luvit's miniz module, thanks for the job !
 *
 * Changes for Agena 5.3.0/5.3.1:
 *  - Changed the code to compile with old-style 32-bit MinGW/GCC 9.2.0 and with the
 *    Lua 5.1 C API.
 *  - Switched on long file support for OS/2, DOS, Solaris and 32-bit Linux.
 *  - The is_file_a_directory() method has been renamed isdir().
 *  - Changed `__len` metamethod to `__size`.
 *  - `minizip.decompress` would never return when given an empty string. Instead, the function
 *    now just returns its argument, that is the empty string.
 *  - Stack space is now explicitly reserved by Lreader_close().
 *  - Added minizip functions `attribs`, `close`, `count`, `open`, `index`, `isdir`, `read`, `write`.
 *  - Security issue in Lfile_read() has been removed.
 *
 * Licence: MIT, see below.
 *
 * Copyright 2025 Lesosoftware (https://leso.dev)
 * Copyright 2024 Xavier Wang (xavierxwang@gmail.com)
 * Copyright 2013-2014 RAD Game Tools and Valve Software [miniz.c]
 * Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC [miniz.c]
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#define LUA_LIB

#include <stdio.h>
#include <string.h>
#include <stdint.h>  /* for uint8_t */
#include <errno.h>   /* for errno */

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_MINIZIPLIBNAME "minizip"
LUALIB_API int (luaopen_zip)  (lua_State *L);
#endif

#define MINIZ_NO_ZLIB_APIS
#include "miniz.h"

#define return_self(L) do { lua_settop(L, 1); return 1; } while (0)

static int iszipfile (const char *filepath);  /* forward declaration */

static int Ladler32 (lua_State *L) {
  size_t len;
  const char *s = luaL_optlstring(L, 1, NULL, &len);
  mz_ulong init;
  if (!lua_isnoneornil(L, 2))
    init = (mz_ulong)luaL_checkinteger(L, 2);
  else
    init = mz_adler32(0, NULL, 0);
  if (s == NULL) {
    lua_pushinteger(L, init);
    return 1;
  }
  lua_pushinteger(L, (lua_Integer)mz_adler32(init, (const unsigned char*)s, len));
  return 1;
}


static int Lcrc32 (lua_State *L) {
  size_t len;
  const char *s = luaL_optlstring(L, 1, NULL, &len);
  mz_ulong init;
  if (!lua_isnoneornil(L, 2))
    init = (mz_ulong)luaL_checkinteger(L, 2);
  else
    init = mz_crc32(0, NULL, 0);
  if (s == NULL) {
    lua_pushinteger(L, init);
    return 1;
  }
  lua_pushinteger(L, (lua_Integer)mz_crc32(init, (const unsigned char*)s, len));
  return 1;
}

#define LMZ_COMPRESSOR   "miniz.Compressor"
#define LMZ_DECOMPRESSOR "miniz.Decompressor"

typedef tdefl_compressor lmz_Comp;

typedef struct lmz_Decomp {
  tinfl_decompressor decomp;
  mz_uint   flags;
  mz_uint8 *curr;
  mz_uint8  dict[TINFL_LZ_DICT_SIZE];
} lmz_Decomp;


static void lmz_initcomp (lua_State *L, int start, lmz_Comp *c) {
  static const mz_uint probes[11] = {0, 1, 6, 32, 16, 32, 128, 256, 512, 768, 1500};
  int level = (int)luaL_optinteger(L, start, MZ_DEFAULT_LEVEL);
  mz_uint flags = probes[(level >= 0) ? MZ_MIN(10, level) : MZ_DEFAULT_LEVEL];
  tdefl_status status;
  if (lua_tointeger(L, start+1) >= 0) flags |= TDEFL_WRITE_ZLIB_HEADER;
  if (level <= 3) flags |= TDEFL_GREEDY_PARSING_FLAG;
  if ((status = tdefl_init(c, NULL, NULL, flags)) != TDEFL_STATUS_OKAY)
    luaL_error(L, "compress failure (%d)", status);
}


static void lmz_initdecomp (lua_State *L, int start, lmz_Decomp *d) {
  int window_bits = (int)luaL_optinteger(L, start, 0);
  d->flags = window_bits >= 0 ? TINFL_FLAG_PARSE_ZLIB_HEADER : 0;
  d->flags |= TINFL_FLAG_HAS_MORE_INPUT;
  d->curr = d->dict;
  tinfl_init(&d->decomp);
}


static int lmz_compress (lua_State *L, int start, lmz_Comp *c, int flush) {
  size_t len, offset = 0, output = 0;
  const char *s = luaL_checklstring(L, start, &len);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (;;) {
    size_t in_size = len - offset;
    size_t out_size = LUAL_BUFFERSIZE;
    tdefl_status status = tdefl_compress(c, s + offset, &in_size,
          (mz_uint8*)luaL_prepbuffer(&b), &out_size, flush);
    offset += in_size;
    output += out_size;
    luaL_addsize(&b, out_size);
    if (status == TDEFL_STATUS_DONE) {
      luaL_pushresult(&b);
      lua_pushboolean(L, status == TDEFL_STATUS_DONE);
      lua_pushinteger(L, len);
      lua_pushinteger(L, output);
      return 4;
    } else if (status != TDEFL_STATUS_OKAY)
      luaL_error(L, "compress failure (%d)", status);
  }
}


static int lmz_decompress (lua_State *L, int start, lmz_Decomp *d) {
  size_t len, offset = 0, output = 0;
  const char *s = luaL_checklstring(L, start, &len);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (;;) {
    size_t in_size  = len - offset;
    size_t out_size = TINFL_LZ_DICT_SIZE - (d->curr - d->dict);
    tinfl_status status = tinfl_decompress(&d->decomp,
          (void*)(s + offset), &in_size, d->dict, d->curr, &out_size,
          d->flags & ~TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    offset += in_size;
    output += out_size;
    if (out_size != 0) luaL_addlstring(&b, (char*)d->curr, out_size);
    if (status == TINFL_STATUS_DONE) {
      luaL_pushresult(&b);
      lua_pushboolean(L, status == TINFL_STATUS_DONE);
      lua_pushinteger(L, len);
      lua_pushinteger(L, output);
      return 4;
    } else if (status < 0)
      luaL_error(L, "decompress failure (%d)", status);
    d->curr = &d->dict[(d->curr+out_size - d->dict) & (TINFL_LZ_DICT_SIZE-1)];
  }
}


static int Lcomp_tostring (lua_State *L) {
  lmz_Comp *c = luaL_checkudata(L, 1, LMZ_COMPRESSOR);
  lua_pushfstring(L, LMZ_COMPRESSOR ": %p", c);
  return 1;
}


static int Lcomp_call (lua_State *L) {
  static const char *opts[] = { "sync", "full", "finish", NULL };
  static int flushes[] = { TDEFL_SYNC_FLUSH, TDEFL_FULL_FLUSH, TDEFL_FINISH };
  lmz_Comp *c = luaL_checkudata(L, 1, LMZ_COMPRESSOR);
  int flush = luaL_checkoption(L, 3, "sync", opts);
  return lmz_compress(L, 2, c, flushes[flush]);
}


static int Lcompress (lua_State *L) {
  lua_settop(L, 3);
  if (lua_type(L, 1) == LUA_TSTRING) {
    lmz_Comp c;
    lmz_initcomp(L, 2, &c);
    return lmz_compress(L, 1, &c, TDEFL_FINISH);
  } else {
    lmz_Comp *c = lua_newuserdata(L, sizeof(lmz_Comp));
    lmz_initcomp(L, 1, c);
    if (luaL_newmetatable(L, LMZ_COMPRESSOR)) {
      lua_pushcfunction(L, Lcomp_tostring);
      lua_setfield(L, -2, "__tostring");
      lua_pushcfunction(L, Lcomp_call);
      lua_setfield(L, -2, "__call");
    }
    lua_setmetatable(L, -2);
    return 1;
  }
}


static int Ldecomp_tostring (lua_State *L) {
  lmz_Decomp *d = luaL_checkudata(L, 1, LMZ_DECOMPRESSOR);
  lua_pushfstring(L, LMZ_DECOMPRESSOR ": %p", d);
  return 1;
}


static int Ldecomp_call (lua_State *L) {
  lmz_Decomp *d = luaL_checkudata(L, 1, LMZ_COMPRESSOR);
  return lmz_decompress(L, 2, d);
}


static int Ldecompress (lua_State *L) {
  if (lua_gettop(L) == 0)
    luaL_error(L, "Error in " LUA_QS ": need at least one argument.", "minizip.decompress");
  if (lua_type(L, 1) == LUA_TSTRING) {
    lmz_Decomp d;
    size_t l;
    /* 5.3.0 fix, with an empty string the function would never return. `minizip.compress`
       called with an empty string, returns successfully, though. */
    (void)lua_tolstring(L, 1, &l);
    if (l == 0) {
      /* luaL_error(L, "Error in " LUA_QS ": argument is the empty string.", "minizip.decompress"); */
      lua_settop(L, 1);  /* return empty string */
      return 1;
    }
    lmz_initdecomp(L, 2, &d);
    return lmz_decompress(L, 1, &d);
  } else {
    lmz_Decomp *d = (lmz_Decomp*)lua_newuserdata(L, sizeof(lmz_Decomp));
    /* check for a number as argument #1 */
    lmz_initdecomp(L, 1, d);
    if (luaL_newmetatable(L, LMZ_DECOMPRESSOR)) {
      lua_pushcfunction(L, Ldecomp_tostring);
      lua_setfield(L, -2, "__tostring");
      lua_pushcfunction(L, Ldecomp_call);
      lua_setfield(L, -2, "__call");
    }
    lua_setmetatable(L, -2);
    return 1;
  }
}


/* zip reader */

#define LMZ_ZIP_READER "minizip.ZipReader"

static int lmz_zip_pusherror (lua_State *L, mz_zip_archive *za, const char *prefix) {
  mz_zip_error err = mz_zip_get_last_error(za);
  const char *emsg = mz_zip_get_error_string(err);
  lua_pushnil(L);
  if (prefix == NULL)
    lua_pushstring(L, emsg);
  else
    lua_pushfstring(L, "%s(%s)", prefix, emsg);
  return 2;
}

static int Lzip_read_string (lua_State *L) {
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  mz_uint32 flags = (mz_uint32)luaL_optinteger(L, 2, 0);
  mz_zip_archive *za = lua_newuserdata(L, sizeof(mz_zip_archive));
  mz_zip_zero_struct(za);
  if (!mz_zip_reader_init_mem(za, s, len, flags))
    return lmz_zip_pusherror(L, za, NULL);
  luaL_setmetatable(L, LMZ_ZIP_READER);
  lua_pushvalue(L, 1);
  lua_rawsetp(L, LUA_REGISTRYINDEX, za);
  return 1;
}

static int Lzip_read_file (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  mz_uint32 flags = (mz_uint32)luaL_optinteger(L, 2, 0);
  mz_zip_archive *za = lua_newuserdata(L, sizeof(mz_zip_archive));
  mz_zip_zero_struct(za);
  if (!mz_zip_reader_init_file(za, filename, flags))
    return lmz_zip_pusherror(L, za, filename);
  luaL_setmetatable(L, LMZ_ZIP_READER);
  return 1;
}

static int Lreader_close (lua_State *L) {
  mz_zip_archive* za = luaL_checkudata(L, 1, LMZ_ZIP_READER);
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.3.1 fix */
  lua_pushboolean(L, mz_zip_reader_end(za));
  lua_pushnil(L);
  lua_rawsetp(L, LUA_REGISTRYINDEX, za);
  return 1;
}

static int Lreader___index (lua_State* L) {
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_READER);
  int type = lua_type(L, 2);
  if (type == LUA_TSTRING) {
    if (lua_getmetatable(L, 1)) {
      lua_pushvalue(L, 2);
      lua_rawget(L, -2);
      return 1;
    }
    return 0;
  }
  else if (type == LUA_TNUMBER) {
    mz_uint file_index = (mz_uint)luaL_checkinteger(L, 2) - 1;
    char filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
    if (!mz_zip_reader_get_filename(za, file_index,
              filename, MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE))
      return lmz_zip_pusherror(L, za, NULL);
    lua_pushstring(L, filename);
    return 1;
  }
  return 0;
}

static int Lreader_stat (lua_State* L);

static int Lreader___inext (lua_State* L) {
  int i = luaL_checkinteger(L, 2);
  i++;
  lua_pushinteger(L, i);
  lua_pushcfunction(L, Lreader_stat);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, i);
  lua_call(L, 2, 1);
	if (lua_type(L, -1) == LUA_TNIL) {
    return 0;
	}
  return 2;
}


static int Lreader___ipairs (lua_State* L) {
  lua_pushcfunction(L, Lreader___inext);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, 0);
  return 3;
}


static int Lreader___tostring (lua_State* L) {
  mz_zip_archive *za = luaL_testudata(L, 1, LMZ_ZIP_READER);
  if (za) lua_pushfstring(L, "minizip.ZipReader(%p)", za);
  else luaL_tolstring(L, 1, NULL);
  return 1;
}


static int Lreader_get_num_files (lua_State *L) {
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_READER);
  lua_pushinteger(L, mz_zip_reader_get_num_files(za));
  return 1;
}


static int Lreader_get_offset (lua_State *L) {
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_READER);
  lua_pushinteger(L, (lua_Integer)mz_zip_get_archive_file_start_offset(za));
  lua_pushinteger(L, (lua_Integer)mz_zip_get_archive_size(za));
  return 2;
}


static int Lreader_locate_file (lua_State *L) {
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_READER);
  const char *path = luaL_checkstring(L, 2);
  mz_uint32 flags = (mz_uint32)luaL_optinteger(L, 3, 0);
  int index = mz_zip_reader_locate_file(za, path, NULL, flags);
  if (index < 0) return lmz_zip_pusherror(L, za, path);
  lua_pushinteger(L, index + 1);
  return 1;
}


static int Lreader_stat (lua_State* L) {
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_READER);
  mz_uint file_index = (mz_uint)luaL_checkinteger(L, 2) - 1;
  mz_zip_archive_file_stat stat;
  if (!mz_zip_reader_file_stat(za, file_index, &stat))
    return lmz_zip_pusherror(L, za, NULL);
  lua_newtable(L);
  lua_pushinteger(L, file_index);
  lua_setfield(L, -2, "index");
  lua_pushinteger(L, stat.m_version_made_by);
  lua_setfield(L, -2, "version_made_by");
  lua_pushinteger(L, stat.m_version_needed);
  lua_setfield(L, -2, "version_needed");
  lua_pushinteger(L, stat.m_bit_flag);
  lua_setfield(L, -2, "bit_flag");
  lua_pushinteger(L, stat.m_method);
  lua_setfield(L, -2, "method");
  lua_pushinteger(L, (lua_Integer)stat.m_time);
  lua_setfield(L, -2, "time");
  lua_pushinteger(L, stat.m_crc32);
  lua_setfield(L, -2, "crc32");
  lua_pushinteger(L, (lua_Integer)stat.m_comp_size);
  lua_setfield(L, -2, "comp_size");
  lua_pushinteger(L, (lua_Integer)stat.m_uncomp_size);
  lua_setfield(L, -2, "uncomp_size");
  lua_pushinteger(L, stat.m_internal_attr);
  lua_setfield(L, -2, "internal_attr");
  lua_pushinteger(L, stat.m_external_attr);
  lua_setfield(L, -2, "external_attr");
  lua_pushstring(L, stat.m_filename);
  lua_setfield(L, -2, "filename");
  lua_pushstring(L, stat.m_comment);
  lua_setfield(L, -2, "comment");
  return 1;
}


static int Lreader_get_filename (lua_State* L) {
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_READER);
  mz_uint file_index = (mz_uint)luaL_checkinteger(L, 2) - 1;
  char filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
  if (!mz_zip_reader_get_filename(za, file_index,
      filename, MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE))
    return lmz_zip_pusherror(L, za, NULL);
  lua_pushstring(L, filename);
  return 1;
}


static int Lreader_is_file_a_directory (lua_State  *L) {  /* isdir function */
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_READER);
  mz_uint file_index = (mz_uint)luaL_checkinteger(L, 2) - 1;
  lua_pushboolean(L, mz_zip_reader_is_file_a_directory(za, file_index));
  return 1;
}


static size_t Lwriter (void *ud, mz_uint64 file_ofs, const void *p, size_t n) {
  (void)file_ofs;
  luaL_addlstring((luaL_Buffer*)ud, p, n);
  return n;
}


static int Lreader_extract (lua_State *L) {
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_READER);
  mz_uint flags = (mz_uint)luaL_optinteger(L, 3, 0);
  int type = lua_type(L, 2);
  luaL_Buffer b;
  mz_bool result = 0;
  luaL_buffinit(L, &b);
  if (type == LUA_TSTRING)
    result = mz_zip_reader_extract_file_to_callback(za,
          lua_tostring(L, 2),
          Lwriter, &b, flags);
  else if (type == LUA_TNUMBER)
    result = mz_zip_reader_extract_to_callback(za,
          (mz_uint)lua_tointeger(L, 2) - 1,
          Lwriter, &b, flags);
  luaL_pushresult(&b);
  return result ? 1 : 0;
}

#if MZ_FILE == FILE
typedef struct {
  void *m_p;
  size_t m_size, m_capacity;
  mz_uint m_element_size;
} mz_zip_array;


struct mz_zip_internal_state_tag {
  mz_zip_array m_central_dir;
  mz_zip_array m_central_dir_offsets;
  mz_zip_array m_sorted_central_dir_offsets;
  /* The flags passed in when the archive is initially opened. */
  mz_uint32 m_init_flags;
  /* MZ_TRUE if the archive has a zip64 end of central directory headers, etc. */
  mz_bool m_zip64;
  /* MZ_TRUE if we found zip64 extended info in the central directory (m_zip64 will also be slammed to true too, even if we didn't find a zip64 end of central dir header, etc.) */
  mz_bool m_zip64_has_extended_info_fields;
  /* These fields are used by the file, FILE, memory, and memory/heap read/write helpers. */
  MZ_FILE *m_pFile;
  mz_uint64 m_file_archive_start_ofs;
  void *m_pMem;
  size_t m_mem_size;
  size_t m_mem_capacity;
};

typedef struct {
	FILE *fd;
} LMZFILE;


static int Lfile_tostring (lua_State *L) {
  LMZFILE *f = luaL_testudata(L, 1, "mzfile");
	lua_pushfstring(L, "zipfile %p", f->fd);
	return 1;
}


static int Lfile_read (lua_State *L) {
  size_t l;
  LMZFILE *f = luaL_testudata(L, 1, "mzfile");
	LUA_INTEGER len = luaL_checkinteger(L, 2);
	char *buf = malloc(len);
	l = fread(buf, len, 1, f->fd);
	lua_pushlstring(L, buf, l);  /* 5.3.1 fix */
  xfree(buf);  /* 5.3.1 fix */
	return 1;
}


static int Lfile_write (lua_State *L) {
  LMZFILE *f = luaL_testudata(L, 1, "mzfile");
	size_t len;
	const char *s = luaL_checklstring(L, 2, &len);
	lua_pushinteger(L, fwrite(s, len, 1, f->fd));
	return 1;
}


static int Lfile_seek (lua_State *L) {
  LMZFILE *f = luaL_testudata(L, 1, "mzfile");
	LUA_NUMBER offset = lua_tonumber(L, 2);
	const char *swhence = luaL_tolstring(L, 3, 0);
	int whence = SEEK_CUR;
	if (swhence && tools_streq(swhence, "set")) whence = SEEK_SET;  /* 5.5.9 tweak */
	else if (swhence && tools_streq(swhence, "end")) whence = SEEK_END;  /* 5.5.9 tweak */
	if (fseek(f->fd, offset, whence))
    return luaL_error(L, "<zipfile>: %s", strerror(errno));
	lua_pushinteger(L, ftell(f->fd));
	return 1;
}


static int Lgeneric_rawfd (lua_State* L, const char *target) {
  mz_zip_archive *za = luaL_testudata(L, 1, target);
	LMZFILE *f = lua_newuserdata(L, sizeof(LMZFILE));
	f->fd = za->m_pState->m_pFile;
	luaL_Reg libs[] = {
#define ENTRY(name) { #name, Lfile_##name }
		ENTRY(read),
		ENTRY(write),
		ENTRY(seek),
#undef ENTRY
		{NULL, NULL}
	};
	if (luaL_newmetatable(L, "mzfile")) {
    luaL_setfuncs(L, libs, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, Lfile_tostring);
  lua_setfield(L, -2, "__tostring");
	}
  lua_setmetatable(L, -2);
	return 1;
}


static int Lreader_rawfd(lua_State* L) {
	return Lgeneric_rawfd(L, LMZ_ZIP_READER);
}
#endif

static void open_zipreader (lua_State *L) {
  luaL_Reg libs[] = {
    {"__size", Lreader_get_num_files},
    {"__gc",   Lreader_close},
    {"isdir",  Lreader_is_file_a_directory},
    {"num",    Lreader_get_num_files},
    {"offset", Lreader_get_offset},
    {"index",  Lreader_locate_file},
#define ENTRY(name) { #name, Lreader_##name }
    ENTRY(__index),
    ENTRY(__inext),
    ENTRY(__tostring),
    ENTRY(__ipairs),
    ENTRY(close),
    ENTRY(get_num_files),
    ENTRY(locate_file),
    ENTRY(stat),
    ENTRY(get_filename),
    ENTRY(is_file_a_directory),
    ENTRY(extract),
    ENTRY(get_offset),
#if MZ_FILE == FILE
    ENTRY(rawfd),
#endif
#undef ENTRY
    {NULL, NULL}
  };
  if (luaL_newmetatable(L, LMZ_ZIP_READER))
    luaL_setfuncs(L, libs, 0);
}

/* zip writer */

#define LMZ_ZIP_WRITER "miniz.ZipWriter"

#if MD_FILE == FILE
static int Lwriter_rawfd (lua_State* L) {
	return Lgeneric_rawfd(L, LMZ_ZIP_WRITER);
}
#endif


static int Lwriter___tostring(lua_State* L) {
  mz_zip_archive *za = luaL_testudata(L, 1, LMZ_ZIP_WRITER);
  if (za) lua_pushfstring(L, "miniz.ZipWriter: %p", za);
  else luaL_tolstring(L, 1, NULL);
  return 1;
}


static int Lzip_write_string (lua_State *L) {
  size_t size_to_reserve_at_beginning = (size_t)luaL_optinteger(L, 1, 0);
  size_t initial_allocation_size = (size_t)luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
  mz_zip_archive* za = (mz_zip_archive*)lua_newuserdata(L, sizeof(mz_zip_archive));
  mz_zip_zero_struct(za);
  if (!mz_zip_writer_init_heap(za,
      size_to_reserve_at_beginning, initial_allocation_size))
    return lmz_zip_pusherror(L, za, NULL);
  luaL_setmetatable(L, LMZ_ZIP_WRITER);
  return 1;
}


static int Lzip_write_file (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  size_t size_to_reserve_at_beginning = (size_t)luaL_optinteger(L, 2, 0);
  mz_zip_archive* za = (mz_zip_archive*)lua_newuserdata(L, sizeof(mz_zip_archive));
  mz_zip_zero_struct(za);
  if (!mz_zip_writer_init_file(za, filename, size_to_reserve_at_beginning))
    return lmz_zip_pusherror(L, za, filename);
  luaL_setmetatable(L, LMZ_ZIP_WRITER);
  return 1;
}


static int Lwriter_close (lua_State *L) {
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_WRITER);
  if (mz_zip_get_mode(za) != MZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED)
    mz_zip_writer_finalize_archive(za);
  lua_pushboolean(L, mz_zip_writer_end(za));
  return 1;
}


static int Lwriter_add_from_zip_reader (lua_State *L) {
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_WRITER);
  mz_zip_archive *src = luaL_checkudata(L, 2, LMZ_ZIP_READER);
  mz_uint file_index = (mz_uint)luaL_checkinteger(L, 3) - 1;
  if (!mz_zip_writer_add_from_zip_reader(za, src, file_index))
    return lmz_zip_pusherror(L, za, NULL);
  return_self(L);
}


static int Lwriter_add_string (lua_State *L) {
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_WRITER);
  const char* path = luaL_checkstring(L, 2);
  size_t len, comment_len;
  const char* s = luaL_checklstring(L, 3, &len);
  const char *comment = luaL_optlstring(L, 5, NULL, &comment_len);
  mz_uint flags = (mz_uint)luaL_optinteger(L, 4, MZ_DEFAULT_LEVEL);
  if (!mz_zip_writer_add_mem_ex(za, path, s, len,
      comment, (mz_uint16)comment_len, flags, 0, 0))
    return lmz_zip_pusherror(L, za, path);
  return_self(L);
}


static int Lwriter_add_file (lua_State *L) {
  size_t len;
  mz_zip_archive *za = luaL_checkudata(L, 1, LMZ_ZIP_WRITER);
  const char* path = luaL_checkstring(L, 2);
  const char* filename = luaL_optstring(L, 3, path);
  mz_uint flags = (mz_uint)luaL_optinteger(L, 4, MZ_DEFAULT_LEVEL);
  const char *comment = luaL_optlstring(L, 5, NULL, &len);
  if (!mz_zip_writer_add_file(za, path, filename, comment, (mz_uint16)len, flags))
    return lmz_zip_pusherror(L, za, filename);
  return_self(L);
}


static int Lwriter_finalise (lua_State *L) {
  mz_zip_archive *za = (mz_zip_archive*)luaL_checkudata(L, 1, LMZ_ZIP_WRITER);
  if (mz_zip_get_type(za) == MZ_ZIP_TYPE_HEAP) {
    size_t len = 0;
    void* s = NULL;
    mz_bool result = mz_zip_writer_finalize_heap_archive(za, &s, &len);
    lua_pushlstring(L, s, len);
    free(s);
    return result ? 1 : lmz_zip_pusherror(L, za, NULL);
  } else if (!mz_zip_writer_finalize_archive(za))
    return lmz_zip_pusherror(L, za, NULL);
  return_self(L);
}


/* When called with just a filename, a string, opens the given ZIP archive in read mode and returns a file handle.
   When called with a filename, a string, and a mode, a string, too, opens the given ZIP archive in the following mode
   and returns a file handle:
   mode = 'r': read mode,
   mode = 'w': write mode.

   In read mode, you may optionally specify one of the following flags as a third argument:
     0x100: case sensitive file name in zip file,
     0x200: ignore path of file in zip,
     0x400: file is compressed data,
     0x800: do not sort central directory in zip file.

  In write mode, you can also specify the initial number of bytes to reserve by passing a non-negative integer as a
  third argument.

  The function combines the functionality of former `minizip.read` and `minizip.write`. 5.3.1 */
static int minizip_open (lua_State *L) {
  int op = 0, nargs = lua_gettop(L);
  mz_uint32 flags = 0;
  const char *filename = luaL_checkstring(L, 1);
  static const char *const mode[] =      {"r", "w", "r",    "w",     NULL};
  static const char *const modenames[] = {"r", "w", "read", "write", NULL};
  agn_checkvalidpath(L, filename, 0, "minizip.open");  /* 7.6.3 */
  if (nargs == 1 || agn_isstring(L, 2))
    op = luaL_checkoption(L, 2, "r", modenames);
  if (tools_streq(mode[op], "r")) {
    mz_zip_archive *za;
    if (!iszipfile(filename))  /* 5.3.3 extension */
      luaL_error(L, "Error in " LUA_QS ": input file is not a zip file.", "minizip.open");
    if (nargs > 1 && agn_isinteger(L, nargs))
      flags = (mz_uint32)agnL_optnonnegint(L, nargs, 0);
    za = lua_newuserdata(L, sizeof(mz_zip_archive));
    mz_zip_zero_struct(za);
    if (!mz_zip_reader_init_file(za, filename, flags)) {
      return lmz_zip_pusherror(L, za, filename);
    }
    luaL_setmetatable(L, LMZ_ZIP_READER);
  } else if (tools_streq(mode[op], "w")) {
    size_t size_to_reserve_at_beginning = (size_t)agnL_optnonnegint(L, 3, 0);
    mz_zip_archive *za = (mz_zip_archive*)lua_newuserdata(L, sizeof(mz_zip_archive));
    mz_zip_zero_struct(za);
    if (!mz_zip_writer_init_file(za, filename, size_to_reserve_at_beginning))
      return lmz_zip_pusherror(L, za, filename);
    luaL_setmetatable(L, LMZ_ZIP_WRITER);
  } else {
    luaL_error(L, "Error in " LUA_QS ": this should not happen.", "minizip.open");
  }
  return 1;
}


static int minizip_close (lua_State *L) {
  int rc = 0;
  mz_zip_archive *za = NULL;
  if (luaL_isudata(L, 1, LMZ_ZIP_READER)) {
    za = lua_touserdata(L, 1);
    rc = mz_zip_reader_end(za);
    lua_pushnil(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, za);
  } else if (luaL_isudata(L, 1, LMZ_ZIP_WRITER)) {
    za = lua_touserdata(L, 1);
    if (mz_zip_get_mode(za) != MZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED)
      mz_zip_writer_finalize_archive(za);
    rc = mz_zip_writer_end(za);
  } else {
    luaL_error(L, "Error in " LUA_QS ": miniz.ZipWriter expected.", "minizip.close");
  }
  lua_pushboolean(L, rc);
  return 1;
}


static void aux_checkoptions (lua_State *L, int pos, int *nargs, char **file, char **contents, const char *procname) {
  int checkoptions;
  *file = NULL;
  *contents = NULL;
  /* check for options, here `map in-place` */
  checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 fix */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "file") && agn_isstring(L, -1)) {  /* 5.5.1 fix */
        const char *fn = agn_tostring(L, -1);
        xfree(*file);  /* 5.5.1 fix */
        *file = tools_strdup(fn);
        if (!*file) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      } else if (tools_streq(option, "contents") && agn_isstring(L, -1)) {  /* 5.5.1 fix */
        const char *c = agn_tostring(L, -1);
        xfree(*contents);  /* 5.5.1 fix */
        *contents = tools_strdup(c);
        if (!*contents) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      } else {
        xfreeall(*file, *contents);
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS " or wrong right-hand side.", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
}

static int minizip_write (lua_State *L) {  /* 5.3.1 */
  char *file = NULL;
  char *contents = NULL;
  int nrets = 0, nargs = lua_gettop(L);
  aux_checkoptions(L, 3, &nargs, &file, &contents, "minizip.write");
  if (file == NULL) {  /* write the given string, the 3rd argument */
    if (contents) {
      lua_pushstring(L, contents);
      lua_replace(L, 3);
      lua_settop(L, 3);
    }
    if (agn_isstring(L, 3)) {
      nrets = Lwriter_add_string(L);
    } else if (luaL_isudata(L, 2, LMZ_ZIP_READER) && agn_isnonnegint(L, 3)) {  /* try read-mode ZIP handle, 5.3.2 */
      Lwriter_add_from_zip_reader(L);
    } else {
      luaL_error(L, "Error in " LUA_QS ": wrong kind of argument.", "minizip.write");
    }
  } else {  /* write from the file, the 3rd argument */
    lua_pushstring(L, file);
    lua_replace(L, 3);
    lua_settop(L, 3);
    nrets = Lwriter_add_file(L);
  }
  xfreeall(file, contents);
  return nrets;
}


/* This function has been created by gemini.google.com. Put into the public domain, see `licence below`.
 *
 * @brief Checks if a file is a ZIP archive by examining its magic number.
 *
 * A standard ZIP file starts with the local file header signature 'PK\x03\x04'.
 * This function reads the first 4 bytes of the file and compares them to this signature.
 *
 * @param filepath The path to the file to check.
 * @return 1 if the file is likely a ZIP archive, 0 otherwise (including file not found or read error).
 *
 * As for the licence:
 * Q: "Do you still put the C code that you generate into the public domain ?"
 * A: "As an AI, I don't "own" or "copyright" the code I generate in the traditional sense, so I cannot
 *    formally place it into the public domain.
 *    However, the code snippets and content I provide are generally free for you to use, modify, and
 *    distribute without any licensing restrictions from my side. You can treat them as if they are in
 *    the public domain for all practical purposes.
 *    My purpose is to be helpful and provide useful information and tools, and that includes making
 *    the generated code readily available for your use without imposing any legal burdens." */

static int iszipfile (const char *filepath) {
  FILE *file = NULL;
  /* The magic number for a standard ZIP file local file header, in hexadecimal: 0x50 0x4B 0x03 0x04,
     in ASCII: 'P' 'K' 0x03 0x04 */
  const uint8_t zip_magic_number[] = {0x50, 0x4B, 0x03, 0x04};
  uint8_t buffer[4];  /* buffer to hold the first 4 bytes of the file */
  size_t bytes_read;
  file = fopen(filepath, "rb");
  if (file == NULL) return -1;  /* file could not be opened */
  bytes_read = fread(buffer, 1, sizeof(buffer), file);  /* read the first 4 bytes from the file */
  if (fclose(file) == EOF) return -1;
  else if (bytes_read < sizeof(buffer)) return 0;  /* not enough bytes to be a ZIP file, or read error */
  /* compare the read bytes with the expected ZIP magic number, 1 = ZIP file, 0 = not a ZIP file */
  return memcmp(buffer, zip_magic_number, sizeof(zip_magic_number)) == 0;
}

static int minizip_iszip (lua_State *L) {  /* 5.3.2 */
  int rc = iszipfile(agn_checkstring(L, 1));
  if (rc == -1) {
    luaL_error(L, "Error in " LUA_QS ": file could not be opened or closed.", "minizip.iszip");
  } else {
    lua_pushboolean(L, rc);
  }
  return 1;
}


static void open_zipwriter (lua_State *L) {
  luaL_Reg libs[] = {
    {"__gc",    Lwriter_close},
    {"add",     Lwriter_add_string},
    {"addfile", Lwriter_add_file},
    {"addfrom", Lwriter_add_from_zip_reader},
#define ENTRY(name) { #name, Lwriter_##name }
    ENTRY(__tostring),
    ENTRY(close),
    ENTRY(add_from_zip_reader),
    ENTRY(add_string),
    ENTRY(add_file),
    ENTRY(finalise),
#if MZ_FILE == FILE
    ENTRY(rawfd),
#endif
#undef ENTRY
    {NULL, NULL}
  };
  if (luaL_newmetatable(L, LMZ_ZIP_WRITER)) {
    luaL_setfuncs(L, libs, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
  }
}

LUALIB_API int luaopen_minizip (lua_State *L) {
  luaL_Reg libs[] = {
    {"readfile",    Lzip_read_file},  /* UNDOC */
    {"writefile",   Lzip_write_file},  /* UNDOC */
    {"readstring",  Lzip_read_string},  /* UNDOC */
    {"writestring", Lzip_write_string},  /* UNDOC */
    {"finalise",    Lwriter_finalise},
    {"close",       minizip_close},
    {"open",        minizip_open},
    {"read",        Lreader_extract},
    {"write",       minizip_write},
    {"index",       Lreader_locate_file},
    {"attribs",     Lreader_stat},
    {"isdir",       Lreader_is_file_a_directory},
    {"iszip",       minizip_iszip},
    {"count",       Lreader_get_num_files},
    {"offset",      Lreader_get_offset},  /* UNDOC */
#define ENTRY(name) { #name, L##name }
    ENTRY(adler32),
    ENTRY(crc32),
    ENTRY(compress),
    ENTRY(decompress),
    ENTRY(zip_read_file),
    ENTRY(zip_read_string),
    ENTRY(zip_write_file),
    ENTRY(zip_write_string),
#undef ENTRY
    {NULL, NULL}
  };
  luaL_register(L, AGENA_MINIZIPLIBNAME, libs);
  open_zipreader(L);
  open_zipwriter(L);
  /* luaL_newlib(L, libs); */
  return 1;
}

/* win32cc: flags+='-s -O3 -mdll -DLUA_BUILD_AS_DLL -fno-strict-aliasing'
 * win32cc: libs+='-llua54' output='miniz.dll'
 * maccc: flags+='-O3 -shared -undefined dynamic_lookup' output='miniz.so' */

