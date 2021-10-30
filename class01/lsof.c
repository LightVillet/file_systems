#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/sysmacros.h>

#define PROC_PATH "/proc/"
#define MAX_STR_LEN 256

/*********************** Implementation of lsof -d ^mem ************************/

struct lsof_data {
	char command[MAX_STR_LEN];
	char pid[MAX_STR_LEN];
	char user[MAX_STR_LEN];
	char fd[MAX_STR_LEN];
	char type[MAX_STR_LEN];
	char device[MAX_STR_LEN];
	char size_off[MAX_STR_LEN];
	char node[MAX_STR_LEN];
	char name[MAX_STR_LEN];
};

inline static void print_lsof_data(const struct lsof_data* info)
{
	printf("%-9.9s %5.5s %16.16s %4.4s%-2.2s %7.7s %18.18s %9.9s %10.10s %s\n",
		info->command, //commmand
		info->pid, //pid
		info->user, //user
		info->fd, //fd
		"", //RWU
		info->type, //type
		info->device, //device
		"", //size/off
		info->node, //node
		info->name //name
		);
}

inline static void get_process_command(const char* pid, char* comm)
{
	char path[MAX_STR_LEN];
	strcpy(path, PROC_PATH);
	strcat(path, pid);
	strcat(path, "/comm");
	FILE* comm_file = fopen(path, "r");
	if (comm_file == NULL)
	{
		fprintf(stderr, "Error: %s\n", strerror(errno));
		return;
	}
	fscanf(comm_file, "%s", comm);
	fclose(comm_file);
}

inline static void get_process_user(const char* pid, char* user)
{
	char path[MAX_STR_LEN];
	strcpy(path, PROC_PATH);
	strcat(path, pid);
	struct stat pstat;
	int res = stat(path, &pstat);
	if (res == -1)
	{
		fprintf(stderr, "Error: %s\n", strerror(errno));
		return;
	}
	struct passwd* pwd = getpwuid(pstat.st_uid);
	if (pwd == NULL)
	{
		fprintf(stderr, "Error: %s\n", strerror(errno));
		return;
	}
	strcpy(user, pwd->pw_name);
}

inline static void get_file_type(const char* path, char* type)
{
	struct stat statbuf;
	int res = stat(path, &statbuf);
	if (res == -1 && errno == EACCES)
	{
		if (errno == EACCES) // Not error, just lack of permissions
		{
			strcpy(type, "unknown");
			errno = 0;
			return;
		}
		else // This is error
		{
		printf("Error: %s\n", strerror(errno));
		return;
		}
	}
	switch (statbuf.st_mode & S_IFMT)
	{
		case S_IFSOCK:
			strcpy(type, "sock");
			break;
		case S_IFLNK:
			strcpy(type, "link");
			break;
		case S_IFREG:
			strcpy(type, "REG");
			break;
		case S_IFBLK:
			strcpy(type, "BLK");
			break;
		case S_IFDIR:
			strcpy(type, "DIR");
			break;
		case S_IFCHR:
			strcpy(type, "CHR");
			break;
		case S_IFIFO:
			strcpy(type, "FIFO");
			break;
		default:
			strcpy(type, "a_inode");
	}
}

inline static void get_file_real_name(const char* path, char* name)
{
	char* buf = malloc(sizeof(char) * MAX_STR_LEN);
	int len = readlink(path, buf, MAX_STR_LEN - 1);
	if (len < 0)
	{
		if (errno == EACCES) // Not an error, just lack of permissions
		{
			strcpy(name, path);
			strcat(name, " (readlink: Permission denied)"); // TODO: normal string error
			errno = 0;
			free(buf);
			return;
		}
		else
		{
			fprintf(stderr, "Error: %s\n", strerror(errno));
			free(buf);
			return;
		}
	}
	buf[len] = '\0';
	struct stat statbuf;
	int res = stat(path, &statbuf);
	if (res == -1)
	{
		fprintf(stderr, "Error: %s\n", strerror(errno));
		free(buf);
		return;
	}
	char* net_name;
	switch (statbuf.st_mode & S_IFMT)
	{
		case S_IFREG:
		case S_IFDIR:
		case S_IFCHR:
			strcpy(name, buf);
			break;
		case S_IFSOCK:
			strcpy(name, "UNKNOWN TYPE");
			break;
		case S_IFIFO:
			net_name = strchr(buf, ':');
			strcpy(name, "pipe");
			break;
		default:
			net_name = strchr(buf, ':');
			strcpy(name, net_name + 1);
	}
	free(buf);
}

inline static void get_file_node(const char* path, char* node)
{
	struct stat statbuf;
	int res = stat(path, &statbuf);
	if (res == -1)
	{
		if (errno == EACCES)
		{
			strcpy(node, "");
			return;
		}
		fprintf(stderr, "Error: %s\n", strerror(errno));
		return;
	}
	sprintf(node, "%lu", statbuf.st_ino);
}

inline static void get_file_device(const char* path, char* device)
{
	struct stat statbuf;
	int res = stat(path, &statbuf);
	if (res < 0)
	{
		if (errno == EACCES)
		{
			strcpy(device, "");
			return;
		}
		else
		{
			fprintf(stderr, "Error: %s\n", strerror(errno));
			return;
		}
	}
	if (statbuf.st_rdev)
		sprintf(device, "%u,%u", major(statbuf.st_rdev), minor(statbuf.st_rdev));
	else
		sprintf(device, "%u,%u", major(statbuf.st_dev), minor(statbuf.st_dev));
}

inline static void print_proc_info(const char* pid)
{
	char path[MAX_STR_LEN];
	struct lsof_data* info = (struct lsof_data*)malloc(sizeof(struct lsof_data));
	strcpy(info->pid, pid);
	get_process_command(pid, info->command);
	get_process_user(pid, info->user);
	
	// cwd
	strcpy(info->fd, "cwd");
	strcpy(path, PROC_PATH);
	strcat(path, pid);
	strcat(path, "/cwd");
	get_file_type(path, info->type);
	get_file_device(path, info->device);
	get_file_node(path, info->node);
	get_file_real_name(path, info->name);
	// TODO SIZE/OFF
	print_lsof_data(info);
	
	// rtd
	strcpy(info->fd, "rtd");
	strcpy(path, PROC_PATH);
	strcat(path, pid);
	strcat(path, "/root");
	get_file_type(path, info->type);
	get_file_device(path, info->device);
	get_file_node(path, info->node);
	get_file_real_name(path, info->name);
	// TODO SIZE/OFF
	print_lsof_data(info);
	
	// txt
	strcpy(info->fd, "txt");
	strcpy(path, PROC_PATH);
	strcat(path, pid);
	strcat(path, "/exe");
	get_file_type(path, info->type);
	get_file_device(path, info->device);
	get_file_node(path, info->node);
	get_file_real_name(path, info->name);
	// TODO SIZE/OFF
	print_lsof_data(info);
	
	// fd
	strcpy(path, PROC_PATH);
	strcat(path, pid);
	strcat(path, "/fd/");
	DIR* fddir = opendir(path);
	if (fddir == NULL)
	{
		if (errno == EACCES) // Not error, just lask of permissions
		{
			strcpy(info->fd, "NOFD");
			strcpy(info->type, "");
			strcpy(info->device, "");
			strcpy(info->size_off, "");
			strcpy(info->node, "");
			path[strlen(path) - 1] = '\0';
			strcpy(info->name, path);
			strcat(info->name, " (opendir: Permission denied)"); // TODO: create normal string error
			print_lsof_data(info);
			free(info);
			return;
		}
		else // This is an error
		{
			fprintf(stderr, "Error: %s\n", strerror(errno));
			free(info);
			return;
		}
	}
	// Scan fd dir
	struct dirent* file;
	while ((file = readdir(fddir)))
	{
		if (file->d_name[0] < '0' || file->d_name[0] > '9')
			continue;
		strcpy(path, PROC_PATH);
		strcat(path, pid);
		strcat(path, "/fd/");
		strcat(path, file->d_name);
		strcpy(info->fd, file->d_name);
		get_file_type(path, info->type);
		get_file_device(path, info->device);
		get_file_node(path, info->node);
		get_file_real_name(path, info->name);
		print_lsof_data(info);
		errno = 0;
	}
	if (errno)
	{
		fprintf(stderr, "Error: %s\n", strerror(errno));
		closedir(fddir);
		free(info);
		return;
	}
	closedir(fddir);
	free(info);
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
	
	printf("%-9s %5s %16s %4s%-2s %7s %18s %9s %10s %s\n", "COMMAND", "PID", "USER", "FD", "", "TYPE", "DEVICE", "SIZE/OFF", "NODE", "NAME");
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
