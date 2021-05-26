/*
 * N-ary tree implementation
 * based on this post - https://stackoverflow.com/a/29122886
 * 
 * Represents tree this way:

 Root -> NULL
 |
 V
Child-1.1 -> Child-1.2 -> ... -> Child-1.n -> NULL
 |              |                   |            
 |              V                   V
 |              ...              Child-1.n.1 -> ... -> NULL
 V
Child-1.1.1 -> Child-1.1.2 -> ... -> NULL
 |
 ... etc

*/

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct node {
    char *data;
    struct node *child;   // point to children of this node
    struct node *next;    // point to next node at same level
};

typedef struct node node;

node *new_node(char *data);
node *add_sibling(node *n, char *data);
node *add_child(node *n, char *data);
node *get_child(node *n, char *data);
node *get_node(node *root, char *path);
