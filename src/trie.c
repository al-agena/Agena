/* This library implements a binary crit-bit tree. It is based on Adam Langley critbit C library, taken from

   https://github.com/agl/critbit

   No licence information is given there.

   Adam Langley about his library: "This code is taken from Dan Bernstein's qhasm and implements a binary crit-bit
   (also known as PATRICIA) tree for |NUL| terminated strings. Crit-bit trees are underused and it's this author's
   hope that a good example will aid their adoption."

   For the licence of the Agena-specific code, see agena.h (MIT licence). */

#define trie_c
#define LUA_LIB

#define _POSIX_C_SOURCE 200112
#define uint8 uint8_t
#define uint32 uint32_t

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef __DJGPP__
#include <malloc.h>
#endif

#include "agena.h"

#include "agenalib.h"
#include "agnhlps.h"
#include "agnxlib.h"

#define checktrie(L, n)   (Trie *)luaL_checkudata(L, n, AGENA_TRIELIBNAME)
#define istrie(L,n)       (luaL_isudata(L, n, AGENA_TRIELIBNAME))

typedef struct {
  void   *child[2];
  uint32 byte;
  uint8  otherbits;
} critbit0_node;

typedef struct{
  void *root;
} critbit0_tree;

static int critbit0_contains (critbit0_tree *t, const char *u, size_t ulen) {
  const uint8 *ubytes = (void *)u;
  uint8 *p = t->root;
  if (!p) return 0;
  while (1 & (intptr_t)p) {
    critbit0_node *q = (void *)(p - 1);
    uint8 c = 0;
    if (q->byte < ulen) c = ubytes[q->byte];
    const int direction = (1 + (q->otherbits | c)) >> 8;
    p = q->child[direction];
  }
  return tools_streq(u, (const char *)p);
}

static int critbit0_insert (critbit0_tree *t, const char *u, size_t ulen) {
  const uint8 * const ubytes = (void *)u;
  uint8 *p = t->root;
  if (!p) {
    char *x;
    x = (char *)tools_memalign(sizeof(void *), ulen + 1); // valloc(sizeof(void *)*(ulen + 1));
    if (!x) return 0;
    memcpy(x, u, ulen + 1);
    t->root = x;
    return 2;
  }
  while (1 & (intptr_t)p) {
    critbit0_node *q = (void*)(p - 1);
    uint8 c = 0;
    if (q->byte<ulen) c = ubytes[q->byte];
    const int direction= (1 + (q->otherbits | c)) >> 8;
    p = q->child[direction];
  }
  uint32 newbyte;
  uint32 newotherbits;
  for (newbyte = 0; newbyte < ulen; ++newbyte) {
    if (p[newbyte] != ubytes[newbyte]) {
      newotherbits = p[newbyte] ^ ubytes[newbyte];
      goto different_byte_found;
    }
  }
  if (p[newbyte] != 0) {
    newotherbits = p[newbyte];
    goto different_byte_found;
  }
  return 1;
different_byte_found:
  newotherbits |= newotherbits >> 1;
  newotherbits |= newotherbits >> 2;
  newotherbits |= newotherbits >> 4;
  newotherbits = (newotherbits & ~(newotherbits >> 1)) ^ 255;
  uint8 c = p[newbyte];
  int newdirection = (1 + (newotherbits | c)) >> 8;
  critbit0_node *newnode;
  newnode = (critbit0_node *)tools_memalign(sizeof(void *), sizeof(critbit0_node));
  if (!newnode) return 0;
  char *x = (char *)tools_memalign(sizeof(void *), ulen + 1);
  if (!x) {
    tools_memaligned_free(newnode);
    return 0;
  }
  memcpy(x, ubytes, ulen + 1);
  newnode->byte = newbyte;
  newnode->otherbits = newotherbits;
  newnode->child[1 - newdirection] = x;
  void **wherep = &t->root;
  for (;;) {
    uint8 *p = *wherep;
    if (!(1 & (intptr_t)p)) break;
    critbit0_node *q = (void *)(p - 1);
    if (q->byte > newbyte) break;
    if (q->byte == newbyte && q->otherbits > newotherbits) break;
    uint8 c = 0;
    if (q->byte < ulen) c = ubytes[q->byte];
    const int direction = (1 + (q->otherbits | c)) >> 8;
    wherep = q->child + direction;
  }
  newnode->child[newdirection] = *wherep;
  *wherep = (void *)(1 + (char *)newnode);
  return 2;
}

static int critbit0_delete (critbit0_tree*t, const char *u, size_t ulen) {
  const uint8 *ubytes = (void *)u;
  uint8 *p = t->root;
  void **wherep = &t->root;
  void **whereq = 0;
  critbit0_node *q = 0;
  int direction = 0;
  if (!p) return 0;
  while (1 & (intptr_t)p) {
    whereq = wherep;
    q = (void *)(p - 1);
    uint8 c = 0;
    if (q->byte < ulen) c = ubytes[q->byte];
    direction = (1 + (q->otherbits | c)) >> 8;
    wherep = q->child + direction;
    p = *wherep;
  }
  if (!tools_streq(u, (const char *)p)) return 0;
  tools_memaligned_free(p);
  if (!whereq) {
    t->root = 0;
    return 1;
  }
  *whereq = q->child[1 - direction];
  tools_memaligned_free(q);
  return 1;
}

static void traverse (void *top) {
  uint8 *p = top;
  if (1 & (intptr_t)p) {
    critbit0_node *q = (void *)(p - 1);
    traverse(q->child[0]);
    traverse(q->child[1]);
    tools_memaligned_free(q);
  } else {
    tools_memaligned_free(p);
  }
}

static void critbit0_clear (critbit0_tree *t) {
  if (t->root) traverse(t->root);
  t->root = NULL;
}

static void cb0_traverse (void *top, int *size) {
  uint8 *p = top;
  if (1 & (intptr_t)p) {
    critbit0_node *q = (void *)(p - 1);
    if (q->child[0]) (void)(*size)++;
    cb0_traverse(q->child[0], size);
    if (q->child[1]) (void)(*size)++;
    cb0_traverse(q->child[1], size);
  }
}

static int critbit0_traverse (critbit0_tree *t) {
  int i = 0;
  if (t->root) {
    i++;
    cb0_traverse(t->root, &i);
  }
  return i;
}

static int allprefixed_traverse (uint8 *top, int(*handle)(const char *, void *), void *arg) {
  if (1 & (intptr_t)top) {
    int direction;
    critbit0_node *q = (void*)(top - 1);
    for (direction = 0; direction < 2; ++direction) {
      switch (allprefixed_traverse(q->child[direction], handle, arg)) {
        case 1:  break;  /* search for further entries */
        case 0:  return 0;  /* do not search for further entries and return result */
        default: return -1;
      }
    }
    return 1;
  }
  return handle((const char *)top, arg);
}

int critbit0_allprefixed (critbit0_tree *t, const char *prefix, int(*handle)(const char *, void *), void *arg) {
  size_t i;
  const uint8 *ubytes = (void*)prefix;
  const size_t ulen = strlen(prefix);
  uint8 *p = t->root;
  uint8 *top = p;
  if (!p) return 1;
  while (1 & (intptr_t)p) {
    critbit0_node *q = (void *)(p - 1);
    uint8 c = 0;
    if (q->byte < ulen) c = ubytes[q->byte];
    const int direction = (1 + (q->otherbits | c)) >> 8;
    p = q->child[direction];
    if (q->byte < ulen) top = p;
  }
  for (i=0; i < ulen; ++i) {
    if (p[i] != ubytes[i]) return 1;
  }
  return allprefixed_traverse(top, handle, arg);
}

/* Agena Library Functions ******************************************************************************* */

typedef struct {
  critbit0_tree *trie;
  int registry;  /* registry, for attribute information, stored to index */
  int size;
  int type;      /* 0 = prefix tree, 1 = suffix tree */
} Trie;


/* The function creates a trie and returns it. The trie has an internal status table that you can access
   with `trie.getstore`. */
static int trie_new (lua_State *L) {
  int nargs = lua_gettop(L);
  Trie *c = (Trie *)lua_newuserdata(L, sizeof(Trie));
  if (!c)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "trie.new");
  lua_setmetatabletoobject(L, -1, AGENA_TRIELIBNAME, 0);
  agn_setutypestring(L, -1, AGENA_TRIELIBNAME);
  lua_createtable(L, 0, 0);
  c->registry = luaL_ref(L, LUA_REGISTRYINDEX);
  c->trie = calloc(1, sizeof(critbit0_tree));
  if (!c->trie) {  /* 7.3.4 fix */
    agn_poptop(L);  /* pop new userdata */
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "trie.new");
  }
  c->size = 0;
  c->type = (nargs > 0);  /* 0 = prefix tree, 1 = suffix tree */
  return 1;
}


/* Inserts number or string v into trie t. If v is a number, it is converted to a string before insertion.
   The function returns 2 if v is a new element, 1 if v has already been stored in t and 0 if there was a
   failure during insertion, for example because of insufficient memory. */
static int trie_include (lua_State *L) {
  int rc;
  size_t l;
  char *str;
  Trie *c = checktrie(L, 1);
  agnL_checkudfreezed(L, 1, AGENA_TRIELIBNAME, "trie.include");
  str = (char *)luaL_checklstring(L, 2, &l);
  if (c->type) str = str_reverse(str, l);
  rc = critbit0_insert(c->trie, str, l);
  lua_pushinteger(L, rc);
  if (rc) c->size++;
  if (c->type) { xfree(str); }
  return 1;
}


/* Removes the number or string v from trie t and returns `true` on success and `false` on failure,
   that is v is not part of t. */
static int trie_remove (lua_State *L) {
  int rc;
  size_t l;
  char *str;
  Trie *c = checktrie(L, 1);
  agnL_checkudfreezed(L, 1, AGENA_TRIELIBNAME, "trie.remove");
  str = (char *)luaL_checklstring(L, 2, &l);
  if (c->type) str = str_reverse(str, l);
  rc = critbit0_delete(c->trie, str, l);
  lua_pushboolean(L, rc != 0);
  if (rc) c->size--;
  if (c->type) { xfree(str); }
  return 1;
}


/* The function returns the internal status table of trie t, same as the expression t(). */
static int trie_getstore (lua_State *L) {
  Trie *c = checktrie(L, 1);
  return agnL_calludata(L, lua_gettop(L), c->registry, AGENA_TRIELIBNAME);
}


/* Checks whether the given string or number v is stored in trie t and returns `true` or `false`. If v
   is a number it is converted to a string before the search starts. The function searches for the
   entire string, not just a prefix match. */
static int trie_has (lua_State *L) {
  int rc;
  size_t l;
  Trie *c = checktrie(L, 1);
  char *str = (char *)agn_checklstring(L, 2, &l);
  if (c->type) str = str_reverse(str, l);
  rc = critbit0_contains(c->trie, str, l);
  lua_pushboolean(L, rc);
  if (c->type) { xfree(str); }
  return 1;
}


/* Finds all the strings in trie t that match the given prefix, a string, and returns them in alphabetical
   order in a sequence.

   When called without prefix, the function returns all strings in t. Alternatively you can pass the empty
   string for prefix to also dump the entire contents.

   If there is no match, the function returns `null`. */
int find_callback (const char *s, void *arg) {
  lua_State *L = arg;
  lua_pushstring(L, s);
  lua_seqinsert(L, -2);
  return 1;  /* must return 1 to traverse entire trie, 0 to get only first match */
}

static int trie_find (lua_State *L) {
  int nargs, seqlen, i;
  size_t l;
  char *str = NULL;
  nargs = lua_gettop(L);
  Trie *c = checktrie(L, 1);
  if (nargs == 1) {
    str = "";
  } else {
    str = (char *)agn_checklstring(L, 2, &l);
    if (c->type) str = str_reverse(str, l);
  }
  if (agnL_optboolean(L, 3, 0)) {  /* 5.5.11 extension */
    lua_pushboolean(L, critbit0_contains(c->trie, str, l));
    if (c->type) { xfree(str); }  /* 5.5.12 fix */
    return 1;
  }
  agn_createseq(L, 0);
  critbit0_allprefixed(c->trie, str, find_callback, L);
  seqlen = agn_seqsize(L, -1);
  if (c->type && seqlen) {  /* suffix tree and at least one result ? */
    int rc;
    char *str = NULL;
    for (i=1; i <= seqlen; i++) {
      str = (char *)agn_seqrawgetilstring(L, -1, i, &l, &rc);
      if (str == NULL) {
        luaL_error(L, "Error in " LUA_QS ": failed to search in suffix tree.", "trie.find");
      }
      str = str_reverse(str, l);
      lua_seqrawsetilstring(L, -1, i, str, l);
      xfree(str);
    }
    /* up to now, the search result is not sorted */
    luaL_checkstack(L, 2, "not enough stack space");
    lua_getglobal(L, "sort");
    if (!lua_isfunction(L, -1))
      luaL_error(L, "Error in " LUA_QS ": could not fetch internal sorting function.", "trie.find");
    lua_pushvalue(L, -2);
    lua_call(L, 1, 0);
  } else if (seqlen == 0) {  /* no match, return null */
    agn_poptop(L);
    lua_pushnil(L);
  }
  if (c->type) { xfree(str); }
  return 1;
}


static int trie_attrib (lua_State *L) {
  Trie *c = checktrie(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_createtable(L, 0, 2);
  lua_rawsetstringinteger(L, -1, "length", c->size);
  lua_rawsetstringboolean(L, -1, "isempty", c->size == 0);
  lua_rawsetstringinteger(L, -1, "nodes", critbit0_traverse(c->trie));
  lua_rawsetstringboolean(L, -1, "suffixtree", c->type);
  return 1;
}

/* Metamthods ***************************************************************** */

static int mt_size (lua_State *L) {
  Trie *c = checktrie(L, 1);
  lua_pushnumber(L, c->size);
  return 1;
}

static int mt_empty (lua_State *L) {
  Trie *c = checktrie(L, 1);
  lua_pushboolean(L, c->size == 0);
  return 1;
}

static int mt_filled (lua_State *L) {
  Trie *c = checktrie(L, 1);
  lua_pushboolean(L, c->size != 0);
  return 1;
}

static int mt_tostring (lua_State *L) {  /* at the console, the trie is formatted as follows: */
  if (luaL_isudata(L, 1, AGENA_TRIELIBNAME))
    lua_pushfstring(L, "trie(%p)", lua_topointer(L, 1));
  else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}

static int mt_in (lua_State *L) {
  int rc;
  size_t l;
  char *str = (char *)agn_checklstring(L, 1, &l);
  Trie *c = checktrie(L, 2);
  if (c->type) str = str_reverse(str, l);
  rc = critbit0_contains(c->trie, str, l);
  lua_pushboolean(L, rc);
  if (c->type) { xfree(str); }
  return 1;
}

static int mt_notin (lua_State *L) {
  int rc;
  size_t l;
  char *str = (char *)agn_checklstring(L, 1, &l);
  Trie *c = checktrie(L, 2);
  if (c->type) str = str_reverse(str, l);
  rc = critbit0_contains(c->trie, str, l);
  lua_pushboolean(L, !rc);
  if (c->type) { xfree(str); }
  return 1;
}

static int mt_gc (lua_State *L) {
  Trie *c;
  lua_lock(L);
  c = checktrie(L, 1);
  critbit0_clear(c->trie);
  xfree(c->trie);
  luaL_unref(L, LUA_REGISTRYINDEX, c->registry);  /* delete registry table */
  lua_setmetatabletoobject(L, 1, NULL, 1);
  lua_unlock(L);
  return 0;
}


static const struct luaL_Reg trie_trielib [] = {  /* metamethods for tries */
  {"attrib",     trie_attrib},
  {"find",       trie_find},
  {"getstore",   trie_getstore},
  {"has",        trie_has},
  {"include",    trie_include},
  {"remove",     trie_remove},
  {"__gc",       mt_gc},          /* please do not forget garbage collection */
  {"__in",       mt_in},          /* `in` operator */
  {"__notin",    mt_notin},       /* `notin` operator */
  {"__size",     mt_size},        /* retrieve the number of entries in `n' */
  {"__empty",    mt_empty},       /* metamethod for `empty` operator */
  {"__filled",   mt_filled},      /* metamethod for `filled` operator */
  {"__tostring", mt_tostring},    /* for output at the console, e.g. print(n) */
  {"__call",     trie_getstore},  /* to retrieve the internal store of a trie */
  {NULL, NULL}
};

static const luaL_Reg trielib[] = {
  {"attrib",     trie_attrib},
  {"find",       trie_find},
  {"getstore",   trie_getstore},
  {"has",        trie_has},
  {"include",    trie_include},
  {"new",        trie_new},
  {"remove",     trie_remove},
  {NULL, NULL}
};


/*
** Open trie library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_TRIELIBNAME);  /* create metatable for file handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, trie_trielib);  /* methods */
}

LUALIB_API int luaopen_trie (lua_State *L) {
  /* metamethods for tries */
  createmeta(L);
  /* register library */
  luaL_register(L, AGENA_TRIELIBNAME, trielib);
  return 1;
}