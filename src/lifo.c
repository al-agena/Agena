/* This library implements a Last-in First-out Stack, implemented as a Circular queue.

   The underlying `cirque` implementation has been taken from Martin Broadhurst's exemplary website
   http://www.martinbroadhurst.com/cirque-in-c.html (defunct, unfortunately)

   For the licence of the Agena-specific code, see agena.h (MIT licence). */

#define lifo_c
#define LUA_LIB

#include <stdlib.h>

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"

#define checklifo(L, n)     (Stack *)luaL_checkudata(L, n, AGENA_LIFOLIBNAME)
#define islifo(L,n)         (luaL_isudata(L, n, AGENA_LIFOLIBNAME))

/* The code is based on a Circular queue implementation written by Martin Broadhurst, dated August 23, 2016,
   adapted for lifos. Unknown licence.

   Taken from the now defunct URL: http://www.martinbroadhurst.com/cirque-in-c.html

   This implementation reallocates the buffer when it becomes full. */

struct stack {
  unsigned int top;  /* first free slot, counting from zero */
  unsigned int is_full;
  int *entries;
  unsigned int size;
};

typedef struct stack stack;

static stack *stack_create (int initsize) {
  stack *queue = malloc(sizeof(stack));
  if (queue) {
    initsize = tools_adjustmultiple(initsize, 4);
    queue->entries = malloc(initsize * sizeof(int));
    if (queue->entries) {
      queue->size = initsize;
      queue->top = 0;
      queue->is_full = 0;
    } else {
      free(queue);
      queue = NULL;
    }
  }
  return queue;
}

static void stack_delete (stack *queue) {
  if (queue) {
    free(queue->entries);
    free(queue);
  }
}

static void stack_resize (stack *queue) {
  /* newsize will always greater than queue->size and will be a multiple of 4 */
  int newsize = agn_newsize(NULL, queue->size);
  int *temp = realloc(queue->entries, newsize * sizeof(int));
  if (!temp) return; /* 7.7.7 K&R fix */
  queue->entries = temp;
  if (queue->entries) {
    queue->top = queue->size;
    queue->size = newsize;
    queue->is_full = 0;
  }
}

static unsigned int stack_is_empty (const stack *queue) {
  return queue->top == 0;
}

static unsigned int stack_insert (stack *queue, int data) {
  unsigned int result;
  if (queue->is_full) {
    stack_resize(queue);
    if (queue->is_full) {
      result = 0;
    }
  }
  if (!queue->is_full) {
    queue->entries[queue->top++] = data;
    if (queue->top == queue->size) {
      queue->is_full = 1;
    }
    result = 1;
  }
  return result;
}

static int stack_remove (stack *queue) {
  int data = LUA_NOREF;
  if (!stack_is_empty(queue)) {
    if (queue->is_full) {
      queue->is_full = 0;
    }
    data = queue->entries[--queue->top];
    queue->entries[queue->top] = LUA_NOREF;
  }
  return data;
}

static int stack_peek (const stack *queue) {
  return stack_is_empty(queue) ? LUA_NOREF : queue->entries[queue->top - 1];
}

static unsigned int stack_get_count (const stack *queue) {
  return stack_is_empty(queue) ? 0 : queue->top;
}

static int stack_has (lua_State *L, const stack *queue, int idx, int idxv) {
  int rc = 0;
  if (!stack_is_empty(queue)) {
    unsigned int h = 0;
    do {
      lua_getfenvi(L, idx, queue->entries[h++]);
      if (lua_equal(L, -1, idxv)) rc = 1;
      agn_poptop(L);
    } while (!rc && h < queue->top);
  }
  return rc;
}

/* Agena Library Functions ******************************************************************************* */

typedef struct {
  stack *queue;
  int registry;  /* registry, for attribute information, stored to index */
} Stack;

#define INITSIZE 8
/* The function creates an empty lifo queue and returns it. */
static int lifo_new (lua_State *L) {
  Stack *c;
  int initsize = agnL_optposint(L, 1, INITSIZE);
  c = (Stack *)lua_newuserdata(L, sizeof(Stack));
  if (!c)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "lifo.new");
  lua_setmetatabletoobject(L, -1, AGENA_LIFOLIBNAME, 0);
  agn_setutypestring(L, -1, AGENA_LIFOLIBNAME);
  lua_createtable(L, initsize, 0);  /* 5.4.4 change, dont crowd the registry, use the separate environment table */
  lua_setfenv(L, -2);
  lua_createtable(L, 0, 0);
  c->registry = luaL_ref(L, LUA_REGISTRYINDEX);
  c->queue = stack_create(initsize);
  return 1;
}


/* Inserts a new key~value pair into the queue. The function returns nothing. */
static int lifo_include (lua_State *L) {
  Stack *c = checklifo(L, 1);
  luaL_checkany(L, 2);
  agnL_checkudfreezed(L, 1, AGENA_LIFOLIBNAME, "lifo.include");
  lua_getfenv(L, 1);  /* 5.4.4 change, dont crowd the registry, use a separate table */
  lua_replace(L, 1);
  lua_settop(L, 2);
  /* store unique reference to value and pop value */
  stack_insert(c->queue, luaL_ref(L, 1));
  return 0;
}


static int lifo_remove (lua_State *L) {
  int idx;
  Stack *c = checklifo(L, 1);
  agnL_checkudfreezed(L, 1, AGENA_LIFOLIBNAME, "lifo.remove");
  idx = stack_remove(c->queue);  /* get the index number of bottom element and remove it from queue */
  luaL_checkstack(L, 2, "not enough stack space");
  lua_getfenvi(L, 1, idx);  /* push value to be removed onto the stack */
  lua_getfenv(L, 1);  /* get internal values table */
  luaL_unref(L, -1, idx);  /* remove key~value pair from internal values table */
  agn_poptop(L);  /* pop internal values table */
  return 1;  /* return value just popped */
}


static int lifo_peek (lua_State *L) {
  Stack *c = checklifo(L, 1);
  lua_getfenvi(L, 1, stack_peek(c->queue));
  return 1;
}


static int lifo_getstore (lua_State *L) {
  Stack *c = checklifo(L, 1);
  return agnL_calludata(L, lua_gettop(L), c->registry, AGENA_LIFOLIBNAME);
}


static int lifo_has (lua_State *L) {
  Stack *c = checklifo(L, 1);
  lua_pushboolean(L, stack_has(L, c->queue, 1, 2));
  return 1;
}


static int lifo_attrib (lua_State *L) {
  Stack *c = checklifo(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_createtable(L, 0, 4);
  lua_rawsetstringinteger(L, -1, "maxsize", c->queue->size);
  lua_rawsetstringinteger(L, -1, "top", c->queue->top);
  lua_rawsetstringboolean(L, -1, "isempty", c->queue->top == 0);
  lua_rawsetstringboolean(L, -1, "isfull", c->queue->is_full);
  return 1;
}


static int lifo_shrink (lua_State *L) {
  Stack *c = checklifo(L, 1);
  int possize = c->queue->top == 0 ? INITSIZE : tools_adjustmultiple(c->queue->top, 4);
  int rc = 0;
  if (c->queue->size > INITSIZE && possize < c->queue->size) {
    int *temp = realloc(c->queue->entries, possize*sizeof(int));
    if (temp == NULL) {
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "lifo.shrink");
    }
    rc = 1;
    c->queue->entries = temp;
    c->queue->size = possize;
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushboolean(L, rc);
  lua_pushinteger(L, c->queue->size);
  return 2;
}


/* Metamthods ***************************************************************** */

static int mt_size (lua_State *L) {
  Stack *c = checklifo(L, 1);
  lua_pushnumber(L, stack_get_count(c->queue));
  return 1;
}

static int mt_empty (lua_State *L) {
  Stack *c = checklifo(L, 1);
  lua_pushboolean(L, stack_is_empty(c->queue));
  return 1;
}

static int mt_filled (lua_State *L) {
  Stack *c = checklifo(L, 1);
  lua_pushboolean(L, !stack_is_empty(c->queue));
  return 1;
}

static int mt_tostring (lua_State *L) {  /* at the console, the queue is formatted as follows: */
  if (luaL_isudata(L, 1, AGENA_LIFOLIBNAME))
    lua_pushfstring(L, "lifo(%p)", lua_topointer(L, 1));
  else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}

static int mt_in (lua_State *L) {
  Stack *c = checklifo(L, 2);
  lua_pushboolean(L, stack_has(L, c->queue, 2, 1));
  return 1;
}

static int mt_notin (lua_State *L) {
  Stack *c = checklifo(L, 2);
  lua_pushboolean(L, !stack_has(L, c->queue, 2, 1));
  return 1;
}

static int mt_gc (lua_State *L) {
  Stack *c;
  lua_lock(L);
  c = checklifo(L, 1);
  stack_delete(c->queue);
  lua_pushnil(L);  /* destroy environment table */
  lua_setfenv(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, c->registry);  /* delete registry table */
  lua_setmetatabletoobject(L, 1, NULL, 1);
  lua_unlock(L);
  return 0;
}


static const struct luaL_Reg lifo_lifolib [] = {  /* metamethods */
  {"attrib",       lifo_attrib},
  {"getstore",     lifo_getstore},
  {"has",          lifo_has},
  {"include",      lifo_include},
  {"peek",         lifo_peek},
  {"remove",       lifo_remove},
  {"shrink",       lifo_shrink},
  {"__gc",         mt_gc},          /* please do not forget garbage collection */
  {"__in",         mt_in},          /* `in` operator */
  {"__notin",      mt_notin},       /* `notin` operator */
  {"__size",       mt_size},        /* retrieve the number of entries in `n' */
  {"__empty",      mt_empty},       /* metamethod for `empty` operator */
  {"__filled",     mt_filled},      /* metamethod for `filled` operator */
  {"__tostring",   mt_tostring},    /* for output at the console, e.g. print(n) */
  {"__call",       lifo_getstore},  /* to retrieve the internal store of a lifo queue */
  {NULL, NULL}
};

static const luaL_Reg lifolib[] = {
  {"attrib",       lifo_attrib},
  {"getstore",     lifo_getstore},
  {"has",          lifo_has},
  {"include",      lifo_include},
  {"new",          lifo_new},
  {"peek",         lifo_peek},
  {"remove",       lifo_remove},
  {"shrink",       lifo_shrink},
  {NULL, NULL}
};


/*
** Open lifo library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_LIFOLIBNAME);  /* create metatable for file handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, lifo_lifolib);  /* methods */
}

LUALIB_API int luaopen_lifo (lua_State *L) {
  /* metamethods for lifo stacks */
  createmeta(L);
  /* register library */
  luaL_register(L, AGENA_LIFOLIBNAME, lifolib);
  return 1;
}