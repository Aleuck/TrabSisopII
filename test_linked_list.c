#include <stdio.h>
#include "linked_list.h"

int main(int argc, char *argv[])
{
	struct linked_list list;

	int value = 1;

	ll_init(sizeof(int), &list);
	ll_del("1", &list);
	ll_put("1", &value, &list);
	ll_del("1", &list);
	ll_put("1", &value, &list);
	ll_put("4", &value, &list);
	ll_put("2", &value, &list);
	ll_put("5", &value, &list);
	ll_put("9", &value, &list);
	ll_put("8", &value, &list);
	ll_put("3", &value, &list);
	fprintf(stderr, "Retrieved item: %d\n", value);
	fprintf(stderr, "List before removal of 1, 5, 3:\n");
	ll_fprint(stderr, &list);
	ll_del("1", &list);
	ll_del("5", &list);
	ll_del("3", &list);
	fprintf(stderr, "List after removals:\n");
	ll_fprint(stderr, &list);
	ll_term(&list);

	return 0;
}
