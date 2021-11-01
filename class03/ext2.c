#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "ext2.h"

/******************* 
 * Command that I used to create ext2:
 * sudo mke2fs -b 1024 -d ./ext2/ -O ^sparse_super  -O ^resize_inode -O ^large_file -O ^filetype -O ^ext_attr -O ^dir_index -e panic -I 128 -t ext2 -c -c -L ext2_fs -r 0 /dev/sdc15 1m
 * ./ext2/ directory contains small_file and large_file and directory sample_dir
 * sample_dir contains very_large_file that uses indirect blocks
 ******************/

/*******************
 * Program works with n block groups, but for n > 1 it hasn't implemented yet (old joke but true for this program from now)
 ******************/

#define DEVICE_PATH "/dev/sdc15" // TODO

/***** Defines to simplier calculating little-endian bytes to int *****/
#define read__le32(A, B, C, D)  ((A) + ((B) << 8) + ((C) << 16) + ((D) << 24))
#define read__le16(A, B)        ((A) + ((B) << 8))

/***** Global superblock and group descriptor for easier usage *****/
struct ext2_super_block superblock;
struct ext2_group_desc group_descriptor;

static void read_super_block(const unsigned char *buf, struct ext2_super_block *sb) {
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
	// EXT2_DYNAMIC_REV
	sb->s_first_ino = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_inode_size = read__le16(buf[i++], buf[i++]);
	sb->s_block_group_nr = read__le16(buf[i++], buf[i++]);
	sb->s_feature_compat = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_feature_incompat = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	sb->s_feature_ro_compat = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	strncpy(sb->s_uuid, buf + i, 16);
	i += 16;
	strncpy(sb->s_volume_name, buf + i, 16);
	i += 16;
	strncpy(sb->s_last_mounted, buf + i, 64);
	i += 64;
	// TODO
	if (sb->s_magic != 61267) {
		fprintf(stderr, "Man, this is NOT an ext2, don't try to fool me!\n");
		exit(3);
	}
	if (sb->s_feature_incompat != 0) {
		fprintf(stderr, "File system has incompatible featues! Exiting...\n");
		exit(4);
	}
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
	// EXT2_DYNAMIC_REV
	printf("First non-reserved inode: %u\n", sb->s_first_ino);
	printf("Size of inode structure: %u\n", sb->s_inode_size);
	printf("Block group # of this superblock: %u\n", sb->s_block_group_nr);
	printf("Compatible feature set: %u\n", sb->s_feature_compat);
	printf("Incompatible feature set: %u\n", sb->s_feature_incompat);
	printf("Readonly-compatible feature set: %u\n", sb->s_feature_ro_compat);
	printf("128-bit uuid for volume: %s\n", sb->s_uuid);
	printf("Volume name: %s\n", sb->s_volume_name);
	printf("Directory where last mounted: %s\n", sb->s_last_mounted);
	// TODO
}

static void read_group_desc(const unsigned char *buf, struct ext2_group_desc *gd) {
	int i = 0;
	gd->bg_block_bitmap = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	gd->bg_inode_bitmap = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	gd->bg_inode_table = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	gd->bg_free_blocks_count = read__le16(buf[i++], buf[i++]);
	gd->bg_free_inodes_count = read__le16(buf[i++], buf[i++]);
	gd->bg_used_dirs_count = read__le16(buf[i++], buf[i++]);
}

static void print_group_desc(const struct ext2_group_desc *gd) {
	printf("Blocks bitmap block: %u\n", gd->bg_block_bitmap);
	printf("Inodes bitmap block: %u\n", gd->bg_inode_bitmap);
	printf("Inode table block: %u\n", gd->bg_inode_table);
	printf("Free block count: %u\n", gd->bg_free_blocks_count);
	printf("Free inodes count: %u\n", gd->bg_free_inodes_count);
	printf("Directories count: %u\n", gd->bg_used_dirs_count);
}

static void read_inode(const unsigned char *buf, struct ext2_inode *inode) {
	int i = 0;
	inode->i_mode = read__le16(buf[i++], buf[i++]);
	inode->i_uid = read__le16(buf[i++], buf[i++]);
	inode->i_size = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	inode->i_atime = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	inode->i_ctime = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	inode->i_mtime = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	inode->i_dtime = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	inode->i_gid = read__le16(buf[i++], buf[i++]);
	inode->i_links_count = read__le16(buf[i++], buf[i++]);
	inode->i_blocks = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	inode->i_flags = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	inode->i_osd1 = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	for (int count = 0; count < EXT2_N_BLOCKS; count++)
		inode->i_block[count] = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	inode->i_generation = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	inode->i_file_acl = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	inode->i_dir_acl = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
	inode->i_faddr = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
}

static void print_inode(const struct ext2_inode *inode) {
	printf("File mode: %u\n", inode->i_mode);
	printf("Low 16 bits of Owner Uid: %u\n", inode->i_uid);
	printf("Size in bytes: %u\n", inode->i_size);
	printf("Access time: %u\n", inode->i_atime);
	printf("Creation time: %u\n", inode->i_ctime);
	printf("Modification time: %u\n", inode->i_mtime);
	printf("Deletion time: %u\n", inode->i_dtime);
	printf("Low 16 bits of Group Id: %u\n", inode->i_gid);
	printf("Links count: %u\n", inode->i_links_count);
	printf("Blocks count: %u\n", inode->i_blocks);
	printf("File flags: %u\n", inode->i_flags);
	// TODO
}

static void read_file_data_from_blocks(int fd, __le32 *blocks, unsigned char *data, __le32 size) {
	__le32 block_size = 1024 << superblock.s_log_block_size;
	__le32 readed = 0;
	__le32 offset = 0;
	int res = 0;
	__le32 *direct_blocks = blocks;
	__le32 indirect_blocks[EXT2_N_BLOCKS];
	/***** TODO double and triple indirect blocks *****/
	while (readed != size) {
		offset = readed / block_size % EXT2_IND_BLOCK; // TODO 
		if (readed / block_size == EXT2_IND_BLOCK) { /***** Goto indirect blocks *****/
			res = lseek(fd, direct_blocks[EXT2_IND_BLOCK] * block_size, SEEK_SET);
			if (res < 0) {
				fprintf(stderr, "Error seeking %s: %s\n", DEVICE_PATH, strerror(errno));
				exit(3);
			}
			for (int i = 0; i < EXT2_N_BLOCKS; i++) { /***** Read indirect blocks *****/
				res = read(fd, indirect_blocks + i,sizeof( __le32));
				if (res != sizeof(__le32)) {
					fprintf(stderr, "Error reading %s: %s\n", DEVICE_PATH, strerror(errno));
					exit(2);
				}
			}
			direct_blocks = indirect_blocks;
		}
		res = lseek(fd, direct_blocks[offset] * block_size, SEEK_SET);
		if (res < 0) {
			fprintf(stderr, "Error seeking %s: %s\n", DEVICE_PATH, strerror(errno));
			exit(3);
		}
		if (readed + block_size <= size) {
			res = read(fd, data + readed, block_size);
			if (res != block_size) {
				fprintf(stderr, "Error reading %s: %s\n", DEVICE_PATH, strerror(errno));
				exit(2);
			}	
		}
		else {
			res = read(fd, data + readed, size - readed);
			if (res != size - readed) {
				fprintf(stderr, "Error reading %s: %s\n", DEVICE_PATH, strerror(errno));
				exit(2);
			}
		}
		readed += res;
	}
}

static void print_dir_entry(const struct ext2_dir_entry *dir, const char *name) {
	printf("Inode number: %u\n", dir->inode);
	printf("Directory entry length: %u\n", dir->rec_len);
	printf("Name length: %u\n", dir->name_len);
	printf("File name: %s\n", name);

}

static void read_directory_content(int fd, __le32 *blocks, unsigned char *data, __le32 size) {
	/***** TODO read indirect blocks *****/
	__le32 block_size = 1024 << superblock.s_log_block_size;
	struct ext2_dir_entry dir;
	int res, i;
	int offset = 0;
	int readed = 0;
	char buf[sizeof(__le32)];
	res = lseek(fd, blocks[readed / block_size] * block_size, SEEK_SET);
	if (res < 0) {
		fprintf(stderr, "Error seeking %s: %s\n", DEVICE_PATH, strerror(errno));
		exit(3);
	}
	while (1) {
		res = read(fd, buf, sizeof(__le32));
		if (res != sizeof(__le32)) {
			fprintf(stderr, "Error reading %s: %s\n", DEVICE_PATH, strerror(errno));
			exit(2);
		}
		i = 0;
		dir.inode = read__le32(buf[i++], buf[i++], buf[i++], buf[i++]);
		if (!dir.inode)
			break;
		res = read(fd, buf, sizeof(__le16) * 2);
		if (res != sizeof(__le16) * 2) {
			fprintf(stderr, "Error reading %s: %s\n", DEVICE_PATH, strerror(errno));
			exit(2);
		}
		i = 0;
		dir.rec_len = read__le16(buf[i++], buf[i++]);
		dir.name_len = read__le16(buf[i++], buf[i++]);
		res = read(fd, data + offset, dir.name_len);
		if (res != dir.name_len) {
			fprintf(stderr, "Error reading %s: %s\n", DEVICE_PATH, strerror(errno));
			exit(2);
		}
		offset += res;
		data[offset] = '\n';
		offset++;
		res = lseek(fd, dir.rec_len - dir.name_len - sizeof(__le32) - 2*sizeof(__le16), SEEK_CUR);
		if (res < 0) {
			fprintf(stderr, "Error seeking %s: %s\n", DEVICE_PATH, strerror(errno));
			exit(3);
		}
	}
};

static unsigned char* read_inode_data(int fd, __le32 inode_number) {
	/***** Returns allocated file data array	*****/
	/***** Need to free after usage 			*****/
	struct ext2_inode inode;
	char buf[superblock.s_inode_size];
	int res = 0;
	__le32 block_size = 1024 << superblock.s_log_block_size;
	res = lseek(fd, block_size * group_descriptor.bg_inode_table + inode_number * superblock.s_inode_size, SEEK_SET);
	if (res < 0) {
		fprintf(stderr, "Error seeking %s: %s\n", DEVICE_PATH, strerror(errno));
		exit(3);
	}
	res = read(fd, buf, superblock.s_inode_size);
	if (res != superblock.s_inode_size)	{
		fprintf(stderr, "Error reading %s: %s\n", DEVICE_PATH, strerror(errno));
		exit(2);
	}
	read_inode(buf, &inode);
	unsigned char *data = (unsigned char*)malloc(sizeof(char) * inode.i_size);
	if ((inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFREG)
		read_file_data_from_blocks(fd, inode.i_block, data, inode.i_size);
	else if ((inode.i_mode & EXT2_S_IFMT) == EXT2_S_ISDIR)
		read_directory_content(fd, inode.i_block, data, inode.i_size);
	else
		printf("I DONT KNOW!\n");
	return data;
}

int main() {
	int fd = open(DEVICE_PATH, O_RDONLY);
	if (fd < 0)	{
		fprintf(stderr, "Error opening %s: %s\n", DEVICE_PATH, strerror(errno));
		return 1;
	}
	unsigned char *buf = (unsigned char*)malloc(sizeof(unsigned char) * 1024);
	int res;
	/***** Skip boot record *****/
	res = read(fd, buf, 1024);
	if (res != 1024) {
		fprintf(stderr, "Error reading %s: %s\n", DEVICE_PATH, strerror(errno));
		return 2;
	}
	/***** Read superblock *****/
	res = read(fd, buf, sizeof(struct ext2_super_block));
	if (res != sizeof(struct ext2_super_block)) {
		fprintf(stderr, "Error reading %s: %s\n", DEVICE_PATH, strerror(errno));
		return 2;
	}
	read_super_block(buf, &superblock);
	//print_super_block(&superblock);
	/***** Read group desciptor *****/
	res = read(fd, buf, sizeof(struct ext2_group_desc));
	if (res != sizeof(struct ext2_group_desc)) {
		fprintf(stderr, "Error reading %s: %s\n", DEVICE_PATH, strerror(errno));
		return 2;
	}
	read_group_desc(buf, &group_descriptor);
	//print_group_desc(&group_descriptor);
	/***** Reading inode *****/
	unsigned char *data = read_inode_data(fd, EXT2_ROOT_INODE_NO);
	printf("%s", data);
	free(data);
	free(buf);
	close(fd);
	return 0;
}
