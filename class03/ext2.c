#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "ext2.h"

#define DEVICE_PATH "/dev/sdb1" // TODO

static int read_file_inode(const struct ext2_inode *inode) {

	return 1; // TODO
}

static void read_super_block(const unsigned char *buf, struct ext2_super_block *sb) {
#define read__le32(A, B, C, D)	((A) + ((B) << 8) + ((C) << 16) + ((D) << 24))
#define read__le16(A, B)		((A) + ((B) << 8))
	int i = 0;
	sb->s_inodes_count = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_blocks_count = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_r_blocks_count = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_free_blocks_count = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_free_inodes_count = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_first_data_block = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_log_block_size = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_log_frag_size = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_blocks_per_group = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_frags_per_group = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_inodes_per_group = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_mtime = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_wtime = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_mnt_count = read__le16(buf[i++], buf[i++]);
	sb->s_max_mnt_count = read__le16(buf[i++], buf[i++]);
	sb->s_magic = read__le16(buf[i++], buf[i++]);
	sb->s_state = read__le16(buf[i++], buf[i++]);
	sb->s_errors = read__le16(buf[i++], buf[i++]);
	sb->s_minor_rev_level = read__le16(buf[i++], buf[i++]);
	sb->s_lastcheck = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_checkinterval = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_creator_os = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_rev_level = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_def_resuid = read__le16(buf[i++], buf[i++]);
	sb->s_def_resgid = read__le16(buf[i++], buf[i++]);
	// TODO
#undef read__le32
#undef read__le16
}

static void print_super_block(const struct ext2_super_block *sb) {
	printf("Inodes count: %u\n", sb->s_inodes_count);
	printf("Blocks count: %u\n", sb->s_blocks_count);
	printf("Reserved blocks count: %u\n", sb->s_r_blocks_count);
	printf("Free blocks count: %u\n", sb->s_free_blocks_count);
	printf("Free inodes count: %u\n", sb->s_free_inodes_count);
	printf("Firt data block: %u\n", sb->s_first_data_block);
	printf("Block size: %u\n", sb->s_log_block_size);
	printf("Fragment size: %u\n", sb->s_log_frag_size);
	printf("Blocks per group: %u\n", sb->s_blocks_per_group);
	printf("Fragments per group: %u\n", sb->s_frags_per_group);
	printf("Inodes per group: %u\n", sb->s_inodes_per_group);
	printf("Mount time: %u\n", sb->s_mtime);
	printf("Write time: %u\n", sb->s_wtime);
	printf("Mount count: %u\n", sb->s_mnt_count);
	printf("Maximal mount count: %u\n", sb->s_max_mnt_count);
	printf("Magic signature (must be 61267): %u\n", sb->s_magic);
	printf("File system state: %u\n", sb->s_state);
	printf("Behaviour when detecting errors: %u\n", sb->s_errors);
	printf("Minor revision level: %u\n", sb->s_minor_rev_level);
	printf("Time of last check: %u\n", sb->s_lastcheck);
	printf("Max time between intervals: %u\n", sb->s_checkinterval);
	printf("OS creator: %u\n", sb->s_creator_os);
	printf("Revision level: %u\n", sb->s_rev_level);
	printf("Default uid for reserved blocks: %u\n", sb->s_def_resuid);
	printf("Default gid for reserved blocks: %u\n", sb->s_def_resgid);
	// TODO
}

int main() {
	int fd = open(DEVICE_PATH, O_RDONLY);
	if (fd < 0)	{
		fprintf(stderr, "Error opening %s: %s\n", DEVICE_PATH, strerror(errno));
		return 1;
	}
	unsigned char *buf = malloc(sizeof(unsigned char) * 1024);
	int res;
	int bytes_readed = 0;
	/***** Skip boot record *****/
	res = read(fd, buf, 1024);
	if (res != 1024) {
		fprintf(stderr, "Error reading %s: %s\n", DEVICE_PATH, strerror(errno));
		return 2;
	}
	struct ext2_super_block sb;
	res = read(fd, buf, sizeof(struct ext2_super_block));
	read_super_block(buf, &sb);
	print_super_block(&sb);
	//printf("%02X ", buf[0]);
	free(buf);
	close(fd);
	return 0;
}
