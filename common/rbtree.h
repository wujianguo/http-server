#ifndef _RBTREE_H_
#define _RBTREE_H_


typedef enum _boolean
{
    FALSE = 0, 
    TRUE = 1
}BOOL;

typedef enum _color
{
	RED = 0,
	BLACK = 1
}COLOR;

typedef void* KEY_TYPE;
typedef int (*COMPARE_FUNC)(KEY_TYPE a, KEY_TYPE b);
typedef void (*DATA_TO_STRING_FUNC)(void* data, char* str);
typedef void* (*STRING_TO_DATA_FUNC)(char* str);

typedef struct _tagRedBlackTreeNode
{
	KEY_TYPE key;
	void* data;
	COLOR color;
	struct _tagRedBlackTreeNode *left, *right, *parent;
}RED_BLACK_TREE_NODE;

typedef struct _tagRedBlackTree
{
	COMPARE_FUNC cmp_fn; 
	RED_BLACK_TREE_NODE *root;
	unsigned int size;
}RED_BLACK_TREE;

RED_BLACK_TREE* new_rbtree(COMPARE_FUNC cmp_fn);
void delete_rbtree(RED_BLACK_TREE* p_rbtree);

RED_BLACK_TREE_NODE* rbtree_search(RED_BLACK_TREE* p_rbtree, KEY_TYPE key);

RED_BLACK_TREE_NODE* rbtree_insert(RED_BLACK_TREE* p_rbtree, KEY_TYPE key, void* data);
void rbtree_erase(RED_BLACK_TREE* p_rbtree, KEY_TYPE key);

void dump_rbtree_to_file(RED_BLACK_TREE* p_rbtree, const char* file_path, DATA_TO_STRING_FUNC to_string);
RED_BLACK_TREE* load_rbtree_from_file(const char* file_path, STRING_TO_DATA_FUNC to_data);

RED_BLACK_TREE_NODE* rbtree_midorder_begin(RED_BLACK_TREE* p_rbtree);
RED_BLACK_TREE_NODE* rbtree_midorder_pre(RED_BLACK_TREE* p_rbtree, RED_BLACK_TREE_NODE* p_node);
RED_BLACK_TREE_NODE* rbtree_midorder_next(RED_BLACK_TREE* p_rbtree, RED_BLACK_TREE_NODE* p_node);
// int 				 rbtree_midorder_end(RED_BLACK_TREE* p_rbtree, RED_BLACK_TREE_NODE* p_node);

#endif