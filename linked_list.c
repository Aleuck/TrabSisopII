#include <string.h> /* memset */
#include <stdlib.h>

#include "linked_list.h"

#include "logging.h"

static void die(const char *msg) {
	flogerror("%s", msg);
	exit(1);
}

int ll_init(size_t value_size, struct linked_list *list) {
	list->length = 0;
	list->value_size = value_size;
	list->first = NULL;
}

int ll_put(const char *key, const void *value, struct linked_list *list) {
	struct ll_item *new = (struct ll_item *)malloc(sizeof(struct ll_item));
	if (new == NULL) goto malloc_err1;

	new->key = (char *)malloc(strlen(key) + 1);
	if (new->key == NULL) goto malloc_err2;
	memcpy(new->key, key, strlen(key) + 1);

	new->value = malloc(list->value_size);
	if (new->value == NULL) goto malloc_err3;
	memcpy(new->value, value, list->value_size);

	new->next = NULL;

	if (list->first == NULL) {
		list->first = new;
	} else {
		struct ll_item *parent = list->first;
		while (parent->next != NULL) {
			parent = parent->next;
		}
		parent->next = new;
	}

	list->length++;

	return 0;

malloc_err3:
	free(new->key);
malloc_err2:
	free(new);
malloc_err1:
	die("ll_put(): malloc failed.");
}

/* Find item previous to the one with a given `key`. Put its reference in
 * `ret`.
 *
 * Return 1 if the first one has key equal to `key`, so there's no previous.
 * Return -1 if there's no item with key equal to`key`.
 * Return 0 on success. */
int find_prev(const char *key, const struct linked_list *list, struct ll_item **ret) {
	struct ll_item *item = list->first;

	if (item == NULL) {
		return -1;
	}

	if (strcmp(item->key, key) == 0) {
		*ret = NULL;
		return 1;
	}

	while (item->next != NULL) {
		if (strcmp(item->next->key, key) == 0) {
			*ret = item;
			return 0;
		}
		item = item->next;
	}

	return -1;
}

struct ll_item *find(const char *key, const struct linked_list *list) {
	struct ll_item *item = list->first;

	while (item != NULL) {
		if (strcmp(item->key, key) == 0) {
			return item;
		}
		item = item->next;
	}

	return NULL;
}

void *ll_getref(const char *key, const struct linked_list *list) {
	struct ll_item *item = find(key, list);

	if (item == NULL) {
		return NULL;
	} else {
		return item->value;
	}
}

int ll_get(const char *key, const struct linked_list *list, void *value) {
	struct ll_item *item = find(key, list);

	if (item == NULL) {
		return -1;
	} else {
		memcpy(value, item->value, list->value_size);
		return 0;
	}
}

int ll_del(const char *key, struct linked_list *list) {
	struct ll_item *prev;
	struct ll_item *tmp;
	int ret = find_prev(key, list, &prev);

	if (ret == 1) {
		tmp = list->first;
		list->first = list->first->next;
	} else if (ret == 0) {
		tmp = prev->next;
		prev->next = prev->next->next;
	} else {
		return -1;
	}

	free(tmp->key);
	free(tmp->value);
	free(tmp);
	list->length--;
	return 0;
}

static void term_ll_item_list(struct ll_item *item) {
	if (item->next != NULL) {
		term_ll_item_list(item->next);
	}

	free(item->key);
	free(item->value);
	free(item);
}

int ll_term(struct linked_list *list) {
	if (list->first != NULL)
		term_ll_item_list(list->first);
	return 0;
}

void ll_fprint(FILE *stream, const struct linked_list *list) {
	fprintf(stream, "Length: %d\n", list->length);
	fprintf(stream, "Value size: %zu\n", list->value_size);

	int i = 0;
	struct ll_item *item = list->first;
	fprintf(stream, "List (index, key):\n");
	while (item != NULL) {
		fprintf(stream, "%d: %s\n", i, item->key);
		item = item->next;
		i++;
	}
}
