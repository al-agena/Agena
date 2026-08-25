/* This library implements a single-linked list through userdata.

   Attempts to directly store references to Lua TValues and barrier them with various methods to protect them
   from the garbage collector failed. The elegant luaL_ref mechanism in Lua 5.1, however, is very fast, we use
   it to store the llist values into the Lua registry.

   The underlying singly-linked list implementation has been taken from Martin Broadhurst's exemplary website
   http://www.martinbroadhurst.com/data-structures.html */

#define llist_c
#define LUA_LIB

#include <stdlib.h>

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"

#define checkllist(L, n)      (Slist *)luaL_checkudata(L, n, "llist")
#define checkllistnode(L, n)  (Slnode *)luaL_checkudata(L, n, "llistnode")

#define aux_islorulist(L,idx)  (luaL_isudata(L, idx, "llist") || (agn_isutype(L, idx, "ulist") && luaL_isudata(L, idx, "ulist")))


typedef struct Slnode {
  int data;  /* unique reference created by luaL_ref into the registry storing the Lua value */
  struct Slnode *next;
} Slnode;


/* If you CHANGE this structure, keep in mind initialisations in llist_gc, llist_list and llist_replicate ! */
typedef struct {
  Slnode *head;
  Slnode *tail;
  int registry;           /* registry, for attribute information, stored to index 0; 2.15.1 */
  size_t size;            /* number of nodes */
  size_t nodesize;        /* ulists only: number of elements per sequence */
  lua_Number fillfactor;  /* ulists only: percentage of elements stored per sequence, 0 < fillfactor < 1 */
} Slist;

/*          {                 { data :: int/ref to registry table
            {  head :: Slnode { next :: Slnode
            {
   Slist := {  tail :: Slnode { data :: int/ref to registry table
            {                 { next :: Slnode
            {
            {     (tail is used just to easily append nodes without having to traverse the entire list before)
            {
            {  size :: size_t, nodesize :: size_t, fillfactor :: lua_Number, see above
            {  registry :: int ref to the list's registry table  */

static Slnode *sl_createnode (lua_State *L, int udx, int idx) {
  Slnode *node = malloc(sizeof(Slnode));
  if (node) {  /* put value into the registry _table_ */
    luaL_checkstack(L, 2, "not enough stack space");
    lua_getfenv(L, udx);
    lua_pushvalue(L, idx > 0 ? idx : idx - 1);  /* value to be put into registry table */
    node->data = luaL_ref(L, -2);  /* store unique reference to value and pop value */
    node->next = NULL;
    agn_poptop(L);
  }
  return node;
}

static void sl_initlist (Slist *list, Slnode *node) {  /* 2.15.2 */
  list->head = node;
  list->tail = node;
  list->size = 0;
  list->registry = 0;
}

static void sl_append (Slist *list, Slnode *node) {
  if (list->head == NULL) {   /* list is still empty, assign element as the very first node */
    list->head = node;
    list->tail = node;
  } else {  /* do not change header node; the next statement actually updates the pointer address to
    the new node to be inserted in the previous Slnode */
    list->tail->next = node;  /* first assign node to tail ... */
    list->tail = node;        /* ... then update last entry pointer */
  }
  list->size++;
}

static void sl_prepend (Slist *list, Slnode *node) {
  if (list->tail == NULL) {   /* list is empty yet, add the first node */
    list->head = node;
    list->tail = node;
  } else {
    node->next = list->head;  /* put former head node into second position */
    list->head = node;        /* assign new node to head */
  }
  list->size++;
}

static void sl_removehead (Slist *list) {
  if (list->head) {
    Slnode *temp = list->head;
    list->head = list->head->next;
    if (list->head == NULL) {  /* list is now empty */
      list->tail = NULL;
    }
    xfree(temp);
    list->size--;
    if (list->size == 1) {
      list->tail = list->head;
    }
  }
}

static void sl_remove (Slist *list, Slnode *node, Slnode *previous) {
  if (node == list->head) {
    sl_removehead(list);
  } else {
    previous->next = node->next;
    list->size--;
    if (list->size == 1) {
      list->tail = list->head;
    } else if (node == list->tail) {
      list->tail = previous;
    }
    xfree(node);
  }
}

static int llist_iterate (lua_State *L);

/* the list must be on the stack top */
static void aux_additerator (lua_State *L, int (*f)(lua_State *)) {
  luaL_checkstack(L, 5, "not enough stack space");
  lua_getmetatable(L, -1);
  lua_pushcfunction(L, f);     /* push iterator */
  lua_pushvalue(L, -3);        /* push list */
  lua_pushinteger(L, 1);       /* starting position */
  lua_pushtrue(L);             /* retrieve both index and value */
  lua_call(L, 3, 1);           /* create iterator */
  lua_setfield(L, -2, "__iter");  /* assign to metatable */
  agn_poptop(L);  /* pop metatable and leave list on the stack top, to returned */
}

static int llist_new (lua_State *L) {  /* create a new userdata slist  */
  size_t i, nargs;
  Slist *list;
  Slnode *node;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  list = (Slist *)lua_newuserdata(L, sizeof(Slist));
  lua_createtable(L, 0, 0);  /* 5.4.4 change, dont crowd the registry, use the separate environment table */
  lua_setfenv(L, -2);  /* pops the table from the stack */
  if (list)
    sl_initlist(list, NULL);  /* 2.15.2 */
  else
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "llist.new");
  lua_setmetatabletoobject(L, -1, "llist", 1);  /* assign the metatable defined below to the userdata */
  for (i=1; i <= nargs; i++) {
    lua_lock(L);
    node = sl_createnode(L, -1, i);
    if (node == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "llist.new");
    sl_append(list, node);
    lua_unlock(L);
  }
  lua_createtable(L, 0, 0);
  list->registry = luaL_ref(L, LUA_REGISTRYINDEX);  /* store unique ref to registry table and pop table */
  aux_additerator(L, &llist_iterate);
  return 1;
}


static int llist_append (lua_State *L) {
  Slist *list;
  Slnode *node;
  size_t nargs, i;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (lua_gettop(L) < 2)
    luaL_error(L, "Error in " LUA_QS ": need at least an llist and a value.", "llist.append");
  list = (Slist *)checkllist(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "llist.append");  /* 5.2.0 extension */
  for (i=2; i <= nargs; i++) {
    lua_lock(L);
    node = sl_createnode(L, 1, i);
    if (node == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "llist.append");
    sl_append(list, node);
    lua_unlock(L);
  }
  return 0;
}


static int llist_prepend (lua_State *L) {
  Slist *list;
  Slnode *node;
  size_t i, nargs;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (lua_gettop(L) < 2)
    luaL_error(L, "Error in " LUA_QS ": need at least an llist and a value.", "llist.prepend");
  list = (Slist *)checkllist(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "llist.prepend");  /* 5.2.0 extension */
  for (i=2; i <= nargs; i++) {
    lua_lock(L);
    node = sl_createnode(L, 1, i);
    if (node == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "llist.prepend");
    sl_prepend(list, node);
    lua_unlock(L);
  }
  return 0;
}


static int llist_put (lua_State *L) {
  Slist *list;
  Slnode *newnode;
  int idx;
  if (lua_gettop(L) != 3)
    luaL_error(L, "Error in " LUA_QS ": need an llist, an index, and any value.", "llist.put");
  lua_lock(L);
  if (!aux_islorulist(L, 1))
    luaL_error(L, "Error in " LUA_QS ": expected an llist or ulist, got %s.", "l/ulist.put", luaL_typename(L, 1));
  list = (Slist *)lua_touserdata(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "llist.put");  /* 5.2.0 extension */
  idx = tools_posrelat(agn_checkinteger(L, 2), list->size);  /* 2.15.1 */
  if (idx < 1 || idx > list->size + 1)
    luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "llist.put", idx);
  newnode = sl_createnode(L, 1, 3);  /* new node currently is at index 3 on the stack */
  if (newnode == NULL)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "llist.put");
  if (list->size + 1 == idx) {  /* append to end of list */
    sl_append(list, newnode);  /* takes care of shifting middle element itself */
  } else {  /* insert new node into list, shifting existing ones into open space */
    size_t i;
    Slnode *node, *previous;
    previous = NULL;
    node = list->head;
    for (i=1; i < idx; i++) {  /* move to given list position */
      previous = node;
      node = node->next;
    }
    if (node == list->head) {    /* new node shall be added to first position */
      sl_prepend(list, newnode); /* 2.15.2 */
    } else {
      newnode->next = node;      /* move existing node `up` one position */
      previous->next = newnode;  /* add address to new node to previous node */
      list->size++;
    }
  }
  return 0;
}


static int llist_purge (lua_State *L) {
  Slist *list;
  Slnode *previous, *node;
  int idx;
  size_t i;
  if (lua_gettop(L) == 0)
    luaL_error(L, "Error in " LUA_QS ": need an llist and optionally an index.", "llist.purge");
  lua_lock(L);
  if (!aux_islorulist(L, 1))
    luaL_error(L, "Error in " LUA_QS ": expected an llist or ulist, got %s.", "l/ulist.purge", luaL_typename(L, 1));
  list = (Slist *)lua_touserdata(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "llist.purge");  /* 5.2.0 extension */
  idx = tools_posrelat(agnL_optinteger(L, 2, list->size), list->size);  /* 2.15.1, 2.15.4 change */
  if (idx < 1 || idx > list->size)
    luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "llist.purge", idx);
  previous = NULL;
  node = list->head;
  for (i=1; i < idx; i++) {  /* get previous and the current node at user-given position */
    previous = node;
    node = node->next;
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_getfenvi(L, 1, node->data);  /* 2.15.4 */
  lua_getfenv(L, 1);
  luaL_unref(L, -1, node->data);
  agn_poptop(L);
  /* if node == NULL, then we will remove the entire head */
  sl_remove(list, node, previous);
  return 1;
}


/* Creates a new userdata slist node, assigns a metatable to it, and leaves the new userdata node on the top of the stack. */
static int llist_node (lua_State *L, Slnode *node, int meta) {
  Slnode **newnode;
  if (node == NULL) {
    lua_pushnil(L);
  }
  else {
    lua_lock(L);
    /* see answer on nabble.com by Michal Kottman, Aug 27, 2011; 8:03 pm on `Re: Struggling with userdata concept`
       at http://lua.2524044.n2.nabble.com/Struggling-with-userdata-concepts-td6732223.html */
    newnode = (Slnode **)lua_newuserdata(L, sizeof(Slnode *));
    *newnode = node; /* store the pointer inside the userdata */
    if (*newnode) {
      (*newnode)->data = node->data;
      (*newnode)->next = node->next;
    } else
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "llist.iterate");
    lua_pushvalue(L, meta);  /* push metatable */
    if (!lua_istable(L, -1)) {
      agn_poptop(L);  /* pop newuserdata */
      luaL_error(L, "Error in " LUA_QS ": userdata metatable does not exist.", "llist.iterate");
    }
    agn_setudmetatable(L, -2);
    lua_unlock(L);
  }
  return 1;  /* pushed node lightuserdata on stack */
}


static int iterate (lua_State *L) {
  Slnode *node;
  void *upval;
  int i, flag, offset, pushidx, lastone;
  flag = 0; lastone = 0;
  upval = lua_touserdata(L, lua_upvalueindex(2));  /* leave this here, otherwise Agena would crash */
  if (upval == NULL) {
    lua_pushnil(L);
    return 1;
  }
  offset = lua_tointeger(L, 1);
  pushidx = lua_tointeger(L, lua_upvalueindex(6));
  if (agn_istrue(L, lua_upvalueindex(3))) {  /* we got the very first traversal, but the list might still be empty ... */
    int pos;
    Slist *list = (Slist *)upval;
    if (list->head == NULL) {  /* ... list is empty ?  Do not change Boolean first position flag */
      lua_pushnil(L);
      return 1;
    }
    llist_node(L, list->head, lua_upvalueindex(1));  /* create a userdata node */
    lua_replace(L, lua_upvalueindex(2));             /* make it an upvalue */
    lua_pushfalse(L);                                /* flag so that this `if' branch is never entered again */
    lua_replace(L, lua_upvalueindex(3));
    node = *(Slnode **)lua_touserdata(L, lua_upvalueindex(2));  /* get freshly created userdata node */
    pos = lua_tointeger(L, lua_upvalueindex(4));
    for (i=1; i < pos + offset && node != NULL; i++) {
      node = node->next;
      flag = 1;
    }
  } else {
    node = *(Slnode **)upval;
    for (i=0; i < offset + 1 && node != NULL; i++) {
      node = node->next;
    }
    flag = 1;
  }
  if (pushidx) {
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushinteger(L, pushidx++);
    lua_pushinteger(L, pushidx);
    lua_replace(L, lua_upvalueindex(6));
  }
  if (node == NULL) {
    lua_pushnil(L);
    if (pushidx) {
      lua_pushvalue(L, lua_upvalueindex(4));  /* put 4 start position */
      lua_replace(L, lua_upvalueindex(6));    /* into 6 additionally push index flag */
      upval = lua_touserdata(L, lua_upvalueindex(2));
      lua_pushtrue(L);  /* mark as ready-for-first traversal */
      lua_replace(L, lua_upvalueindex(3));    /* into slot 3 */
      pushidx = 0;
      lastone = 1;
    }
  } else {
    lua_getfenvi(L, lua_upvalueindex(5), node->data);
  }
  if (flag) {  /* do not create a node for list->head again */
    if (!lastone)
      llist_node(L, node, lua_upvalueindex(1));
    else
      lua_pushvalue(L, lua_upvalueindex(5));  /* put 5 llist backup */
    lua_replace(L, lua_upvalueindex(2));  /* update node userdata upvalue */
  }
  return 1 + (pushidx != 0);
}

static int llist_iterate (lua_State *L) {
  int pos, pushidx;
  if (!aux_islorulist(L, 1))  /* 2.15.4 extension */
    luaL_error(L, "Error in " LUA_QS ": expected an llist or ulist, got %s.", "l/ulist.iterate", luaL_typename(L, 1));
  pos = luaL_optint(L, 2, 1);
  if (pos < 1)
    luaL_error(L, "Error in " LUA_QS ": start index must be positive.", "l/ulist.iterate");
  pushidx = agnL_optboolean(L, 3, 0);  /* 7.1.1 extension */
  luaL_checkstack(L, 6, "not enough stack space");  /* 3.18.4 fix */
  luaL_getmetatable(L, "llistnode");  /* 1 metatable */
  if (!lua_istable(L, -1)) {
    agn_poptop(L);  /* pop anything */
    luaL_error(L, "Error in " LUA_QS ": userdata metatable does not exist.", "l/ulist.iterate");
  }
  lua_pushvalue(L, 1);      /* 2 llist */
  lua_pushtrue(L);          /* 3 first-read flag */
  lua_pushinteger(L, pos);  /* 4 start position */
  lua_pushvalue(L, 1);      /* 5 llist, for the values in the environment table of the list, as pos #2 will be overwritten later */
  lua_pushinteger(L, pushidx ? pos : 0);  /* 6 additionally push index flag */
  lua_pushcclosure(L, &iterate, 6);
  return 1;
}


static int llist_replicate (lua_State *L) {
  Slist *source, *target;
  Slnode *node, *newnode;
  source = checkllist(L, 1);
  target = (Slist *)lua_newuserdata(L, sizeof(Slist));
  lua_createtable(L, source->size, 0);
  lua_setfenv(L, -2);  /* pops the table from the stack */
  if (target)
    sl_initlist(target, NULL);  /* 2.15.2 */
  else
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "llist.replicate");
  lua_setmetatabletoobject(L, -1, "llist", 1);  /* assign the metatable defined below to the userdata */
  for (node = source->head; node != NULL; node = node->next) {
    lua_getfenvi(L, 1, node->data);
    lua_lock(L);
    newnode = sl_createnode(L, -2, -1);
    if (newnode == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "llist.replicate");
    sl_append(target, newnode);
    agn_poptop(L);
    lua_unlock(L);
  }
  lua_createtable(L, 0, 0);
  target->registry = luaL_ref(L, LUA_REGISTRYINDEX);  /* store unique ref to registry table and pop table */
  return 1;
}


static int llist_getsize (lua_State *L) {  /* return the number of allocated slots in the userdata llist */
  Slist *list = checkllist(L, 1);
  lua_pushnumber(L, list->size);
  return 1;
}


static int llist_empty (lua_State *L) {  /* 2.15.4 */
  Slist *list = checkllist(L, 1);
  lua_pushboolean(L, list->size == 0);
  return 1;
}


static int llist_filled (lua_State *L) {  /* 2.15.4 */
  Slist *list = checkllist(L, 1);
  lua_pushboolean(L, list->size != 0);
  return 1;
}


/* returns succeeding values from a singly-linked list. It is at least twice as fast as individually reading the items. 2.15.2 */
static int llist_getitem (lua_State *L) {
  size_t i, idx, n;
  Slnode *node;
  int nargs = lua_gettop(L);
  Slist *list = checkllist(L, 1);
  if (nargs == 2 && !agn_isinteger(L, 2)) {  /* OOP method, 5.5.14 */
    return agn_initmethodcall(L, "llist", 5);
  }
  idx = tools_posrelat(agn_checkinteger(L, 2), list->size);  /* 2.15.1 */
  n = agnL_optposint(L, 3, 1);
  luaL_checkstack(L, n, "too many values");  /* 2.31.7 fix */
  if (idx < 1 || idx + n - 1 > list->size) {
    if (idx == 0)
      lua_rawgeti(L, LUA_REGISTRYINDEX, list->registry);
    else
      lua_pushnil(L);
    return 1;
  }
  node = list->head;
  for (i=1; i < idx; i++) {
    node = node->next;
  }
  for (i=0; i < n; i++) {
    lua_getfenvi(L, 1, node->data);
    node = node->next;
  }
  return n;
}


static int llist_getregistry (lua_State *L) {  /* return llist registry table, i.e. a[0] */
  Slist *list = checkllist(L, 1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, list->registry);
  return 1;
}


static int llist_in (lua_State *L) {
  Slist *list;
  Slnode *node, *tail;
  int r, nargs;
  nargs = lua_gettop(L);
  if (nargs != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "llist.__in");
  list = checkllist(L, 2);
  if ((tail = list->tail) != NULL) {  /* at first check last element */
    lua_getfenvi(L, 2, tail->data);
    if (lua_equal(L, 1, -1)) {
      agn_poptop(L);
      lua_pushtrue(L);
      return 1;
    }
    agn_poptop(L);
  }
  r = 0;
  for (node = list->head; node != NULL; node = node->next) {
    lua_getfenvi(L, 2, node->data);
    if ((r = lua_equal(L, 1, -1))) {
      agn_poptop(L);
      break;
    }
    agn_poptop(L);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int llist_notin (lua_State *L) {  /* 2.18.0 */
  Slist *list;
  Slnode *node, *tail;
  int r, nargs;
  nargs = lua_gettop(L);
  if (nargs != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "llist.__notin");
  list = checkllist(L, 2);
  if ((tail = list->tail) != NULL) {  /* at first check last element */
    lua_getfenvi(L, 2, tail->data);
    if (lua_equal(L, 1, -1)) {
      agn_poptop(L);
      lua_pushfalse(L);
      return 1;
    }
    agn_poptop(L);
  }
  r = 0;
  for (node = list->head; node != NULL; node = node->next) {
    lua_getfenvi(L, 2, node->data);
    if ((r = lua_equal(L, 1, -1))) {
      agn_poptop(L);
      break;
    }
    agn_poptop(L);
  }
  lua_pushboolean(L, !r);
  return 1;
}


static int llist_eq (lua_State *L) {
  Slist *list1, *list2;
  Slnode *node1, *node2;
  int r, nargs;
  nargs = lua_gettop(L);
  r = 1;
  if (nargs != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "llist.__eq");
  list1 = checkllist(L, 1);
  list2 = checkllist(L, 2);
  if (list1->size != list2->size)
    r = 0;
  else {
    for (node1 = list1->head, node2 = list2->head; node1 != NULL && node2 != NULL;
         node1 = node1->next, node2 = node2->next) {
      lua_getfenvi(L, 1, node1->data);
      lua_getfenvi(L, 2, node2->data);
      if ((r = lua_rawequal(L, -1, -2)) == 0) {
        agn_poptoptwo(L);
        break;
      }
      agn_poptoptwo(L);
    }
  }
  lua_pushboolean(L, r);
  return 1;
}


static int llist_aeq (lua_State *L) {
  Slist *list1, *list2;
  Slnode *node1, *node2;
  int r, nargs;
  nargs = lua_gettop(L);
  r = 1;
  if (nargs != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "llist.__aeq");
  list1 = checkllist(L, 1);
  list2 = checkllist(L, 2);
  if (list1->size != list2->size)
    r = 0;
  else {
    for (node1 = list1->head, node2 = list2->head; node1 != NULL && node2 != NULL;
         node1 = node1->next, node2 = node2->next) {
      lua_getfenvi(L, 1, node1->data);
      lua_getfenvi(L, 2, node2->data);
      if ((r = lua_rawaequal(L, -1, -2)) == 0) {
        agn_poptoptwo(L);
        break;
      }
      agn_poptoptwo(L);
    }
  }
  lua_pushboolean(L, r);
  return 1;
}


static int llist_setitem (lua_State *L) {  /* set a value at the given index position, overwriting existing one */
  size_t i, idx;
  Slnode *node;
  Slist *list;
  if (lua_gettop(L) != 3)
    luaL_error(L, "Error in " LUA_QS ": need an llist, an index, and a value.", "llist.setitem");
  list = checkllist(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "llist.setitem");  /* 5.2.0 extension */
  idx = tools_posrelat(agn_checkinteger(L, 2), list->size);  /* 2.15.1 */
  if (idx < 1 || idx > list->size + 1) return 0;  /* 2.15.1 fix */
  if (lua_isnil(L, 3)) {  /* 5.4.5 patch, otherwise any value would be deleted randomly */
    lua_settop(L, 2);
    return llist_purge(L);
  }
  if (list->size + 1 == idx) {
    node = sl_createnode(L, 1, 3);
    if (node == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "llist.setitem");
    sl_append(list, node);
  } else {  /* replace existing item */
    node = list->head;
    for (i=1; i < idx; i++) {  /* proceed to given position */
      node = node->next;
    }
    lua_getfenv(L, 1);
    luaL_unref(L, -1, node->data);  /* delete reference to former value */
    lua_pushvalue(L, 3);  /* enter substitute into the registry */
    node->data = luaL_ref(L, -2);
    agn_poptop(L);
  }
  return 0;
}


static int llist_tostring (lua_State *L) {  /* at the console, the llist is formatted as follows: */
  if (luaL_isudata(L, 1, "llist"))  /* 2.15.1 change */
    lua_pushfstring(L, "llist(%p)", lua_topointer(L, 1));
  else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}


static int llist_totable (lua_State *L) {  /* 2.15.4 */
  Slist *list;
  Slnode *node;
  size_t c;
  list = checkllist(L, 1);
  lua_createtable(L, list->size, 0);
  c = 0;
  for (node = list->head; node != NULL; node = node->next) {
    lua_getfenvi(L, 1, node->data);
    lua_rawseti(L, -2, ++c);
  }
  return 1;
}


static int llist_toseq (lua_State *L) {  /* 2.15.4 */
  Slist *list;
  Slnode *node;
  size_t c;
  list = checkllist(L, 1);
  agn_createseq(L, list->size);
  c = 0;
  for (node = list->head; node != NULL; node = node->next) {
    lua_getfenvi(L, 1, node->data);
    if (lua_isnil(L, -1)) { agn_poptop(L); continue; }
    lua_seqrawseti(L, -2, ++c);
  }
  return 1;
}


static int llist_dump (lua_State *L) {  /* 2.15.4, based on ulist_dump */
  Slist *list;
  Slnode *node, *temp;
  size_t c;
  lua_lock(L);
  list = checkllist(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "llist.dump");  /* 5.2.0 extension */
  luaL_checkstack(L, 2, "not enough stack space");
  agn_createseq(L, list->size);
  c = 0;
  node = list->head;
  while (node) {
    temp = node->next;
    lua_getfenvi(L, 1, node->data);
    lua_seqrawseti(L, -2, ++c);
    xfree(node);
    node = temp;
  }
  /* we will now reset the list so it can be reused, 5.4.5 */
  sl_initlist(list, NULL);
  /* we cannot simply drop the environment table and register a new one, as Agena would crash.
     So empty it. */
  lua_getfenv(L, 1);
  agn_cleanse(L, -1, 1);
  agn_poptop(L);
  lua_unlock(L);
  return 1;  /* return the entire previous contents in a sequence */
}


static void aux_gc (lua_State *L, const char *what) {  /* garbage collect deleted userdata */
  Slist *list;
  Slnode *node, *temp;
  int result;
  (void)L;
  lua_lock(L);
  list = luaL_getudata(L, 1, what, &result);  /* 2.11.3, since we have two metatables, we use this alternative
    checker to avoid crashes when the `restart` statements tries to gc them. Changed 2.14.6. */
  if (list == NULL) return;  /* 2.11.3 */
  lua_pushnil(L);
  lua_setfenv(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, list->registry);  /* delete registry table */
  lua_setmetatabletoobject(L, 1, NULL, 1);
  node = list->head;
  while (node != NULL) {
    temp = node->next;
    xfree(node);
    node = temp;
  }
  lua_unlock(L);
  return;
}

static int llist_gc (lua_State *L) {
  aux_gc(L, "llist");
  return 0;
}

static int ulist_gc (lua_State *L) {
  aux_gc(L, "ulist");
  return 0;
}

static int llist_nodegc (lua_State *L) {  /* garbage collect deleted userdata */
  lua_setmetatabletoobject(L, 1, NULL, 0);  /* 5.2.0 fix */
  return 0;
}

static int llist_checkllist (lua_State *L) {  /* 2.15.4 */
  if (!luaL_isudata(L, 1, "llist"))
    luaL_error(L, "Error in " LUA_QS ": expected an llist, got %s.", "llist.checkllist", luaL_typename(L, 1));
  return 0;
}


static const struct luaL_Reg llist_llistlib [] = {  /* metamethods for linked lists `n' */
  {"__index",      llist_getitem},   /* n[p], with p the index, counting from 1 */
  {"__writeindex", llist_setitem},   /* n[p] := value, with p the index, counting from 1 */
  {"__size",       llist_getsize},   /* retrieve the number of entries in `n' */
  {"__tostring",   llist_tostring},  /* for output at the console, e.g. print(n) */
  {"__in",         llist_in},        /* `in` operator for linked lists */
  {"__notin",      llist_notin},     /* `notin` operator for linked lists */
  {"__gc",         llist_gc},        /* please do not forget garbage collection */
  {"__eq",         llist_eq},        /* metamethod for `=` operator */
  {"__aeq",        llist_aeq},       /* metamethod for `~=` operator */
  {"__empty",      llist_empty},     /* metamethod for `empty` operator */
  {"__filled",     llist_filled},    /* metamethod for `filled` operator */
  {NULL, NULL}
};


static const struct luaL_Reg llist_llistnodelib [] = {  /* metamethods for nodes */
  {"__gc",         llist_nodegc},    /* needed for llist.iterate */
  {NULL, NULL}
};


static const luaL_Reg llistlib[] = {
  {"append",       llist_append},
  {"checkllist",   llist_checkllist},   /* 2.15.4 */
  {"dump",         llist_dump},         /* 2.15.4 */
  {"getitem",      llist_getitem},      /* 2.15.1 */
  {"getregistry",  llist_getregistry},  /* 2.15.1 */
  {"getsize",      llist_getsize},      /* 2.15.1 */
  {"iterate",      llist_iterate},
  {"new",          llist_new},
  {"prepend",      llist_prepend},
  {"purge",        llist_purge},
  {"put",          llist_put},
  {"replicate",    llist_replicate},
  {"setitem",      llist_setitem},      /* 2.15.1 */
  {"toseq",        llist_toseq},        /* 2.15.4 */
  {"totable",      llist_totable},      /* 2.15.4 */
  {NULL, NULL}
};


#define aux_isulist(L,idx)     (agn_isutype(L, idx, "ulist") && luaL_isudata(L, idx, "ulist"))

static Slist *checkulist (lua_State *L, int idx, const char *procname) {
  if (!aux_isulist(L, idx))
    luaL_error(L, "Error in " LUA_QS ": expected a ulist, got %s.", procname, luaL_typename(L, idx));
  return lua_touserdata(L, idx);
}


/* Creates a new unrolled singly-linked list with the given n number of pre-allocated slots in each internal sequence.
   The default is 128 if n is not given. */

static int ulist_new (lua_State *L) {  /* 2.15.2 */
  size_t n;
  lua_Number fillfactor;
  Slist *list;
  Slnode *node;
  n = agnL_optposint(L, 1, 128);
  fillfactor = agnL_optpositive(L, 2, 0.75);  /* 0.75 is better than 0.5 */
  if (fillfactor >= 1)
    luaL_error(L, "Error in " LUA_QS ": fill factor is 1 or greater.", "ulist.new");
  list = (Slist *)lua_newuserdata(L, sizeof(Slist));
  lua_createtable(L, 0, 0);  /* 5.4.5 change, dont crowd the registry, use the separate environment table */
  lua_setfenv(L, -2);  /* pops the table from the stack */
  if (list)
    sl_initlist(list, NULL);  /* 2.15.2 */
  else
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ulist.new");
  lua_setmetatabletoobject(L, -1, "ulist", 1);  /* assign the ulist_ulistlib metatable defined below to the userdata */
  /* agn_setutypestring(L, -1, "ulist"); */
  agn_createseq(L, n);
  node = sl_createnode(L, -2, -1);
  agn_poptop(L);  /* pop sequence from stack */
  if (node == NULL)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "ulist.new");
  sl_append(list, node);
  lua_createtable(L, 0, 3);
  lua_rawsetstringnumber(L, -1, "node", 1);
  lua_createtable(L, 0, 0);
  agn_setinumber(L, -1, 1, 0);
  lua_setfield(L, -2, "sizes");
  lua_rawsetstringnumber(L, -1, "nodesize", n);
  lua_rawsetstringnumber(L, -1, "fill", fillfactor);
  list->registry = luaL_ref(L, LUA_REGISTRYINDEX);  /* store unique ref to registry table and pop table */
  list->nodesize = n;
  list->fillfactor = fillfactor;
  return 1;
}


static int ulist_getllist (lua_State *L) {
  size_t i, idx;
  Slnode *node;
  Slist *list;
  list = checkulist(L, 1, "ulist.getllist");
  idx = tools_posrelat(agn_checkinteger(L, 2), list->size);
  if (idx < 1 || idx > list->size) {
    lua_pushnil(L);
    return 1;
  }
  node = list->head;
  for (i=1; i < idx; i++) {
    node = node->next;
  }
  lua_getfenvi(L, 1, node->data);
  return 1;
}


static int ulist_getregistry (lua_State *L) {  /* return llist registry table */
  Slist *list = checkulist(L, 1, "ulist.getregistry");
  lua_rawgeti(L, LUA_REGISTRYINDEX, list->registry);
  return 1;
}


/* * Auxiliary Functions ******************************************************************* */

#define aux_checktable(L) { \
  if (!lua_istable(L, -1)) { \
    agn_poptop(L); \
    luaL_error(L, "Error in " LUA_QS ": invalid ulist structure.", "ulist.setitem"); \
  } \
}

#define aux_checkseq(L,procname) { \
  if (!lua_isseq(L, -1)) { \
    agn_poptop(L); \
    luaL_error(L, "Error in " LUA_QS ": invalid ulist structure.", procname); \
  } \
}

static Slnode *aux_getindices (lua_State *L, int udx, Slist *list, size_t pos, size_t *nodeno, size_t *newpos, size_t *seqsize) {
  Slnode *node;
  size_t n;
  n = 0;
  *nodeno = 1;
  *newpos = pos;
  *seqsize = 0;
  node = list->head;
  while (node) {
    lua_getfenvi(L, udx, node->data);
    aux_checkseq(L, "(internal)");
    *seqsize = agn_seqsize(L, -1);
    agn_poptop(L);
    n += *seqsize;
    if (n >= pos) break;
    *newpos -= *seqsize;
    node = node->next;
    if (node != NULL) (*nodeno)++;
  }
  return (*nodeno > list->size) ? NULL : node;  /* NULL if pos out-of-range, node otherwise */
}

int aux_getitem (lua_State *L, int udx, Slist *list, int pos) {  /* 2.15.3 */
  Slnode *node;
  size_t nodeno, seqpos, seqsize;
  pos = tools_posrelat(pos, list->size);
  node = aux_getindices(L, udx, list, pos, &nodeno, &seqpos, &seqsize);
  if (!node) {
    lua_pushnil(L);
    return 1;
  }
  lua_getfenvi(L, udx, node->data);  /* get sequence */
  lua_seqrawgeti(L, -1, seqpos);  /* get sequence item */
  lua_remove(L, -2);  /* delete sequence to keep stack clean */
  return 1;
}

static size_t aux_getsize (lua_State *L, int udx, Slist *list) {  /* 2.15.3 */
  Slnode *node;
  size_t n, s;
  n = 0;
  node = list->head;
  while (node) {
    lua_getfenvi(L, udx, node->data);
    aux_checkseq(L, "(internal)");
    s = agn_seqsize(L, -1);
    agn_poptop(L);
    n += s;
    node = node->next;
  }
  return n;
}

/* ************************************************************************************* */

static int ulist_getindices (lua_State *L) {  /* 40 percent faster than the Agena version, 2.15.1 */
  Slist *list;
  size_t pos, nodeno, seqpos, seqsize;
  list = checkulist(L, 1, "ulist.getindices");
  pos = tools_posrelat(agn_checkinteger(L, 2), list->size);
  if (!aux_getindices(L, 1, list, pos, &nodeno, &seqpos, &seqsize)) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushnumber(L, nodeno);  /* node */
  lua_pushnumber(L, seqpos);  /* position in sequence */
  return 2;
}


/* Returns the item stored at index pos in the unrolled singly-linked list ul. C version is 11 % faster, 2.15.2 */
static int ulist_getitem (lua_State *L) {
  Slist *list;
  Slnode *node;
  size_t pos, nodeno, seqpos, seqsize;
  int nargs = lua_gettop(L);
  list = checkulist(L, 1, "ulist.getitem");
  if (nargs == 2 && !agn_isinteger(L, 2)) {  /* OOP method, 5.5.14 */
    return agn_initmethodcall(L, "ulist", 5);
  }
  pos = tools_posrelat(agn_checkinteger(L, 2), list->size);
  if (pos == 0) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, list->registry);
    return 1;
  }
  if (lua_gettop(L) == 2) {
    node = aux_getindices(L, 1, list, pos, &nodeno, &seqpos, &seqsize);
    if (!node) {
      lua_pushnil(L);
      return 1;
    }
    lua_getfenvi(L, 1, node->data);  /* get sequence */
    lua_seqrawgeti(L, -1, seqpos);  /* get sequence item */
    lua_remove(L, -2);  /* delete sequence to keep stack clean */
    return 1;
  } else {
    size_t i, n, flag;
    n = agnL_optposint(L, 3, 1);  /* 2.15.3 */
    flag = 1; node = NULL;  /* just to prevent compiler warnings */
    for (i=0; i < n; i++) {
      if (flag) node = aux_getindices(L, 1, list, pos, &nodeno, &seqpos, &seqsize);
      if (!node) {
        lua_pushnil(L);
        return i + 1;
      }
      lua_getfenvi(L, 1, node->data);  /* get sequence */
      lua_seqrawgeti(L, -1, seqpos);  /* get sequence item */
      lua_remove(L, -2);  /* delete sequence to keep stack clean */
      flag = (seqpos == seqsize);  /* end of sequence ? -> call aux_getindices again */
    }
    return n;
  }
}


/* Stores value at index pos in the unrolled singly-linked list ul. C version is 6 % faster. 2.15.2 */
static int ulist_setitem (lua_State *L) {
  Slist *list;
  Slnode *node;
  size_t pos, nodeno, seqpos, seqsize;
  if (lua_gettop(L) != 3)
    luaL_error(L, "Error in " LUA_QS ": expected three arguments.", "ulist.setitem");
  list = checkulist(L, 1, "ulist.setitem");
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "ulist.setitem");  /* 5.2.0 extension */
  pos = tools_posrelat(agn_checkinteger(L, 2), list->size);
  if (pos == 0)
    luaL_error(L, "Error in " LUA_QS ": index 0 is out of range.", "ulist.setitem");
  node = aux_getindices(L, 1, list, pos, &nodeno, &seqpos, &seqsize);
  if (!node) {  /* index does not yet exist */
    /*if (seqpos == 1 && (seqsize <= list->nodesize/2)) { */ /* append to existing sequence if it is not yet full */
    if (seqpos == 1 && (seqsize <= list->nodesize * list->fillfactor)) {  /* append to existing sequence if it is not yet full */
      size_t i;
      node = list->head;
      for (i=1; i < nodeno; i++) node = node->next;
      lua_getfenvi(L, 1, node->data);  /* get sequence */
      aux_checkseq(L, "ulist.setitem");
      lua_pushvalue(L, 3);
      lua_seqrawseti(L, -2, seqsize + 1);  /* append value to sequence */
      agn_poptop(L);  /* pop sequence */
      /* update seqsize */
      lua_rawgeti(L, LUA_REGISTRYINDEX, list->registry);
      aux_checktable(L);
      lua_getfield(L, -1, "sizes");  /* add one to existing size entry */
      aux_checktable(L)
      agn_setinumber(L, -1, nodeno, seqsize + 1);  /* pops number */
      agn_poptoptwo(L);  /* drop `sizes' subtable and registry */
    } else if (seqpos == 1) {  /* create and append new sequence and put item into it at pos 1 */
      agn_createseq(L, list->nodesize);
      lua_pushvalue(L, 3);
      lua_seqrawseti(L, -2, 1);  /* set sequence item */
      node = sl_createnode(L, 1, -1);  /* set (`copy` of) sequence into node */
      agn_poptop(L);  /* pop sequence */
      if (node == NULL)
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "llist.setitem");
      sl_append(list, node);  /* append node to list */
      /* adjust status info in registry */
      lua_rawgeti(L, LUA_REGISTRYINDEX, list->registry);
      aux_checktable(L)
      /* extend `sizes' table by one */
      lua_getfield(L, -1, "sizes");
      aux_checktable(L)
      lua_pushinteger(L, 1);
      agn_rawinsert(L, -2);
      agn_poptop(L);  /* drop `sizes' subtable */
      /* increment `node' counter by one */
      lua_pushinteger(L, nodeno + 1);
      lua_setfield(L, -2, "node");
      /* pop registry */
      agn_poptop(L);
    } else
      luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "ulist.setitem", pos);
    return 0;
  }  /* put into existing sequence, overwriting existing field */
  lua_getfenvi(L, 1, node->data);  /* get sequence */
  aux_checkseq(L, "ulist.setitem");
  lua_pushvalue(L, 3);
  lua_seqrawseti(L, -2, seqpos);  /* set sequence item */
  agn_poptop(L);  /* level the stack */
  return 0;
}


static int ulist_getsize (lua_State *L) {
  Slist *list;
  list = checkulist(L, 1, "ulist.getsize");
  lua_pushnumber(L, aux_getsize(L, 1, list));
  return 1;
}


static int ulist_empty (lua_State *L) {  /* 2.15.4 */
  Slist *list;
  list = checkulist(L, 1, "`empty` operator");
  lua_pushboolean(L, aux_getsize(L, 1, list) == 0);
  return 1;
}


static int ulist_filled (lua_State *L) {  /* 2.15.4 */
  Slist *list;
  list = checkulist(L, 1, "`filled` operator");
  lua_pushboolean(L, aux_getsize(L, 1, list) != 0);
  return 1;
}


static int ulist_has (lua_State *L) {
  Slist *list;
  Slnode *node;
  size_t s, i;
  int ismt;
  if (lua_gettop(L) != 2)
    luaL_error(L, "Error in " LUA_QS ": expected two arguments.", "ulist.has");
  ismt = lua_isuserdata(L, 2);
  list = checkulist(L, 1 + ismt, "ulist.has");  /* 2.18.0 RC 2 fix for __in operator */
  node = list->head;
  while (node) {
    lua_getfenvi(L, 1 + ismt, node->data);
    aux_checkseq(L, "ulist.has");
    s = agn_seqsize(L, -1);
    for (i=1; i <= s; i++) {
      lua_seqrawgeti(L, -1, i);
      if (lua_rawequal(L, -1, 2 - ismt)) {
        agn_poptoptwo(L);  /* pop value and requence */
        lua_pushtrue(L);
        return 1;
      }
      agn_poptop(L);  /* pop value */
    }
    agn_poptop(L);  /* pop sequence */
    node = node->next;
  }
  lua_pushfalse(L);
  return 1;
}


static int ulist_hasnot (lua_State *L) {  /* 2.18.0 */
  Slist *list;
  Slnode *node;
  size_t s, i;
  int ismt;
  if (lua_gettop(L) != 2)
    luaL_error(L, "Error in " LUA_QS ": expected two arguments.", "ulist.hasnot");
  ismt = lua_isuserdata(L, 2);
  list = checkulist(L, 1 + ismt, "ulist.has");
  node = list->head;
  while (node) {
    lua_getfenvi(L, 1 + ismt, node->data);
    aux_checkseq(L, "ulist.has");
    s = agn_seqsize(L, -1);
    for (i=1; i <= s; i++) {
      lua_seqrawgeti(L, -1, i);
      if (lua_rawequal(L, -1, 2 - ismt)) {
        agn_poptoptwo(L);  /* pop value and requence */
        lua_pushfalse(L);
        return 1;
      }
      agn_poptop(L);  /* pop value */
    }
    agn_poptop(L);  /* pop sequence */
    node = node->next;
  }
  lua_pushtrue(L);
  return 1;
}


static int ulist_totable (lua_State *L) {  /* 2.15.3, based on ulist_has */
  Slist *list;
  Slnode *node;
  size_t s, i, c;
  list = checkulist(L, 1, "ulist.totable");
  node = list->head;
  lua_createtable(L, list->size * list->nodesize, 0);
  c = 0;
  while (node) {
    lua_getfenvi(L, 1, node->data);
    aux_checkseq(L, "ulist.totable");
    s = agn_seqsize(L, -1);
    for (i=1; i <= s; i++) {
      lua_seqrawgeti(L, -1, i);
      lua_rawseti(L, -3, ++c);
    }
    agn_poptop(L);  /* pop sequence */
    node = node->next;
  }
  return 1;
}


static int ulist_toseq (lua_State *L) {  /* 2.15.4, based on ulist_totable */
  Slist *list;
  Slnode *node;
  size_t s, i, c;
  list = checkulist(L, 1, "ulist.toseq");
  node = list->head;
  agn_createseq(L, list->size * list->nodesize);
  c = 0;
  while (node) {
    lua_getfenvi(L, 1, node->data);
    aux_checkseq(L, "ulist.toseq");
    s = agn_seqsize(L, -1);
    for (i=1; i <= s; i++) {
      lua_seqrawgeti(L, -1, i);
      lua_seqrawseti(L, -3, ++c);
    }
    agn_poptop(L);  /* pop sequence */
    node = node->next;
  }
  return 1;
}


static int ulist_dump (lua_State *L) {  /* 2.15.4, based on ulist_toseq */
  Slist *list;
  Slnode *node, *temp;
  size_t s, i, c;
  (void)L;
  lua_lock(L);
  list = checkulist(L, 1, "ulist.dump");
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "ulist.dump");  /* 5.2.0 extension */
  node = list->head;
  agn_createseq(L, list->size * list->nodesize);
  c = 0;
  while (node) {
    temp = node->next;
    lua_getfenvi(L, 1, node->data);
    aux_checkseq(L, "ulist.dump");
    s = agn_seqsize(L, -1);
    for (i=1; i <= s; i++) {
      lua_seqrawgeti(L, -1, i);
      lua_seqrawseti(L, -3, ++c);
    }
    agn_poptop(L);  /* pop sequence */
    lua_getfenv(L, 1);
    luaL_unref(L, -1, node->data);  /* luaL_unref also deletes the sequence from the registry */
    agn_poptop(L);
    xfree(node);
    node = temp;
  }
  luaL_unref(L, LUA_REGISTRYINDEX, list->registry);
  lua_setmetatabletoobject(L, 1, NULL, 1);  /* 5.2.0 fix */
  lua_unlock(L);
  return 1;
}


static int ulist_isequal (lua_State *L) {  /* 2.15.3; 10 times faster than an implementation in Agena using iterators */
  Slist *list1, *list2;
  Slnode *node1, *node2;
  size_t s1, s2, i1, i2;
  int skip;
  list1 = checkulist(L, 1, "ulist.isequal");
  list2 = checkulist(L, 2, "ulist.isequal");
  node1 = list1->head;
  node2 = list2->head;
  i1 = i2 = s1 = s2 = 0;
  /* first get overall sizes */
  while (node1) {
    lua_getfenvi(L, 1, node1->data);
    s1 += agn_seqsize(L, -1);
    agn_poptop(L);
    node1 = node1->next;
  }
  while (node2) {
    lua_getfenvi(L, 2, node2->data);
    s2 += agn_seqsize(L, -1);
    agn_poptop(L);
    node2 = node2->next;
  }
  if (s1 != s2) {
    lua_pushfalse(L);
    return 1;
  }
  node1 = list1->head;
  node2 = list2->head;
  while (1) {
    skip = 0;
    if (node1 == NULL || node2 == NULL) break;
    lua_getfenvi(L, 1, node1->data);
    aux_checkseq(L, "ulist.isequal");
    lua_getfenvi(L, 2, node2->data);
    aux_checkseq(L, "ulist.isequal");
    /* two sequences are now on the stack */
    s1 = agn_seqsize(L, -2);
    s2 = agn_seqsize(L, -1);
    if (s1 == 0) { i1 = 0; node1 = node1->next; agn_poptoptwo(L); skip = 1; }  /* sequence is empty, get next sequence */
    if (s2 == 0) { i2 = 0; node2 = node2->next; agn_poptoptwo(L); skip = 1; }  /* ditto */
    if (skip) continue;
    lua_seqrawgeti(L, -2, ++i1);
    lua_seqrawgeti(L, -2, ++i2);
    if (!lua_equal(L, -1, -2)) {
      lua_pop(L, 4);
      lua_pushfalse(L);
      return 1;
    }
    lua_pop(L, 4);  /* delete values and sequences */
    if (i1 == s1) { i1 = 0; node1 = node1->next; skip = 1; }  /* ulist sequences may be of different sizes, get next one */
    if (i2 == s2) { i2 = 0; node2 = node2->next; skip = 1; }  /* ditto */
    if (skip) continue;  /* we must check whether the end in _both_ sequences has been reached before continue'ing */
    /* same sequence(s) are being deliberately reloaded and next respective items compared */
  };
  lua_pushtrue(L);
  return 1;
}


static int ulist_swap (lua_State *L) {  /* 2.15.3 */
  Slist *list;
  Slnode *nodea, *nodeb;
  size_t a, b, nodenoa, nodenob, seqposa, seqposb, seqsizea, seqsizeb;
  list = checkulist(L, 1, "ulist.swap");
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "ulist.swap");  /* 5.2.0 extension */
  a = agn_checkposint(L, 2);
  b = agn_checkposint(L, 3);
  if (a == b) return 0;
  nodea = aux_getindices(L, 1, list, a, &nodenoa, &seqposa, &seqsizea);
  if (nodea == NULL)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "ulist.swap", (int)a);
  nodeb = aux_getindices(L, 1, list, b, &nodenob, &seqposb, &seqsizeb);
  if (nodeb == NULL)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "ulist.swap", (int)b);
  /* get list[a] */
  lua_getfenvi(L, 1, nodea->data);
  if (nodea != nodeb) {
    lua_seqrawgeti(L, -1, seqposa);
    /* get list[b] */
    lua_getfenvi(L, 1, nodeb->data);
    lua_seqrawgeti(L, -1, seqposb);
    /* sequ_a  -4
       item_a  -3
       sequ_b  -2
       item_b  -1 */
    /* set item_b into sequ_a[seqposa] and pop item_b */
    lua_seqrawseti(L, -4, seqposa);
    /* sequ_a  -3
       item_a  -2
       sequ_b  -1 */
    /* set item_a into sequ_b[seqposb] and pop item_a */
    lua_pushvalue(L, -2);
    /* sequ_a  -4
       item_a  -3
       sequ_b  -2
       item_a  -1 */
    lua_seqrawseti(L, -2, seqposb);
    lua_pop(L, 2);
  } else {
    lua_seqrawgeti(L, -1, seqposa);
    lua_seqrawgeti(L, -2, seqposb);
    /* seq_a   -3
       item_a  -2
       item b  -1 */
    lua_seqrawseti(L, -3, seqposa);
    lua_seqrawseti(L, -2, seqposb);
  }
  agn_poptop(L);
  return 0;
}


static int ulist_checkulist (lua_State *L) {  /* 2.15.1 */
  if (!aux_isulist(L, 1))
    luaL_error(L, "Error in " LUA_QS ": expected a ulist, got %s.", "ulist.checkulist", luaL_typename(L, 1));
  return 0;
}


static int ulist_isulist (lua_State *L) {  /* 2.15.1 */
  lua_pushboolean(L, aux_isulist(L, 1));
  return 1;
}


static const struct luaL_Reg ulist_ulistlib[] = {  /* metamethods for unrolled singly-linked lists `n' */
  {"__index",      ulist_getitem},  /* n[p], with p the index, counting from 1 */
  {"__writeindex", ulist_setitem},  /* n[p] := value, with p the index, counting from 1 */
  {"__gc",         ulist_gc},       /* please do not forget garbage collection */
  {"__in",         ulist_has},      /* `in` operator for ulists */
  {"__notin",      ulist_hasnot},   /* `notin` operator for ulists */
  {"__size",       ulist_getsize},  /* retrieve the number of entries in `n' */
  {"__empty",      ulist_empty},    /* metamethod for `empty` operator */
  {"__filled",     ulist_filled},   /* metamethod for `filled` operator */
  {NULL, NULL}
};

static const luaL_Reg ulistlib[] = {
  {"checkulist",   ulist_checkulist},
  {"dump",         ulist_dump},
  {"getindices",   ulist_getindices},
  {"getitem",      ulist_getitem},
  {"getllist",     ulist_getllist},
  {"getregistry",  ulist_getregistry},
  {"getsize",      ulist_getsize},
  {"has",          ulist_has},
  {"isequal",      ulist_isequal},
  {"isulist",      ulist_isulist},
  {"new",          ulist_new},
  {"setitem",      ulist_setitem},
  {"swap",         ulist_swap},
  {"toseq",        ulist_toseq},
  {"totable",      ulist_totable},
  {NULL, NULL}
};

/* ***********************************************************************************************
   Doubly-Linked List, taken from http://www.martinbroadhurst.com/linked-list-in-c.html
*  **********************************************************************************************/

typedef struct Dlnode {
  struct Dlnode *next;
  struct Dlnode *previous;
  int data;  /* unique reference created by luaL_ref into the registry storing the Agena value */
} Dlnode;

typedef struct {
  Dlnode *head;
  Dlnode *tail;
  size_t size;   /* number of nodes */
  int registry;  /* registry, for attribute information, stored to index 0 */
} Dlist;


static Dlnode *dl_createnode (lua_State *L, int udx, int idx) {
  Dlnode *node = malloc(sizeof(Dlnode));
  if (node) {  /* put value into the registry _table_ */
    lua_getfenv(L, udx);
    lua_pushvalue(L, idx > 0 ? idx : idx - 1);  /* value to be put into environment table */
    node->data = luaL_ref(L, -2);  /* store unique reference to value and pop value */
    agn_poptop(L);
    node->next = NULL;
    node->previous = NULL;
  }
  return node;
}

static void dl_initlist (Dlist *list, Dlnode *node) {
  list->head = node;
  list->tail = node;
  list->size = 0;
  list->registry = 0;
}

static void dl_prepend (Dlist *list, Dlnode *node) {
  if (list->head != NULL) {
    list->head->previous = node;
    node->next = list->head;
    list->head = node;
  } else {
    list->head = node;
    list->tail = node;
  }
  list->size++;
}

static void dl_append (Dlist *list, Dlnode *node) {
  if (list->tail != NULL) {
    list->tail->next = node;
    node->previous = list->tail;
    list->tail = node;
  } else {
    list->head = node;
    list->tail = node;
  }
  list->size++;
}

static void dl_removehead (Dlist *list) {
  if (list->head != NULL) {
    Dlnode *temp = list->head;
    list->head = list->head->next;
    if (list->head == NULL)
      list->tail = NULL;
    else {
      list->head->previous = NULL;
      if (list->head->next != NULL)
        list->head->next->previous = list->head;
      else
        list->tail = list->head;
    }
    xfree(temp);
    list->size--;
  }
}

static void dl_removetail (Dlist *list) {
  if (list->tail != NULL) {
    Dlnode *temp = list->tail;
    list->tail = list->tail->previous;
    if (list->tail == NULL)
      list->head = NULL;
    else {
      list->tail->next = NULL;
      if (list->tail->previous != NULL)
        list->tail->previous->next = list->tail;
      else
        list->head = list->tail;
    }
    xfree(temp);
    list->size--;
  }
}

/* Remove a given node */
static void dl_remove (Dlist *list, Dlnode *node) {
  if (node == list->head)
    dl_removehead(list);
  else if (node == list->tail)
    dl_removetail(list);
  else {
    node->previous->next = node->next;
    node->next->previous = node->previous;
    xfree(node);
    list->size--;
  }
}

#define aux_isdlist(L,idx)    (agn_isutype(L, idx, "dlist") && luaL_isudata(L, idx, "dlist"))
#define checkdlist(L, n)      (Dlist *)luaL_checkudata(L, n, "dlist")

#define dlist_movetopos(L,i,list,node,idx) { \
  if (idx <= list->size/2) { \
    node = list->head; \
    for (i=1; i < idx; i++) node = node->next; \
  } else { \
    node = list->tail; \
    for (i=list->size; i > idx; i--) node = node->previous; \
  } \
}

static int dlist_iterate (lua_State *L);

static int dlist_new (lua_State *L) {  /* create a new userdata dlist  */
  size_t i, nargs;
  Dlist *list;
  Dlnode *node;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  list = (Dlist *)lua_newuserdata(L, sizeof(Dlist));
  lua_createtable(L, 0, 0);  /* 5.4.5 change, dont crowd the registry, use the separate environment table */
  lua_setfenv(L, -2);  /* pops the table from the stack */
  if (list)
    dl_initlist(list, NULL);
  else
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "dlist.new");
  lua_setmetatabletoobject(L, -1, "dlist", 1);  /* assign the metatable defined below to the userdata */
  for (i=1; i <= nargs; i++) {
    lua_lock(L);
    node = dl_createnode(L, -1, i);
    if (node == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "dlist.new");
    dl_append(list, node);
    lua_unlock(L);
  }
  lua_createtable(L, 0, 0);
  list->registry = luaL_ref(L, LUA_REGISTRYINDEX);  /* store unique ref to registry table and pop table */
  aux_additerator(L, &dlist_iterate);
  return 1;
}


static int dlist_append (lua_State *L) {
  Dlist *list;
  Dlnode *node;
  size_t nargs, i;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (lua_gettop(L) < 2)
    luaL_error(L, "Error in " LUA_QS ": need at least a dlist and a value.", "dlist.append");
  list = (Dlist *)checkdlist(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "dlist.append");  /* 5.2.0 extension */
  for (i=2; i <= nargs; i++) {
    lua_lock(L);
    node = dl_createnode(L, 1, i);
    if (node == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "dlist.append");
    dl_append(list, node);
    lua_unlock(L);
  }
  return 0;
}


static int dlist_prepend (lua_State *L) {
  Dlist *list;
  Dlnode *node;
  size_t i, nargs;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (lua_gettop(L) < 2)
    luaL_error(L, "Error in " LUA_QS ": need at least a dlist and a value.", "dlist.prepend");
  list = (Dlist *)checkdlist(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "dlist.prepend");  /* 5.2.0 extension */
  for (i=2; i <= nargs; i++) {
    lua_lock(L);
    node = dl_createnode(L, 1, i);
    if (node == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "dlist.prepend");
    dl_prepend(list, node);
    lua_unlock(L);
  }
  return 0;
}


static int dlist_put (lua_State *L) {
  Dlist *list;
  Dlnode *newnode;
  int idx;
  if (lua_gettop(L) != 3)
    luaL_error(L, "Error in " LUA_QS ": need a dlist, an index, and any value.", "dlist.put");
  lua_lock(L);
  if (!aux_isdlist(L, 1))
    luaL_error(L, "Error in " LUA_QS ": expected a dlist, got %s.", "dlist.put", luaL_typename(L, 1));
  list = (Dlist *)lua_touserdata(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "dlist.put");  /* 5.2.0 extension */
  idx = tools_posrelat(agn_checkinteger(L, 2), list->size);
  if (idx < 1 || idx > list->size + 1)
    luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "dlist.put", idx);
  newnode = dl_createnode(L, 1, 3);  /* new node currently is at index 3 on the stack */
  if (newnode == NULL)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "dlist.put");
  if (list->size + 1 == idx) {  /* append to end of list */
    dl_append(list, newnode);  /* takes care of shifting middle element itself */
  } else {  /* insert new node into list, shifting existing ones into open space */
    size_t i;
    Dlnode *node;
    /* move to given list position */
    dlist_movetopos(L, i, list, node, idx);
    if (node == list->head) {    /* new node shall be added to first position */
      dl_prepend(list, newnode);
    } else {
      newnode->next = node;                /* move existing node `up` one position */
      newnode->previous = node->previous;  /* add address to new node to previous node */
      node->previous->next = newnode;
      node->previous = newnode;
      list->size++;
    }
  }
  return 0;
}


static int dlist_purge (lua_State *L) {
  Dlist *list;
  Dlnode *node;
  int idx;
  size_t i;
  if (lua_gettop(L) == 0)
    luaL_error(L, "Error in " LUA_QS ": need a dlist and optionally an index.", "dlist.purge");
  lua_lock(L);
  if (!aux_isdlist(L, 1))
    luaL_error(L, "Error in " LUA_QS ": expected a dlist, got %s.", "dlist.purge", luaL_typename(L, 1));
  list = (Dlist *)lua_touserdata(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "dlist.purge");  /* 5.2.0 extension */
  idx = tools_posrelat(agnL_optinteger(L, 2, list->size), list->size);
  if (idx < 1 || idx > list->size)
    luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "dlist.purge", idx);
  /* move to given list position */
  dlist_movetopos(L, i, list, node, idx);
  luaL_checkstack(L, 2, "not enough stack space");
  lua_getfenvi(L, 1, node->data);
  lua_getfenv(L, 1);
  luaL_unref(L, -1, node->data);  /* does not pop environment table */
  agn_poptop(L);  /* so pop it now */
  /* if node == NULL, then we will remove the entire head */
  dl_remove(list, node);
  return 1;
}


/* Creates a new userdata dlist node, assigns a metatable to it, and leaves the new userdata node on the top of the stack. */
static int dlist_node (lua_State *L, Dlnode *node, int meta) {
  Dlnode **newnode;
  if (node == NULL) {
    lua_pushnil(L);
  } else {
    lua_lock(L);
    /* see answer on nabble.com by Michal Kottman, Aug 27, 2011; 8:03 pm on `Re: Struggling with userdata concept`
       at http://lua.2524044.n2.nabble.com/Struggling-with-userdata-concepts-td6732223.html */
    newnode = (Dlnode **)lua_newuserdata(L, sizeof(Dlnode *));
    *newnode = node; /* store the pointer inside the userdata */
    if (*newnode) {
      (*newnode)->data = node->data;
      (*newnode)->next = node->next;
      (*newnode)->previous = node->previous;
    } else
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "dlist.iterate");
    lua_pushvalue(L, meta);  /* push metatable */
    if (!lua_istable(L, -1)) {
      agn_poptop(L);  /* pop newuserdata */
      luaL_error(L, "Error in " LUA_QS ": userdata metatable does not exist.", "dlist.iterate");
    }
    agn_setudmetatable(L, -2);
    lua_unlock(L);
  }
  return 1;  /* pushed node lightuserdata onto the stack */
}

static int diterate (lua_State *L) {
  Dlnode *node;
  void *upval;
  int i, flag, offset, pushidx, lastone;
  flag = 0; lastone = 0;
  upval = lua_touserdata(L, lua_upvalueindex(2));  /* leave this here, otherwise Agena would crash */
  if (upval == NULL) {
    lua_pushnil(L);
    return 1;
  }
  offset = lua_tointeger(L, 1);
  pushidx = lua_tointeger(L, lua_upvalueindex(6));
  if (agn_istrue(L, lua_upvalueindex(3))) {  /* we got the very first traversal, but the list might still be empty ... */
    int pos;
    Dlist *list = (Dlist *)upval;
    if (list->head == NULL) {  /* ... list is empty ?  Do not change Boolean first position flag */
      lua_pushnil(L);
      return 1;
    }
    dlist_node(L, list->head, lua_upvalueindex(1));  /* create a userdata node */
    lua_replace(L, lua_upvalueindex(2));             /* make it an upvalue */
    lua_pushfalse(L);                                /* flag so that this `if' branch will never be entered again */
    lua_replace(L, lua_upvalueindex(3));
    node = *(Dlnode **)lua_touserdata(L, lua_upvalueindex(2));  /* get freshly created userdata node */
    pos = lua_tointeger(L, lua_upvalueindex(4));
    for (i=1; i < pos + offset && node != NULL; i++) {
      node = node->next;
      flag = 1;
    }
  } else {
    node = *(Dlnode **)upval;
    for (i=0; i < offset + 1 && node != NULL; i++)
      node = node->next;
    flag = 1;
  }
  if (pushidx) {
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushinteger(L, pushidx++);
    lua_pushinteger(L, pushidx);
    lua_replace(L, lua_upvalueindex(6));
  }
  if (node == NULL) {
    lua_pushnil(L);
    if (pushidx) {
      lua_pushvalue(L, lua_upvalueindex(4));  /* put 4 start position */
      lua_replace(L, lua_upvalueindex(6));    /* into 6 additionally push index flag */
      upval = lua_touserdata(L, lua_upvalueindex(2));
      lua_pushtrue(L);  /* mark as ready-for-first traversal */
      lua_replace(L, lua_upvalueindex(3));    /* into slot 3 */
      pushidx = 0;
      lastone = 1;
    }
  } else {
    lua_getfenvi(L, lua_upvalueindex(5), node->data);
  }
  if (flag) {  /* do not create a node for list->head again */
    if (!lastone)
      dlist_node(L, node, lua_upvalueindex(1));
    else
      lua_pushvalue(L, lua_upvalueindex(5));  /* put 5 llist backup */
    lua_replace(L, lua_upvalueindex(2));  /* update node userdata upvalue */
  }
  return 1 + (pushidx != 0);
}

static int dlist_iterate (lua_State *L) {
  int pos, pushidx;
  if (!aux_isdlist(L, 1))
    luaL_error(L, "Error in " LUA_QS ": expected a dl, got %s.", "dlist.iterate", luaL_typename(L, 1));
  pos = luaL_optint(L, 2, 1);
  if (pos < 1)
    luaL_error(L, "Error in " LUA_QS ": start index must be positive.", "dlist.iterate");
  pushidx = agnL_optboolean(L, 3, 0);  /* 7.1.1 extension */
  luaL_checkstack(L, 6, "not enough stack space");  /* 3.18.4 fix */
  luaL_getmetatable(L, "dlistnode");  /* 1 metatable */
  if (!lua_istable(L, -1)) {
    agn_poptop(L);  /* pop anything */
    luaL_error(L, "Error in " LUA_QS ": userdata metatable does not exist.", "dlist.iterate");
  }
  lua_pushvalue(L, 1);      /* 2 dlist */
  lua_pushtrue(L);          /* 3 first-read flag */
  lua_pushinteger(L, pos);  /* 4 start position */
  lua_pushvalue(L, 1);      /* 5 dlist, for the values in the environment table of the list, as pos #2 will be overwritten later */
  lua_pushinteger(L, pushidx ? pos : 0);  /* 6 additionally push index flag */
  lua_pushcclosure(L, &diterate, 6);
  return 1;
}


static int dlist_replicate (lua_State *L) {
  Dlist *source, *target;
  Dlnode *node, *newnode;
  source = checkdlist(L, 1);
  target = (Dlist *)lua_newuserdata(L, sizeof(Dlist));
  lua_createtable(L, source->size, 0);
  lua_setfenv(L, -2);  /* pops the table from the stack */
  if (target)
    dl_initlist(target, NULL);
  else
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "dlist.replicate");
  lua_setmetatabletoobject(L, -1, "dlist", 1);  /* assign the metatable defined below to the userdata */
  for (node = source->head; node != NULL; node = node->next) {
    lua_getfenvi(L, 1, node->data);
    lua_lock(L);
    newnode = dl_createnode(L, -2, -1);
    if (newnode == NULL) {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "dlist.replicate");
    }
    dl_append(target, newnode);
    agn_poptop(L);
    lua_unlock(L);
  }
  lua_createtable(L, 0, 0);
  target->registry = luaL_ref(L, LUA_REGISTRYINDEX);  /* store unique ref to registry table and pop table */
  return 1;
}


static int dlist_getsize (lua_State *L) {  /* return the number of allocated slots in the dlist */
  Dlist *list = checkdlist(L, 1);
  lua_pushnumber(L, list->size);
  return 1;
}


static int dlist_empty (lua_State *L) {
  Dlist *list = checkdlist(L, 1);
  lua_pushboolean(L, list->size == 0);
  return 1;
}


static int dlist_filled (lua_State *L) {
  Dlist *list = checkdlist(L, 1);
  lua_pushboolean(L, list->size != 0);
  return 1;
}


/* returns succeeding values from a doubly-linked list. It is at least twice as fast as individually reading the items. */
static int dlist_getitem (lua_State *L) {
  size_t i, idx, n;
  Dlnode *node;
  int nargs = lua_gettop(L);
  Dlist *list = checkdlist(L, 1);
  if (nargs == 2 && !agn_isinteger(L, 2)) {  /* OOP method, 5.5.14 */
    return agn_initmethodcall(L, "dlist", 5);
  }
  idx = tools_posrelat(agn_checkinteger(L, 2), list->size);
  n = agnL_optposint(L, 3, 1);
  luaL_checkstack(L, n, "too many values");  /* 2.31.7 fix */
  if (idx < 1 || idx + n - 1 > list->size) {
    if (idx == 0)
      lua_rawgeti(L, LUA_REGISTRYINDEX, list->registry);
    else
      lua_pushnil(L);
    return 1;
  }
  /* move to given list position */
  dlist_movetopos(L, i, list, node, idx);
  for (i=0; i < n; i++) {
    lua_getfenvi(L, 1, node->data);
    node = node->next;
  }
  return n;
}


static int dlist_getregistry (lua_State *L) {  /* 5.4.0 */
  Dlist *list = checkdlist(L, 1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, list->registry);
  return 1;
}


static int dlist_in (lua_State *L) {
  Dlist *list;
  Dlnode *node, *tail;
  int r, nargs;
  nargs = lua_gettop(L);
  if (nargs != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "dlist.__in");
  list = checkdlist(L, 2);
  if ((tail = list->tail) != NULL) {  /* at first check last element */
    lua_getfenvi(L, 2, tail->data);
    if (lua_equal(L, 1, -1)) {
      agn_poptop(L);
      lua_pushtrue(L);
      return 1;
    }
    agn_poptop(L);
  }
  r = 0;
  for (node = list->head; !r && node != NULL; node = node->next) {
    lua_getfenvi(L, 2, node->data);
    r = lua_equal(L, 1, -1);
    agn_poptop(L);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int dlist_notin (lua_State *L) {
  Dlist *list;
  Dlnode *node, *tail;
  int r, nargs;
  nargs = lua_gettop(L);
  if (nargs != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "dlist.__notin");
  list = checkdlist(L, 2);
  if ((tail = list->tail) != NULL) {  /* at first check last element */
    lua_getfenvi(L, 2, tail->data);
    if (lua_equal(L, 1, -1)) {
      agn_poptop(L);
      lua_pushfalse(L);
      return 1;
    }
    agn_poptop(L);
  }
  r = 0;
  for (node = list->head; !r && node != NULL; node = node->next) {
    lua_getfenvi(L, 2, node->data);
    r = lua_equal(L, 1, -1);
    agn_poptop(L);
  }
  lua_pushboolean(L, !r);
  return 1;
}


static int dlist_eq (lua_State *L) {
  Dlist *list1, *list2;
  Dlnode *node1, *node2;
  int r, nargs;
  nargs = lua_gettop(L);
  r = 1;
  if (nargs != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "dlist.__eq");
  list1 = checkdlist(L, 1);
  list2 = checkdlist(L, 2);
  if (list1->size != list2->size)
    r = 0;
  else {
    for (node1 = list1->head, node2 = list2->head; r && node1 != NULL && node2 != NULL;
         node1 = node1->next, node2 = node2->next) {
      lua_getfenvi(L, 1, node1->data);
      lua_getfenvi(L, 2, node2->data);
      r = lua_rawequal(L, -1, -2);
      agn_poptoptwo(L);
    }
  }
  lua_pushboolean(L, r);
  return 1;
}


static int dlist_aeq (lua_State *L) {
  Dlist *list1, *list2;
  Dlnode *node1, *node2;
  int r, nargs;
  nargs = lua_gettop(L);
  r = 1;
  if (nargs != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "dlist.__aeq");
  list1 = checkdlist(L, 1);
  list2 = checkdlist(L, 2);
  if (list1->size != list2->size)
    r = 0;
  else {
    for (node1 = list1->head, node2 = list2->head; r && node1 != NULL && node2 != NULL;
         node1 = node1->next, node2 = node2->next) {
      lua_getfenvi(L, 1, node1->data);
      lua_getfenvi(L, 2, node2->data);
      r = lua_rawaequal(L, -1, -2);
      agn_poptoptwo(L);
    }
  }
  lua_pushboolean(L, r);
  return 1;
}


static int dlist_setitem (lua_State *L) {  /* set a value at the given index position, overwriting existing one */
  size_t i, idx;
  Dlnode *node;
  Dlist *list;
  if (lua_gettop(L) != 3)
    luaL_error(L, "Error in " LUA_QS ": need a dlist, an index, and a value.", "dlist.setitem");
  list = checkdlist(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "dlist.setitem");  /* 5.2.0 extension */
  idx = tools_posrelat(agn_checkinteger(L, 2), list->size);
  if (idx < 1 || idx > list->size + 1) return 0;
  if (lua_isnil(L, 3)) {
    lua_settop(L, 2);
    return dlist_purge(L);  /* 5.4.5 patch, otherwise any value would be deleted randomly */
  }
  if (list->size + 1 == idx) {
    node = dl_createnode(L, 1, 3);
    if (node == NULL)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "dlist.setitem");
    dl_append(list, node);
  } else {  /* replace existing item */
    /* move to given list position */
    dlist_movetopos(L, i, list, node, idx);
    lua_getfenv(L, 1);
    luaL_unref(L, -1, node->data);  /* delete reference to former value */
    lua_pushvalue(L, 3);  /* enter substitute into the registry */
    node->data = luaL_ref(L, -2);
    agn_poptop(L);
  }
  return 0;
}


static int dlist_tostring (lua_State *L) {  /* at the console, the dlist is formatted as follows: */
  if (luaL_isudata(L, 1, "dlist"))
    lua_pushfstring(L, "dlist(%p)", lua_topointer(L, 1));
  else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}


static int dlist_totable (lua_State *L) {
  Dlist *list;
  Dlnode *node;
  size_t c;
  list = checkdlist(L, 1);
  lua_createtable(L, list->size, 0);
  c = 0;
  for (node = list->head; node != NULL; node = node->next) {
    lua_getfenvi(L, 1, node->data);
    lua_rawseti(L, -2, ++c);
  }
  return 1;
}


static int dlist_toseq (lua_State *L) {
  Dlist *list;
  Dlnode *node;
  size_t c;
  list = checkdlist(L, 1);
  agn_createseq(L, list->size);
  c = 0;
  for (node = list->head; node != NULL; node = node->next) {
    lua_getfenvi(L, 1, node->data);
    if (lua_isnil(L, -1)) { agn_poptop(L); continue; }
    lua_seqrawseti(L, -2, ++c);
  }
  return 1;
}


static int dlist_dump (lua_State *L) {
  Dlist *list;
  Dlnode *node, *temp;
  size_t c;
  (void)L;
  lua_lock(L);
  list = checkdlist(L, 1);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "dlist.dump");  /* 5.2.0 extension */
  node = list->head;
  luaL_checkstack(L, 2, "not enough stack space");
  agn_createseq(L, list->size);
  c = 0;
  while (node) {
    temp = node->next;
    lua_getfenvi(L, 1, node->data);
    lua_seqrawseti(L, -2, ++c);
    xfree(node);
    node = temp;
  }
  /* we will now reset the list so it can be reused, 5.4.5 */
  dl_initlist(list, NULL);
  /* we cannot simply drop the environment table and register a new one, as Agena would crash.
     So empty it. */
  lua_getfenv(L, 1);
  agn_cleanse(L, -1, 1);
  agn_poptop(L);
  lua_unlock(L);
  return 1;
}


/* garbage collect deleted userdata */
static int dlist_gc (lua_State *L) {
  Dlist *list;
  Dlnode *node, *temp;
  int result;
  (void)L;
  lua_lock(L);
  list = luaL_getudata(L, 1, "dlist", &result);  /* since we have two metatables, we use this alternative
    checker to avoid crashes when the `restart` statements tries to gc them. */
  if (list == NULL) return 0;
  lua_pushnil(L);
  lua_setfenv(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, list->registry);  /* delete registry table */
  lua_setmetatabletoobject(L, 1, NULL, 1);
  node = list->head;
  while (node != NULL) {
    temp = node->next;
    xfree(node);
    node = temp;
  }
  lua_unlock(L);
  return 0;
}


static int dlist_nodegc (lua_State *L) {  /* garbage collect deleted userdata */
  lua_setmetatabletoobject(L, 1, NULL, 0);    /* 5.2.0 fix */
  return 0;
}


static int dlist_checkdlist (lua_State *L) {
  if (!luaL_isudata(L, 1, "dlist"))
    luaL_error(L, "Error in " LUA_QS ": expected a dlist, got %s.", "dlist.checkdlist", luaL_typename(L, 1));
  return 0;
}


static const struct luaL_Reg dlist_dlistlib [] = {  /* metamethods for linked lists `n' */
  {"__index",      dlist_getitem},   /* n[p], with p the index, counting from 1 */
  {"__writeindex", dlist_setitem},   /* n[p] := value, with p the index, counting from 1 */
  {"__size",       dlist_getsize},   /* retrieve the number of entries in `n' */
  {"__tostring",   dlist_tostring},  /* for output at the console, e.g. print(n) */
  {"__in",         dlist_in},        /* `in` operator for linked lists */
  {"__notin",      dlist_notin},     /* `notin` operator for linked lists */
  {"__gc",         dlist_gc},        /* please do not forget garbage collection */
  {"__eq",         dlist_eq},        /* metamethod for `=` operator */
  {"__aeq",        dlist_aeq},       /* metamethod for `~=` operator */
  {"__empty",      dlist_empty},     /* metamethod for `empty` operator */
  {"__filled",     dlist_filled},    /* metamethod for `filled` operator */
  {NULL, NULL}
};

static const struct luaL_Reg dlist_dlistnodelib [] = {  /* metamethods for nodes */
  {"__gc",         dlist_nodegc},    /* needed for llist.iterate */
  {NULL, NULL}
};

static const luaL_Reg dlistlib[] = {
  {"append",       dlist_append},
  {"checkdlist",   dlist_checkdlist},
  {"dump",         dlist_dump},
  {"getitem",      dlist_getitem},
  {"getregistry",  dlist_getregistry},
  {"getsize",      dlist_getsize},
  {"iterate",      dlist_iterate},
  {"new",          dlist_new},
  {"prepend",      dlist_prepend},
  {"purge",        dlist_purge},
  {"put",          dlist_put},
  {"replicate",    dlist_replicate},
  {"setitem",      dlist_setitem},
  {"toseq",        dlist_toseq},
  {"totable",      dlist_totable},
  {NULL, NULL}
};


/*
** Open llist library
*/
LUALIB_API int luaopen_llist (lua_State *L) {
  /* metamethods for linked lists */
  luaL_newmetatable(L, "llist");
  luaL_register(L, NULL, llist_llistlib);
  /* metamethod for the nodes (for `llist.iterate`) */
  luaL_newmetatable(L, "llistnode");
  luaL_register(L, NULL, llist_llistnodelib);
  /* register library */
  luaL_register(L, AGENA_LLISTLIBNAME, llistlib);
  /* initialise ULIST package */
  luaL_newmetatable(L, "ulist");
  luaL_register(L, NULL, ulist_ulistlib);
  /* register library */
  luaL_register(L, AGENA_ULISTLIBNAME, ulistlib);
  /* see llist.agn file for insertion of `ulist' into Agena's registry _READLIBBED set so that
     after a restart, `ulist' can be initialised again */
  /* initialise DLIST package */
  luaL_newmetatable(L, "dlist");
  luaL_register(L, NULL, dlist_dlistlib);
  /* metamethod for the nodes (for `dlist.iterate`) */
  luaL_newmetatable(L, "dlistnode");
  luaL_register(L, NULL, dlist_dlistnodelib);
  /* register library */
  luaL_register(L, AGENA_DLISTLIBNAME, dlistlib);
  /* In all "DLL versions" of Agena, register additional package tables dlist and ulist so that they can be deleted from
     the environment when restarting Agena. Otherwise their _metatables_ could not be re-initialised again. In DOS and OS/2,
     leave them untouched since they cannot be recreated during restart as they have been defined in the C and not the
     Agena code. This is equal to:
     if not(os.isdos() or os.isos2() or os.isansi()) then
        insert 'dlist', 'ulist' into debug.getregistry()._READLIBBED
     end; */
#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))  /* 2.31.10 */
  agn_setreadlibbed(L, "dlist");
  agn_setreadlibbed(L, "ulist");
#endif
  return 1;
}

