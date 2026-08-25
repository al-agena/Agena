/* This library implements a First-in First-out Stack, implemented as a Circular queue.

   The underlying `cirque` implementation has been taken from Martin Broadhurst's exemplary website
   http://www.martinbroadhurst.com/cirque-in-c.html (defunct, unfortunately)

   For the licence of the Agena-specific code, see agena.h (MIT licence). */

#define fifo_c
#define LUA_LIB

#include <stdlib.h>

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"

#define checkfifo(L, n)     (Cirque *)luaL_checkudata(L, n, AGENA_FIFOLIBNAME)
#define isfifo(L,n)         (luaL_isudata(L, n, AGENA_FIFOLIBNAME))

#define INITSIZE 8

/* Circular queue in C, written by Martin Broadhurst, dated August 23, 2016. Unknown licence.

   Taken from the now defunct URL: http://www.martinbroadhurst.com/cirque-in-c.html

   Martin Broadhurst: "A circular queue, or ring buffer is an array that wraps around, so that it
   can be used as a queue without walking backwards in memory.

   This implementation reallocates the buffer when it becomes full (i.e., when the head and tail of
   the queue meet)." */

struct cirque {
  unsigned int head;  /* first element */
  unsigned int tail;  /* 1 past the last element */
  unsigned int is_full;
  int *entries;
  unsigned int size;
};

typedef struct cirque cirque;

static cirque *cirque_create (int initsize) {
  cirque *queue = malloc(sizeof(cirque));
  if (queue) {
    initsize = tools_adjustmultiple(initsize, 4);  /* 5.4.1 extension */
    queue->entries = malloc(initsize * sizeof(int));
    if (queue->entries) {
      queue->size = initsize;
      queue->head = 0;
      queue->tail = 0;
      queue->is_full = 0;
    } else {
      free(queue);
      queue = NULL;
    }
  }
  return queue;
}

static void cirque_delete (cirque *queue) {
  if (queue) {
    free(queue->entries);
    free(queue);
  }
}

static int cirque_auxresize (cirque *queue, unsigned int newsize, unsigned int newtail) {
  int rc, *temp;
  temp = malloc(newsize*sizeof(int));
  if ( (rc = (temp != NULL)) ) {
    unsigned int i = 0;
    if (newtail) {  /* 5.4.1 change */
      unsigned int h = queue->head;
      do {
        temp[i++] = queue->entries[h++];
        if (h == queue->size) h = 0;
      } while (h != queue->tail);
    } else {
      for (i=0; i < newsize; i++) queue->entries[i] = 0;
    }
    free(queue->entries);
    queue->entries = temp;
    queue->head = 0;
    queue->tail = newtail;  /* 5.4.1 change */
    queue->size = newsize;  /* 5.4.1 change */
    queue->is_full = 0;
  }
  return rc;  /* 1 on success, 0 on failure */
}

static void cirque_resize (cirque *queue) {
  /* newsize will always be greater than queue->size and will be a multiple of 4 */
  cirque_auxresize(queue, (unsigned int)agn_newsize(NULL, queue->size), queue->size);
}

static unsigned int cirque_is_empty (const cirque *queue) {
  return (queue->head == queue->tail) && !queue->is_full;
}

static unsigned int cirque_get_count (const cirque *queue) {
  unsigned int count;
  if (cirque_is_empty(queue)) {
    count = 0;
  } else if (queue->is_full) {
    count = queue->size;
  } else if (queue->tail > queue->head) {
    count = queue->tail - queue->head;
  } else {
    count = queue->size - queue->head;
    if (queue->tail > 0) {
      count += queue->tail - 1;
    }
  }
  return count;
}

static unsigned int cirque_insert (cirque *queue, int data) {
  unsigned int result;
  if (queue->is_full) {
    cirque_resize(queue);
    if (queue->is_full) {
      result = 0;
    }
  }
  if (!queue->is_full) {
    queue->entries[queue->tail++] = data;
    if (queue->tail == queue->size) {
      queue->tail = 0;
    }
    /* FIXME: all slots but one are occupied, but this marks the queue as being already full: */
    if (queue->tail == queue->head) {
      queue->is_full = 1;
    }
    result = 1;
  }
  return result;
}

static int cirque_remove (cirque *queue) {
  int data = LUA_NOREF;
  if (!cirque_is_empty(queue)) {
    if (queue->is_full) {
      queue->is_full = 0;
    }
    data = queue->entries[queue->head++];
    if (queue->head == queue->size) {
      queue->head = 0;
    } else if (queue->head > 0 && queue->head == queue->tail) {
      /* 5.4.1 extension to avoid wasting memory as the queue is empty now */
      queue->head = queue->tail = 0;
    }
  }
  return data;
}

static int cirque_peek (const cirque *queue) {
  int data = LUA_NOREF;
  if (!cirque_is_empty(queue)) {
    data = queue->entries[queue->head];
  }
  return data;
}

static int cirque_has (lua_State *L, const cirque *queue, int idx, int idxv) {
  int rc = 0;
  if (!cirque_is_empty(queue)) {
    unsigned int h = queue->head;
    do {
      lua_getfenvi(L, idx, queue->entries[h++]);
      if (lua_equal(L, -1, idxv)) rc = 1;
      agn_poptop(L);
      if (h == queue->size) h = 0;
    } while (!rc && (h != queue->tail));
  }
  return rc;
}

/* Agena Library Functions ******************************************************************************* */

typedef struct {
  cirque *queue;
  int registry;  /* registry, for attribute information, stored to index */
} Cirque;


/* The function creates an empty AVL tree and returns it. */
static int fifo_new (lua_State *L) {
  Cirque *c;
  int initsize = agnL_optposint(L, 1, INITSIZE);
  c = (Cirque *)lua_newuserdata(L, sizeof(Cirque));
  if (!c)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "fifo.new");
  lua_setmetatabletoobject(L, -1, AGENA_FIFOLIBNAME, 0);
  agn_setutypestring(L, -1, AGENA_FIFOLIBNAME);
  lua_createtable(L, initsize, 0);  /* 5.4.4 change, dont crowd the registry, use the separate environment table */
  lua_setfenv(L, -2);
  lua_createtable(L, 0, 0);
  c->registry = luaL_ref(L, LUA_REGISTRYINDEX);
  c->queue = cirque_create(initsize);
  return 1;
}


/* Inserts a new key~value pair into the queue. The function returns nothing. */
static int fifo_include (lua_State *L) {
  Cirque *c = checkfifo(L, 1);
  luaL_checkany(L, 2);
  agnL_checkudfreezed(L, 1, AGENA_FIFOLIBNAME, "fifo.include");
  lua_getfenv(L, 1);  /* 5.4.4 change, dont crowd the registry, use a separate table */
  lua_replace(L, 1);
  lua_settop(L, 2);
  /* store unique reference to value and pop value */
  cirque_insert(c->queue, luaL_ref(L, 1));
  return 0;
}


static int fifo_remove (lua_State *L) {
  int idx;
  Cirque *c = checkfifo(L, 1);
  agnL_checkudfreezed(L, 1, AGENA_FIFOLIBNAME, "fifo.remove");
  idx = cirque_remove(c->queue);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_getfenvi(L, 1, idx);
  lua_getfenv(L, 1);
  luaL_unref(L, -1, idx);
  agn_poptop(L);
  return 1;  /* return value just popped */
}


static int fifo_peek (lua_State *L) {
  Cirque *c = checkfifo(L, 1);
  lua_getfenvi(L, 1, cirque_peek(c->queue));
  return 1;
}


static int fifo_getstore (lua_State *L) {
  Cirque *c = checkfifo(L, 1);
  return agnL_calludata(L, lua_gettop(L), c->registry, AGENA_FIFOLIBNAME);
}


static int fifo_has (lua_State *L) {
  Cirque *c = checkfifo(L, 1);
  lua_pushboolean(L, cirque_has(L, c->queue, 1, 2));
  return 1;
}


static int fifo_shrink (lua_State *L) {  /* 5.4.1 */
  int rc;
  unsigned int curlen, maxsize;
  Cirque *c = checkfifo(L, 1);
  rc = 0;
  maxsize = c->queue->size;
  if (maxsize > INITSIZE) {
    curlen = cirque_get_count(c->queue);
    if (curlen <= tools_prevpow2(maxsize)) {
      unsigned int newsize = (curlen < INITSIZE) ? INITSIZE : tools_adjustmultiple(curlen, 4);
      rc = cirque_auxresize(c->queue, newsize, curlen);
    }
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushboolean(L, rc);
  lua_pushinteger(L, c->queue->size);
  return 2;
}


static int fifo_attrib (lua_State *L) {
  Cirque *c = checkfifo(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_createtable(L, 0, 6);
  lua_rawsetstringinteger(L, -1, "maxsize", c->queue->size);
  lua_rawsetstringinteger(L, -1, "length", cirque_get_count(c->queue));  /* 5.4.1 */
  lua_rawsetstringinteger(L, -1, "head", c->queue->head);
  lua_rawsetstringinteger(L, -1, "tail", c->queue->tail);
  lua_rawsetstringboolean(L, -1, "isfull", c->queue->is_full);
  lua_rawsetstringboolean(L, -1, "isempty", cirque_is_empty(c->queue));  /* 5.4.1 */
  return 1;
}

/* Metamthods ***************************************************************** */

static int mt_size (lua_State *L) {
  Cirque *c = checkfifo(L, 1);
  lua_pushnumber(L, cirque_get_count(c->queue));
  return 1;
}

static int mt_empty (lua_State *L) {
  Cirque *c = checkfifo(L, 1);
  lua_pushboolean(L, cirque_is_empty(c->queue));
  return 1;
}

static int mt_filled (lua_State *L) {
  Cirque *c = checkfifo(L, 1);
  lua_pushboolean(L, !cirque_is_empty(c->queue));
  return 1;
}

static int mt_tostring (lua_State *L) {  /* at the console, the queue is formatted as follows: */
  if (luaL_isudata(L, 1, AGENA_FIFOLIBNAME))
    lua_pushfstring(L, "fifo(%p)", lua_topointer(L, 1));
  else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}

static int mt_in (lua_State *L) {
  Cirque *c = checkfifo(L, 2);
  lua_pushboolean(L, cirque_has(L, c->queue, 2, 1));
  return 1;
}

static int mt_notin (lua_State *L) {
  Cirque *c = checkfifo(L, 2);
  lua_pushboolean(L, !cirque_has(L, c->queue, 2, 1));
  return 1;
}

static int mt_gc (lua_State *L) {
  Cirque *c;
  lua_lock(L);
  c = checkfifo(L, 1);
  cirque_delete(c->queue);
  lua_pushnil(L);  /* destroy environment table */
  lua_setfenv(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, c->registry);  /* delete registry table */
  lua_setmetatabletoobject(L, 1, NULL, 1);
  lua_unlock(L);
  return 0;
}


static const struct luaL_Reg fifo_fifolib [] = {  /* metamethods */
  {"__gc",         mt_gc},          /* please do not forget garbage collection */
  {"__in",         mt_in},          /* `in` operator */
  {"__notin",      mt_notin},       /* `notin` operator */
  {"__size",       mt_size},        /* retrieve the number of entries in `n' */
  {"__empty",      mt_empty},       /* metamethod for `empty` operator */
  {"__filled",     mt_filled},      /* metamethod for `filled` operator */
  {"__tostring",   mt_tostring},    /* for output at the console, e.g. print(n) */
  {"__call",       fifo_getstore},  /* to retrieve the internal store of a fifo queue */
  {"attrib",       fifo_attrib},
  {"getstore",     fifo_getstore},
  {"has",          fifo_has},
  {"include",      fifo_include},
  {"peek",         fifo_peek},
  {"remove",       fifo_remove},
  {"shrink",       fifo_shrink},
  {NULL, NULL}
};

static const luaL_Reg fifolib[] = {
  {"attrib",       fifo_attrib},
  {"getstore",     fifo_getstore},
  {"has",          fifo_has},
  {"include",      fifo_include},
  {"new",          fifo_new},
  {"peek",         fifo_peek},
  {"remove",       fifo_remove},
  {"shrink",       fifo_shrink},
  {NULL, NULL}
};


/*
** Open fifo library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_FIFOLIBNAME);  /* create metatable for file handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, fifo_fifolib);  /* methods */
}

LUALIB_API int luaopen_fifo (lua_State *L) {
  /* metamethods for fifo stacks */
  createmeta(L);
  /* register library */
  luaL_register(L, AGENA_FIFOLIBNAME, fifolib);
  return 1;
}