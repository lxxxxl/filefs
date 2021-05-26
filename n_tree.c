#include "n_tree.h"

node *new_node(char *data) {
    node *new_node = malloc(sizeof(node));

    if (new_node) {
        new_node->next = NULL;
        new_node->child = NULL;
        new_node->data = strdup(data);
    }

    return new_node;
}

node *add_sibling(node *n, char *data) {
    if (n == NULL)
        return NULL;

    while (n->next)
        n = n->next;

    return (n->next = new_node(data));
}

node *add_child(node *n, char *data) {
    if (n == NULL)
        return NULL;

    if (n->child)
        return add_sibling(n->child, data);
    else
        return (n->child = new_node(data));
}

node *get_child(node *n, char *data) {
	if (n == NULL || n->child == NULL)
		return NULL;

	n = n->child;
	while (n) {
		if (strcmp(n->data, data) == 0)
			return n;
		n = n->next;
	}

	return NULL;
}

node *get_node(node *root, char *path) {
	char *pch, *saveptr;
	node *current_node = root;
	pch = strtok_r(path,"/", &saveptr);
	while (pch != NULL) {
		current_node = get_child(current_node, pch);
		if (!current_node)
			return NULL;
		pch = strtok_r(NULL, "/", &saveptr);
	}
	return current_node;
}
