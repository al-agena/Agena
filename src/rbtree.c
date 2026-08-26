/**
 * MIT Licence
 *
 * Copyright (c) 2023 Mathieu Rabine
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Modifications, especially the Agena integration, by awalz, also MIT licenced.
 *
 * The genuine "C" part has been taken from: https://github.com/mrabine/rbtree
 *
 * Note: Red-black trees store an element only once, all duplicates are ignored
 * when trying to insert them into the tree.
 */

/*
import rbtree;

a := rbtree.new();

tostring(a):

for i from 10 downto 1 do
   rbtree.include(a, i)
od;

rbtree.entries(a):

10 in a, 11 in a:

rbtree.find(a, 0):

empty a, filled a:

rbtree.remove(a, 10):

10 in a:

size a:

rbtree.min(a), rbtree.max(a):

rbtree.minmax(a):

f := rbtree.iterate(a)

while x := f() do
   print(x)
od
*/

#define rbtree_c
#define LUA_LIB

#include <stdlib.h>

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnxlib.h"
#include "rbtree.h"

int INLINE rb_compare_dbl (double a, double b) {
  return (a > b) - (a < b);
}

int is_red (struct rbnode *node) {
  return node ? node->red : 0;
}

struct rbnode *make_node (double data) {
  struct rbnode *node = malloc(sizeof(struct rbnode));
  if (node != NULL) {
    node->link[0] = node->link[1] = node->parent = NULL;
    node->data = data;
    node->red = 1;
  }
  return node;
}

struct rbnode *single_rotate (struct rbnode *node, int dir) {
  struct rbnode *save = node->link[!dir];
  if (save != NULL) {
    node->link[!dir] = save->link[dir];
    if (node->link[!dir] != NULL)
      node->link[!dir]->parent = node;
    save->parent = node->parent;
    save->link[dir] = node;
    node->parent = save;
    node->red = 1;
    save->red = 0;
  }
  return save;
}

struct rbnode *double_rotate (struct rbnode *node, int dir) {
  node->link[!dir] = single_rotate(node->link[!dir], !dir);
  return single_rotate(node, dir);
}

struct rbtree *rb_create (rb_compare *compare, rb_destroy *destroy) {
  struct rbtree *tree = malloc(sizeof(struct rbtree));
  if (tree != NULL) {
    tree->comp  = compare;
    tree->del   = destroy;
    tree->root  = NULL;
    tree->count = 0;
  }
  return tree;
}

void rb_delete (struct rbtree *tree) {
  if (tree != NULL) {
    struct rbnode *node = tree->root;
    struct rbnode *save = NULL;
    while (node != NULL) {
      if (node->link[0] == NULL) {
        save = node->link[1];
        /* if (tree->del != NULL && node->data != NULL)
           tree->del(node->data); */
        free(node);
        node = NULL;
      } else {
        save = node->link[0];
        node->link[0] = save->link[1];
        save->link[1] = node;
      }
      node = save;
    }
    free(tree);
  }
}

double rb_insert (struct rbtree *tree, double data) {
  double inserted = AGN_NAN;
  if (tree->root == NULL) {
    tree->root = make_node(data);
    if (tree->root == NULL) return AGN_NAN;
    inserted = tree->root->data;
    ++tree->count;
  } else {
    struct rbnode head = { { 0 } };
    struct rbnode *g, *t;
    struct rbnode *p, *q;
    int dir, last, comp;
    dir = last = 0;
    t = &head;
    g = p = NULL;
    q = t->link[1] = tree->root;
    for (;;) {
      if (q == NULL) {  /* if value is already in tree, this is false */
        p->link[dir] = q = make_node(data);
        if (q == NULL) {
          return AGN_NAN;
        }
        q->parent = p;
        inserted = q->data;
        ++tree->count;
      } else if (is_red(q->link[0]) && is_red(q->link[1])) {  /* if value is already in tree, this is false */
        q->red = 1;
        q->link[0]->red = q->link[1]->red = 0;
      }
      if (is_red(q) && is_red(p)) {  /* if value is already in tree, this is false */
        int dir2 = t->link[1] == g;
        if (q == p->link[last]) {
          t->link[dir2] = single_rotate(g, !last);
        } else {
          t->link[dir2] = double_rotate(g, !last);
        }
      }
      if (!isnan(inserted) || (comp = tree->comp(q->data, data)) == 0) {
        break;
      }
      last = dir;
      dir = comp < 0;
      if (g != NULL) {
        t = g;
      }
      g = p, p = q;
      q = q->link[dir];
    }
    tree->root = head.link[1];
  }
  tree->root->red = 0;
  return inserted;
}

int rb_find (struct rbtree *tree, double data) {
  if (tree != NULL) {
    struct rbnode *node = tree->root;
    int comp, c;
    c = 0;
    while (node != NULL) {
      c++;
      if ((comp = tree->comp(node->data, data)) == 0)
        return c;
      node = node->link[comp < 0];
    }
  }
  return 0;
}

int rb_remove (struct rbtree *tree, double data) {
  int rc = 0;
  if (tree->root != NULL) {
    struct rbnode head = { { 0 } };
    struct rbnode *q, *p, *g;
    struct rbnode *f = NULL;
    int dir = 1;
    q = &head;
    g = p = NULL;
    q->link[1] = tree->root;
    while (q->link[dir] != NULL) {  /* traverse tree */
      int last = dir;
      g = p, p = q;
      q = q->link[dir];
      int comp = tree->comp(q->data, data);  /* 0 if current value x = data, -1 if x < data, +1 if x > data */
      dir = comp < 0;
      if (comp == 0) {  /* we have a match */
        f = q;  /* thus flag it; the loop will not necessarily quit */
      }
      /* the following will be executed if necessary regardless of whether we have a match currently */
      if (!is_red(q) && !is_red(q->link[dir])) {
        if (is_red(q->link[!dir])) {
          p = p->link[last] = single_rotate(q, dir);
        } else if (!is_red(q->link[!dir])) {
          struct rbnode *s = p->link[!last];
          if (s != NULL) {
            if (!is_red(s->link[!last]) && !is_red(s->link[last])) {
              p->red = 0;
              s->red = 1;
              q->red = 1;
            } else {
              int dir2 = g->link[1] == p;
              if (is_red(s->link[last])) {
                g->link[dir2] = double_rotate(p, last);
              } else if (is_red(s->link[!last])) {
                g->link[dir2] = single_rotate(p, last);
              }
              q->red = g->link[dir2]->red = 1;
              g->link[dir2]->link[0]->red = 0;
              g->link[dir2]->link[1]->red = 0;
            }
          }
        }
      }
    }
    if (f != NULL) {
      /* if (tree->del != NULL && f->data != NULL) tree->del(f->data); */
      f->data = q->data;
      p->link[p->link[1] == q] = q->link[q->link[0] == NULL];
      --tree->count;
      free(q);
      rc = 1;
    }
    tree->root = head.link[1];
    if (tree->root != NULL) {
      tree->root->red = 0;
    }
  }
  return rc;
}

int rb_empty (struct rbtree *tree) {
  return (tree != NULL) ? rb_size(tree) == 0 : 1;
}

size_t rb_size (struct rbtree *tree) {
  return (tree != NULL) ? tree->count : 0;
}


/* Iterator part *******************************************************************/

double it_begin (struct rbtree *tree, struct rbiter *it) {
  if (it != NULL && tree != NULL) {
    it->tree = tree;
    it->node = tree->root;
    if (it->node != NULL) {
      while (it->node->link[0] != NULL) {
        it->node = it->node->link[0];
      }
      return it->node->data;
    }
  }
  return AGN_NAN;
}

double it_end (struct rbtree *tree, struct rbiter *it) {
  if (it != NULL && tree != NULL) {
    it->tree = tree;
    it->node = tree->root;
    if (it->node != NULL) {
      while (it->node->link[1] != NULL) {
        it->node = it->node->link[1];
      }
      return it->node->data;
    }
  }
  return AGN_NAN;
}

int it_next (struct rbiter *it, double *value) {
  if (it != NULL) {
    if (it->node == NULL) {
      *value = it_begin(it->tree, it);
      return !tools_isnan(*value);
    } else if (it->node->link[1] == NULL) {
      struct rbnode *q, *p;
      for (p = it->node, q = p->parent; ; p = q, q = q->parent) {
        if (q == NULL || p == q->link[0]) {
          it->node = q;
          if (it->node != NULL) {
            *value = it->node->data;
            return 1;
          }
          return 0;
        }
      }
    } else {
      it->node = it->node->link[1];
      while (it->node->link[0] != NULL) {
        it->node = it->node->link[0];
      }
      *value = it->node->data;
      return 1;
    }
  }
  return 0;
}

double it_prev (struct rbiter *it) {
  if (it != NULL) {
    if (it->node == NULL) {
      return it_end(it->tree, it);
    } else if (it->node->link[0] == NULL) {
      struct rbnode *q, *p;
      for (p = it->node, q = p->parent; ; p = q, q = q->parent) {
        if (q == NULL || p == q->link[1]) {
          it->node = q;
          return it->node != NULL ? it->node->data : AGN_NAN;
        }
      }
    } else {
      it->node = it->node->link[0];
      while (it->node->link[1] != NULL) {
        it->node = it->node->link[1];
      }
      return it->node->data;
    }
  }
  return AGN_NAN;
}

double it_cur (struct rbiter *it) {
  if (it != NULL) {
    return it->node != NULL ? it->node->data : AGN_NAN;
  }
  return AGN_NAN;
}


/* The Agena part ******************************************************************/

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_RBTREELIBNAME "rbtree"
LUALIB_API int (luaopen_rbtree) (lua_State *L);
#endif


typedef struct {
  struct rbtree *tree;
  struct rbiter it;
  int registry;  /* registry, for attribute information, stored to index 0, currently unused */
} Rbtree;

#define checkrbtree(L, n)      (Rbtree *)luaL_checkudata(L, n, "rbtree")
#define isrbtree(L,n)          (luaL_isudata(L, n, "rbtree"))

/* The function creates an empty rbtree and returns it. */
static int rbtree_new (lua_State *L) {
  Rbtree *a;
  a = (Rbtree *)lua_newuserdata(L, sizeof(Rbtree));
  if (!a)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "rbtree.new");
  lua_setmetatabletoobject(L, -1, "rbtree", 0);
  agn_setutypestring(L, -1, "rbtree");
  lua_createtable(L, 0, 0);
  a->registry = luaL_ref(L, LUA_REGISTRYINDEX);
  a->tree = rb_create(rb_compare_dbl, NULL);
  return 1;
}


/* Inserts a number into the tree. Returns true on success and false otherwise. */
static int rbtree_include (lua_State *L) {
  Rbtree *a = checkrbtree(L, 1);
  lua_Number r = rb_insert(a->tree, agn_checknumber(L, 2));
  lua_pushboolean(L, !tools_isnan(r));
  return 1;
}


/* Deletes an element from the tree and returns true on success and false if the element to be deleted could not be found in the tree. */
static int rbtree_remove (lua_State *L) {
  int rc;
  Rbtree *a;
  if (isrbtree(L, 1)) {
    a = lua_touserdata(L, 1);
    rc = rb_remove(a->tree, agn_checknumber(L, 2));  /* 3.9.4 extension */
  } else if (lua_isfunction(L, 1)) {  /* 4.4.4 */
    int r, c;
    double value;
    struct rbiter it;
    a = checkrbtree(L, 2);
    rc = 0;
    c = rb_size(a->tree);
    while (c-- && rb_size(a->tree)) {
      /* we need to call it_begin first instead of it_next to get a valid it pointer */
      value = it_begin(a->tree, &it);
      luaL_checkstack(L, 2, "not enough stack space");
      lua_pushvalue(L, 1);
      lua_pushnumber(L, value);
      lua_call(L, 1, 1);
      r = lua_istrue(L, -1);  /* 5.0.1 fix */
      agn_poptop(L);
      if (r) {
        if ( (rc = rb_remove(a->tree, value) ) == 0)
          luaL_error(L, "Error in " LUA_QS ": something went wrong.", "rbtree.remove");
        continue;
      }
      while (it_next(&it, &value)) {
        luaL_checkstack(L, 2, "not enough stack space");
        lua_pushvalue(L, 1);
        lua_pushnumber(L, value);
        lua_call(L, 1, 1);
        r = lua_istrue(L, -1);  /* 5.0.1 fix */
        agn_poptop(L);
        if (r) {
          if ( (rc = rb_remove(a->tree, value) ) == 0)
            luaL_error(L, "Error in " LUA_QS ": something went wrong.", "rbtree.remove");
          break;
        }
      }
    }
  } else {
    rc = 0;
    luaL_error(L, "Error in " LUA_QS ": wrong kind of arguments.", "rbtree.remove");
  }
  lua_pushboolean(L, rc);
  return 1;
}


/* Searches for an element in the tree and returns two results: A Boolean indicating whether the element has been found, and the height
   of the element in the tree, which is 0 on failure. */
static int rbtree_find (lua_State *L) {
  int c;
  Rbtree *a = checkrbtree(L, 1);
  c = rb_find(a->tree, agn_checknumber(L, 2));
  lua_pushboolean(L, c != 0);  /* found or not found ? */
  lua_pushinteger(L, c);  /* height of element to be searched for, 3.9.4 */
  return 2;
}


/* The function returns all entries in the rbtree in a new table, in the same order as currently represented by the tree. */
static int rbtree_entries (lua_State *L) {
  int length;
  Rbtree *a = checkrbtree(L, 1);
  length = rb_size(a->tree);
  lua_createtable(L, length, 0);
  if (length != 0) {
    double value;
    struct rbiter it = { };  /* 6.2.3 fix to prevent compiler warnings */
    int i = 0;
    /* we need to call it_begin first instead of it_next to get a valid it pointer */
    value = it_begin(a->tree, &it);
    agn_setinumber(L, -1, ++i, value);
    while (it_next(&it, &value)) {
      agn_setinumber(L, -1, ++i, value);
    }
  }
  return 1;
}


static int rbtree_purge (lua_State *L) {
  int r, rc, c;
  double value;
  struct rbiter it;
  luaL_argcheck(L, lua_isfunction(L, 1), 1, "expected a function");
  Rbtree *a = checkrbtree(L, 2);
  rc = 0;
  c = rb_size(a->tree);
  while (c-- && rb_size(a->tree)) {
    /* we need to call it_begin first instead of it_next to get a valid it pointer */
    value = it_begin(a->tree, &it);
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushvalue(L, 1);
    lua_pushnumber(L, value);
    lua_call(L, 1, 1);
    r = lua_istrue(L, -1);  /* 5.0.1 fix */
    agn_poptop(L);
    if (r) {
      if ( (rc = rb_remove(a->tree, value) ) == 0)
        luaL_error(L, "Error in " LUA_QS ": something went wrong.", "rbtree.purge");
      continue;
    }
    while (it_next(&it, &value)) {
      luaL_checkstack(L, 2, "not enough stack space");
      lua_pushvalue(L, 1);
      lua_pushnumber(L, value);
      lua_call(L, 1, 1);
      r = lua_istrue(L, -1);  /* 5.0.1 fix */
      agn_poptop(L);
      if (r) {
        if ( (rc = rb_remove(a->tree, value) ) == 0)
          luaL_error(L, "Error in " LUA_QS ": something went wrong.", "rbtree.purge");
        break;
      }
    }
  }
  lua_pushboolean(L, rc);
  return 1;
}


/* Returns the smallest value in the tree, in O(1) time. */
static int rbtree_min (lua_State *L) {
  Rbtree *a = checkrbtree(L, 1);
  if (rb_size(a->tree) != 0) {
    struct rbiter it;
    lua_pushnumber(L, it_begin(a->tree, &it));
  } else
    lua_pushfail(L);
  return 1;
}


/* Returns the largest value in the tree, in O(1) time. */
static int rbtree_max (lua_State *L) {
  Rbtree *a = checkrbtree(L, 1);
  if (rb_size(a->tree) != 0) {
    struct rbiter it;
    lua_pushnumber(L, it_end(a->tree, &it));
  } else
    lua_pushfail(L);
  return 1;
}


/* Returns both the smallest and the largest value in the tree, in O(1) time. */
static int rbtree_minmax (lua_State *L) {
  int rc;
  Rbtree *a = checkrbtree(L, 1);
  if ( (rc = (rb_size(a->tree) != 0)) ) {
    struct rbiter it;
    lua_pushnumber(L, it_begin(a->tree, &it));
    lua_pushnumber(L, it_end(a->tree, &it));
  } else
    lua_pushfail(L);
  return 1 + rc;
}


/* Returns an iterator function that when called returns one element after another from red-black tree a. If there are no more elements left, the iterator function returns `null`. Example usage:

> import rbtree;

> a := rbtree.new();

> for i from 3 downto 1 do
>    rbtree.include(a, ln(i))
> od;

> f := rbtree.iterate(a);

> while x := f() do
>    print(x)
> od;

> f():  # traversal complete, no more element left */
static int iterate (lua_State *L) {
  double value;
  Rbtree *a = lua_touserdata(L, lua_upvalueindex(1));
  int iter = lua_tointeger(L, lua_upvalueindex(2));
  if (rb_size(a->tree) == 0) {
    lua_pushnil(L);
    return 1;
  }
  if (iter == 0) {
   /* we need to call it_begin first instead of it_next to get a valid it pointer */
    value = it_begin(a->tree, &a->it);
    lua_pushnumber(L, value);
    lua_pushinteger(L, 1);
    lua_replace(L, lua_upvalueindex(2));
  } else {
    if (it_next(&a->it, &value)) {
      lua_pushnumber(L, value);
    } else {
      lua_pushnil(L);
    }
  }
  return 1;
}

static int rbtree_iterate (lua_State *L) {  /* 3.9.4 */
  (void)checkrbtree(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.18.4 fix */
  lua_pushvalue(L, 1);
  lua_pushinteger(L, 0);  /* iteration already started ? */
  lua_pushcclosure(L, &iterate, 2);
  return 1;
}

/* Metamethods ******************************************************************************************* */

static int mt_size (lua_State *L) {
  Rbtree *a = checkrbtree(L, 1);
  lua_pushnumber(L, rb_size(a->tree));
  return 1;
}

static int mt_gc (lua_State *L) {
  Rbtree *a;
  lua_lock(L);
  a = checkrbtree(L, 1);
  rb_delete(a->tree);
  luaL_unref(L, LUA_REGISTRYINDEX, a->registry);  /* delete registry table */
  lua_setmetatabletoobject(L, 1, NULL, 1);
  lua_unlock(L);
  return 0;
}

static int mt_in (lua_State *L) {
  Rbtree *a = checkrbtree(L, 2);
  lua_pushboolean(L, rb_find(a->tree, agn_checknumber(L, 1)));
  return 1;
}

static int mt_notin (lua_State *L) {
  Rbtree *a = checkrbtree(L, 2);
  lua_pushboolean(L, !rb_find(a->tree, agn_checknumber(L, 1)));
  return 1;
}

static int mt_empty (lua_State *L) {
  Rbtree *a = checkrbtree(L, 1);
  lua_pushboolean(L, rb_size(a->tree) == 0);
  return 1;
}

static int mt_filled (lua_State *L) {
  Rbtree *a = checkrbtree(L, 1);
  lua_pushboolean(L, rb_size(a->tree) != 0);
  return 1;
}

static int mt_tostring (lua_State *L) {  /* at the console, the rbtree is formatted as follows: */
  if (luaL_isudata(L, 1, "rbtree"))
    lua_pushfstring(L, "rbtree(%p)", lua_topointer(L, 1));
  else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}


static const struct luaL_Reg rb_treelib [] = {  /* metamethods for rbtrees `n' */
  {"entries",      rbtree_entries},
  {"find",         rbtree_find},
  {"include",      rbtree_include},
  {"max",          rbtree_max},
  {"min",          rbtree_min},
  {"minmax",       rbtree_minmax},
  {"remove",       rbtree_remove},
  {"__in",         mt_in},          /* `in` operator for rbtrees */
  {"__notin",      mt_notin},       /* `notin` operator for rbtrees */
  {"__size",       mt_size},        /* retrieve the number of entries in `n' */
  {"__empty",      mt_empty},       /* metamethod for `empty` operator */
  {"__filled",     mt_filled},      /* metamethod for `filled` operator */
  {"__tostring",   mt_tostring},    /* for output at the console, e.g. print(n) */
  {"__gc",         mt_gc},          /* please do not forget garbage collection */
  {NULL, NULL}
};

static const luaL_Reg rblib[] = {
  {"entries",      rbtree_entries},
  {"find",         rbtree_find},
  {"new",          rbtree_new},
  {"include",      rbtree_include},
  {"iterate",      rbtree_iterate},
  {"max",          rbtree_max},
  {"min",          rbtree_min},
  {"minmax",       rbtree_minmax},
  {"purge",        rbtree_purge},
  {"remove",       rbtree_remove},
  {NULL, NULL}
};


/*
** Open rbtree library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, "rbtree");  /* create metatable for rbtree */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, rb_treelib);  /* methods */
}

LUALIB_API int luaopen_rbtree (lua_State *L) {
  /* metamethods for rbtrees */
  createmeta(L);
  /* register library */
  luaL_register(L, AGENA_RBTREELIBNAME, rblib);
  return 1;
}

