#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdio.h>

struct ll_item {
	char *key;
	void *value;
	struct ll_item *next;
};

struct linked_list {
	int length;
	size_t value_size;
	struct ll_item *first;
};

/* Initialize `list` for use with items of size `value_size`. */
int ll_init(size_t value_size, struct linked_list *list);

/* Terminate `list` cleaning up its resourced. */
int ll_term(struct linked_list *list);

/* Put item with `key`, `value` pair into `list`. */
int ll_put(const char *key, const void *value, struct linked_list *list);

/* Get reference to the value stored with `key` in `list`. */
void *ll_getref(const char *key, const struct linked_list *list);

/* Get item with `key` from `list`, copy its value to `value`. */
int ll_get(const char *key, const struct linked_list *list, void *value);

/* Delete item with `key` from `list`. */
int ll_del(const char *key, struct linked_list *list);

/* Print all the keys in `list`. */
void ll_fprint(FILE *stream, const struct linked_list *list);

#endif /* LINKED_LIST_H */
