/*
** $Id: readstat.c, initiated November 04, 2026 $
** readstat binding for .sav SPSS Statistics Data Files
** See Copyright Notice in agena.h
** Created mostly by Gemini AI.
*/

#define readstat_c
#define LUA_LIB

/*
import readstat
L := readstat.read('../data/hotel.sav');
for i, j in L do print(i, j) od

readstat.save(L, 'hotelnew.sav');

L := readstat.read("C:/Program Files/PSPP-TESTING/share/pspp/examples/nhtsa.sav");
readstat.save(L, "nhtsa.new.sav");
K := readstat.read("nhtsa.new.sav");
L = K:
*/

#ifndef _WIN64
#ifndef _off64_t
#define _off64_t long long
#endif
#endif

#include <readstat.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* for strcasecmp, strncasecmp */
#include <sys/types.h>
#include <unistd.h>   /* for access */

#include "agena.h"
#include "agenalib.h"
#include "agnxlib.h"
#include "prepdefs.h"  /* for __SOLARIS, IS32BIT */

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_READSTATLIBNAME "readstat"
LUALIB_API int (luaopen_readstat) (lua_State *L);
#endif

/* Ensure ReadStat 1.1.9 success macro is defined */
#ifndef READSTAT_HANDLER_OK
#define READSTAT_HANDLER_OK 0
#endif


#define AGN_MAXFIELDS 16*1024
/* Theoretically an SPSS .sav file can have 2,147,483,647 = 2^31-1 columns */
static int *aux_checkreadoptions (lua_State *L, int pos, int *nargs, const char *procname) {
  int checkoptions = 1;  /* check n options; CHANGE THIS if you add/delete options */
  int *fields = NULL;
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1 */
    /* check for properly allocated slots will be done by agn_pairgetiall() */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "fields")) {
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
        }
        /* > readstat.read('../testall/hotel.sav', fields=[20 (invalid), 5 (valid), 1 (valid), 2 (valid)]) yields:
           fields[19]=1, fields[4]=2, fields[0]=3, fields[1]=4 */
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


/* Context structure to keep track of our Lua state and tables */
typedef struct {
  lua_State *L;
  int columns_count;
  int *fields;
} l_readstat_ctx_t;


/* Callback 1: Required placeholder for modern metadata sequence */
static int handle_metadata (readstat_metadata_t *metadata, void *user_ctx) {
  l_readstat_ctx_t *ctx = (l_readstat_ctx_t *)user_ctx;
  lua_State *L = ctx->L;
  int obs_count = readstat_get_row_count(metadata);
  int var_count = readstat_get_var_count(metadata);
  if (ctx->fields) {
    int i, valid_count = 0;
    int limit = (var_count < AGN_MAXFIELDS) ? var_count : AGN_MAXFIELDS;
    for (i=0; i < limit; i++) {
      if (ctx->fields[i] > 0) valid_count++;
    }
    var_count = valid_count;
  }
  lua_pushstring(L, "rows");
  lua_pushinteger(L, obs_count);
  lua_settable(L, -3);
  lua_pushstring(L, "columns");
  lua_pushinteger(L, var_count);
  lua_settable(L, -3);
  const char *compstr = "none";  /* 7.12.7 extension */
  readstat_compress_t comp = readstat_get_compression(metadata);
  switch (comp) {
    case READSTAT_COMPRESS_ROWS:
      compstr = "rows";
      break;
    case READSTAT_COMPRESS_BINARY:
      compstr = "binary";
      break;
    case READSTAT_COMPRESS_NONE:
    default:
      compstr = "none";
      break;
  }
  lua_pushstring(L, "compression");
  lua_pushstring(L, compstr);
  lua_settable(L, -3);
  return READSTAT_HANDLER_OK;
}

/* Callback 2: Handles Metadata (Variables) - Integrates your original layout with explicit missing information blocks */
static int handle_variable (int index, readstat_variable_t *variable,
                            const char *val_labels, void *user_ctx) {
  int idx;
  l_readstat_ctx_t *ctx = (l_readstat_ctx_t *)user_ctx;
  lua_State *L = ctx->L;
  idx = index + 1;
  if (ctx->fields) {
    if (idx > AGN_MAXFIELDS) {
      xfree(ctx->fields);
      luaL_error(L, "Error in " LUA_QS ": column %d out-of-range.", "readstat.read", idx);
    }
    int pos = ctx->fields[idx - 1];
    if (pos == 0) return READSTAT_HANDLER_OK;
    idx = pos;
  }
  const char *var_name = readstat_variable_get_name(variable);
  const char *var_label = readstat_variable_get_label(variable);
  readstat_type_t var_type = readstat_variable_get_type(variable);
  /* Get 'variables' sub-table using your readable string label */
  luaL_checkstack(L, 6, "not enough stack space");
  lua_pushstring(L, "variables");
  lua_gettable(L, -2);
  lua_pushinteger(L, idx);  /* Lua 1-indexed */
  lua_newtable(L);
  /* 1. Variable Name */
  lua_pushstring(L, "name");
  lua_pushstring(L, var_name);
  lua_settable(L, -3);
  /* 2. Variable Description Label */
  lua_pushstring(L, "label");
  lua_pushstring(L, var_label ? var_label : "");
  lua_settable(L, -3);
  /* 3. Variable Value Type */
  lua_pushstring(L, "type");
  /* READSTAT_TYPE_STRING_REF represents the variable-size string type in Stata files which are not read here.
     Just included in case the package is extended in the future. Extended 7.12.11. */
  if (var_type == READSTAT_TYPE_STRING || var_type == READSTAT_TYPE_STRING_REF) {
    lua_pushstring(L, "string");
  } else if (var_type == READSTAT_TYPE_DOUBLE || var_type == READSTAT_TYPE_FLOAT ||
             var_type == READSTAT_TYPE_INT32  || var_type == READSTAT_TYPE_INT16 ||
             var_type == READSTAT_TYPE_INT8) {  /* added 7.12.11 */
    lua_pushstring(L, "numeric");
  } else {
    lua_pushstring(L, "unknown");
  }
  lua_settable(L, -3);
  /* 4a. Track value label mapping dictionary name */
  lua_pushstring(L, "value_labels");
  lua_pushstring(L, val_labels ? val_labels : "");
  lua_settable(L, -3);
  /* 4b. Variable Display Format (e.g. "F8.0", "F8.2", "A20", "DATE11"), 7.12.7 extension*/
  const char *var_format = readstat_variable_get_format(variable);
  if (var_format && var_format[0] != '\0') {
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushstring(L, "format");
    lua_pushstring(L, var_format);
    lua_settable(L, -3);
  }
  /* 5. EXPLICIT MISSING VALUES DEFINITIONS REGION */
  int missing_count = readstat_variable_get_missing_ranges_count(variable);
  if (missing_count > 0) {
    int i;
    lua_pushstring(L, "missing_rules");
    lua_newtable(L);
    for (i=0; i < missing_count; i++) {
      readstat_value_t lo_val = readstat_variable_get_missing_range_lo(variable, i);
      readstat_value_t hi_val = readstat_variable_get_missing_range_hi(variable, i);
      lua_pushinteger(L, i + 1);
      lua_newtable(L);
      readstat_type_t type = readstat_value_type(lo_val);
      if (type == READSTAT_TYPE_DOUBLE || type == READSTAT_TYPE_FLOAT) {
        double lo = readstat_double_value(lo_val);
        double hi = readstat_double_value(hi_val);
        if (lo == hi) {
          lua_pushstring(L, "type");  lua_pushstring(L, "discrete"); lua_settable(L, -3);
          lua_pushstring(L, "value"); lua_pushnumber(L, lo);         lua_settable(L, -3);
        } else {
          lua_pushstring(L, "type"); lua_pushstring(L, "range");    lua_settable(L, -3);
          lua_pushstring(L, "lo");   lua_pushnumber(L, lo);         lua_settable(L, -3);
          lua_pushstring(L, "hi");   lua_pushnumber(L, hi);         lua_settable(L, -3);
        }
      } else if (type == READSTAT_TYPE_STRING) {
        lua_pushstring(L, "type");  lua_pushstring(L, "discrete"); lua_settable(L, -3);
        lua_pushstring(L, "value"); lua_pushstring(L, readstat_string_value(lo_val)); lua_settable(L, -3);
      }
      lua_settable(L, -3);
    }
    lua_settable(L, -3);
  }
  lua_settable(L, -3);
  lua_pop(L, 1);  /* Pop 'variables' sub-table */
  ctx->columns_count++;
  return READSTAT_HANDLER_OK;
}

/* Callback 3: Handles Value Labels - maps all dictionary variants using original layout format */
static int handle_value_label (const char *val_labels, readstat_value_t value,
                               const char *label, void *user_ctx) {
  l_readstat_ctx_t *ctx = (l_readstat_ctx_t *)user_ctx;
  lua_State *L = ctx->L;
  /* Get 'value_labels' sub-table using your readable string label */
  luaL_checkstack(L, 5, "not enough stack space");
  lua_pushstring(L, "value_labels");
  lua_gettable(L, -2);
  lua_pushstring(L, val_labels);
  lua_rawget(L, -2);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    luaL_checkstack(L, 3, "not enough stack space");
    lua_newtable(L);
    lua_pushstring(L, val_labels);
    lua_pushvalue(L, -2);
    lua_rawset(L, -4);
  }
  readstat_type_t type = readstat_value_type(value);
  if (type == READSTAT_TYPE_STRING || type == READSTAT_TYPE_STRING_REF) {  /* added 7.12.11 */
    lua_pushstring(L, readstat_string_value(value));
  } else if (type == READSTAT_TYPE_INT32 || type == READSTAT_TYPE_INT16 ||
             type == READSTAT_TYPE_INT8) {  /* added 7.12.11 */
    lua_pushinteger(L, readstat_int32_value(value));
  } else {
    lua_pushnumber(L, readstat_double_value(value));
  }
  lua_pushstring(L, label);
  lua_rawset(L, -3);
  lua_pop(L, 2);
  return READSTAT_HANDLER_OK;
}

/* Callback 4: Handles Row Data - Safely preserves all user-defined values inside the core matrix data array */
static int handle_data (int row_index, readstat_variable_t *variable, readstat_value_t value, void *user_ctx) {
  int idx;
  l_readstat_ctx_t *ctx = (l_readstat_ctx_t *)user_ctx;
  lua_State *L = ctx->L;
  int var_index = readstat_variable_get_index(variable);
  idx = var_index + 1;
  if (ctx->fields) {
    if (idx > AGN_MAXFIELDS) {
      xfree(ctx->fields);
      luaL_error(L, "Error in " LUA_QS ": column %d out-of-range.", "readstat.read", idx);
    }
    int pos = ctx->fields[idx - 1];
    if (pos == 0) return READSTAT_HANDLER_OK;
    idx = pos;
  }
  luaL_checkstack(L, 4, "not enough stack space");
  /* Get 'data' sub-table using readable string label */
  lua_pushliteral(L, "data");
  lua_gettable(L, -2);
  lua_pushinteger(L, row_index + 1);
  lua_rawget(L, -2);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    luaL_checkstack(L, 3, "not enough stack space");
    lua_newtable(L);
    lua_pushinteger(L, row_index + 1);
    lua_pushvalue(L, -2);
    lua_rawset(L, -4);
  }
  lua_pushinteger(L, idx);
  readstat_type_t type = readstat_value_type(value);
  if (readstat_value_is_system_missing(value)) {
    lua_pushnil(L);
  } else if (type == READSTAT_TYPE_STRING || type == READSTAT_TYPE_STRING_REF) {  /* extended 7.12.10 */
    lua_pushstring(L, readstat_string_value(value));
  } else if (type == READSTAT_TYPE_DOUBLE || type == READSTAT_TYPE_FLOAT) {
    lua_pushnumber(L, readstat_double_value(value));
  } else if (type == READSTAT_TYPE_INT32 || type == READSTAT_TYPE_INT16 || type == READSTAT_TYPE_INT8) {  /* extended 7.12.10 */
    lua_pushinteger(L, readstat_int32_value(value));
  } else {
    lua_pushnil(L);
  }
  lua_rawset(L, -3);
  lua_pop(L, 2);
  return READSTAT_HANDLER_OK;
}

/* Main parsing wrapper: readstat.read("file.sav") */
static int readstat_read (lua_State *L) {
  int *fields, nargs = lua_gettop(L);
  const char *filename = luaL_checkstring(L, 1);
  if (!tools_exists(filename)) {
    luaL_error(L, "Error in " LUA_QS ": file does not exist or is inaccessible.", "readstat.read");
  }
  fields = aux_checkreadoptions(L, 2, &nargs, "readstat.read");
  readstat_parser_t *parser = readstat_parser_init();
  readstat_set_metadata_handler(parser, &handle_metadata);
  readstat_set_variable_handler(parser, &handle_variable);
  readstat_set_value_handler(parser, &handle_data);
  readstat_set_value_label_handler(parser, &handle_value_label);
  /* Create master return table: data = [] */
  luaL_checkstack(L, 3, "not enough stack space");
  lua_newtable(L);
  /* data.variables = [] */
  lua_pushstring(L, "variables");
  lua_newtable(L);
  lua_settable(L, -3);
  /* data.rows = [] */
  lua_pushstring(L, "data");
  lua_newtable(L);
  lua_settable(L, -3);
  /* data.value_labels = [] */
  lua_pushstring(L, "value_labels");
  lua_newtable(L);
  lua_settable(L, -3);
  l_readstat_ctx_t ctx = { .L = L, .columns_count = 0, .fields = fields };
  readstat_error_t error = readstat_parse_sav(parser, filename, &ctx);
  readstat_parser_free(parser);
  xfree(fields);
  if (error != READSTAT_OK) {
    lua_pushnil(L);
    lua_pushstring(L, readstat_error_message(error));
    return 2;
  }
  return 1;
}


/* Main writing wrapper: readstat.save(data_table, "output.sav") */

/* Callback: Low-level disk stream writer callback */

#if (defined(_WIN32) && !defined(_WIN64)) || defined(__SOLARIS) || defined(__OS2__) || \
    (defined(__linux__) && defined(__ILP32__)) || \
    (defined(__ARMCPU) && defined(__linux__) && defined(IS32BIT))
int handle_write (const void *pos, unsigned int len, void *user_ctx) {
  FILE *f = (FILE *)user_ctx;
  return (int)fwrite(pos, 1, len, f);
}
#else
#ifdef _WIN64
ssize_t handle_write (const void *pos, size_t len, void *user_ctx) {
#else
long int handle_write (const void *pos, size_t len, void *user_ctx) {
#endif
  FILE *f = (FILE *)user_ctx;
  return (ssize_t)fwrite(pos, 1, len, f);
}
#endif

/* Main writing wrapper: readstat.write("output.sav", data_table) */
static int aux_checksaveoptions (lua_State *L, int pos, int *nargs, readstat_compress_t *compression, const char *procname) {
  int checkoptions, rc;
  rc = 0;
  /* default compression if no option has been given or there is no compression field */
  *compression = READSTAT_COMPRESS_ROWS;
  lua_pushstring(L, "compression");
  lua_gettable(L, 1);  /* Get compression field */
  if (agn_isstring(L, -1)) {
    const char *comp = agn_tostring(L, -1);
    if (tools_streq(comp, "rows")) {  /* standard .sav bytecode compression */
      *compression = READSTAT_COMPRESS_ROWS;
    } else if (tools_streq(comp, "binary")) {  /* ZLIB block compression, typically .zsav */
      *compression = READSTAT_COMPRESS_BINARY;
    } else if (tools_streq(comp, "none")) {  /* Uncompressed, largest file */
      *compression = READSTAT_COMPRESS_NONE;
    } else {
      luaL_error(L, "Error in " LUA_QS ": unknown compression " LUA_QS ".", procname, comp);
    }
  }
  lua_pop(L, 1);
  checkoptions = 1;  /* check n options; CHANGE THIS if you add/delete options */
  /* check for properly allocated slots will be done by agn_pairgetiall() */
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1, 7.7.8 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streqx(option, "compression", "compress", NULL)) {  /* 7.12.12 "compress" extension */
        const char *comp = agn_checkstring(L, -1);
        if (tools_streq(comp, "none")) {
          *compression = READSTAT_COMPRESS_NONE;  /* no compression */
        } else if (tools_streqx(comp, "binary", "zlib", NULL)) {  /* zlib compression */
          *compression = READSTAT_COMPRESS_BINARY;
        } else if (tools_streqx(comp, "rows", "default", NULL)) {
          *compression = READSTAT_COMPRESS_ROWS;
        } else {
          luaL_error(L, "Error in " LUA_QS ": unknown compression " LUA_QS ".", procname, comp);
        }
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
  return rc;
}

static int readstat_write (lua_State *L) {
  const char *filename = luaL_checkstring(L, 2);
  luaL_checktype(L, 1, LUA_TTABLE);  /* Ensure parameter 2 is the data table */
  int nargs = lua_gettop(L);
  readstat_compress_t compression = READSTAT_COMPRESS_ROWS;
  aux_checksaveoptions(L, 3, &nargs, &compression, "readstat.write");
  FILE *f = fopen(filename, "wb");
  if (!f) {
    luaL_error(L, "Error in " LUA_QS ": cannot open output file for writing.", "readstat.write");
    return 1;
  }
  readstat_writer_t *writer = readstat_writer_init();
  readstat_set_data_writer(writer, &handle_write);
  /* Track variable pointers internally to assign label sets directly later */
  readstat_variable_t *variables_list[1000] = { NULL };
  char variable_categories[1000][256];
  memset(variable_categories, 0, sizeof(variable_categories));
  luaL_checkstack(L, 10, "not enough stack space"); /* Expanded headspace for processing rules tables */
  /* --- PHASE 1: EXTRACT VARIABLES & REGISTER SCHEMA --- */
  lua_pushstring(L, "variables");
  lua_gettable(L, 1);  /* Get data.variables table */
  int var_count = 0;
  if (!lua_istable(L, -1)) {  /* 7.12.11 */
    luaL_error(L, "Error in " LUA_QS ": input table is missing the " LUA_QS " field.", "readstat.write", "variables");
  } else {
    var_count = lua_rawlen(L, -1);
    if (var_count > 1000) {  /* Hard boundary safety check */
      readstat_writer_free(writer);
      fclose(f);
      luaL_error(L, "Error in " LUA_QS ": only 1,000 columns allowed, got %d.", "readstat.write", var_count);
      return 1;
    }
    int i;
    for (i=1; i <= var_count; i++) {
      lua_rawgeti(L, -1, i);  /* Get variable properties table */
      if (lua_istable(L, -1)) {
        /* 1.1 Get name */
        lua_pushstring(L, "name");
        lua_gettable(L, -2);
        const char *v_name = lua_tostring(L, -1);
        lua_pop(L, 1);
        /* 1.2 Get type */
        lua_pushstring(L, "type");
        lua_gettable(L, -2);
        const char *v_type = lua_tostring(L, -1);
        lua_pop(L, 1);
        /* 1.3 Get label */
        lua_pushstring(L, "label");
        lua_gettable(L, -2);
        const char *v_label = lua_tostring(L, -1);
        lua_pop(L, 1);
        /* 1.4 Get categories string name */
        lua_pushstring(L, "value_labels");
        lua_gettable(L, -2);
        const char *v_cats = lua_tostring(L, -1);
        if (v_cats) {
          strncpy(variable_categories[i - 1], v_cats, 255);
          variable_categories[i - 1][255] = '\0';
        }
        lua_pop(L, 1);
        /* 1.5 Get display format BEFORE readstat_add_variable */
        lua_pushstring(L, "format");
        lua_gettable(L, -2);
        const char *v_format = lua_tostring(L, -1);
        lua_pop(L, 1);
        readstat_type_t rs_type = READSTAT_TYPE_DOUBLE;
        size_t storage_width = 0;
        if (v_type && strcmp(v_type, "string") == 0) {
          rs_type = READSTAT_TYPE_STRING;
          /* Parse byte width from SPSS string format like "A4" -> 4 */
          if (v_format && (v_format[0] == 'A' || v_format[0] == 'a') && v_format[1] != '\0') {
            long parsed = strtol(v_format + 1, NULL, 10);
            if (parsed > 0) {
              storage_width = (size_t)parsed;
            }
          }
          /* Fallback if no valid "A#" format was provided */
          if (storage_width == 0) {
            storage_width = 255;
          }
        }
        /* Register variable pointer into tracking list with proper storage_width */
        variables_list[i - 1] = readstat_add_variable(
            writer,
            v_name ? v_name : "UNKNOWN",
            rs_type,
            storage_width
        );
        if (variables_list[i - 1]) {
          if (v_label) {
            readstat_variable_set_label(variables_list[i - 1], v_label);
          }
          /* Set display format if present, otherwise assign a valid default */
          if (v_format && v_format[0] != '\0') {
            readstat_variable_set_format(variables_list[i - 1], v_format);
          } else if (rs_type == READSTAT_TYPE_DOUBLE) {
            readstat_variable_set_format(variables_list[i - 1], "F8.4");
          } else if (rs_type == READSTAT_TYPE_STRING) {
            char default_fmt[32];
            snprintf(default_fmt, sizeof(default_fmt), "A%zu", storage_width);
            readstat_variable_set_format(variables_list[i - 1], default_fmt);
          }
          /* 1.6 WRITE BACK MISSING VALUES RULES */
          lua_pushstring(L, "missing_rules");
          lua_gettable(L, -2);
          if (lua_istable(L, -1)) {
            int rule_count = lua_rawlen(L, -1);
            int r_idx;
            for (r_idx=1; r_idx <= rule_count; r_idx++) {
              lua_rawgeti(L, -1, r_idx);
              if (lua_istable(L, -1)) {
                lua_pushstring(L, "type");
                lua_gettable(L, -2);
                const char *r_type = lua_tostring(L, -1);
                lua_pop(L, 1);
                if (rs_type == READSTAT_TYPE_STRING) {
                  lua_pushstring(L, "value");
                  lua_gettable(L, -2);
                  const char *r_val = lua_tostring(L, -1);
                  lua_pop(L, 1);
                  if (r_val) {
                    readstat_variable_add_missing_string_value(variables_list[i - 1], r_val);
                  }
                } else {
                  if (r_type && strcmp(r_type, "discrete") == 0) {
                    lua_pushstring(L, "value");
                    lua_gettable(L, -2);
                    double r_val = lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    readstat_variable_add_missing_double_value(variables_list[i - 1], r_val);
                  } else if (r_type && strcmp(r_type, "range") == 0) {
                    lua_pushstring(L, "lo");
                    lua_gettable(L, -2);
                    double r_lo = lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    lua_pushstring(L, "hi");
                    lua_gettable(L, -2);
                    double r_hi = lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    readstat_variable_add_missing_double_range(variables_list[i - 1], r_lo, r_hi);
                  }
                }
              }
              lua_pop(L, 1); /* Pop single rule properties table */
            }
          }
          lua_pop(L, 1); /* Pop missing_rules sub-table */
        }
      }
      lua_pop(L, 1);  /* Pop variable properties table */
    }
  }
  lua_pop(L, 1);  /* Pop data.variables sub-table */
  /* --- PHASE 2: PARSE VALUE LABELS & BIND DIRECTLY TO VARIABLES (Deduplicated & Ordered) --- */
  lua_pushstring(L, "value_labels");
  lua_gettable(L, 1);  /* Get data.value_labels table */
  if (lua_istable(L, -1)) {
    /* Track which dictionary names we have already added to the file */
    char added_dicts[1000][256];
    int v, added_count = 0;
    memset(added_dicts, 0, sizeof(added_dicts));
    for (v=0; v < var_count; v++) {
      /* Only process if the variable has a label set and it's not an empty string */
      if (variable_categories[v][0] != '\0' && variables_list[v] != NULL) {
        const char *dict_name = variable_categories[v];
        /* Check if we already created this label set */
        int already_added = 0;
        int d;
        for (d=0; d < added_count; d++) {
          if (strcmp(added_dicts[d], dict_name) == 0) {
            already_added = 1;
            break;
          }
        }
        /* If it's a new dictionary, look it up in Lua and add it to the file once */
        if (!already_added) {
          lua_pushstring(L, dict_name);
          lua_gettable(L, -2);
          if (lua_istable(L, -1)) {
            readstat_label_set_t *label_set = readstat_add_label_set(writer, READSTAT_TYPE_DOUBLE, dict_name);
            if (label_set) {
              lua_pushnil(L);
              while (lua_next(L, -2) != 0) {
                double cat_code = lua_tonumber(L, -2);
                const char *cat_label = lua_tostring(L, -1);
                readstat_label_double_value(label_set, cat_code, cat_label);
                lua_pop(L, 1);
              }
              /* Bind this label set to EVERY variable that shares it */
              int bind_v;
              for (bind_v=0; bind_v < var_count; bind_v++) {
                if (strcmp(variable_categories[bind_v], dict_name) == 0 && variables_list[bind_v] != NULL) {
                  readstat_variable_set_label_set(variables_list[bind_v], label_set);
                }
              }
              /* Remember that we've added this dictionary */
              strncpy(added_dicts[added_count++], dict_name, 255);
            }
          }
          lua_pop(L, 1); /* Pop the specific label sub-table */
        }
      }
    }
  }
  lua_pop(L, 1); /* Pop data.value_labels sub-table */
  /* --- PHASE 3: CALCULATE ROWS COUNT AND BEGIN FILE GENERATION --- */
  lua_pushstring(L, "data");
  lua_gettable(L, 1);  /* Get data.rows table */
  int row_count = 0;
  if (!lua_istable(L, -1)) {  /* 7.12.11 */
    luaL_error(L, "Error in " LUA_QS ": input table is missing the " LUA_QS " field.", "readstat.write", "data");
  } else {
    row_count = lua_rawlen(L, -1);
  }
  readstat_writer_set_compression(writer, compression);  /* 7.12.7 */
  readstat_error_t error = readstat_begin_writing_sav(writer, f, row_count);
  if (error == READSTAT_OK && row_count > 0) {
    /* --- PHASE 4: ITERATE ROWS AND WRITE CELL VALUES ---, fixed 7.12.11 to properly write strings */
    int r;
    for (r=1; r <= row_count; r++) {
      lua_rawgeti(L, -1, r);
      if (lua_istable(L, -1)) {
        readstat_begin_row(writer);
        int c;
        for (c=1; c <= var_count; c++) {
          readstat_variable_t *variable = variables_list[c - 1];
          lua_rawgeti(L, -1, c);
          if (lua_isnil(L, -1)) {
            readstat_insert_missing_value(writer, variable);
          } else if (readstat_variable_get_type(variable) == READSTAT_TYPE_STRING) {
            const char *str = lua_tostring(L, -1);  /* 7.12.12 safeguard, better be sure than sorry */
            if (str == NULL) {  /* did we get a non-convertible type ? */
              readstat_insert_missing_value(writer, variable);
            } else {
              readstat_insert_string_value(writer, variable, str);
            }
          } else {  /* 7.12.12 extension for Booleans and non-convertible data */
            int exception = 0;
            lua_Number n = (lua_isboolean(L, -1)) ? lua_toboolean(L, -1) : agn_tonumberx(L, -1, &exception);
            if (exception) {
              readstat_insert_missing_value(writer, variable);
            } else {
              readstat_insert_double_value(writer, variable, n);
            }
          }
          lua_pop(L, 1);
        }
        readstat_end_row(writer);
      }
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);  /* Pop data.rows sub-table */
  /* --- PHASE 5: CLEANUP & FLUSH METADATA --- */
  readstat_end_writing(writer);
  readstat_writer_free(writer);
  fclose(f);
  if (error != READSTAT_OK) {
    lua_pushnil(L);
    lua_pushstring(L, readstat_error_message(error));
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}


/* ********************************************************************************************* */
/* * The iterator, added 7.12.10, created by Gemini AI                                         * */
/* ********************************************************************************************* */

typedef struct {
  lua_State *L;
  int cells_idx;   /* Stack index of the flat 1D Lua table holding all cell strings */
  int col_count;
  int max_cell;
} l_flat_ctx_t;

/* Callback 1: Determine total number of columns */
static int iter_handle_variable (int index, readstat_variable_t *variable,
                                 const char *val_labels, void *user_ctx) {
  l_flat_ctx_t *ctx = (l_flat_ctx_t *)user_ctx;
  if (index >= ctx->col_count) {
    ctx->col_count = index + 1;
  }
  return READSTAT_HANDLER_OK;
}

static int iter_handle_data (int row_index, readstat_variable_t *variable,
                             readstat_value_t value, void *user_ctx) {
  l_flat_ctx_t *ctx = (l_flat_ctx_t *)user_ctx;
  lua_State *L = ctx->L;
  int var_index = readstat_variable_get_index(variable);
  if (row_index < 0 || var_index < 0 || ctx->col_count <= 0) {
    return READSTAT_HANDLER_OK;
  }
  /* Always update max_cell first so trailing empty cells in the last row don't truncate row_count */
  int flat_idx = (row_index * ctx->col_count) + var_index + 1;
  if (flat_idx > ctx->max_cell) {
    ctx->max_cell = flat_idx;
  }
  if (readstat_value_is_system_missing(value)) {
    return READSTAT_HANDLER_OK;
  }
  readstat_type_t type = readstat_value_type(value);
  if (type == READSTAT_TYPE_STRING || type == READSTAT_TYPE_STRING_REF) {
    const char *raw = readstat_string_value(value);
    const char *p = raw;
    /* Skip leading whitespace to detect empty or space-padded SPSS strings */
    while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    if (!p || *p == '\0') {
      return READSTAT_HANDLER_OK; /* Whitespace-only or empty -> leave hole */
    }
    lua_pushinteger(L, flat_idx);
    lua_pushstring(L, raw);
    lua_rawset(L, ctx->cells_idx);
  } else if (type == READSTAT_TYPE_DOUBLE || type == READSTAT_TYPE_FLOAT) {
    lua_pushinteger(L, flat_idx);
    lua_pushnumber(L, readstat_double_value(value));
    lua_rawset(L, ctx->cells_idx);
  } else if (type == READSTAT_TYPE_INT32 || type == READSTAT_TYPE_INT16 || type == READSTAT_TYPE_INT8) {
    lua_pushinteger(L, flat_idx);
    lua_pushinteger(L, readstat_int32_value(value));
    lua_rawset(L, ctx->cells_idx);
  }
  return READSTAT_HANDLER_OK;
}

/* Iterator closure: builds one sparse row table with holes for nil cells */
static int iter (lua_State *L) {
  int col_count   = (int)lua_tointeger(L, lua_upvalueindex(2));
  int row_count   = (int)lua_tointeger(L, lua_upvalueindex(3));
  int current_row = (int)lua_tointeger(L, lua_upvalueindex(4));
  if (current_row >= row_count || col_count <= 0) {  /* End of rows -> returns null */
    lua_pushnil(L);
    return 1;
  }
  lua_pushinteger(L, current_row + 1);
  lua_replace(L, lua_upvalueindex(4));
  lua_createtable(L, col_count, 0);
  int c, base = current_row * col_count;
  for (c=1; c <= col_count; c++) {
    lua_rawgeti(L, lua_upvalueindex(1), base + c);
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1); /* Leave hole at key c */
    } else {
      lua_rawseti(L, -2, c);
    }
  }
  return 1;
}

/* Factory: f := readstat.iterate("file.sav") */
static int readstat_iterate (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  if (!tools_exists(filename)) {
    luaL_error(L, "Error in " LUA_QS ": file does not exist or is inaccessible.", "readstat.iterate");
    return 0;
  }
  /* 1. Create the table ONCE (sits at stack slot 2) */
  lua_newtable(L);
  int cells_idx = lua_gettop(L);
  l_flat_ctx_t ctx = {
    .L = L,
    .cells_idx = cells_idx,
    .col_count = 0,
    .max_cell = 0
  };
  readstat_parser_t *parser = readstat_parser_init();
  if (!parser) {
    luaL_error(L, "Error in " LUA_QS ": failed to initialise parser.", "readstat.iterate");
    return 0;
  }
  readstat_set_variable_handler(parser, &iter_handle_variable);
  readstat_set_value_handler(parser, &iter_handle_data);
  readstat_error_t error = readstat_parse_sav(parser, filename, &ctx);
  readstat_parser_free(parser);
  if (error != READSTAT_OK) {
    luaL_error(L, "Error in " LUA_QS ": got readstat error %s.", "readstat.iterate",
      readstat_error_message(error));
    return 0;
  }
  int row_count = (ctx.col_count > 0) ? (ctx.max_cell / ctx.col_count) : 0;
  /* 2. Push the 3 numbers on top of the table (slots 3, 4, 5) */
  lua_pushinteger(L, ctx.col_count);
  lua_pushinteger(L, row_count);
  lua_pushinteger(L, 0);
  /* 3. Consume slots 2, 3, 4, 5 (table + 3 numbers) and return the iterator */
  lua_pushcclosure(L, iter, 4);
  return 1;
}


/* Register the module functions for Lua 5.1 / Agena core */
static const struct luaL_Reg readstat_funcs[] = {
  {"iterate", readstat_iterate},
  {"read",    readstat_read},
  {"write",   readstat_write},
  {NULL,      NULL}
};

int luaopen_readstat(lua_State *L) {
  luaL_register(L, AGENA_READSTATLIBNAME, readstat_funcs);
  return 1;
}
