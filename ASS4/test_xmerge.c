// Include necessary header files
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define __NR_xmerge 336

struct xmerge_params {
	char *outfile;
	char **infiles;
	unsigned int num_files;
	int oflags;
	mode_t mode;
	int *ofile_count;
};

int main(int argc, char *argv[])
{
	mode_t mode = 0;
	int oflags = O_WRONLY;
	int num_files = 0;
	int checkm = 0;
	int checkf = 0;
	int ofile_count = 0;
	struct xmerge_params xp;

	for (int x = 1; x < argc; x++) {
		if (strcmp(argv[x], "-m") == 0) {
			checkm = 1;
			mode = strtol(argv[x + 1], NULL, 8);
			x++;
		} else if (strcmp(argv[x], "-a") == 0) {
			checkf = 1;
			oflags = oflags | O_APPEND;
		} else if (strcmp(argv[x], "-c") == 0) {
			checkf = 1;
			oflags = oflags | O_CREAT;
		} else if (strcmp(argv[x], "-t") == 0) {
			checkf = 1;
			oflags = oflags | O_TRUNC;
		} else if (strcmp(argv[x], "-e") == 0) {
			checkf = 1;
			oflags = oflags | O_EXCL;
		} else {
			xp.outfile = argv[x];
			xp.infiles = argv + x + 1;
			xp.num_files = argc - x - 1;
			break;
		}
	}
	
	if (checkm == 0) {
		mode = S_IRUSR | S_IWUSR;
	}
	if (checkf == 0) {
		oflags = O_CREAT | O_WRONLY | O_APPEND;
	}

	xp.ofile_count = &ofile_count;
	xp.oflags = oflags;
	xp.mode = mode;

	long result =  syscall(__NR_xmerge, &xp, sizeof(struct xmerge_params)); // Fill in the remaining arguments

	if(result >= 0) {
		printf("%d bytes merged\n", result);
		printf("%d file(s) merged\n", *(xp.ofile_count));
	} else {
		perror("ERROR");
		printf("%d file(s) merged\n", *(xp.ofile_count));
	} 
	return 0;
}
