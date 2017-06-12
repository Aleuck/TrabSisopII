#include <stdio.h>
#include <string.h>
#include "dropboxUtil.h"

int main(int argc, char *argv[])
{
	struct file_info info1;
	strcpy(info1.name, "filename");
	strcpy(info1.extension, "ext");
	strcpy(info1.last_modified, "1234567890");
	info1.size = 12345;

	fprintf(stderr, "info1\n");
	fprint_file_info(stderr, &info1);

	char buf[BUF_FILE_INFO_LEN];
	serialize_file_info(&info1, buf);

	struct file_info info2;
	deserialize_file_info(&info2, buf);

	fprintf(stderr, "\ninfo2\n");
	fprint_file_info(stderr, &info2);

	return 0;
}
