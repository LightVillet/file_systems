#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

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

u32 le32_to_cpu(__le32 x) {
	if (IS_BIG_ENDIAN)
		return __builtin_bswap32(x);
	else
		return x;
}

u16 le16_to_cpu(__le16 x) {
	if (IS_BIG_ENDIAN)
		return __builtin_bswap16(x);
	else
		return x;
}

static int read_super_block(int fd, struct ext2_super_block *sb) {
	struct ext2_super_block sb_on_disk;
	int res = pread(fd, &sb_on_disk, sizeof(sb_on_disk), BOOT_LOADER_SPACE);
	if (res != sizeof(sb_on_disk))
		return 1; // Error reading fs
	sb->s_inodes_count = le32_to_cpu(sb_on_disk.s_inodes_count);
	sb->s_blocks_count = le32_to_cpu(sb_on_disk.s_blocks_count);
	sb->s_r_blocks_count = le32_to_cpu(sb_on_disk.s_r_blocks_count);
	sb->s_free_blocks_count = le32_to_cpu(sb_on_disk.s_free_blocks_count);
	sb->s_free_inodes_count = le32_to_cpu(sb_on_disk.s_free_inodes_count);
	sb->s_first_data_block = le32_to_cpu(sb_on_disk.s_first_data_block);
	sb->s_log_block_size = le32_to_cpu(sb_on_disk.s_log_block_size);
	sb->s_log_frag_size = le32_to_cpu(sb_on_disk.s_log_frag_size);
	sb->s_blocks_per_group = le32_to_cpu(sb_on_disk.s_blocks_per_group);
	sb->s_frags_per_group = le32_to_cpu(sb_on_disk.s_frags_per_group);
	sb->s_inodes_per_group = le32_to_cpu(sb_on_disk.s_inodes_per_group);
	sb->s_mtime = le32_to_cpu(sb_on_disk.s_mtime);
	sb->s_wtime = le32_to_cpu(sb_on_disk.s_wtime);
	sb->s_mnt_count = le16_to_cpu(sb_on_disk.s_mnt_count);
	sb->s_max_mnt_count = le16_to_cpu(sb_on_disk.s_max_mnt_count);
	sb->s_magic = le16_to_cpu(sb_on_disk.s_magic);
	sb->s_state = le16_to_cpu(sb_on_disk.s_state);
	sb->s_errors = le16_to_cpu(sb_on_disk.s_errors);
	sb->s_minor_rev_level = le16_to_cpu(sb_on_disk.s_minor_rev_level);
	sb->s_lastcheck = le32_to_cpu(sb_on_disk.s_lastcheck);
	sb->s_checkinterval = le32_to_cpu(sb_on_disk.s_checkinterval);
	sb->s_creator_os = le32_to_cpu(sb_on_disk.s_creator_os);
	sb->s_rev_level = le32_to_cpu(sb_on_disk.s_rev_level);
	sb->s_def_resuid = le16_to_cpu(sb_on_disk.s_def_resuid);
	sb->s_def_resgid = le16_to_cpu(sb_on_disk.s_def_resgid);
	//// EXT2_DYNAMIC_REV
	sb->s_first_ino = le32_to_cpu(sb_on_disk.s_first_ino);
	sb->s_inode_size = le16_to_cpu(sb_on_disk.s_inode_size);
	sb->s_block_group_nr = le16_to_cpu(sb_on_disk.s_block_group_nr);
	sb->s_feature_compat = le32_to_cpu(sb_on_disk.s_feature_compat);
	sb->s_feature_incompat = le32_to_cpu(sb_on_disk.s_feature_incompat);
	sb->s_feature_ro_compat = le32_to_cpu(sb_on_disk.s_feature_ro_compat);
	//strncpy(sb->s_uuid, sb_on_disk.s_uuid, 16);
	strncpy(sb->s_volume_name, sb_on_disk.s_volume_name, 16);
	strncpy(sb->s_last_mounted, sb_on_disk.s_last_mounted, 64);
	// TODO
	if (sb->s_magic != 61267)
		return 2; // Not ext2
	if (sb->s_feature_incompat != 0)
		return 3; // Incompatible fs
	return 0;
}

static int read_group_desc(int fd, struct ext2_group_desc *gd) {
	struct ext2_group_desc gd_on_disk;
	int res = pread(fd, &gd_on_disk, sizeof(gd_on_disk), BOOT_LOADER_SPACE + sizeof(struct ext2_super_block));
	if (res != sizeof(gd_on_disk))
		return 1; // Error reading fs
	gd->bg_block_bitmap = le32_to_cpu(gd_on_disk.bg_block_bitmap);
	gd->bg_inode_bitmap = le32_to_cpu(gd_on_disk.bg_inode_bitmap);
	gd->bg_inode_table = le32_to_cpu(gd_on_disk.bg_inode_table);
	gd->bg_free_blocks_count = le16_to_cpu(gd_on_disk.bg_free_blocks_count);
	gd->bg_free_inodes_count = le16_to_cpu(gd_on_disk.bg_free_inodes_count);
	gd->bg_used_dirs_count = le16_to_cpu(gd_on_disk.bg_used_dirs_count);
	return 0;
}

int read_inode(const struct ext2 *ext2, struct ext2_inode *inode, u32 ino) {
	/*** Because inodes count begins from 1 ***/
	ino--;
	struct ext2_inode inode_on_disk;
	u32 offset = ext2->blocksize * ext2->inode_table + ino * ext2->inode_size;
	int res = pread(ext2->fd, &inode_on_disk, sizeof(struct ext2_inode), offset);
	if (res != sizeof(struct ext2_inode))
		return 1;
	inode->i_mode = le16_to_cpu(inode_on_disk.i_mode);
	inode->i_uid = le16_to_cpu(inode_on_disk.i_uid);
	inode->i_size = le32_to_cpu(inode_on_disk.i_size);
	inode->i_atime = le32_to_cpu(inode_on_disk.i_atime);
	inode->i_ctime = le32_to_cpu(inode_on_disk.i_ctime);
	inode->i_mtime = le32_to_cpu(inode_on_disk.i_mtime);
	inode->i_dtime = le32_to_cpu(inode_on_disk.i_dtime);
	inode->i_gid = le16_to_cpu(inode_on_disk.i_gid);
	inode->i_links_count = le16_to_cpu(inode_on_disk.i_links_count);
	inode->i_blocks = le32_to_cpu(inode_on_disk.i_blocks);
	inode->i_flags = le32_to_cpu(inode_on_disk.i_flags);
	inode->i_osd1 = le32_to_cpu(inode_on_disk.i_osd1);
	for (int count = 0; count < EXT2_N_BLOCKS; ++count)
		inode->i_block[count] = le32_to_cpu(inode_on_disk.i_block[count]);
	inode->i_generation = le32_to_cpu(inode_on_disk.i_generation);
	inode->i_file_acl = le32_to_cpu(inode_on_disk.i_file_acl);
	inode->i_dir_acl = le32_to_cpu(inode_on_disk.i_dir_acl);
	inode->i_faddr = le32_to_cpu(inode_on_disk.i_faddr);
	return 0;
}

int ext2_open(struct ext2 *ext2, const char *path) {
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1; // Error opening fs
	ext2->fd = fd;
	int res;
	/*** Reading superblock ***/
	struct ext2_super_block superblock;
	res = read_super_block(fd, &superblock);
	if (res)
		return res;
	ext2->blocksize = 1024 << superblock.s_log_block_size;
	ext2->inode_size = superblock.s_inode_size;
	/*** Reading group secriptor ***/
	struct ext2_group_desc group_descriptor;
	res = read_group_desc(fd, &group_descriptor);
	if (res)
		return res;
	ext2->inode_table = group_descriptor.bg_inode_table;
	return 0;
}

int ext2_close(const struct ext2 *ext2) {
	errno = 0;
	close(ext2->fd);
	return errno;
}

