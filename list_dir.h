#ifndef LIST_DIR_H
#define LIST_DIR_H

#include <stdio.h>
#include "linked_list.h"

/* Return a linked list of struct file_info for the directory in
 * `dir_path`. */
struct linked_list get_file_list(const char *dir_path);

/* Return a list of files that are:
 * - Newer in `list1` than in `list2`, or
 * - Exist in `list1`, but not in `list2` */
struct linked_list need_update(struct linked_list *list1, struct linked_list *list2);

/* Print file list to stream. */
void frpint_file_list(FILE *stream, struct linked_list *list);

#endif /* LIST_DIR_H */
