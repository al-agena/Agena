/**
 * MIT License
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
 * Note: Red-black trees store an element only once, all duplicates are ignored
 * when trying to insert them into the tree.
 */

#ifndef _RBTREE_H_
#define _RBTREE_H_

#include <stdbool.h>
#include <stddef.h>


/* comparison function pointer */
typedef int rb_compare (double left, double right);

/* destroy function pointer */
typedef void rb_destroy (void *data);

/**
 * @brief tree node.
 */
struct rbnode {
  struct rbnode *link[2];     /* subtrees */
  struct rbnode *parent;      /* parent */
  double data;                /* the data */
  int    red;                 /* color: red 1, black 0 */
  int    padding;             /* 4 bytes padding */
};

/**
 * @brief tree.
 */
struct rbtree {
  rb_compare    *comp;        /* compare nodes */
  rb_destroy    *del;         /* delete nodes data */
  struct rbnode *root;        /* root node */
  size_t         count;       /* number of nodes */
};

/**
 * @brief create the tree context.
 * @param compare comparison function.
 * @param destroy delete function (optional).
 * @return tree context.
 */
struct rbtree *rb_create (rb_compare *compare, rb_destroy *destroy);

/**
 * @brief delete tree.
 * @param tree tree context.
 * @note if a destroy function was provided on a prior call to rb_create all elements are deleted.
 */
void rb_delete (struct rbtree *tree);

/**
 * @brief inserts elements in the tree.
 * @param data element to insert.
 * @param tree tree context.
 * @return element inserted.
 */
double rb_insert (struct rbtree *tree, double data);

/**
 * @brief finds element in the tree.
 * @param data element to find.
 * @param tree tree context.
 * @returns height of element found; if not found, returns 0.
 */
int rb_find (struct rbtree *tree, double data);

/**
 * @brief remove element from the tree.
 * @param data element to remove.
 * @param tree tree context.
 * @note if a destroy function was provided on a prior call to rb_create element is deleted.
 * @returns 1 if the element has been found and could be deleted, and 0 otherwise.
 */
int rb_remove (struct rbtree *tree, double data);

/**
 * @brief checks whether the tree is empty.
 * @param tree tree context.
 * @return true if the tree is empty, false otherwise.
 */
int rb_empty (struct rbtree *tree);

/**
 * @brief returns the number of elements in the tree.
 * @param tree tree context.
 * @return the number of elements in the tree.
 */
size_t rb_size (struct rbtree *tree);

/**
 * @brief iterator.
 */
struct rbiter {
  struct rbnode *node;  /* current node */
  struct rbtree *tree;  /* tree */
};

/**
 * @brief move iterator on first element of the tree.
 * @param it iterator.
 * @param tree tree context.
 * @return first element of the tree, NULL if empty.
 */
double it_begin (struct rbtree *tree, struct rbiter *it);

/**
 * @brief move iterator on last element of the tree.
 * @param it iterator.
 * @param tree tree context.
 * @return last element of the tree, NULL if empty.
 */
double it_end (struct rbtree *tree, struct rbiter *it);

/**
 * @brief move iterator to the next element of the tree.
 * @param it iterator.
 * @return 1 = we have a next element, 0 = no elements left.
 */
int it_next (struct rbiter *it, double *value);

/**
 * @brief move iterator to the previous element of the tree.
 * @param it iterator.
 * @return previous element of the tree.
 */
double it_prev (struct rbiter *it);

/**
 * @brief get current element of the tree pointed by iterator.
 * @param it iterator.
 * @return current element of the tree.
 */
double it_cur (struct rbiter *it);

#endif

