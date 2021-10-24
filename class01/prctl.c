#include <sys/prctl.h>
#include <linux/prctl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

int main(int argc, char* argv[])
{
	int res = prctl(PR_SET_MM, PR_SET_MM_ARG_START, argv[1], 0, 0);
	if (res == -1)
		printf("Error: %s\n", strerror(errno));
	while (1);
	return 0;
}
