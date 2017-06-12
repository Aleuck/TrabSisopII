/* Compile with
 * gcc test_list_dir.c list_dir.c linked_list.c logging.c -D LOGLEVEL=5
 */

#include "list_dir.h"
#include "linked_list.h"

int main(int argc, char *argv[])
{
	fprintf(stderr, "Local files:\n");
	struct linked_list local_files = get_file_list("list_dir_test_files/local");
	frpint_file_list(stderr, &local_files);

	fprintf(stderr, "\nRemote files:\n");
	struct linked_list remote_files = get_file_list("list_dir_test_files/remote/");
	frpint_file_list(stderr, &remote_files);

	fprintf(stderr, "\nLocal files that need update:\n");
	struct linked_list need_update_local = need_update(&remote_files, &local_files);
	frpint_file_list(stderr, &need_update_local);

	fprintf(stderr, "\nRemote files that need update:\n");
	struct linked_list need_update_remote = need_update(&local_files, &remote_files);
	frpint_file_list(stderr, &need_update_remote);

	ll_term(&local_files);
	ll_term(&remote_files);
	ll_term(&need_update_local);
	ll_term(&need_update_remote);
	return 0;
}
