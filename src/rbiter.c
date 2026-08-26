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

// rbtree.
#include "rbiter.h"

// =========================================================================
//   CLASS     :
//   METHOD    : it_beg
// =========================================================================
void* it_beg (struct rbiter* it, struct rbtree* tree) {
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
  return NULL;
}

// =========================================================================
//   CLASS     :
//   METHOD    : it_end
// =========================================================================
void* it_end (struct rbiter* it, struct rbtree* tree) {
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
  return NULL;
}

// =========================================================================
//   CLASS     :
//   METHOD    : it_next
// =========================================================================
void* it_next (struct rbiter* it) {
  if (it != NULL) {
    if (it->node == NULL) {
      return it_beg (it, it->tree);
    } else if (it->node->link[1] == NULL) {
      struct rbnode *q, *p;
      for (p = it->node, q = p->parent; ; p = q, q = q->parent) {
        if (q == NULL || p == q->link[0]) {
          it->node = q;
          return it->node != NULL ? it->node->data : NULL;
        }
      }
    } else {
      it->node = it->node->link[1];
      while (it->node->link[0] != NULL) {
        it->node = it->node->link[0];
      }
      return it->node->data;
    }
  }
  return NULL;
}

// =========================================================================
//   CLASS     :
//   METHOD    : it_prev
// =========================================================================
void* it_prev (struct rbiter* it) {
  if (it != NULL) {
    if (it->node == NULL) {
      return it_end (it, it->tree);
    } else if (it->node->link[0] == NULL) {
      struct rbnode *q, *p;
      for (p = it->node, q = p->parent; ; p = q, q = q->parent) {
        if (q == NULL || p == q->link[1]) {
          it->node = q;
          return it->node != NULL ? it->node->data : NULL;
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
  return NULL;
}

// =========================================================================
//   CLASS     :
//   METHOD    : it_cur
// =========================================================================
void* it_cur (struct rbiter* it) {
  if (it != NULL) {
    return it->node != NULL ? it->node->data : NULL;
  }
  return NULL;
}
