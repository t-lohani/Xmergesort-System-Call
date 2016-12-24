#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdbool.h>
#include "myinput.h"

#ifndef __NR_xmergesort
#error xmergesort system call not defined
#endif


struct inputstruct myInput;

int main(int argc, char *const argv[])
{
	int rc;
	extern int optind;
	int c;
	unsigned int data_written = 0;
	unsigned int flagVal = 0;
	bool errVal = false;
	int uf_len = 0, af_len = 0, if_len = 0, tf_len = 0, df_len = 0;

	while ((c = getopt(argc, argv, "uaitd")) != -1) {

		switch (c) {
		case 'u':
			flagVal = flagVal + 1;
			uf_len++;
			break;

		case 'a':
			flagVal = flagVal + 2;
			af_len++;
			break;

		case 'i':
			flagVal = flagVal + 4;
			if_len++;
			break;

		case 't':
			flagVal = flagVal + 16;
			tf_len++;
			break;

		case 'd':
			flagVal = flagVal + 32;
			df_len++;
			break;

		case '?':
			errVal = true;
			break;
		}

	}

	myInput.flags = flagVal;
	myInput.data = &data_written;

	if (errVal) {
		printf("Input options are incorrect");
		goto exit;
	}

	if (uf_len > 1 || af_len > 1 || if_len > 1 || tf_len > 1 || df_len > 1) {
		printf("Repeated flags. Terminating system call");
		goto exit;
	}

	if (optind < argc) {
		myInput.outFile = argv[optind];
		printf(argv[optind]);
		optind++;
	} else {
		printf("First argument null");
	}

	if (optind < argc) {
		myInput.inFile1 = argv[optind];
		printf(argv[optind]);
		optind++;
	} else {
		printf("Second argument null");
	}

	if (optind < argc) {
		myInput.inFile2 = argv[optind];
		printf(argv[optind]);
	} else {
		printf("Third argument null");
	}

	void *dummy = (void *) &myInput;

	rc = syscall(__NR_xmergesort, dummy);

	if (rc == 0)
		printf("syscall returned %d\n", rc);
	else
		printf("syscall returned %d (errno=%d)\n", rc, errno);

	exit:
	exit(rc);
}
