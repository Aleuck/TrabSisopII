/* Compile with
 * gcc test_list_dir.c list_dir.c linked_list.c logging.c -D LOGLEVEL=5
 */

#include "list_dir.h"
#include "linked_list.h"

int main(int argc, char *argv[])
{
	struct linked_list local_files = get_file_list(".");
	frpint_file_list(stderr, &local_files);
	ll_term(&local_files);
	return 0;
}
