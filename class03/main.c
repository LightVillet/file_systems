#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "ext2.h"

struct ext2_inode_blocks_iter
{
    struct ext2 *ext2;
    struct ext2_inode ino;

    u64 offset;
	u32 *current_blocks;
	u32 indirect_blocks[EXT2_N_BLOCKS];
};

int ext2_inode_blocks_iter_new(struct ext2_inode_blocks_iter *iter, struct ext2 *ext2, u32 ino) {
	int res;
	iter->ext2 = ext2;
	iter->offset = 0;
	res = read_inode(ext2, &iter->ino, ino);
	if (res)
		return res;
	iter->current_blocks = iter->ino.i_block;
	return 0;
}

int ext2_inode_blocks_iter_next(struct ext2_inode_blocks_iter *iter, void *buf) {
	/*** Return number of readed bytes ***/
	/*** Or -1 on reading error ***/
	u32 res;
	u32 size = iter->ino.i_size;
	u32 block_size = iter->ext2->blocksize;
	if (iter->offset == size)
		return 0;
	if (iter->offset / iter->ext2->blocksize == EXT2_IND_BLOCK) { /*** Read indrect blocks ***/
		res = pread(iter->ext2->fd, iter->indirect_blocks, sizeof(u32) * EXT2_N_BLOCKS, iter->ino.i_block[EXT2_IND_BLOCK] * block_size);
		if (res != sizeof(u32) * EXT2_N_BLOCKS)
			return -1;
		iter->current_blocks = iter->indirect_blocks;
	}
	if (iter->offset + block_size <= size) {
		res = pread(iter->ext2->fd, buf, block_size, iter->current_blocks[iter->offset / block_size % EXT2_IND_BLOCK] * block_size); // TODO
		if (res != block_size)
			return -1;
	}
	else {
		res = pread(iter->ext2->fd, buf, size - iter->offset, iter->current_blocks[iter->offset / block_size % EXT2_IND_BLOCK] * block_size); // TODO
		if (res != size - iter->offset)
			return -1;
	}
	iter->offset += res;
	return res;
}

u8* read_dir_from_buf(u8 *buf, struct ext2_dir_entry *dir) {
	/*** Return allocated memory for name ***/
	/*** Need to free ***/
	struct ext2_dir_entry dir_on_disk;
	memcpy(&dir_on_disk, buf, sizeof(dir_on_disk));
	dir->inode = le32_to_cpu(dir_on_disk.inode);
	dir->rec_len = le16_to_cpu(dir_on_disk.rec_len);
	dir->name_len = le16_to_cpu(dir_on_disk.name_len);
	u8 *name = malloc(dir->name_len + 1);
	memcpy(name, buf + sizeof(dir_on_disk), dir->name_len);
	name[dir->name_len] = '\0';
	return name;
}

int print_inode_data(struct ext2 *ext2, const u32 inode_number) {
	int res;
	struct ext2_inode_blocks_iter iter;
	res = ext2_inode_blocks_iter_new(&iter, ext2, inode_number);
	if (res)
		return res;
	u8 *buf = malloc(ext2->blocksize);
	struct ext2_dir_entry dir;
	do {
		res = ext2_inode_blocks_iter_next(&iter, buf);
		if (res < 0) {
			free(buf);
			return 1;
		}
		if (res == 0)
			break;
		if (IFREG(iter.ino.i_mode)) // Print file content
			printf("%.*s", res, buf);
		else if (ISDIR(iter.ino.i_mode)) { // Print directory content
			u32 offset = 0;
			while (offset != iter.ext2->blocksize) {
				u8 *name = read_dir_from_buf(buf + offset, &dir);
				if (dir.inode == 0)
					break;
				printf("%s\n", name);
				free(name);
				offset += dir.rec_len;
			}
		} else {
			printf("I DON'T KNOW!\n");
			break;
		}
	} while (res > 0);
	free(buf);
	return 0;
}

int get_ino_in_dir_by_name(struct ext2 *ext2, const u32 inode_number, const char *req_name) {
	/*** Returns inode number of file in directory ***/
	/*** Zero if not found and negative number on error ***/
	int res;
	struct ext2_inode_blocks_iter iter;
	res = ext2_inode_blocks_iter_new(&iter, ext2, inode_number);
	if (res)
		return res;
	u8 *buf = malloc(ext2->blocksize);
	struct ext2_dir_entry dir;
	do {
		res = ext2_inode_blocks_iter_next(&iter, buf);
		if (res < 0) {
			free(buf);
			return -1;
		}
		if (res == 0)
			break;
		if (ISDIR(iter.ino.i_mode)) { // Iterate directory content
			u32 offset = 0;
			while (offset != iter.ext2->blocksize) {
				u8 *name = read_dir_from_buf(buf + offset, &dir);
				if (dir.inode == 0)
					break;
				if (!strcmp(req_name, (char *)name)) {
					free(buf);
					free(name);
					return dir.inode;
				}
				free(name);
				offset += dir.rec_len;
			}
		} else {
			free(buf);
			return -2;
		}
	} while (res > 0);
	free(buf);
	return 0;
}

int print_file_by_path(struct ext2 *ext2, const char *path) {
	int res;
	char **dir = malloc(sizeof(char*) * 256); // Array of parsed directories
	int count = 0; // Number of elements in char **dir
	char *cur_path = strdup(path);
	char *copied_path; // For dirname and basename
	strcpy(cur_path, path);
	while (strlen(cur_path) > 1) {
		copied_path = strdup(cur_path);
		dir[count] = basename(copied_path);
		count++;
		cur_path = dirname(cur_path);
	}
	int inode_number = EXT2_ROOT_INO;
	count--;
	do {
		inode_number = get_ino_in_dir_by_name(ext2, inode_number, dir[count]);
		if (inode_number < 0)
			return inode_number;
		count--;
		} while (count >= 0);
	print_inode_data(ext2, inode_number);
	free(dir);
	return 0;
}

int main()
{
	int res;
	struct ext2 ext2;
	res = ext2_open(&ext2, "/dev/sdc15");
	if (res)
		return res;
	res = print_file_by_path(&ext2, "/sample_dir/.././sample_dir/1/1");
	if (res)
		return res;
	ext2_close(&ext2);
	return 0;
}
