#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define MAX_STR_LEN 256
#define PROC_PATH "/proc/"

/********************** Implementation of "pc -e" *********************************/
/********************** Wuthout time because I'm very tired now ******************/

inline static void tty_from_tty_nr(int tty_nr, char* tty)
{
	if (tty_nr == 0)
	{
		strcpy(tty, "?");
		return;
	}
	// Magic numbers from man proc
	// in /proc/[pid]/stat/(7)
	char minor_device[MAX_STR_LEN];
	sprintf(minor_device, "%d", tty_nr & 255);
	int major_device = (tty_nr & 65535) >> 8;
	// Magic numbers from https://github.com/torvalds/linux/blob/master/Documentation/admin-guide/devices.txt
	// I trust magic numbers, I trust Torvalds
	switch (major_device)
	{
		case 4:
			strcpy(tty, "tty");
			strcat(tty, minor_device);
			break;
		case 136:
		case 137:
		case 138:
		case 139:
		case 140:
		case 141:
		case 142:
		case 143:
			strcpy(tty, "pts/");
			strcat(tty, minor_device);
			break;
		default:
			strcpy(tty, "?");
	}
}

inline static void print_proc_info(const char* pid)
{
	char statpath[MAX_STR_LEN];
	strcpy(statpath, PROC_PATH);
	strcat(statpath, pid);
	strcat(statpath, "/stat");
	FILE* procstat = fopen(statpath, "r");
	if (procstat == NULL)
	{
		fprintf(stderr, "Error: %s\n", strerror(errno));
		return;
	}
	char comm[MAX_STR_LEN], state[MAX_STR_LEN], ppid[MAX_STR_LEN],
		pgrp[MAX_STR_LEN], session[MAX_STR_LEN], tty[MAX_STR_LEN];
	char parenthes;

	fscanf(procstat, "%s%[^)]%*c%c%s%s%s%s%s",
			comm, /* escape pid because we already have it */
			comm,
			&parenthes, /* Need to escape ')' */
			state,
			ppid,
			pgrp,
			session,
			tty);
	tty_from_tty_nr(atoi(tty), tty);
	printf("%7s %-8s %8s %s\n", pid, tty, "NO TIME(", comm + 2);
	fclose(procstat);
	return;
}


int main()
{
	DIR* procdir = opendir(PROC_PATH);
	if (procdir == NULL)
	{
		fprintf(stderr, "Error: %s\n", strerror(errno));
		return -1;
	}
	printf("%7s %-8s %8s %s\n", "PID", "TTY", "TIME", "CMD");
	struct dirent* file;
	errno = 0;
	while ((file = readdir(procdir)))
	{
		if (file->d_type == DT_DIR && file->d_name[0] >= '0' && file->d_name[0] <= '9')
			print_proc_info(file->d_name);
		errno = 0;
	}
	if (errno)
	{
		fprintf(stderr, "Error: %s\n", strerror(errno));
		closedir(procdir);
		return -1;
	}

	closedir(procdir);
	return 0;
}
