#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <endian.h>

#include "ext2.h"

/******************* 
 * Command that I used to create ext2:
 * sudo mke2fs -b 1024 -d ./ext2/ -O ^sparse_super  -O ^resize_inode -O ^large_file -O ^filetype -O ^ext_attr -O ^dir_index -e panic -I 128 -t ext2 -c -c -L ext2_fs -r 0 /dev/sdc15 15m
 * ./ext2/ directory contains small_file and large_file and directory sample_dir
 * sample_dir contains a lot dirs with files containing "test" and very_large_file that uses indirect blocks
 ******************/

static int read_super_block(int fd, struct ext2_super_block *sb) {
	struct ext2_super_block sb_on_disk;
	int res = pread(fd, &sb_on_disk, sizeof(sb_on_disk), BOOT_LOADER_SPACE);
	if (res != sizeof(sb_on_disk)) {
		errno = ERR_FS_IO; // Error reading fs
		return -1;
	}
	sb->s_inodes_count = le32toh(sb_on_disk.s_inodes_count);
	sb->s_blocks_count = le32toh(sb_on_disk.s_blocks_count);
	sb->s_r_blocks_count = le32toh(sb_on_disk.s_r_blocks_count);
	sb->s_free_blocks_count = le32toh(sb_on_disk.s_free_blocks_count);
	sb->s_free_inodes_count = le32toh(sb_on_disk.s_free_inodes_count);
	sb->s_first_data_block = le32toh(sb_on_disk.s_first_data_block);
	sb->s_log_block_size = le32toh(sb_on_disk.s_log_block_size);
	sb->s_log_frag_size = le32toh(sb_on_disk.s_log_frag_size);
	sb->s_blocks_per_group = le32toh(sb_on_disk.s_blocks_per_group);
	sb->s_frags_per_group = le32toh(sb_on_disk.s_frags_per_group);
	sb->s_inodes_per_group = le32toh(sb_on_disk.s_inodes_per_group);
	sb->s_mtime = le32toh(sb_on_disk.s_mtime);
	sb->s_wtime = le32toh(sb_on_disk.s_wtime);
	sb->s_mnt_count = le16toh(sb_on_disk.s_mnt_count);
	sb->s_max_mnt_count = le16toh(sb_on_disk.s_max_mnt_count);
	sb->s_magic = le16toh(sb_on_disk.s_magic);
	sb->s_state = le16toh(sb_on_disk.s_state);
	sb->s_errors = le16toh(sb_on_disk.s_errors);
	sb->s_minor_rev_level = le16toh(sb_on_disk.s_minor_rev_level);
	sb->s_lastcheck = le32toh(sb_on_disk.s_lastcheck);
	sb->s_checkinterval = le32toh(sb_on_disk.s_checkinterval);
	sb->s_creator_os = le32toh(sb_on_disk.s_creator_os);
	sb->s_rev_level = le32toh(sb_on_disk.s_rev_level);
	sb->s_def_resuid = le16toh(sb_on_disk.s_def_resuid);
	sb->s_def_resgid = le16toh(sb_on_disk.s_def_resgid);
	//// EXT2_DYNAMIC_REV
	sb->s_first_ino = le32toh(sb_on_disk.s_first_ino);
	sb->s_inode_size = le16toh(sb_on_disk.s_inode_size);
	sb->s_block_group_nr = le16toh(sb_on_disk.s_block_group_nr);
	sb->s_feature_compat = le32toh(sb_on_disk.s_feature_compat);
	sb->s_feature_incompat = le32toh(sb_on_disk.s_feature_incompat);
	sb->s_feature_ro_compat = le32toh(sb_on_disk.s_feature_ro_compat);
	//strncpy(sb->s_uuid, sb_on_disk.s_uuid, 16);
	strncpy(sb->s_volume_name, sb_on_disk.s_volume_name, 16);
	strncpy(sb->s_last_mounted, sb_on_disk.s_last_mounted, 64);
	// TODO
	if (sb->s_magic != 61267) {
		errno = ERR_FS_NOT_EXT2;
		return -1;
	}
	if (sb->s_feature_incompat != 0) {
		errno = ERR_FS_INCOMPAT;
		return -1;
	}
	return 0;
}

static int read_group_desc(int fd, struct ext2_group_desc *gd, u32 blocksize, u32 blocks_per_group, u32 blockgroup_no) {
	struct ext2_group_desc gd_on_disk;
	u32 block_group_offset = blockgroup_no * blocks_per_group * blocksize;
	u32 offset = BOOT_LOADER_SPACE + block_group_offset + sizeof(struct ext2_super_block);
	int res = pread(fd, &gd_on_disk, sizeof(gd_on_disk), offset);
	if (res != sizeof(gd_on_disk)) {
		errno = ERR_FS_IO;
		return -1;
	}
	gd->bg_block_bitmap = le32toh(gd_on_disk.bg_block_bitmap);
	gd->bg_inode_bitmap = le32toh(gd_on_disk.bg_inode_bitmap);
	gd->bg_inode_table = le32toh(gd_on_disk.bg_inode_table);
	gd->bg_free_blocks_count = le16toh(gd_on_disk.bg_free_blocks_count);
	gd->bg_free_inodes_count = le16toh(gd_on_disk.bg_free_inodes_count);
	gd->bg_used_dirs_count = le16toh(gd_on_disk.bg_used_dirs_count);
	return 0;
}

static int get_inode_table_by_inode_number(const struct ext2 *ext2, u32 inode_number) {
	int res;
	struct ext2_group_desc gd;
	u32 blockgroup_no = inode_number / ext2->inodes_per_group;
	res = read_group_desc(ext2->fd, &gd, ext2->blocksize, ext2->blocks_per_group, blockgroup_no);
	if (res)
		return res;
	return gd.bg_inode_table;
}

int read_inode(const struct ext2 *ext2, struct ext2_inode *inode, u32 ino) {
	/*** Because inodes count begins from 1 ***/
	ino--;
	int res;
	struct ext2_inode inode_on_disk;
	u32 inode_index = ino % ext2->inodes_per_group;
	u32 inode_table = get_inode_table_by_inode_number(ext2, ino);
	u32 offset = inode_table * ext2->blocksize + inode_index * ext2->inode_size;
	res = pread(ext2->fd, &inode_on_disk, sizeof(struct ext2_inode), offset);
	if (res != sizeof(struct ext2_inode)) {
		errno = ERR_FS_IO;
		return -1;
	}
	inode->i_mode = le16toh(inode_on_disk.i_mode);
	inode->i_uid = le16toh(inode_on_disk.i_uid);
	inode->i_size = le32toh(inode_on_disk.i_size);
	inode->i_atime = le32toh(inode_on_disk.i_atime);
	inode->i_ctime = le32toh(inode_on_disk.i_ctime);
	inode->i_mtime = le32toh(inode_on_disk.i_mtime);
	inode->i_dtime = le32toh(inode_on_disk.i_dtime);
	inode->i_gid = le16toh(inode_on_disk.i_gid);
	inode->i_links_count = le16toh(inode_on_disk.i_links_count);
	inode->i_blocks = le32toh(inode_on_disk.i_blocks);
	inode->i_flags = le32toh(inode_on_disk.i_flags);
	inode->i_osd1 = le32toh(inode_on_disk.i_osd1);
	for (int count = 0; count < EXT2_N_BLOCKS; ++count)
		inode->i_block[count] = le32toh(inode_on_disk.i_block[count]);
	inode->i_generation = le32toh(inode_on_disk.i_generation);
	inode->i_file_acl = le32toh(inode_on_disk.i_file_acl);
	inode->i_dir_acl = le32toh(inode_on_disk.i_dir_acl);
	inode->i_faddr = le32toh(inode_on_disk.i_faddr);
	return 0;
}

int ext2_open(struct ext2 *ext2, const char *path) {
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return errno; // Error opening fs, returning errno from open
	ext2->fd = fd;
	int res;
	/*** Reading superblock ***/
	struct ext2_super_block superblock;
	res = read_super_block(fd, &superblock);
	if (res)
		return res;
	ext2->blocksize = 1024 << superblock.s_log_block_size;
	ext2->inode_size = superblock.s_inode_size;
	ext2->inodes_per_group = superblock.s_inodes_per_group;
	ext2->blocks_per_group = superblock.s_blocks_per_group;
	return 0;
}

int ext2_close(const struct ext2 *ext2) {
	int res = close(ext2->fd);
	return res;
}

