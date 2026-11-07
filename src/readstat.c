/*
** $Id: readstat.c, initiated November 04, 2026 $
** SPSS sav readstat binding
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
#include "prepdefs.h"  /* for __SOLARIS */

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_READSTATLIBNAME "readstat"
LUALIB_API int (luaopen_readstat) (lua_State *L);
#endif

/* Ensure ReadStat 1.1.9 success macro is defined */
#ifndef READSTAT_HANDLER_OK
#define READSTAT_HANDLER_OK 0
#endif

/* Context structure to keep track of our Lua state and tables */
typedef struct {
  lua_State *L;
  int columns_count;
} l_readstat_ctx_t;


/* Extrem schlankes Status-Objekt: Hält nur Metadaten und die gesuchte Zeilennummer */
typedef struct {
  const char *filename;
  int var_count;
  int row_to_yield;
  int row_found;
  lua_State *L;
} l_stream_state_t;

/* Callback 1: Required placeholder for modern metadata sequence */
static int handle_metadata(readstat_metadata_t *metadata, void *user_ctx) {
  l_readstat_ctx_t *ctx = (l_readstat_ctx_t *)user_ctx;
  lua_State *L = ctx->L;
  int obs_count = readstat_get_row_count(metadata);
  int var_count = readstat_get_var_count(metadata);
  lua_pushstring(L, "rows");
  lua_pushinteger(L, obs_count);
  lua_settable(L, -3);
  lua_pushstring(L, "columns");
  lua_pushinteger(L, var_count);
  lua_settable(L, -3);
  return READSTAT_HANDLER_OK;
}

/* Callback 2: Handles Metadata (Variables) - Exactly your original structure but uses string key */
static int handle_variable (int index, readstat_variable_t *variable,
                            const char *val_labels, void *user_ctx) {
  l_readstat_ctx_t *ctx = (l_readstat_ctx_t *)user_ctx;
  lua_State *L = ctx->L;
  const char *var_name = readstat_variable_get_name(variable);
  const char *var_label = readstat_variable_get_label(variable);
  readstat_type_t var_type = readstat_variable_get_type(variable);
  /* Get 'variables' sub-table using your readable string label */
  luaL_checkstack(L, 4, "not enough stack space");
  lua_pushstring(L, "variables");
  lua_gettable(L, -2);
  lua_pushinteger(L, index + 1);  /* Lua 1-indexed */
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
  if (var_type == READSTAT_TYPE_STRING) {
    lua_pushstring(L, "string");
  } else if (var_type == READSTAT_TYPE_DOUBLE || var_type == READSTAT_TYPE_FLOAT ||
         var_type == READSTAT_TYPE_INT32  || var_type == READSTAT_TYPE_INT16) {
    lua_pushstring(L, "numeric");
  } else {
    lua_pushstring(L, "unknown");
  }
  lua_settable(L, -3);
  /* 4. Track value label mapping dictionary name */
  lua_pushstring(L, "value_labels");
  lua_pushstring(L, val_labels ? val_labels : "");
  lua_settable(L, -3);
  lua_settable(L, -3);
  lua_pop(L, 1);  /* Pop 'variables' sub-table */
  ctx->columns_count++;
  return READSTAT_HANDLER_OK;
}

/* Callback 3: Handles Value Labels - Exactly your original structure but uses string key */
static int handle_value_label (const char *val_labels, readstat_value_t value,
                               const char *label, void *user_ctx) {
  l_readstat_ctx_t *ctx = (l_readstat_ctx_t *)user_ctx;
  lua_State *L = ctx->L;
  /* Get 'value_labels' sub-table using your readable string label */
  luaL_checkstack(L, 4, "not enough stack space");
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
  if (type == READSTAT_TYPE_STRING) {
    lua_pushstring(L, readstat_string_value(value));
  } else {
    lua_pushnumber(L, readstat_double_value(value));
  }
  lua_pushstring(L, label);
  lua_rawset(L, -3);
  lua_pop(L, 2);
  return READSTAT_HANDLER_OK;
}

/* Callback 4: Handles Row Data - Exactly your original structure but uses string key */
static int handle_value (int row_index, readstat_variable_t *variable, readstat_value_t value, void *user_ctx) {
  l_readstat_ctx_t *ctx = (l_readstat_ctx_t *)user_ctx;
  lua_State *L = ctx->L;
  int var_index = readstat_variable_get_index(variable);
  luaL_checkstack(L, 4, "not enough stack space");
  /* Get 'rows' sub-table using your readable string label */
  lua_pushstring(L, "data");
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
  lua_pushinteger(L, var_index + 1);
  readstat_type_t type = readstat_value_type(value);
  if (readstat_value_is_system_missing(value)) {
    lua_pushnil(L);
  } else if (type == READSTAT_TYPE_STRING) {
    lua_pushstring(L, readstat_string_value(value));
  } else if (type == READSTAT_TYPE_DOUBLE || type == READSTAT_TYPE_FLOAT) {
    lua_pushnumber(L, readstat_double_value(value));
  } else if (type == READSTAT_TYPE_INT32 || type == READSTAT_TYPE_INT16) {
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
  const char *filename = luaL_checkstring(L, 1);
  if (!tools_exists(filename)) {
    luaL_error(L, "Error in " LUA_QS ": file does not exist or inaccessible.", "readstat.read");
  }
  readstat_parser_t *parser = readstat_parser_init();
  readstat_set_metadata_handler(parser, &handle_metadata);
  readstat_set_variable_handler(parser, &handle_variable);
  readstat_set_value_handler(parser, &handle_value);
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
  l_readstat_ctx_t ctx = { .L = L, .columns_count = 0 };
  readstat_error_t error = readstat_parse_sav(parser, filename, &ctx);
  readstat_parser_free(parser);
  if (error != READSTAT_OK) {
    lua_pushnil(L);
    lua_pushstring(L, readstat_error_message(error));
    return 2;
  }
  return 1;
}


/* Main writing wrapper: readstat.write("output.sav", data_table) */

/* Callback: Low-level disk stream writer callback */

#if (defined(_WIN32) && !defined(_WIN64)) || defined(__SOLARIS) || defined(__OS2__)
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
static int readstat_save (lua_State *L) {
  const char *filename = luaL_checkstring(L, 2);
  luaL_checktype(L, 1, LUA_TTABLE);  /* Ensure parameter 2 is the data table */
  FILE *f = fopen(filename, "wb");
  if (!f) {
    luaL_error(L, "Error in " LUA_QS ": cannot open output file for writing.", "readstat.save");
    return 1;
  }
  readstat_writer_t *writer = readstat_writer_init();
  readstat_set_data_writer(writer, &handle_write);
  /* Track variable pointers internally to assign label sets directly later */
  readstat_variable_t *variables_list[1000] = { NULL };
  char variable_categories[1000][256];
  memset(variable_categories, 0, sizeof(variable_categories));
  luaL_checkstack(L, 6, "not enough stack space");
  /* --- PHASE 1: EXTRACT VARIABLES & REGISTER SCHEMA --- */
  lua_pushstring(L, "variables");
  lua_gettable(L, 1);  /* Get data.variables table */
  int var_count = 0;
  if (lua_istable(L, -1)) {
    var_count = lua_rawlen(L, -1);
    if (var_count > 1000) {  /* Hard boundary safety check */
      readstat_writer_free(writer);
      fclose(f);
      luaL_error(L, "Error in " LUA_QS ": only 1,000 columns allowed, got %d.", "readstat.save", var_count);
      return 1;
    }
    int i;
    for (i=1; i <= var_count; i++) {
      lua_rawgeti(L, -1, i);  /* Get variable properties table */
      if (lua_istable(L, -1)) {
        /* Get name */
        lua_pushstring(L, "name");
        lua_gettable(L, -2);
        const char *v_name = lua_tostring(L, -1);
        lua_pop(L, 1);
        /* Get type */
        lua_pushstring(L, "type");
        lua_gettable(L, -2);
        const char *v_type = lua_tostring(L, -1);
        lua_pop(L, 1);
        /* Get label */
        lua_pushstring(L, "label");
        lua_gettable(L, -2);
        const char *v_label = lua_tostring(L, -1);
        lua_pop(L, 1);
        /* Get categories string name */
        lua_pushstring(L, "value_labels");
        lua_gettable(L, -2);
        const char *v_cats = lua_tostring(L, -1);
        if (v_cats) {
          strncpy(variable_categories[i - 1], v_cats, 255);
        }
        lua_pop(L, 1);
        readstat_type_t rs_type = READSTAT_TYPE_DOUBLE;
        if (v_type && strcmp(v_type, "string") == 0) {
          rs_type = READSTAT_TYPE_STRING;
        }
        /* Register variable pointer into tracking list */
        variables_list[i - 1] = readstat_add_variable(writer, v_name ? v_name : "UNKNOWN", rs_type, 0);
        if (variables_list[i - 1] && v_label) {
          if (v_label) {
            readstat_variable_set_label(variables_list[i - 1], v_label);
          }
          if (rs_type == READSTAT_TYPE_DOUBLE) {
            readstat_variable_set_format(variables_list[i - 1], "F8.4");
          }
        }
      }
      lua_pop(L, 1);  /* Pop variable properties table */
    }
  }
  lua_pop(L, 1);  /* Pop data.variables sub-table */
  /* --- PHASE 2: PARSE VALUE LABELS & BIND DIRECTLY TO VARIABLES --- */
  lua_pushstring(L, "value_labels");
  lua_gettable(L, 1);  /* Get data.value_labels table */
  if (lua_istable(L, -1)) {
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      if (lua_istable(L, -1)) {
        const char *dict_name = lua_tostring(L, -2);
        readstat_label_set_t *label_set = readstat_add_label_set(writer, READSTAT_TYPE_DOUBLE, dict_name);
        if (label_set) {
          lua_pushnil(L);
          while (lua_next(L, -2) != 0) {
            double cat_code = lua_tonumber(L, -2);
            const char *cat_label = lua_tostring(L, -1);
            readstat_label_double_value(label_set, cat_code, cat_label);
            lua_pop(L, 1);
          }
          /* Instantly attach this label set to any variable that queries it */
          int v;
          for (v=0; v < var_count; v++) {
            if (strcmp(variable_categories[v], dict_name) == 0 && variables_list[v] != NULL) {
              readstat_variable_set_label_set(variables_list[v], label_set);
            }
          }
        }
      }
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1); /* Pop data.value_labels sub-table */
  /* --- PHASE 3: CALCULATE ROWS COUNT AND BEGIN FILE GENERATION --- */
  lua_pushstring(L, "data");
  lua_gettable(L, 1);  /* Get data.rows table */
  int row_count = 0;
  if (lua_istable(L, -1)) {
    row_count = lua_rawlen(L, -1);
  }
  readstat_error_t error = readstat_begin_writing_sav(writer, f, row_count);
  if (error == READSTAT_OK && row_count > 0) {
    /* --- PHASE 4: ITERATE ROWS AND WRITE CELL VALUES --- */
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
            readstat_insert_string_value(writer, variable, lua_tostring(L, -1));
          } else {
            readstat_insert_double_value(writer, variable, lua_tonumber(L, -1));
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

/* Register the module functions for Lua 5.1 / Agena core */
static const struct luaL_Reg readstat_funcs[] = {
  {"read",  readstat_read},
  {"save",  readstat_save},
  {NULL,    NULL}
};

int luaopen_readstat(lua_State *L) {
  luaL_register(L, AGENA_READSTATLIBNAME, readstat_funcs);
  return 1;
}
