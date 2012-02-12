/*
 *  Copyright (C) 2006 Mike Melanson (mike at multimedia.cx)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file xbfs.c
 * \author Mike Melanson
 * \brief Interpret the Xbox XDVD filesystem
 */

#include <ctype.h>

#include "tree.h"
#include "xdvdfs.h"

// cast this constant as an unsigned long long in the hopes that the compiler will
// always to the right 64-bit math
#define SECTOR_SIZE 2048ULL
#define NAME_MAX_SIZE 1024
#define XDVD_SIGNATURE "MICROSOFT*XBOX*MEDIA"
#define XDVD_SIGNATURE_SIZE 0x14

// global file description since main program needs to access it
int xbfs_fd;

//! Extract \c xbfsfile structure from FUSE context.
static inline struct xbfsfile *get_xbfsfile_from_context(void)
{
	return (struct xbfsfile *)fuse_get_context()->private_data;
}

// **********************************************************************
// xbfs operations
// **********************************************************************

// **********************************************************************
// FUSE operations
// Here comes FUSE operations, please consult fuse.h for description of
// each operation.
// **********************************************************************

/*!
 * \brief Get file attributes.
 */
static int xbfs_getattr(const char *path, struct stat *stbuf)
{
	return tree_getattr(path, stbuf, get_xbfsfile_from_context()->tree,
		get_xbfsfile_from_context()->fd);
}

/*!
 * \brief File open operation.
 */
static int xbfs_open(const char *path, struct fuse_file_info *fi)
{
	return tree_open(path, fi, get_xbfsfile_from_context()->tree);
}

/*!
 * \brief Read data from an open file.
 */
static int xbfs_read(const char *path, char *buf, size_t size,
		    off_t offset, struct fuse_file_info *fi)
{
	return tree_read(path, buf, size, offset, fi,
		 get_xbfsfile_from_context()->tree,
		 get_xbfsfile_from_context()->fd,
		 &get_xbfsfile_from_context()->mutex);
}

/*!
 * Open directory.
 */
static int xbfs_opendir(const char *path, struct fuse_file_info *fi)
{
	return tree_opendir(path, fi, get_xbfsfile_from_context()->tree);
}

/*!
 * \brief Read directory.
 */
static int xbfs_readdir(const char *path, void *buf,
		       fuse_fill_dir_t filler, off_t offset,
		       struct fuse_file_info *fi)
{
	return tree_readdir(path, buf, filler, offset, fi,
			    get_xbfsfile_from_context()->tree);
}

// xbfs_recurse_file_subtree() calls this function
static void xbfs_recurse_directory(
	char *name_buffer,
	int fd,
	off_t filesystem_base_offset,
	unsigned int dir_entry_sector,
	unsigned int dir_entry_size,
	struct tree *root);

/*!
 * \brief Recurse through a directory structure.
 */
static void xbfs_recurse_file_subtree(
	char *name_buffer,
	int fd,
	off_t filesystem_base_offset,
	unsigned char *dir_entry,
	unsigned int dir_entry_size,
	int filerecord_offset,
	struct tree *root)
{
	unsigned int subtree_offset;
	unsigned int file_sector;
	unsigned int file_size;
	unsigned char file_attributes;
	unsigned char filename_size;
	int is_dir;
	int start_index;
	char name_copy[NAME_MAX_SIZE];

	// if there is not enough data left in the buffer for a minimal file record, get out
	if (filerecord_offset + 0xD >= dir_entry_size)
		return;

	// process left subtree
	subtree_offset = LE_16(&dir_entry[filerecord_offset]) * 4;
	if (subtree_offset)
		xbfs_recurse_file_subtree(name_buffer, fd,
			filesystem_base_offset, dir_entry, dir_entry_size, 
			subtree_offset, root);

	// process file
	file_sector = LE_32(&dir_entry[filerecord_offset + 4]);
	file_size = LE_32(&dir_entry[filerecord_offset + 8]);
	file_attributes = dir_entry[filerecord_offset + 0xC];
	is_dir = file_attributes & 0x10;
	filename_size = dir_entry[filerecord_offset + 0xD];
	strcpy(name_copy, name_buffer);
	start_index = strlen(name_copy);
	memcpy(&name_copy[start_index], &dir_entry[filerecord_offset + 0xE],
		filename_size);
	name_copy[start_index + filename_size] = '\0';

	if (is_dir) {
		name_copy[start_index + filename_size] = '/';
		name_copy[start_index + filename_size + 1] = '\0';
		xbfs_recurse_directory(name_copy, 
			fd,
			filesystem_base_offset,
			file_sector,
			file_size,
			root);
	} else {
		fprintf(stderr, " inserting %s: sector 0x%X, 0x%X bytes, attribute byte = 0x%X, %s\n", 
			name_copy, file_sector, file_size, file_attributes, 
			is_dir ? "directory" : "");

		tree_insert(root, name_copy, strlen(name_copy),
			filesystem_base_offset + file_sector * SECTOR_SIZE, file_size);
	}

	// process right subtree
	subtree_offset = LE_16(&dir_entry[filerecord_offset + 2]) * 4;
	if (subtree_offset)
		xbfs_recurse_file_subtree(name_buffer, fd,
			filesystem_base_offset, dir_entry, dir_entry_size, 
			subtree_offset, root);
}

/*!
 * \brief Recurse through a directory structure.
 */
static void xbfs_recurse_directory(
	char *name_buffer,
	int fd,
	off_t filesystem_base_offset,
	unsigned int dir_entry_sector,
	unsigned int dir_entry_size,
	struct tree *root)
{
	unsigned char *dir_entry;
	off_t current_offset = filesystem_base_offset;

	fprintf(stderr, "loading directory %s @ sector 0x%X, 0x%X bytes\n",
		(name_buffer[0]) ? name_buffer : "(root)",
		dir_entry_sector,
		dir_entry_size);

	// allocate a buffer and load the entire directory entry; round up to the
	// nearest sector
	dir_entry = (unsigned char *)malloc(dir_entry_size);
	if (!dir_entry)
		return;

	current_offset += dir_entry_sector * SECTOR_SIZE;
	lseek(fd, current_offset, SEEK_SET);
	if (read(fd, dir_entry, dir_entry_size) != dir_entry_size) {
		free(dir_entry);
		return;
	}

	xbfs_recurse_file_subtree(name_buffer, fd,
		filesystem_base_offset, dir_entry, dir_entry_size, 
		0, root);

	free(dir_entry);
}

/*!
 * \brief Initialize filesystem.
 */
static void *xbfs_init(void)
{
	char name_buffer[NAME_MAX_SIZE];
	unsigned int root_directory_sector;
	unsigned int root_directory_size;
	unsigned char timestamp[8];
	unsigned char sector_buffer[SECTOR_SIZE];
	off_t filesystem_base_offset = 0;

	struct xbfsfile *xbfs = (struct xbfsfile *)malloc(sizeof(struct xbfsfile));
	if (!xbfs) {
		fprintf(stderr,"not enough memory\n");
		close(xbfs_fd);
		fuse_exit(fuse_get_context()->fuse);
		return NULL;
	}

	xbfs->fd = xbfs_fd;

	// scan sectors until the signature is found
	lseek(xbfs->fd, filesystem_base_offset, SEEK_SET);
	while (1) {
		if (read(xbfs->fd, sector_buffer, SECTOR_SIZE) != SECTOR_SIZE) {
			fprintf(stderr, "XDVD signature (%s) not found\n", XDVD_SIGNATURE);
			close(xbfs->fd);
			free(xbfs);
			fuse_exit(fuse_get_context()->fuse);
			return NULL;
		}
		if (!strncmp((char *)sector_buffer, XDVD_SIGNATURE, XDVD_SIGNATURE_SIZE)) {
			// the actual root of the filesystem is 32 sectors back
			filesystem_base_offset -= (32 * SECTOR_SIZE);

			// process the volume descriptor
			root_directory_sector = LE_32(&sector_buffer[0x14]);
			root_directory_size = LE_32(&sector_buffer[0x18]);
			memcpy(timestamp, &sector_buffer[0x1C], 8);

			fprintf(stderr, "root @ sector 0x%X, 0x%X bytes; time = %02X %02X %02X %02X %02X %02X %02X %02X\n",
				root_directory_sector, root_directory_size,
				timestamp[0], timestamp[1], timestamp[2], timestamp[3],
				timestamp[4], timestamp[5], timestamp[6], timestamp[7]);

			break;
		}
		filesystem_base_offset += SECTOR_SIZE;
	}

	pthread_mutex_init(&xbfs->mutex, NULL);

	xbfs->tree = tree_empty();

	name_buffer[0] = 0;

	// build the tree
	xbfs_recurse_directory(name_buffer, xbfs->fd, filesystem_base_offset,
		root_directory_sector, root_directory_size, xbfs->tree);

	return (void *)xbfs;
}

/*!
 * \brief Clean up filesystem.
 */
static void xbfs_destroy(void *context)
{
	struct xbfsfile *xbfs = (struct xbfsfile *)context;

	if (xbfs) {
		close(xbfs->fd);
		tree_free(xbfs->tree);
		free(xbfs);
	}
}

/*!
 * \brief The FUSE file system operations.
 */
struct fuse_operations xbfs_operations = {
	.getattr = xbfs_getattr,
	.open = xbfs_open,
	.read = xbfs_read,
	.opendir = xbfs_opendir,
	.readdir = xbfs_readdir,
	.init = xbfs_init,
	.destroy = xbfs_destroy,
};
