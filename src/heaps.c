/* This library implements an AVL tree, i.e a height-balanced binary search tree in which the heights of a node’s two sub-trees
   are not allowed to differ by more than one.

   The underlying implementation has been taken from Martin Broadhurst's exemplary website
   http://www.martinbroadhurst.com/data-structures.html (defunct, unfortunately) */

#define heaps_c
#define LUA_LIB

#include <stdlib.h>

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"
#include "agnhlps.h"

#define checkavl(L, n)      (Avl *)luaL_checkudata(L, n, "avl")
#define isavl(L,n)          (luaL_isudata(L, n, "avl"))

/* `header` *************************************************************************************************/

struct avltreenode {
  struct avltreenode *left;
  struct avltreenode *right;
  struct avltreenode *parent;
  unsigned int leftheight;
  unsigned int rightheight;
  lua_Number key;
  int data;  /* the actual value is stored in the Agena registry, with `data' its reference */
  int padding;  /* 4 bytes - wasted space reclaim */
};

typedef struct avltreenode avltreenode;

typedef int  (*avltree_cmpfn)(lua_State *L, int idx1, int idx2);
typedef void (*avltree_forfn)(int);

struct avltree {
  avltreenode *root;
  size_t count;
  avltree_cmpfn compare;
  int type;  /* 0 = simple container, 1 = key~value AVL */
  int reserved;  /* 4 bytes - wasted space reclaim */
};

typedef struct avltree avltree;

/* `private` ********************************************************************************************* */

static int  avltree_remove (lua_State *L, int udx, avltree *tree, int idx);
static void avltree_remove_node (lua_State *L, avltree *tree, avltreenode *node);

static int avl_compare (lua_State *L, int idx1, int idx2) {
  if (lua_type(L, idx1) != lua_type(L, idx2)) return 2;
  if (lua_equal(L, idx1, idx2)) return 0;
  if (lua_lessthan(L, idx1, idx2)) return -1;
  return 1;
}

static int avl_comparekeys (lua_State *L, int idx1, int idx2) {
#if (LUAI_BITSINT == 32)
  /* See: https://stackoverflow.com/questions/14579920/fast-sign-of-integer-in-c */
  int x = idx1 - idx2;
  x |= x >> 1;
  return ((unsigned int)(-x) >> 31) - ((unsigned int)x >> 31);
#else
  if (idx1 == idx2) return 0;
  if (idx1 < idx2) return -1;
  return 1;
#endif
}

static avltree *avltree_create (int type) {
  avltree *tree = malloc(sizeof(avltree));
  if (tree != NULL) {
    tree->root = NULL;
    tree->compare = (type) ? avl_comparekeys : avl_compare;
    tree->count = 0;
    tree->type = type;
  }
  return tree;
}

static void avltreenode_delete (lua_State *L, avltreenode *node) {
  xfree(node);
}

static void avltree_empty_recursive (lua_State *L, avltreenode *root) {
  if (root->left)
    avltree_empty_recursive(L, root->left);
  if (root->right)
    avltree_empty_recursive(L, root->right);
  avltreenode_delete(L, root);
}

static void avltree_empty (lua_State *L, avltree *tree) {
  if (tree->root) {
    avltree_empty_recursive(L, tree->root);
    tree->root = NULL;
    tree->count = 0;
  }
}

static void avltree_delete (lua_State *L, avltree *tree) {
  if (tree) {
    avltree_empty(L, tree);
    xfree(tree);
  }
}

/*static void avltree_for_each_recursive (const avltreenode *root, avltree_forfn fun) {
  if (root->left != NULL)
    avltree_for_each_recursive(root->left, fun);
  fun(root->data);
  if (root->right != NULL)
    avltree_for_each_recursive(root->right, fun);
}

static void avltree_for_each (const avltree *tree, avltree_forfn fun) {
  if (tree->root)
    avltree_for_each_recursive(tree->root, fun);
}*/

/* Keep the signature as small as possible: recursion consumes a lot of admin memory and slows down the search
   significantly over time ! Declaring the function void speeds it up significantly. */
static void avltree_in_recursive (lua_State *L, int udx, const avltreenode *root, int *rc) {
  int rv;
  lua_getfenvi(L, udx, root->data);
  rv = avl_compare(L, -1, 1);
  agn_poptop(L);
  if (rv == 0) {
    *rc = root->key;
    return;
  }
  if (root->left != NULL) {
    avltree_in_recursive(L, udx, root->left, rc);
    if (*rc != INT_MIN) return;
  }
  if (root->right != NULL) {
    avltree_in_recursive(L, udx, root->right, rc);
    if (*rc != INT_MIN) return;
  }
  *rc = INT_MIN;
  return;
}

static void avltree_indices_recursive (lua_State *L, int idx, const avltreenode *root, int *key) {
  agn_setinumber(L, idx, (*key)++, root->key);
  if (root->left != NULL) {
    avltree_indices_recursive(L, idx, root->left, key);
  }
  if (root->right != NULL) {
    avltree_indices_recursive(L, idx, root->right, key);
  }
}

static void avltree_indices (lua_State *L, int idx, const avltree *tree) {
  int key = 1;
  if (tree->root) {
    avltree_indices_recursive(L, idx, tree->root, &key);
  }
}

static void avltree_remove_recursive (lua_State *L, int udx, int idx, avltree *tree, avltreenode *root) {  /* 4.4.4 */
  int r;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushvalue(L, idx);
  lua_getfenvi(L, udx, root->data);
  lua_call(L, 1, 1);
  r = lua_istrue(L, -1);  /* 5.0.1 fix */
  agn_poptop(L);
  if (r) {
    avltree_remove_node(L, tree, root);
    return;
  }
  if (root->left != NULL) {
    avltree_remove_recursive(L, udx, idx, tree, root->left);
  }
  if (root->right != NULL) {
    avltree_remove_recursive(L, udx, idx, tree, root->right);
  }
}

static void avltree_remove_functional (lua_State *L, int udx, int idx, avltree *tree) {  /* 4.4.4 */
  if (tree->root) {
    avltree_remove_recursive(L, udx, idx, tree, tree->root);
  }
}

struct avlsearchresult {
  avltreenode *node;
  avltreenode *parent;
};

typedef struct avlsearchresult avlsearchresult;

static int avltree_search (lua_State *L, int udx, const avltree *tree, avlsearchresult *result, int idx, int *height) {
  int found, type, rv;
  found = 0; *height = 0;
  result->node = tree->root;
  type = tree->type;
  if (type) idx = agn_tointeger(L, idx);
  while (!found && result->node != NULL) {
    if (type == 0) {
      lua_getfenvi(L, udx, result->node->data);
      rv = tree->compare(L, -1, idx);
      agn_poptop(L);
    } else {
      rv = tree->compare(L, result->node->key, idx);
    }
    (*height)++;
    if (rv == 0)
      found = 1;
    else {
      result->parent = result->node;
      if (rv > 0)
        result->node = result->node->left;
      else if (rv < 0)
        result->node = result->node->right;
    }
  }
  return found;
}

static avltreenode *avltreenode_create (lua_State *L, int udx, int idx, int type) {
  avltreenode *node = malloc(sizeof(avltreenode));
  if (node) {
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->leftheight = 0;
    node->rightheight = 0;
    node->key = (type) ? agn_tointeger(L, idx) : 0;
    lua_getfenv(L, udx);
    lua_pushvalue(L, idx + type);  /* value to be put into registry table */
    node->data = luaL_ref(L, -2);  /* store unique reference to value and pop value */
    agn_poptop(L);
  }
  return node;
}

static int avltreenode_get_max_height (const avltreenode *node) {
  return (node->leftheight > node->rightheight) ? node->leftheight : node->rightheight;
}

static void avltreenode_fix_height (avltreenode *node) {
  node->leftheight = 0;
  node->rightheight = 0;
  if (node->left)
    node->leftheight = avltreenode_get_max_height(node->left) + 1;
  if (node->right)
    node->rightheight = avltreenode_get_max_height(node->right) + 1;
}

static void avltree_rotate_left (avltree *tree, avltreenode *node) {
  avltreenode *right = node->right;
  if (node == tree->root)
    tree->root = right;
  else if (node == node->parent->left)
    node->parent->left = right;
  else
    node->parent->right = right;
  right->parent = node->parent;
  if (right->left) {
    node->right = right->left;
    node->right->parent = node;
  } else
    node->right = NULL;
  right->left = node;
  node->parent = right;
  avltreenode_fix_height(node);
  avltreenode_fix_height(right);
}

static void avltree_rotate_right (avltree *tree, avltreenode *node) {
  avltreenode *left = node->left;
  if (node == tree->root)
    tree->root = left;
  else if (node == node->parent->left)
    node->parent->left = left;
  else
    node->parent->right = left;
  left->parent = node->parent;
  if (left->right) {
    node->left = left->right;
    node->left->parent = node;
  } else
    node->left = NULL;
  left->right = node;
  node->parent = left;
  avltreenode_fix_height(node);
  avltreenode_fix_height(left);
}

static int avltreenode_get_balance_factor (const avltreenode *node) {
  return node->leftheight - node->rightheight;
}

static void avltree_rebalance (avltree *tree, avltreenode *node) {
  avltreenode *current = node;
  while (current != NULL) {
    avltreenode *parent = current->parent;
    int balance;
    avltreenode_fix_height(current);
    balance = avltreenode_get_balance_factor(current);
    if (balance == -2) {  /* right heavy */
      const int rightbalance = avltreenode_get_balance_factor(current->right);
      if (rightbalance < 0)
        avltree_rotate_left(tree, current);
      else {
        avltree_rotate_right(tree, current->right);
        avltree_rotate_left(tree, current);
      }
    } else if (balance == 2) {  /* left heavy */
      const int leftbalance = avltreenode_get_balance_factor(current->left);
      if (leftbalance > 0)
        avltree_rotate_right(tree, current);
      else {
        avltree_rotate_left(tree, current->left);
        avltree_rotate_right(tree, current);
      }
    }
    current = parent;
  }
}

static void avltree_add (lua_State *L, int udx, avltree *tree, int idx) {
  int height;
  avlsearchresult result;
  result.node = NULL;
  result.parent = NULL;
  (void)height;
  if (avltree_search(L, udx, tree, &result, idx, &height)) {  /* overwrite value at index idx */
    lua_getfenv(L, udx);
    luaL_unref(L, -1, result.node->data);
    lua_pushvalue(L, idx + tree->type);  /* value to be put into registry table */
    result.node->data = luaL_ref(L, -2);  /* store unique reference to value and pop value */
    agn_poptop(L);
  } else {
    int rv;
    avltreenode *node = avltreenode_create(L, udx, idx, tree->type);
    if (result.node == tree->root)
      tree->root = node;
    else {
      if (tree->type == 0) {
        lua_getfenvi(L, udx, result.parent->data);
        rv = tree->compare(L, idx, -1);
        agn_poptop(L);
      } else {
        rv = tree->compare(L, agn_tointeger(L, idx), result.parent->key);
      }
      if (rv < 0)
        result.parent->left = node;
      else
        result.parent->right = node;
      node->parent = result.parent;
      avltree_rebalance(tree, node);
    }
    tree->count++;
  }
}

static int avltree_find (lua_State *L, int udx, const avltree *tree, int idx, int *height) {
  avlsearchresult result;
  result.node = NULL;
  result.parent = NULL;
  if (avltree_search(L, udx, tree, &result, idx, height))
    return result.node->data;
  return LUA_NOREF;
}

static avltreenode *avltreenode_find_min (avltreenode *node) {
  avltreenode *current = node;
  while (current->left) {
    current = current->left;
  }
  return current;
}

static avltreenode *avltreenode_find_max (avltreenode *node) {
  avltreenode *current = node;
  while (current->right) {
    current = current->right;
  }
  return current;
}

static void avltree_remove_node (lua_State *L, avltree *tree, avltreenode *node) {
  if (node->left && node->right) {  /* node with 2 children */
    avltreenode *successor = avltreenode_find_min(node->right);
    node->key = successor->key;
    node->data = successor->data;
    avltree_remove_node(L, tree, successor);
  } else {
    avltreenode *parent = node->parent;
    if (node->left) {  /* node with only left child */
      if (node->parent) {
        if (node == node->parent->left) {
          node->parent->left = node->left;
          node->parent->left->parent = node->parent;
        } else {
          node->parent->right = node->left;
          node->parent->right->parent = node->parent;
        }
      } else {
        tree->root = node->left;
        tree->root->parent = NULL;
      }
    } else if (node->right) {  /* node with only right child */
      if (node->parent) {
        if (node == node->parent->left) {
          node->parent->left = node->right;
          node->parent->left->parent = node->parent;
        } else {
          node->parent->right = node->right;
          node->parent->right->parent = node->parent;
        }
      } else {
        tree->root = node->right;
        tree->root->parent = NULL;
      }
    } else {  /* node with no children */
      if (node->parent) {
        if (node == node->parent->left)
          node->parent->left = NULL;
        else
          node->parent->right = NULL;
      } else
        tree->root = NULL;
    }
    avltreenode_delete(L, node);
    avltree_rebalance(tree, parent);
    tree->count--;
  }
}

static int avltree_remove (lua_State *L, int udx, avltree *tree, int idx) {
  int height, rc;
  avlsearchresult result;
  result.node = NULL;
  result.parent = NULL;
  (void)height;
  rc = avltree_search(L, udx, tree, &result, idx, &height);
  if (rc) {
    /* We must delete the value at this point, not during execution of the subsequent call to avltree_remove_node !
       Also note that the next statement just deletes the value and replaces it with something else. This is normal
       and correct behaviour of luaL_unref, see:
       https://stackoverflow.com/questions/27460543/how-can-i-clean-up-luas-registry */
    luaL_checkstack(L, 2 + tree->type, "not enough stack space");
    if (tree->type)
      lua_pushinteger(L, result.node->key);
    lua_getfenvi(L, udx, result.node->data);
    lua_getfenv(L, 1);
    luaL_unref(L, -1, result.node->data);
    agn_poptop(L);
    avltree_remove_node(L, tree, result.node);
  }
  return rc;
}

static size_t avltree_get_count (const avltree *tree) {
  return tree->count;
}

/* Agena Library Functions ******************************************************************************* */

typedef struct {
  avltree *tree;
  int registry;  /* registry, for attribute information, stored to index 0; 2.15.1 */
  int ttype;     /* AVL trees as implemented here cannot store values of different types */
} Avl;


/* The function creates an empty AVL tree and returns it. */
static int avl_new (lua_State *L) {
  Avl *a;
  int extended = agnL_optboolean(L, 1, 1);  /* = a->tree->type */
  /* a->tree->type = 1: by default, we will use an AVL tree storing integer indices (negative, zero or positive) along
        with any values;
     a->tree->type = 0: this is an AVL tree just storing values, but no indices. In this case, a tree can only include
        values of the same type. This is roughly comparable to an UltraSet, but with only 33 % of UltraSet performance. */
  a = (Avl *)lua_newuserdata(L, sizeof(Avl));
  if (!a)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "avl.new");
  lua_setmetatabletoobject(L, -1, "avl", 0);
  agn_setutypestring(L, -1, "avl");
  lua_createtable(L, 0, 0);  /* 5.4.4 change, don't crowd the registry, use the separate environment table */
  lua_setfenv(L, -2);
  lua_createtable(L, 0, 0);
  a->registry = luaL_ref(L, LUA_REGISTRYINDEX);
  a->tree = avltree_create(extended);
  a->ttype = LUA_TNONE;
  return 1;
}


/* Returns the smallest key along with its associated value from AVL tree h. The function does not remove the data
   from h. */
static int avl_getmin (lua_State *L) {
  int type;
  Avl *a = checkavl(L, 1);
  avltreenode *node, *min;
  node = a->tree->root;
  if (node == NULL) {
    lua_pushnil(L);
    return 1;
  }
  min = avltreenode_find_min(node);
  type = a->tree->type;
  if (type)
    lua_pushinteger(L, min->key);
  lua_getfenvi(L, 1, min->data);  /* 5.4.4 change */
  return 1 + type;
}


/* Returns the largest key along with its associated value from AVL tree h. The function does not remove the data
   from h. */
static int avl_getmax (lua_State *L) {
  int type;
  Avl *a = checkavl(L, 1);
  avltreenode *node, *max;
  node = a->tree->root;
  if (node == NULL) {
    lua_pushnil(L);
    return 1;
  }
  max = avltreenode_find_max(node);
  type = a->tree->type;
  if (type)
    lua_pushinteger(L, max->key);
  lua_getfenvi(L, 1, max->data);
  return 1 + type;
}


/* Returns the smallest and largest keys along with their associated values from AVL tree h. The function does not remove the data
   from h. */
static int avl_getminmax (lua_State *L) {
  int type;
  Avl *a = checkavl(L, 1);
  avltreenode *node, *min, *max;
  node = a->tree->root;
  if (node == NULL) {
    lua_pushnil(L);
    return 1;
  }
  min = avltreenode_find_min(node);
  max = avltreenode_find_max(node);
  type = a->tree->type;
  if (type) {
    lua_pushinteger(L, min->key);
    lua_pushinteger(L, max->key);
  }
  lua_getfenvi(L, 1, min->data);
  lua_getfenvi(L, 1, max->data);
  return 2 + 2*type;
}


/* From the root node, returns the key along with its associated value from AVL tree h. The function does not remove the data
   from h. */
static int avl_getroot (lua_State *L) {
  int type;
  Avl *a = checkavl(L, 1);
  avltreenode *node;
  node = a->tree->root;
  if (node == NULL) {
    lua_pushnil(L);
    return 1;
  }
  type = a->tree->type;
  luaL_checkstack(L, 1 + type, "not enough stack space");
  if (type)
    lua_pushinteger(L, node->key);
  lua_getfenvi(L, 1, node->data);
  return 1 + type;
}


#define checkavltype(L,a,procname) { \
  nargs = lua_gettop(L); \
  type = a->tree->type; \
  if (type == 0 && nargs != 2) \
    luaL_error(L, "Error in " LUA_QS ": need two arguments, got %d.", procname, nargs); \
  else if (type == 1 && nargs < 2) \
    luaL_error(L, "Error in " LUA_QS ": need two or three arguments, got %d.", procname, nargs); \
}

#define checkavlemptyorwrongtype(L,a,procname) { \
  int type = a->tree->type; \
  if (LUA_TNONE == a->ttype || avltree_get_count(a->tree) == 0) { \
    lua_pushnil(L); \
    return 1; \
  } \
  if (type == 0 && lua_type(L, 2) != a->ttype) \
    luaL_error(L, "Error in " LUA_QS ": this AVL tree contains %ss only, got %s.", procname, \
      lua_typename(L, a->ttype), lua_typename(L, lua_type(L, 2))); \
  if (type == 1 && !agn_isinteger(L, 2)) \
    luaL_error(L, "Error in " LUA_QS ": key is not an integer, got %s.", procname, lua_typename(L, lua_type(L, 2))); \
}

/* If just given AVL tree h, removes the key~value pair with the lowest key and returns the key and the value removed. If the optional
   key k is given, removes the matching key~value pair from h and returns the key and the value removed. If the key does not exist,
   the function just returns `null`. (First define avl_remove for it is used in avl_include later on.) */
static int avl_remove (lua_State *L) {
  Avl *a;
  int rc = 0;
  if (isavl(L, 1)) {
    a = lua_touserdata(L, 1);
    if (agn_freeze(L, 1, -1))
      luaL_error(L, "Error in " LUA_QS ": list is read-only.", "avl.remove");  /* 5.2.0 extension */
    if (lua_gettop(L) == 1) {
      lua_settop(L, 1);
      avl_getmin(L);  /* get key and value of minimum node */
      agn_poptop(L);  /* pop value */
      lua_settop(L, 2);
    }
    luaL_checkany(L, 2);
    checkavlemptyorwrongtype(L, a, "avl.remove");
    if (avltree_remove(L, 1, a->tree, 2) == 0) {
      lua_pushnil(L);
    }
    rc = 1 + a->tree->type;
  } else if (lua_isfunction(L, 1)) {  /* 4.4.4 */
    int c;
    avltree *tree;
    a = checkavl(L, 2);
    if (agn_freeze(L, 1, -1))
      luaL_error(L, "Error in " LUA_QS ": list is read-only.", "avl.remove");  /* 5.2.0 extension */
    tree = a->tree;
    c = tree->count;
    while (c--) {
      avltree_remove_functional(L, 2, 1, tree);
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": wrong kind of arguments.", "avl.remove");
  }
  return rc;
}


/* Inserts a new key~value pair into the binary heap h. The function returns nothing. */
static int avl_include (lua_State *L) {
  int typeofvalue, type, nargs;
  Avl *a = checkavl(L, 1);
  checkavltype(L, a, "avl.include");
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": list is read-only.", "avl.include");  /* 5.2.0 extension */
  typeofvalue = lua_type(L, 2 + type);
  if (a->ttype == LUA_TNONE) a->ttype = typeofvalue;
  if (type == 0 && typeofvalue != a->ttype)
    luaL_error(L, "Error in " LUA_QS ": this AVL tree can store %ss only.", "avl.include", lua_typename(L, a->ttype));
  if (type == 1) {
    if (!agn_isinteger(L, 2))
      luaL_error(L, "Error in " LUA_QS ": key is not an integer, got %s.", "avl.include", lua_typename(L, lua_type(L, 2)));
    if (nargs == 2) {  /* 4.2.6 extension for two-argument mode */
      int nextkey;
      avltreenode *node, *max;
      if (lua_isnil(L, 2))
        luaL_error(L, "Error in " LUA_QS ": value must be non-null.", "avl.include");
      node = a->tree->root;
      if (node == NULL) {
        nextkey = 1;
      } else {
        max = avltreenode_find_max(node);
        nextkey = max->key + 1;
      }
      lua_settop(L, 3);  /* first increase top, then replace */
      lua_pushvalue(L, 2);
      lua_replace(L, 3);
      lua_pushinteger(L, nextkey);
      lua_replace(L, 2);
    }
    if (nargs == 3 && lua_type(L, 3) == LUA_TNIL) {
      lua_settop(L, 2);
      avl_remove(L);
      return 0;
    }
  }
  avltree_add(L, 1, a->tree, 2);
  return 0;
}


/* The function returns all indices in the AVL tree h in a new table. */
static int avl_indices (lua_State *L) {
  Avl *a = checkavl(L, 1);
  int length;
  length = avltree_get_count(a->tree);
  if (a->tree->type == 0)
    luaL_error(L, "Error in " LUA_QS ": wrong AVL tree type.", "avl.indices");
  lua_createtable(L, length, length);
  avltree_indices(L, -1, a->tree);
  luaL_checkstack(L, 3, "not enough stack space");  /* 4.11.0 fix */
  lua_getglobal(L, "sort");
  if (!lua_isfunction(L, -1))
    luaL_error(L, "Error in " LUA_QS ": could not fetch internal sorting function.", "avl.indices");
  lua_pushvalue(L, -2);
  lua_pushstring(L, "number");  /* 3.10.2 tune-up */
  lua_call(L, 2, 0);
  return 1;
}


/* The function returns all entries in the AVL tree h in a new table. */
static int avl_entries (lua_State *L) {
  Avl *a = checkavl(L, 1);
  int i, idx, length, height;
  length = avltree_get_count(a->tree);
  if (a->tree->type == 0)
    luaL_error(L, "Error in " LUA_QS ": wrong AVL tree type.", "avl.entries");
  lua_createtable(L, length, length);
  avl_indices(L);  /* get sorted table of all indices in tree */
  if (!lua_istable(L, -1))
    luaL_error(L, "Error in " LUA_QS ": data is corrupt.", "avl.entries");
  length = lua_objlen(L, -1);
  for (i=0; i < length; i++) {  /* traverse all indices and search for corresponding entries */
    lua_rawgeti(L, -1, i + 1);  /* push index */
    idx = avltree_find(L, 1, a->tree, -1, &height);  /* try to find entry */
    (void)height;
    if (idx == LUA_NOREF) {
      lua_pushnil(L);
    } else {
      lua_getfenvi(L, 1, idx);
    }
    lua_remove(L, -2);  /* remove index pushed before */
    lua_rawseti(L, -3, i + 1);  /* insert into resulting table and pop entry */
  }
  agn_poptop(L);
  return 1;
}


static int avl_get (lua_State *L) {
  Avl *a = checkavl(L, 1);
  int idx, height;
  (void)height;
  avltree_get_count(a->tree);
  if (a->tree->type == 0)
    luaL_error(L, "Error in " LUA_QS ": wrong AVL tree type.", "avl.get");
  if (agn_isinteger(L, 2)) {
    idx = avltree_find(L, 1, a->tree, 2, &height);  /* try to find entry for index at stack index 2 */
    if (idx == LUA_NOREF) {
      lua_pushnil(L);
    } else {
      lua_getfenvi(L, 1, idx);
    }
    return 1;
  } else {  /* 4.3.2 call OOP method, 4.4.1 change */
    return agn_initmethodcall(L, "avl", 3);
  }
}


static int avl_attrib (lua_State *L) {
  Avl *a = checkavl(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.4.0 */
  lua_createtable(L, 0, 3);
  avltreenode *node = a->tree->root;
  lua_rawsetstringinteger(L, -1, "maxheight", (node) ? avltreenode_get_max_height(node) + 1 : 0);
  lua_rawsetstringinteger(L, -1, "length", a->tree->count);
  lua_rawsetstringinteger(L, -1, "balancefactor", (node) ? avltreenode_get_balance_factor(node) : 0);
  return 1;
}


/* Retrieves the internal status table of an AVL tree and returns it. You can put any data into the
   status table, like this:

   > import heaps;

   > h := avl.new();

   > st := avl.getstore(h):
   []

   > st[1], st[2] := 10, 20; insert 30 into st;

   > st := avl.getstore(h):
   [10, 20, 30]

   A call to the AVL tree also returns the internal status table, the same way `avl.getstore` does:

   > st := h():
   [10, 20, 30]

   To retrieve the second value in the status table, issue:

   > h(2):
   20

   To set a value into the table, do something like this:

   > h(4, 40):

   > h():
   [10, 20, 30, 40] */

static int avl_getstore (lua_State *L) {  /* 5.4.0 */
  Avl *a = checkavl(L, 1);
  return agnL_calludata(L, lua_gettop(L), a->registry, "avl");
}


/* Metamethods ******************************************************************************************* */

static int mt_size (lua_State *L) {
  Avl *a = checkavl(L, 1);
  lua_pushnumber(L, avltree_get_count(a->tree));
  return 1;
}

#define call_avl_in(L,udx,a,rc) { \
  if (!a->tree->root) { \
    rc = INT_MIN; \
  } else { \
    avltree_in_recursive(L, udx, a->tree->root, &rc); \
  } \
}

static int mt_in (lua_State *L) {
  int height;
  Avl *a = checkavl(L, 2);
  if (a->tree->type) {
    int rc;
    call_avl_in(L, 2, a, rc);
    if (rc != INT_MIN)
      lua_pushinteger(L, rc);
    else
      lua_pushnil(L);
  } else
    lua_pushboolean(L, avltree_find(L, 2, a->tree, 1, &height) != LUA_NOREF);
  return 1;
}

static int mt_notin (lua_State *L) {
  int height;
  Avl *a = checkavl(L, 2);
  if (a->tree->type) {
    int rc;
    call_avl_in(L, 2, a, rc);
    lua_pushboolean(L, rc == INT_MIN);
  } else
    lua_pushboolean(L, avltree_find(L, 2, a->tree, 1, &height) == LUA_NOREF);
  return 1;
}

static int mt_empty (lua_State *L) {
  Avl *a = checkavl(L, 1);
  lua_pushboolean(L, avltree_get_count(a->tree) == 0);
  return 1;
}

static int mt_filled (lua_State *L) {
  Avl *a = checkavl(L, 1);
  lua_pushboolean(L, avltree_get_count(a->tree) != 0);
  return 1;
}

static int mt_writeindex (lua_State *L) {
  lua_settop(L, 3);
  avl_include(L);
  return 0;
}

static int mt_gc (lua_State *L) {
  Avl *a;
  lua_lock(L);
  a = checkavl(L, 1);
  avltree_delete(L, a->tree);
  lua_pushnil(L);  /* destroy environment table */
  lua_setfenv(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, a->registry);  /* delete registry table */
  lua_setmetatabletoobject(L, 1, NULL, 1);
  lua_unlock(L);
  return 0;
}

static int mt_tostring (lua_State *L) {  /* at the console, the avl tree is formatted as follows: */
  if (luaL_isudata(L, 1, "avl"))  /* 2.15.1 change */
    lua_pushfstring(L, "avl(%p)", lua_topointer(L, 1));
  else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}


static const struct luaL_Reg avl_heapslib [] = {  /* metamethods for AVL trees `n' */
  {"attrib",       avl_attrib},
  {"entries",      avl_entries},
  {"get",          avl_get},
  {"getstore",     avl_getstore},
  {"include",      avl_include},
  {"indices",      avl_indices},
  {"getmax",       avl_getmax},
  {"getmin",       avl_getmin},
  {"getminmax",    avl_getminmax},
  {"getroot",      avl_getroot},
  {"remove",       avl_remove},
  {"__index",      avl_get},        /* n[p] read access, with p the index, counting from 1, or initiate OOP method call */
  {"__writeindex", mt_writeindex},  /* n[p] := value, with p the index, counting from 1 */
  {"__gc",         mt_gc},          /* please do not forget garbage collection */
  {"__in",         mt_in},          /* `in` operator for AVL trees */
  {"__notin",      mt_notin},       /* `notin` operator for AVL trees */
  {"__size",       mt_size},        /* retrieve the number of entries in `n' */
  {"__empty",      mt_empty},       /* metamethod for `empty` operator */
  {"__filled",     mt_filled},      /* metamethod for `filled` operator */
  {"__tostring",   mt_tostring},    /* for output at the console, e.g. print(n) */
  {"__call",       avl_getstore},   /* to retrieve the internal store of an AVL heap */
  {NULL, NULL}
};

static const luaL_Reg avllib[] = {
  {"attrib",       avl_attrib},
  {"entries",      avl_entries},
  {"get",          avl_get},
  {"getstore",     avl_getstore},
  {"include",      avl_include},
  {"indices",      avl_indices},
  {"getmax",       avl_getmax},
  {"getmin",       avl_getmin},
  {"getminmax",    avl_getminmax},
  {"getroot",      avl_getroot},
  {"new",          avl_new},
  {"remove",       avl_remove},
  {NULL, NULL}
};


/*
** Open heaps/avl library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, "avl");     /* create metatable for file handles */
  lua_pushvalue(L, -1);            /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, avl_heapslib);  /* avl methods */
}

LUALIB_API int luaopen_heaps (lua_State *L) {  /* we use a different opening function in DOS & OS/2 */
  /* register library */
  luaL_register(L, AGENA_HEAPSLIBNAME, avllib);
  /* metamethods for AVL trees */
  createmeta(L);
  /* In all "DLL versions" of Agena, register additional package tables avl, skew and binary so that they can be deleted from
     the environment when restarting Agena. Otherwise their _metatables_ could not be re-initialised again. In DOS and OS/2,
     leave them untouched since they cannot be recreated during restart as they have been defined in the C and not the
     Agena code. This is equal to:
     if not(os.isdos() or os.isos2() or os.isansi()) then
        insert 'avl', 'skew', 'binary' into debug.getregistry()._READLIBBED
     end; */
#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))  /* 2.31.10 */
  agn_setreadlibbed(L, "avl");
  agn_setreadlibbed(L, "skew");
  agn_setreadlibbed(L, "binary");
#endif
  return 1;
}

