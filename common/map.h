#ifndef _MAP_H_
#define _MAP_H_

#include "rbtree.h"

typedef RED_BLACK_TREE 	MAP;
typedef KEY_TYPE		MAP_KEY_TYPE;

#define new_map(cmp_fn)						new_rbtree(cmp_fn)

#define map_insert(p_map, key, value)		rbtree_insert(p_map, (MAP_KEY_TYPE)key, (void*)value)

#define map_erase(p_map, key)				rbtree_erase(p_map, (MAP_KEY_TYPE)key)

#define map_get(p_map, key, default_value)		({RED_BLACK_TREE_NODE* p_node = rbtree_search(p_map,(MAP_KEY_TYPE)key);p_node?p_node->data:(void*)default_value;})

#define map_get_origin_key(p_map, k)		(rbtree_search(p_map, (MAP_KEY_TYPE)k)->key)

#define map_is_key_in(p_map, key)			(rbtree_search(p_map, (MAP_KEY_TYPE)key)?1:0)

#define map_size(p_map)						(p_map->size)

#define delete_map(p_map)					delete_rbtree(p_map)

#define MAP_FOR_EACH_KEY_VALUE(p_map, key_type, k, value_type, v)		{	key_type k; \
																			value_type v; \
																			RED_BLACK_TREE_NODE* __p_cur_node = rbtree_midorder_begin(p_map); 	\
																			while(__p_cur_node)									\
																			{												 	\
																				k = (key_type)__p_cur_node->key;				\
																				v = (value_type)__p_cur_node->data;			\
																			
#define END_MAP_FOR_EACH(p_map)	__p_cur_node=rbtree_midorder_next(p_map,__p_cur_node);};}

#endif