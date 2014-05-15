#ifndef _SET_H_
#define _SET_H_

#include "rbtree.h"

typedef RED_BLACK_TREE 	SET;
typedef KEY_TYPE		SET_KEY_TYPE;

#define new_set(cmp_fn)						new_rbtree(cmp_fn)

#define set_add(p_set, key)					rbtree_insert(p_set, (SET_KEY_TYPE)key, (void*)0)

#define set_erase(p_set, key)				rbtree_erase(p_set, (SET_KEY_TYPE)key)

#define set_is_in(p_set, key)				(rbtree_search(p_set,(SET_KEY_TYPE)key)?1:0)

#define set_get_origin_key(p_set, k)			(rbtree_search(p_set, (SET_KEY_TYPE)k)->key)

#define set_size(p_set)						(p_set->size)

#define delete_set(p_set)					delete_rbtree(p_set)

#define SET_FOR_EACH(p_set, key_type, k)		{	key_type k; \
													RED_BLACK_TREE_NODE* __p_cur_node = rbtree_midorder_begin(p_set); 	\
													while(__p_cur_node)									\
													{												 	\
														k = (key_type)__p_cur_node->key;				\

																			
#define END_SET_FOR_EACH(p_set)	__p_cur_node=rbtree_midorder_next(p_set,__p_cur_node);};}


#endif