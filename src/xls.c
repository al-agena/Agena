/*
** $Id: xls.c, initiated November 12, 2026 $
** XLSX I/O binding
** See Copyright Notice in agena.h
** Created mostly by Gemini AI.
*/

#define xls_c
#define LUA_LIB

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* for access */

#include <xlsxio_read.h>
#include <xlsxio_write.h>
#include <xls.h>

#ifndef _WIN32
#ifdef unsigned64_t
#undef unsigned64_t
#endif
#ifdef signed64_t
#undef signed64_t
#endif
#define unsigned64_t xlslib_unsigned64_t
#define signed64_t   xlslib_signed64_t
#endif
#include <stdbool.h>
#if !defined(__DJGPP__) && !defined(LUA_ANSI)
#include <xlslib/xlslib.h>
#endif
#ifndef _WIN32
#undef unsigned64_t
#undef signed64_t
#endif

#include "agena.h"
#include "agenalib.h"
#include "agnhlps.h"
#include "agnxlib.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_XLSLIBNAME "xls"
LUALIB_API int (luaopen_xls) (lua_State *L);
#endif


/* Helper: detect file format by magic bytes (1 = xlsx, 2 = xls, 0 = unknown) */
static int getxlsformat (const char *filename) {
  FILE *f = fopen(filename, "rb");
  if (!f) return 0;
  unsigned char header[4] = {0};
  size_t n = fread(header, 1, 4, f);
  fclose(f);
  if (n < 4) return 0;
  if (header[0] == 0x50 && header[1] == 0x4B && header[2] == 0x03 && header[3] == 0x04) {
    return 1;  /* OOXML / ZIP / .xlsx */
  }
  if (header[0] == 0xD0 && header[1] == 0xCF && header[2] == 0x11 && header[3] == 0xE0) {
    return 2;  /* BIFF8 / OLE2 / .xls */
  }
  return 0;
}

typedef struct {
  size_t rows;
  size_t cols;
} SheetGeometry;

/* Required no-op cell callback so xlsxio advances cell/column state */
static int dims_cell_cb (size_t row, size_t col, const XLSXIOCHAR *value, void *callbackdata) {
  (void)row;
  (void)col;
  (void)value;
  (void)callbackdata;
  return 0;  /* return 0 to continue processing */
}

static int dims_row_cb (size_t row, size_t maxcol, void *callbackdata) {
  (void)row;
  SheetGeometry *geom = (SheetGeometry *)callbackdata;
  geom->rows++;  /* count non-empty rows, matching xlsxio_read */
  if (maxcol > geom->cols) {
    geom->cols = maxcol;
  }
  return 0;  /* return 0 to continue processing */
}

/* Helper: read worksheet dimensions rows and columns from a legacy BIFF8 .xls file */
static int push_xls_row_table (lua_State *L, xlsRow *row, int max_col, int convert, int *fields, int *out_cols, const char *funcname);

static int dims_xls_legacy (lua_State *L, const char *filename, const char *sheetname) {
  xls_error_t err = LIBXLS_OK;
  xlsWorkBook *wb = xls_open_file(filename, NULL, &err);
  if (!wb) {
    return luaL_error(L, "Error in " LUA_QS ": cannot open " LUA_QS " (%s).", "xls.dims", filename, xls_getError(err));
  }
  int sheetidx = 0;
  if (sheetname != NULL) {
    int i, found = 0, sheet_count = wb->sheets.count;
    for (i=0; i < sheet_count; i++) {
      if (wb->sheets.sheet[i].name && strcmp((const char *)wb->sheets.sheet[i].name, sheetname) == 0) {
        sheetidx = i;
        found = 1;
        break;
      }
    }
    if (!found || sheetidx > INT_MAX) {
      xls_close_WB(wb);
      return luaL_error(L, "Error in " LUA_QS ": cannot open worksheet " LUA_QS ".", "xls.dims", sheetname);
    }
  }
  xlsWorkSheet *ws = xls_getWorkSheet(wb, sheetidx);
  if (!ws || xls_parseWorkSheet(ws) != LIBXLS_OK) {
    if (ws) xls_close_WS(ws);
    xls_close_WB(wb);
    return luaL_error(L, "Error in " LUA_QS ": could not process worksheet.", "xls.dims");
  }
  int rows = 0, cols = 0;
  if (ws->rows.row) {
    int row_limit = ws->rows.lastrow + 1;
    int max_col = ws->rows.lastcol;
    int r;
    for (r=0; r < row_limit; r++) {
      xlsRow *row = &ws->rows.row[r];
      if (!row->cells.cell) {
        continue;
      }
      int c, cell_count = row->cells.count;
      int col_limit = ((max_col + 1) < cell_count) ? (max_col + 1) : cell_count;
      int has_data = 0;
      int row_maxcol = 0;
      for (c=0; c < col_limit; c++) {
        if (row->cells.cell[c].id != 0x0201) { /* 0x0201 = BLANK record */
          has_data = 1;
          row_maxcol = c + 1;
        }
      }
      if (has_data) {
        rows++;
        if (row_maxcol > cols) {
          cols = row_maxcol;
        }
      }
    }
  }
  xls_close_WS(ws);
  xls_close_WB(wb);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushnumber(L, (lua_Number)rows);
  lua_pushnumber(L, (lua_Number)cols);
  return 2;
}

static int xls_dims (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  if (!tools_exists(filename)) {
    luaL_error(L, "Error in " LUA_QS ": file does not exist or is inaccessible.", "xls.dims");
  }
  const char *sheetname = luaL_optstring(L, 2, NULL);
  int fmt = getxlsformat(filename);
  if (fmt == 2) {
    return dims_xls_legacy(L, filename, sheetname);
  } else if (fmt != 1) {
    luaL_error(L, "Error in " LUA_QS ": unrecognized Excel file format for " LUA_QS ".", "xls.dims", filename);
  }
  /* OOXML (.xlsx) via xlsxio */
  xlsxioreader reader = xlsxioread_open(filename);
  if (!reader) {
    luaL_error(L, "Error in " LUA_QS ": cannot open " LUA_QS ".", "xls.dims", filename);
  }
  SheetGeometry geom = {0, 0};
  int result = xlsxioread_process(
    reader,
    sheetname,
    XLSXIOREAD_SKIP_EMPTY_ROWS,
    dims_cell_cb,  /* must be non-NULL */
    dims_row_cb,
    &geom
  );
  xlsxioread_close(reader);
  if (result != 0) {
    luaL_error(L, "Error in " LUA_QS ": could not process worksheet.", "xls.dims");
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushinteger(L, (lua_Integer)geom.rows);
  lua_pushinteger(L, (lua_Integer)geom.cols);
  return 2;  /* returns rows, columns */
}


/* list all sheet names in an .xlsx file */
/* Helper: list all sheet names in a legacy BIFF8 .xls file */
static int sheets_xls_internal (lua_State *L, const char *filename) {
  xls_error_t err = LIBXLS_OK;
  xlsWorkBook *wb = xls_open_file(filename, NULL, &err);  /* 7.12.9 fix to suppress `creation of UTF-8 locale failed` warnings */
  if (!wb) {
    return luaL_error(L, "Error in " LUA_QS ": cannot open " LUA_QS " (%s).", "xls.sheets", filename, xls_getError(err));
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_newtable(L);
  int idx = 1;
  WORD i;
  for (i=0; i < wb->sheets.count; i++) {
    if (wb->sheets.sheet[i].name != NULL) {
      lua_pushstring(L, (const char *)wb->sheets.sheet[i].name);
      lua_rawseti(L, -2, idx++);
    }
  }
  xls_close_WB(wb);
  return 1;
}

static int xls_sheets (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  if (!tools_exists(filename)) {
    luaL_error(L, "Error in " LUA_QS ": file does not exist or is inaccessible.", "xls.sheets");
  }
  int fmt = getxlsformat(filename);
  if (fmt == 2) {
    return sheets_xls_internal(L, filename);
  } else if (fmt != 1) {
    luaL_error(L, "Error in " LUA_QS ": unrecognized Excel file format for " LUA_QS ".", "xls.sheets", filename);
  }
  /* OOXML (.xlsx) via xlsxio */
  xlsxioreader xlsxioread = xlsxioread_open(filename);
  if (!xlsxioread) {
    luaL_error(L, "Error in " LUA_QS ": cannot open " LUA_QS ".", "xls.sheets", filename);
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_newtable(L);
  xlsxioreadersheetlist sheetlist = xlsxioread_sheetlist_open(xlsxioread);
  if (sheetlist) {
    const char *sheetname;
    int idx = 1;
    while ((sheetname = xlsxioread_sheetlist_next(sheetlist)) != NULL) {
      lua_pushstring(L, sheetname);
      lua_rawseti(L, -2, idx++);
    }
    xlsxioread_sheetlist_close(sheetlist);
  }
  xlsxioread_close(xlsxioread);
  return 1;
}

#define AGN_MAXFIELDS 16*1024
static int *aux_checkreadoptions (lua_State *L, int pos, int *nargs, int *convert, const char *procname) {
  int checkoptions = 1;  /* check n options; CHANGE THIS if you add/delete options */
  *convert = 1;    /* convert strings representing nuumbers to Agena numbers */
  int *fields = NULL;
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1 */
    /* check for properly allocated slots will be done by agn_pairgetiall() */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "convert")) {
        *convert = agn_checkboolean(L, -1);
      } else if (tools_streq(option, "fields")) {
        int i, pos;
        if (!lua_istable(L, -1)) {
          luaL_error(L, "Error in " LUA_QS ": table expected with " LUA_QS " option.", procname, "fields");
        }
        size_t size = luaL_getn(L, -1);
        if (size == 0) {
          luaL_error(L, "Error in " LUA_QS ": non-empty table expected with " LUA_QS " option.", procname, "fields");
        }
        fields = agn_malloc(L, AGN_MAXFIELDS * sizeof(int), "xls.read", NULL);
        memset(fields, 0, AGN_MAXFIELDS);
        for (i=1; i <= size; i++) {
          pos = (int)agn_getinumber(L, -1, i);
          pos = tools_posrelat(pos, size);
          if (pos < 1 || pos > AGN_MAXFIELDS) {
            agn_poptoptwo(L);
            xfree(fields);
            luaL_error(L, "Error in " LUA_QS ": field %d is out-of-range.", procname, pos);
            return NULL;
          }
          fields[pos - 1] = i;
          /* > xls.read('../testall/hotel.xls', fields=[20 (invalid), 5 (valid), 1 (valid), 2 (valid)]) yields:
               fields[19]=1, fields[4]=2, fields[0]=3, fields[1]=4 */
        }
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
        return NULL;
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    lua_pop(L, 2);
  }
  return fields;
}

/* Helper: re legacy .xls via libxls. Legacy .xls files are technically called BIFF (Binary Interchange File Format)
  files, wrapped inside an OLE2 Compound Document container (officially known as the Compound File Binary Format,
  or CFBF). The following function works with BIFF8 xls files as known from Microsoft Excel 97, 2000, XP, and 2003.
  Updated Full-Table Reader (read_xls_legacy) — Zero-Overflow Edition, 7.12.9 */
static int read_xls_legacy (lua_State *L, const char *filename, const char *sheetname, int convert, int *fields) {
  xls_error_t err = LIBXLS_OK;
  xlsWorkBook *wb = xls_open_file(filename, NULL, &err);
  if (!wb) {
    xfree(fields);
    return luaL_error(L, "Error in " LUA_QS ": cannot read " LUA_QS " (%s).", "xls.read", filename, xls_getError(err));
  }
  int sheetidx = 0;
  if (sheetname != NULL) {
    int i, found = 0, sheet_count = wb->sheets.count;
    for (i=0; i < sheet_count; i++) {
      if (wb->sheets.sheet[i].name && strcmp((const char *)wb->sheets.sheet[i].name, sheetname) == 0) {
        sheetidx = i;
        found = 1;
        break;
      }
    }
    if (!found || sheetidx > INT_MAX) {
      xls_close_WB(wb);
      xfree(fields);
      return luaL_error(L, "Error in " LUA_QS ": cannot open worksheet " LUA_QS ".", "xls.read", sheetname);
    }
  }
  xlsWorkSheet *ws = xls_getWorkSheet(wb, sheetidx);
  if (!ws || xls_parseWorkSheet(ws) != LIBXLS_OK) {
    if (ws) xls_close_WS(ws);
    xls_close_WB(wb);
    xfree(fields);
    return luaL_error(L, "Error in " LUA_QS ": cannot parse worksheet.", "xls.read");
  }
  luaL_checkstack(L, 6, "not enough stack space");
  lua_createtable(L, 0, 3);
  lua_pushliteral(L, "data");
  lua_newtable(L);
  int rowidx = 0, maxcolidx = 0;
  if (ws->rows.row) {
    int row_limit = ws->rows.lastrow + 1;
    int max_col = (int)ws->rows.lastcol;
    int r;
    for (r=0; r < row_limit; r++) {
      int colidx = 0;
      if (push_xls_row_table(L, &ws->rows.row[r], max_col, convert, fields, &colidx, "xls.read")) {
        if (rowidx >= INT_MAX) {
          xls_close_WS(ws);
          xls_close_WB(wb);
          xfree(fields);
          return luaL_error(L, "Error in " LUA_QS ": row count exceeds Lua table limit.", "xls.read");
        }
        if (colidx > maxcolidx) {
          maxcolidx = colidx;
        }
        rowidx++;
        lua_rawseti(L, -2, rowidx);
      }
    }
  }
  xls_close_WS(ws);
  xls_close_WB(wb);
  xfree(fields);
  lua_settable(L, -3);
  lua_pushliteral(L, "rows");
  lua_pushnumber(L, (lua_Number)rowidx);
  lua_settable(L, -3);
  lua_pushliteral(L, "columns");
  lua_pushnumber(L, (lua_Number)maxcolidx);
  lua_settable(L, -3);
  return 1;
}

static int xls_read (lua_State *L) {
  int convert = 1, nargs = lua_gettop(L);
  const char *filename = luaL_checkstring(L, 1);
  const char *sheetname = NULL;  /* get first sheet by default */
  if (nargs > 1 && agn_isstring(L, 2)) {
    sheetname = agn_tostring(L, 2);
  }
  if (!tools_exists(filename)) {
    luaL_error(L, "Error in " LUA_QS ": file does not exist or is inaccessible.", "xls.read");
  }
  int *fields = aux_checkreadoptions(L, 2, &nargs, &convert, "xls.read");
  int fmt = getxlsformat(filename);
  if (fmt == 2) {  /* Excel Binary File Format (.xls) */
    return read_xls_legacy(L, filename, sheetname, convert, fields);
  } else if (fmt != 1) {
    xfree(fields);
    luaL_error(L, "Error in " LUA_QS ": unrecognised Excel file format.", "xls.read");
  }
  /* OOXML (.xlsx) via xlsxio */
  xlsxioreader xlsxioread = xlsxioread_open(filename);
  if (!xlsxioread) {
    xfree(fields);
    luaL_error(L, "Error in " LUA_QS ": cannot read " LUA_QS ".", "xls.read", filename);
  }
  xlsxioreadersheet sheet = xlsxioread_sheet_open(xlsxioread, sheetname, XLSXIOREAD_SKIP_EMPTY_ROWS);
  if (!sheet) {
    xlsxioread_close(xlsxioread);
    xfree(fields);
    luaL_error(L, "Error in " LUA_QS ": cannot open worksheet.", "xls.read");
  }
  luaL_checkstack(L, 5, "not enough stack space");
  lua_createtable(L, 0, 3);
  lua_pushliteral(L, "data");
  lua_newtable(L);  /* table of rows */
  int rowidx, maxcolidx;
  rowidx = maxcolidx = 0;
  while (xlsxioread_sheet_next_row(sheet)) {
    lua_newtable(L);  /* inner table for current row */
    char *value;
    int colidx = 0, pos = 0;
    while ((value = xlsxioread_sheet_next_cell(sheet)) != NULL) {
/*
import xls
L := xls.read('../testall/antarct.xlsx', fields=[3, 1, 2])
L.data[1]:
*/
      colidx++;
      if (fields) {
        if (colidx > AGN_MAXFIELDS) {
          xfree(fields);
          luaL_error(L, "Error in " LUA_QS ": column %d out-of-range.", "xls.read", colidx);
        }
        pos = fields[colidx - 1];
        if (pos == 0) goto label;
      }
      /* push and store the current cell as a string, or as a number if convertible */
      lua_pushstring(L, value);
      if (convert) {
        int exception = 0;
        lua_Number n = agn_tonumberx(L, -1, &exception);
        if (!exception) {
          lua_pop(L, 1);
          lua_pushnumber(L, n);
        }
      }
      lua_rawseti(L, -2, (fields) ? pos : colidx);  /* set string or number into row table */
label:
      xlsxioread_free(value);  /* free memory allocated by xlsxio */
    }
    if (colidx > maxcolidx) maxcolidx = colidx;
    lua_rawseti(L, -2, ++rowidx);
  }
  xlsxioread_sheet_close(sheet);
  xlsxioread_close(xlsxioread);
  xfree(fields);
  lua_settable(L, -3);
  lua_pushliteral(L, "rows");
  lua_pushinteger(L, rowidx);
  lua_settable(L, -3);
  lua_pushliteral(L, "columns");
  lua_pushinteger(L, maxcolidx);
  lua_settable(L, -3);
  return 1;
}


/* write a 2D Lua table to a new .xls or .xlsx file, renamed from xls.save to xls.write 7.12.13 */
static int xls_write (lua_State *L) {
  size_t l;
  int r, c, num_rows, num_cols;
  luaL_checktype(L, 1, LUA_TTABLE);
  const char *filename = luaL_checklstring(L, 2, &l);
  const char *sheetname = luaL_optstring(L, 3, "Sheet1");
  luaL_checkstack(L, 2, "not enough stack space");
  lua_getfield(L, 1, "data");
  if (lua_isnil(L, -1)) {  /* 7.12.13 improvement of error message */
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": input table is malformed, " LUA_QS " section is missing.",
      "xls.write", "data");
  }
  if (!lua_istable(L, -1)) {  /* 7.12.13 improvement of error message */
    luaL_error(L, "Error in " LUA_QS ": input table is malformed, " LUA_QS " section is of type %s.",
      "xls.write", "data", luaL_typename(L, -1));
  }
  if (l >= 4 && strcasecmp(filename + l - 4, ".xls") == 0) {  /* write legacy Release 1.2 BIFF8 XLS file, 7.12.13 */
#if defined(__DJGPP__) || defined(LUA_ANSI)
    luaL_error(L, "Error in " LUA_QS ": cannot create legacy XLS files on this platform.", "xls.write");
#else
    workbook *wb = xlsNewWorkbook();
    if (!wb) {
      luaL_error(L, "Error in " LUA_QS ": cannot allocate workbook.", "xls.write");
    }
    worksheet *ws = xlsWorkbookSheet(wb, sheetname);
    if (!ws) {
      xlsDeleteWorkbook(wb);
      luaL_error(L, "Error in " LUA_QS ": cannot create worksheet " LUA_QS ".", "xls.write", sheetname);
    }
    num_rows = luaL_getn(L, -1);
    for (r=1; r <= num_rows; r++) {
      lua_rawgeti(L, -1, r);  /* push row table */
      if (!lua_istable(L, -1)) {
        xlsDeleteWorkbook(wb);
        luaL_error(L, "Error in " LUA_QS ": row %d is of invalid type %s.", "xls.write", r,
          luaL_typename(L, -1));  /* 7.12.13 improvement of error message */
      }
      num_cols = luaL_getn(L, -1);
      for (c=1; c <= num_cols; c++) {
        lua_rawgeti(L, -1, c);  /* push cell in row */
        int t = lua_type(L, -1);
        unsigned32_t row_idx = (unsigned32_t)(r - 1);
        unsigned32_t col_idx = (unsigned32_t)(c - 1);
        if (t == LUA_TNUMBER) {
          xlsWorksheetNumberDbl(ws, row_idx, col_idx, (double)agn_tonumber(L, -1), NULL);
        } else if (t == LUA_TBOOLEAN) {
          xlsWorksheetBoolean(ws, row_idx, col_idx, lua_toboolean(L, -1), NULL);
        } else if (t == LUA_TSTRING) {
          xlsWorksheetLabel(ws, row_idx, col_idx, agn_tostring(L, -1), NULL);
        } else {  /* null or unsupported type -> leave empty (or emit blank cell) */
          /* Nothing needed for truly empty cells; optionally use:
             xlsWorksheetBlank(ws, row_idx, col_idx, NULL); */
        }
        lua_pop(L, 1);  /* pop cell value */
      }
      lua_pop(L, 1);  /* pop row table */
    }
    lua_pop(L, 1);  /* pop "data" table */
    int err = xlsWorkbookDump(wb, filename);
    xlsDeleteWorkbook(wb);
    if (err != 0) {
      luaL_error(L, "Error in " LUA_QS ": cannot write " LUA_QS " (error code %d).",
        "xls.write", filename, err);
    }
#endif
  } else {  /* write OOXML .xlsx file */
    xlsxiowriter handle = xlsxiowrite_open(filename, sheetname);
    if (!handle) {
      luaL_error(L, "Error in " LUA_QS ": cannot create " LUA_QS ".", "xls.write", filename);
    }
    num_rows = luaL_getn(L, -1);
    for (r=1; r <= num_rows; r++) {
      lua_rawgeti(L, -1, r);  /* push row table */
      if (!lua_istable(L, -1)) {
        xlsxiowrite_close(handle);
        luaL_error(L, "Error in " LUA_QS ": row %d is of invalid type %s.", "xls.write", r,
          luaL_typename(L, -1));  /* 7.12.13 improvement of error message */
      }
      num_cols = luaL_getn(L, -1);
      for (c=1; c <= num_cols; c++) {
        lua_rawgeti(L, -1, c);  /* push cell in row */
        int t = lua_type(L, -1);
        if (t == LUA_TNUMBER) {
          xlsxiowrite_add_cell_float(handle, agn_tonumber(L, -1));
        } else if (t == LUA_TBOOLEAN) {
          xlsxiowrite_add_cell_int(handle, lua_toboolean(L, -1));
        } else if (t == LUA_TSTRING) {
          xlsxiowrite_add_cell_string(handle, agn_tostring(L, -1));
        } else {  /* null or unsupported type -> do not emit XML <c> element for that cell, just create an empty cell */
          xlsxiowrite_add_cell_string(handle, NULL);
        }
        lua_pop(L, 1);  /* pop cell value */
      }
      xlsxiowrite_next_row(handle);
      lua_pop(L, 1);  /* pop row table */
    }
    lua_pop(L, 1);  /* pop "data" table */
    xlsxiowrite_close(handle);
  }
  return 0;
}


/* ********************************************************************************************* */
/* * The iterators, added 7.12.9, created by Gemini AI                                         * */
/* ********************************************************************************************* */

#define XLSX_ITER_MT "xls.xlsx_iter"

typedef struct {
  xlsxioreader reader;
  xlsxioreadersheet sheet;
  int convert;
  int rowidx;
  int *fields;
  size_t fieldslen;
} xlsx_iter_t;

static void xlsx_iter_cleanup (xlsx_iter_t *it) {
  if (it->sheet) {
    xlsxioread_sheet_close(it->sheet);
    it->sheet = NULL;
  }
  if (it->reader) {
    xlsxioread_close(it->reader);
    it->reader = NULL;
  }
  if (it->fields) {
    xfree(it->fields);
    it->fields = NULL;
  }
}

static int xlsx_iter_gc (lua_State *L) {
  xlsx_iter_t *it = (xlsx_iter_t *)luaL_checkudata(L, 1, XLSX_ITER_MT);
  xlsx_iter_cleanup(it);
  return 0;
}

static int xlsx_iter_next (lua_State *L) {
  xlsx_iter_t *it = (xlsx_iter_t *)lua_touserdata(L, lua_upvalueindex(1));
  if (!it || !it->sheet) {
    lua_pushnil(L);
    return 1;
  }
  if (!xlsxioread_sheet_next_row(it->sheet)) {
    xlsx_iter_cleanup(it);
    lua_pushnil(L);
    return 1;
  }
  lua_newtable(L);
  char *value;
  int colidx = 0, pos = 0;
  int last_nonempty = 0;
  while ((value = xlsxioread_sheet_next_cell(it->sheet)) != NULL) {
    if (colidx >= INT_MAX) {
      xlsxioread_free(value);
      xlsx_iter_cleanup(it);
      return luaL_error(L, "Error in " LUA_QS ": column count exceeds Lua table limit.", "xls.iterate");
    }
    int is_empty = (value[0] == '\0');
    colidx++;
    if (it->fields) {
      if (colidx > AGN_MAXFIELDS) {
        xlsxioread_free(value);
        xlsx_iter_cleanup(it);
        luaL_error(L, "Error in " LUA_QS ": column %d out-of-range.", "xls.read", colidx);
      }
      pos = it->fields[colidx - 1];
      if (pos == 0) goto label;
    }
    /* push and store the current cell as a string, or as a number if convertible */
    lua_pushstring(L, value);
    if (it->convert) {
      int exception = 0;
      lua_Number n = agn_tonumberx(L, -1, &exception);
      if (!exception) {
        lua_pop(L, 1);
        lua_pushnumber(L, n);
        is_empty = 0;
      }
    }
    if (!is_empty) {
      last_nonempty = colidx;
    }
    lua_rawseti(L, -2, (it->fields) ? pos : colidx);
label:
    xlsxioread_free(value);
  }
  /* Trim any trailing empty strings at the end of the row (such as phantom J1) */
  while (colidx > last_nonempty) {
    lua_pushnil(L);
    lua_rawseti(L, -2, colidx);
    colidx--;
  }
  if (it->rowidx == INT_MAX) {
    xlsx_iter_cleanup(it);
    return luaL_error(L, "Error in " LUA_QS ": row count overflow.", "xls.iterate");
  }
  it->rowidx++;
  return 1;
}

/* ==========================================================================
   2. Legacy .xls (libxls-1.6.4) Iterator — Zero-Overflow Edition
   ========================================================================== */

#define XLS_ITER_MT "xls.xls_iter"

typedef struct {
  xlsWorkBook *wb;
  xlsWorkSheet *ws;
  int curr_row;
  int rowidx;  /* row index in resulting table */
  int *fields;
  int convert;
} xls_iter_t;

static void xls_iter_cleanup (xls_iter_t *it) {
  if (it->ws) {
    xls_close_WS(it->ws);
    it->ws = NULL;
  }
  if (it->wb) {
    xls_close_WB(it->wb);
    it->wb = NULL;
  }
  if (it->fields) {
    xfree(it->fields);
    it->fields = NULL;
  }
}

static int xls_iter_gc (lua_State *L) {
  xls_iter_t *it = (xls_iter_t *)luaL_checkudata(L, 1, XLS_ITER_MT);
  xls_iter_cleanup(it);
  return 0;
}

/* Helper to safely read a single .xls row into a Lua table on top of the stack.
   Returns 1 if the row contained data, 0 if it was completely blank. */
static int push_xls_row_table (lua_State *L, xlsRow *row, int max_col, int convert, int *fields, int *out_cols, const char *funcname) {
  if (!row || !row->cells.cell) return 0;
  int c, cell_count = row->cells.count;
  int col_limit = (cell_count <= 65536 && (max_col + 1) < cell_count) ? (max_col + 1) : cell_count;
  int has_data = 0;
  for (c=0; c < col_limit; c++) {
    if (row->cells.cell[c].id != 0x0201) { /* 0x0201 = BLANK record */
      has_data = 1;
      break;
    }
  }
  if (!has_data) return 0;
  if (col_limit > INT_MAX) {
    return luaL_error(L, "Error in " LUA_QS ": column count exceeds Lua table limit.", funcname);
  }
  lua_createtable(L, col_limit, 0);
  int colidx = 0, pos = 0;
  for (c=0; c < col_limit; c++) {
    xlsCell *cell = &row->cells.cell[c];
    colidx++;
    if (fields) {
      if (colidx > AGN_MAXFIELDS) {
        xfree(fields);
        luaL_error(L, "Error in " LUA_QS ": column %d out-of-range.", "xls.read", colidx);
      }
      pos = fields[colidx - 1];
      if (pos == 0) continue;
    }
    if (cell->id == 0x0201 || cell->str == NULL) {
      lua_pushliteral(L, "");
    } else if ((cell->id == 0x0205 || cell->id == 0x0006) && strcmp((const char *)cell->str, "bool") == 0) {  /* 7.12.13 fix */
      if (convert) {
        lua_pushboolean(L, cell->d != 0.0);
      } else {
        lua_pushstring(L, (cell->d != 0.0) ? "TRUE" : "FALSE");
      }
    } else if (convert && (cell->id == 0x0203 || cell->id == 0x027E || cell->id == 0x00BD)) {
      lua_pushnumber(L, (lua_Number)cell->d);
    } else {
      lua_pushstring(L, (const char *)cell->str);
      if (convert) {
        int exception = 0;
        lua_Number n = agn_tonumberx(L, -1, &exception);
        if (!exception) {
          lua_pop(L, 1);
          lua_pushnumber(L, n);
        }
      }
    }
    lua_rawseti(L, -2, (fields) ? pos : colidx);
  }
  if (out_cols) {
    *out_cols = colidx;
  }
  return 1;
}

static int xls_iter_next (lua_State *L) {
  xls_iter_t *it = (xls_iter_t *)lua_touserdata(L, lua_upvalueindex(1));
  if (!it || !it->ws) {
    lua_pushnil(L);
    return 1;
  }
  if (!it->ws->rows.row) {
    xls_iter_cleanup(it);
    lua_pushnil(L);
    return 1;
  }
  int row_limit = it->ws->rows.lastrow + 1;
  int max_col = it->ws->rows.lastcol;
  while (it->curr_row < row_limit) {
    int r = it->curr_row++;
    if (push_xls_row_table(L, &it->ws->rows.row[r], max_col, it->convert, it->fields, NULL, "xls.iterate")) {
      if (it->rowidx == INT_MAX) {
        xls_iter_cleanup(it);
        return luaL_error(L, "Error in " LUA_QS ": row count overflow.", "xls.iterate");
      }
      it->rowidx++;
      return 1;
    }
  }
  xls_iter_cleanup(it);
  lua_pushnil(L);
  return 1;
}

static int xls_iterate_legacy (lua_State *L, const char *filename, const char *sheetname, int convert, int *fields) {
  xls_iter_t *it = (xls_iter_t *)lua_newuserdata(L, sizeof(xls_iter_t));
  it->wb = NULL;
  it->ws = NULL;
  it->curr_row = 0;
  it->convert = convert;
  it->rowidx = 0;
  it->fields = fields;
  luaL_getmetatable(L, XLS_ITER_MT);
  lua_setmetatable(L, -2);
  xls_error_t err = LIBXLS_OK;
  it->wb = xls_open_file(filename, NULL, &err);
  if (!it->wb) {
    lua_pop(L, 1);
    return luaL_error(L, "Error in " LUA_QS ": cannot read " LUA_QS " (%s).", "xls.iterate", filename, xls_getError(err));
  }
  int sheetidx = 0;
  if (sheetname != NULL) {
    int found = 0;
    int i;
    int sheet_count = it->wb->sheets.count;
    for (i=0; i < sheet_count; i++) {
      if (it->wb->sheets.sheet[i].name && strcmp((const char *)it->wb->sheets.sheet[i].name, sheetname) == 0) {
        sheetidx = i;
        found = 1;
        break;
      }
    }
    if (!found || sheetidx > INT_MAX) {
      xls_iter_cleanup(it);
      lua_pop(L, 1);
      return luaL_error(L, "Error in " LUA_QS ": cannot open worksheet " LUA_QS ".", "xls.iterate", sheetname);
    }
  }
  it->ws = xls_getWorkSheet(it->wb, sheetidx);
  if (!it->ws || xls_parseWorkSheet(it->ws) != LIBXLS_OK) {
    xls_iter_cleanup(it);
    lua_pop(L, 1);
    return luaL_error(L, "Error in " LUA_QS ": cannot parse worksheet.", "xls.iterate");
  }
  lua_pushcclosure(L, xls_iter_next, 1);
  return 1;
}

/* ==========================================================================
   3. Unified Factory Function (xls.iterate)
   ========================================================================== */

static int xls_iterate (lua_State *L) {
  const char *filename, *sheetname;
  int *fields, fmt, convert = 1, nargs = lua_gettop(L);
  filename = luaL_checkstring(L, 1);
  if (!tools_exists(filename)) {
    return luaL_error(L, "Error in " LUA_QS ": file does not exist or is inaccessible.", "xls.iterate");
  }
  sheetname = NULL;
  fields = aux_checkreadoptions(L, 2, &nargs, &convert, "xls.iterate");  /* we do not use fields here */
  if (nargs > 1 && agn_isstring(L, 2)) {
    sheetname = agn_tostring(L, 2);
  }
  fmt = getxlsformat(filename);
  if (fmt == 2) {
    return xls_iterate_legacy(L, filename, sheetname, convert, fields);
  } else if (fmt != 1) {
    xfree(fields);
    return luaL_error(L, "Error in " LUA_QS ": unrecognised Excel file format.", "xls.iterate");
  }
  xlsx_iter_t *it = (xlsx_iter_t *)lua_newuserdata(L, sizeof(xlsx_iter_t));
  it->reader = NULL;
  it->sheet = NULL;
  it->convert = convert;
  it->rowidx = 0;
  it->fields = fields;
  luaL_getmetatable(L, XLSX_ITER_MT);
  lua_setmetatable(L, -2);
  it->reader = xlsxioread_open(filename);
  if (!it->reader) {
    /* to avoid double-frees, do not run xfree(fields) here */
    lua_pop(L, 1);
    return luaL_error(L, "Error in " LUA_QS ": cannot read " LUA_QS ".", "xls.iterate", filename);
  }
  it->sheet = xlsxioread_sheet_open(it->reader, sheetname, XLSXIOREAD_SKIP_EMPTY_ROWS);
  if (!it->sheet) {
    /* to avoid double-frees, do not run xfree(fields) here */
    xlsx_iter_cleanup(it);
    lua_pop(L, 1);
    return luaL_error(L, "Error in " LUA_QS ": cannot open worksheet.", "xls.iterate");
  }
  lua_pushcclosure(L, xlsx_iter_next, 1);
  return 1;
}


static const luaL_Reg xls_funcs[] = {
  {"dims",     xls_dims},
  {"iterate",  xls_iterate},
  {"read",     xls_read},
  {"write",    xls_write},
  {"sheets",   xls_sheets},
  {NULL, NULL}
};


int luaopen_xls (lua_State *L) {
  /* Metatable for .xlsx iterator userdata */
  luaL_newmetatable(L, XLSX_ITER_MT);
  lua_pushliteral(L, "__gc");
  lua_pushcfunction(L, xlsx_iter_gc);
  lua_settable(L, -3);
  lua_pop(L, 1);  /* pop metatable */
  /* Metatable for legacy .xls iterator userdata */
  luaL_newmetatable(L, XLS_ITER_MT);
  lua_pushliteral(L, "__gc");
  lua_pushcfunction(L, xls_iter_gc);
  lua_settable(L, -3);
  lua_pop(L, 1);  /* pop metatable */
  luaL_register(L, AGENA_XLSLIBNAME, xls_funcs);
  return 1;
}

