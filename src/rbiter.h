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
 */

#ifndef _ITERATOR_H_
#define _ITERATOR_H_

// rbtree.
#include "rbtree.h"

/**
 * @brief iterator.
 */
struct rbiter {
  struct rbnode *node;        // current node.
  struct rbtree *tree;        // tree.
};

/**
 * @brief move iterator on first element of the tree.
 * @param it iterator.
 * @param tree tree context.
 * @return first element of the tree, NULL if empty.
 */
void* it_beg  (struct rbiter *it, struct rbtree *tree);

/**
 * @brief move iterator on last element of the tree.
 * @param it iterator.
 * @param tree tree context.
 * @return last element of the tree, NULL if empty.
 */
void* it_end  (struct rbiter *it, struct rbtree *tree);

/**
 * @brief move iterator to the next element of the tree.
 * @param it iterator.
 * @return next element of the tree.
 */
void* it_next (struct rbiter *it);

/**
 * @brief move iterator to the previous element of the tree.
 * @param it iterator.
 * @return previous element of the tree.
 */
void* it_prev (struct rbiter *it);

/**
 * @brief get current element of the tree pointed by iterator.
 * @param it iterator.
 * @return current element of the tree.
 */
void* it_cur  (struct rbiter *it);

#endif
