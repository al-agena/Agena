/*
** $Id: xls.c, initiated November 12, 2026 $
** XLSX I/O binding
** See Copyright Notice in agena.h
** Created mostly by Gemini AI.
*/

/*
import xls
xls.sheets('../antarct.xlsx')
L := xls.read('../antarct.xlsx')
K := xls.read('../antarct.xls')
*/

#define xls_c
#define LUA_LIB

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* for access */

#include <xlsxio_read.h>
#include <xlsxio_write.h>
#include <xls.h>

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

/* Helper: read worksheet dimensions from a legacy BIFF8 .xls file */
static int dims_xls_legacy (lua_State *L, const char *filename, const char *sheetname) {
  WORD i, r, c;
  size_t rows, cols;
  xls_error_t err = LIBXLS_OK;
  xlsWorkBook *wb = xls_open_file(filename, "UTF-8", &err);
  if (!wb) {
    return luaL_error(L, "Error in " LUA_QS ": cannot open " LUA_QS " (%s).", "xls.dims", filename, xls_getError(err));
  }
  /* Find worksheet by name, or default to sheet 0 */
  int sheetidx = 0;
  if (sheetname != NULL) {
    sheetidx = -1;
    for (i=0; i < wb->sheets.count; i++) {
      if (wb->sheets.sheet[i].name && strcmp((const char *)wb->sheets.sheet[i].name, sheetname) == 0) {
        sheetidx = i;
        break;
      }
    }
    if (sheetidx < 0) {
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
  /* Count non-empty rows and find max column index to match xlsxio_dimensions */
  rows = cols = 0;
  for (r=0; r <= ws->rows.lastrow; r++) {
    xlsRow *row = &ws->rows.row[r];
    int has_data = 0;
    size_t row_maxcol = 0;
    for (c=0; c <= ws->rows.lastcol; c++) {
      if (row->cells.cell[c].id != 0x0201) { /* 0x0201 = BLANK record */
        has_data = 1;
        row_maxcol = (size_t)(c + 1);
      }
    }
    if (has_data) {
      rows++;
      if (row_maxcol > cols) {
        cols = row_maxcol;
      }
    }
  }
  xls_close_WS(ws);
  xls_close_WB(wb);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushinteger(L, (lua_Integer)rows);
  lua_pushinteger(L, (lua_Integer)cols);
  return 2;  /* returns rows, columns */
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
  xlsWorkBook *wb = xls_open_file(filename, "UTF-8", &err);
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


static void aux_checkreadoptions (lua_State *L, int pos, int *nargs, int *convert, const char *procname) {
  int checkoptions = 1;  /* check n options; CHANGE THIS if you add/delete options */
  *convert = 1;
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1 */
    /* check for properly allocated slots will be done by agn_pairgetiall() */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "convert")) {
        *convert = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
}

/* Helper: read legacy .xls via libxls. Legacy .xls files are technically called BIFF (Binary Interchange File Format)
  files, wrapped inside an OLE2 Compound Document container (officially known as the Compound File Binary Format,
  or CFBF). The following function works with BIFF8 xls files as known from Microsoft Excel 97, 2000, XP, and 2003. */
static int read_xls_legacy (lua_State *L, const char *filename, const char *sheetname, int convert) {
  xls_error_t err = LIBXLS_OK;
  xlsWorkBook *wb = xls_open_file(filename, "UTF-8", &err);
  if (!wb) {
    return luaL_error(L, "Error in " LUA_QS ": cannot read " LUA_QS " (%s).", "xls.read", filename, xls_getError(err));
  }
  /* Find worksheet by name, or default to sheet 0 */
  int sheetidx = 0;
  if (sheetname != NULL) {
    sheetidx = -1;
    WORD i;
    for (i=0; i < wb->sheets.count; i++) {
      if (wb->sheets.sheet[i].name && strcmp((const char *)wb->sheets.sheet[i].name, sheetname) == 0) {
        sheetidx = i;
        break;
      }
    }
    if (sheetidx < 0) {
      xls_close_WB(wb);
      return luaL_error(L, "Error in " LUA_QS ": cannot open worksheet " LUA_QS ".", "xls.read", sheetname);
    }
  }
  xlsWorkSheet *ws = xls_getWorkSheet(wb, sheetidx);
  if (!ws || xls_parseWorkSheet(ws) != LIBXLS_OK) {
    if (ws) xls_close_WS(ws);
    xls_close_WB(wb);
    return luaL_error(L, "Error in " LUA_QS ": cannot parse worksheet.", "xls.read");
  }
  luaL_checkstack(L, 5, "not enough stack space");
  lua_createtable(L, 0, 3);
  lua_pushliteral(L, "data");
  lua_newtable(L);
  int rowidx = 0, maxcolidx = 0;
  WORD r, c;
  for (r=0; r <= ws->rows.lastrow; r++) {
    xlsRow *row = &ws->rows.row[r];
    /* Skip completely empty rows to match XLSXIOREAD_SKIP_EMPTY_ROWS */
    int has_data = 0;
    for (c=0; c <= ws->rows.lastcol; c++) {
      if (row->cells.cell[c].id != 0x0201) { /* 0x0201 = BLANK record */
        has_data = 1;
        break;
      }
    }
    if (!has_data) continue;
    lua_newtable(L);
    int colidx = 0;
    for (c=0; c <= ws->rows.lastcol; c++) {
      xlsCell *cell = &row->cells.cell[c];
      colidx++;
      if (cell->id == 0x0201 || cell->str == NULL) {
        lua_pushliteral(L, "");
      } else if (convert && (cell->id == 0x0203 || cell->id == 0x027E || cell->id == 0x00BD)) {
        /* Numeric cell (NUMBER, RK, MULRK) */
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
      lua_rawseti(L, -2, colidx);
    }
    if (colidx > maxcolidx) maxcolidx = colidx;
    lua_rawseti(L, -2, ++rowidx);
  }
  xls_close_WS(ws);
  xls_close_WB(wb);
  lua_settable(L, -3);
  lua_pushliteral(L, "rows");
  lua_pushinteger(L, rowidx);
  lua_settable(L, -3);
  lua_pushliteral(L, "columns");
  lua_pushinteger(L, maxcolidx);
  lua_settable(L, -3);
  return 1;
}

static int xls_read (lua_State *L) {
  int convert = 1, nargs = lua_gettop(L);
  const char *filename = luaL_checkstring(L, 1);
  const char *sheetname = NULL;  /* get first sheet by default */
  aux_checkreadoptions(L, 2, &nargs, &convert, "xls.read");
  if (nargs > 1 && agn_isstring(L, 2)) {
    sheetname = agn_tostring(L, 2);
  }
  if (!tools_exists(filename)) {
    luaL_error(L, "Error in " LUA_QS ": file does not exist or is inaccessible.", "xls.read");
  }
  int fmt = getxlsformat(filename);
  if (fmt == 2) {  /* Excel Binary File Format (.xls) */
    return read_xls_legacy(L, filename, sheetname, convert);
  } else if (fmt != 1) {
    luaL_error(L, "Error in " LUA_QS ": unrecognised Excel file format.", "xls.read");
  }
  /* OOXML (.xlsx) via xlsxio */
  xlsxioreader xlsxioread = xlsxioread_open(filename);
  if (!xlsxioread) {
    luaL_error(L, "Error in " LUA_QS ": cannot read " LUA_QS ".", "xls.read", filename);
  }
  xlsxioreadersheet sheet = xlsxioread_sheet_open(xlsxioread, sheetname, XLSXIOREAD_SKIP_EMPTY_ROWS);
  if (!sheet) {
    xlsxioread_close(xlsxioread);
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
    int colidx = 0;
    while ((value = xlsxioread_sheet_next_cell(sheet)) != NULL) {
      lua_pushstring(L, value);
      if (convert) {
        int exception = 0;
        lua_Number n = agn_tonumberx(L, -1, &exception);
        if (!exception) {
          lua_pop(L, 1);
          lua_pushnumber(L, n);
        }
      }
      lua_rawseti(L, -2, ++colidx);
      xlsxioread_free(value);  /* free memory allocated by xlsxio */
    }
    if (colidx > maxcolidx) maxcolidx = colidx;
    lua_rawseti(L, -2, ++rowidx);
  }
  xlsxioread_sheet_close(sheet);
  xlsxioread_close(xlsxioread);
  lua_settable(L, -3);
  lua_pushliteral(L, "rows");
  lua_pushinteger(L, rowidx);
  lua_settable(L, -3);
  lua_pushliteral(L, "columns");
  lua_pushinteger(L, maxcolidx);
  lua_settable(L, -3);
  return 1;
}


/* Helper: write a 2D Lua table to a new .xlsx file */
static int xls_save (lua_State *L) {
  int r, c, num_rows, num_cols;
  luaL_checktype(L, 1, LUA_TTABLE);
  const char *filename = luaL_checkstring(L, 2);
  const char *sheetname = luaL_optstring(L, 3, "Sheet1");
  luaL_checkstack(L, 2, "not enough stack space");
  lua_getfield(L, 1, "data");
  if (!lua_istable(L, -1)) {
    luaL_error(L, "Error in " LUA_QS ": input table is malformed.", "xls.save");
  }
  xlsxiowriter handle = xlsxiowrite_open(filename, sheetname);
  if (!handle) {
    luaL_error(L, "Error in " LUA_QS ": cannot create " LUA_QS ".", "xls.save", filename);
  }
  num_rows = luaL_getn(L, -1);
  for (r=1; r <= num_rows; r++) {
    lua_rawgeti(L, -1, r);  /* push row table */
    if (!lua_istable(L, -1)) {
      lua_pop(L, 2);
      xlsxiowrite_close(handle);
      luaL_error(L, "Error in " LUA_QS ": input table is malformed.", "xls.save");
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
      } else {  /* null or unsupported type -> empty cell */
        xlsxiowrite_add_cell_string(handle, "");
      }
      lua_pop(L, 1);  /* pop cell value */
    }
    xlsxiowrite_next_row(handle);
    lua_pop(L, 1);  /* pop row table */
  }
  lua_pop(L, 1);  /* pop "data" table */
  xlsxiowrite_close(handle);
  return 0;
}


static const luaL_Reg xls_funcs[] = {
  {"dims",     xls_dims},
  {"read",     xls_read},
  {"save",     xls_save},
  {"sheets",   xls_sheets},
  {NULL, NULL}
};

int luaopen_xls (lua_State *L) {
  luaL_register(L, AGENA_XLSLIBNAME, xls_funcs);
  return 1;
}

