#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>

#include "ext2.h"

struct ext2_inode_blocks_iter
{
	struct ext2 *ext2;
	struct ext2_inode ino;
	
	u64 offset;
	u32 *current_blocks;
	u32 *indirect_blocks;
	u32 block_offset;
};

int ext2_inode_blocks_iter_new(struct ext2_inode_blocks_iter *iter, struct ext2 *ext2, u32 ino) {
	int res;
	iter->ext2 = ext2;
	iter->offset = 0;
	iter->block_offset = 0;
	res = read_inode(ext2, &iter->ino, ino);
	if (res)
		return res;
	iter->current_blocks = iter->ino.i_block;
	iter->indirect_blocks = malloc(iter->ext2->blocksize);
	return 0;
}

int ext2_inode_blocks_iter_next(struct ext2_inode_blocks_iter *iter, void *buf) {
	/*** Return number of readed bytes ***/
	/*** Or -1 and set errno on error ***/
	u32 res;
	u32 size = iter->ino.i_size;
	u32 block_size = iter->ext2->blocksize;
	int fd = iter->ext2->fd;
	if (iter->offset == size)
		return 0;
	if (iter->offset / block_size == EXT2_IND_BLOCK) { /*** Read indrect blocks ***/
		u64 ind_blocks_offset = iter->ino.i_block[EXT2_IND_BLOCK] * block_size;
		for (u64 i = 0; i < block_size / sizeof(u32); ++i) {
			u32 u32_on_disk;
			res = pread(fd, &u32_on_disk, sizeof(u32), ind_blocks_offset + i * sizeof(u32));
			if (res != sizeof(u32)) {
				errno = ERR_FS_IO;
				return -1;
			}
			iter->indirect_blocks[i] = le32toh(u32_on_disk);
		}
		iter->current_blocks = iter->indirect_blocks;
		iter->block_offset = 0;
	}
	int block_no = iter->current_blocks[iter->block_offset];
	if (iter->offset + block_size <= size) {
		res = pread(fd, buf, block_size, block_no * block_size);
		if (res != block_size) {
			errno = ERR_FS_IO;
			return -1;
		}
	}
	else {
		res = pread(fd, buf, size - iter->offset, block_no * block_size);
		if (res != size - iter->offset) {
			errno = ERR_FS_IO;
			return -1;
		}
	}
	iter->offset += res;
	iter->block_offset++;
	return res;
}

int ext2_inode_blocks_iter_end(struct ext2_inode_blocks_iter *iter) {
	free(iter->indirect_blocks);
	return 0;
}

u8* read_dir_from_buf(u8 *buf, struct ext2_dir_entry *dir) {
	/*** Return allocated memory for name ***/
	/*** Need to free ***/
	struct ext2_dir_entry dir_on_disk;
	memcpy(&dir_on_disk, buf, sizeof(dir_on_disk));
	dir->inode = le32toh(dir_on_disk.inode);
	dir->rec_len = le16toh(dir_on_disk.rec_len);
	dir->name_len = le16toh(dir_on_disk.name_len);
	u8 *name = malloc(dir->name_len + 1);
	memcpy(name, buf + sizeof(dir_on_disk), dir->name_len);
	name[dir->name_len] = '\0';
	return name;
}

int print_inode_data(struct ext2 *ext2, const u32 inode_number) {
	int res;
	struct ext2_inode_blocks_iter iter;
	u8 *buf = malloc(ext2->blocksize);
	res = ext2_inode_blocks_iter_new(&iter, ext2, inode_number);
	if (res)
		goto out_print_inode_data;
	struct ext2_dir_entry dir;
	do {
		res = ext2_inode_blocks_iter_next(&iter, buf);
		if (res <= 0)
			goto out_print_inode_data;
		if (IFREG(iter.ino.i_mode)) // Print file content
			printf("%.*s", res, buf);
		else if (ISDIR(iter.ino.i_mode)) { // Print directory content
			u32 offset = 0;
			while (offset != iter.ext2->blocksize) {
				u8 *name = read_dir_from_buf(buf + offset, &dir);
				printf("%s\n", name);
				if (dir.inode == 0) {
					free(name);
					break;
				}
				free(name);
				offset += dir.rec_len;
			}
		} else {
			printf("I DON'T KNOW!\n");
			break;
		}
	} while (res > 0);
out_print_inode_data:
	ext2_inode_blocks_iter_end(&iter);
	free(buf);
	return res;
}

int get_ino_in_dir_by_name(struct ext2 *ext2, const u32 inode_number, const char *req_name) {
	/*** Returns inode number of file in directory ***/
	/*** Zero if not found and negative number on error ***/
	int res;
	struct ext2_inode_blocks_iter iter;
	res = ext2_inode_blocks_iter_new(&iter, ext2, inode_number);
	u8 *buf = malloc(ext2->blocksize);
	if (res)
		goto out_get_ino_in_dir_by_name;
	struct ext2_dir_entry dir;
	do {
		res = ext2_inode_blocks_iter_next(&iter, buf);
		if (res <= 0)
			goto out_get_ino_in_dir_by_name;
		if (ISDIR(iter.ino.i_mode)) { // Iterate directory content
			u32 offset = 0;
			while (offset != iter.ext2->blocksize) {
				u8 *name = read_dir_from_buf(buf + offset, &dir);
				if (dir.inode == 0)
					break;
				if (!strcmp(req_name, (char *)name)) {
					free(buf);
					free(name);
					ext2_inode_blocks_iter_end(&iter);
					return dir.inode;
				}
				free(name);
				offset += dir.rec_len;
			}
		} else {
			errno = ERR_FS_NOT_DIR;
			res = -1;
			goto out_get_ino_in_dir_by_name;
		}
	} while (res > 0);
out_get_ino_in_dir_by_name:
	ext2_inode_blocks_iter_end(&iter);
	free(buf);
	return res;
}

int print_file_by_path(struct ext2 *ext2, const char *path) {
	/*** This is the only function that lose memory
	**** This is due to weird dirname and basename
	**** They don't allocate memory and may modify their arg
	**** This is why I have to strdup a lot of strings
	**** I can fix it implementing my own dirname and basename
	**** But it is just juggling with strings that I don't want to do rn
	***/
	int res = 0;
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
			goto out_print_file_by_path;
		if (inode_number == 0) {
			res = -1;
			errno = ERR_FS_NOT_FOUND;
			goto out_print_file_by_path;
		}
		count--;
		} while (count >= 0);
	print_inode_data(ext2, inode_number);
out_print_file_by_path:
	free(dir);
	return res;
}

int main()
{
	int res;
	struct ext2 ext2;
	res = ext2_open(&ext2, "/dev/sdc15");
	if (res) {
		printf("Error: %s\n", strerror(errno));
		goto out_main;
	}
	res = print_file_by_path(&ext2, "/some"); // In blockgroup 1
	if (res) {
		printf("Error: %s\n", strerror(errno));
		goto out_main;
	}
out_main:
	ext2_close(&ext2);
	return 0;
}
