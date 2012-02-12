/*
 *  Copyright (C) 2005 Janusz Dziemidowicz (rraptorr@nails.eu.org)
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
 * \file tree.h
 * \author Janusz Dziemidowicz
 * \brief Directory hierarchy abstraction header file.
 */

#ifndef _TREE_H_
#define _TREE_H_

//! This is needed for strndup.
#define _GNU_SOURCE

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

//! FUSE library compliance level.
#define FUSE_USE_VERSION 25
#include <fuse.h>

/*!
 * \brief GRAF directory hierarchy structure.
 *
 * This structure represents directory structure from GRAF. You should
 * insert files into this structure (filenames can contain '/',
 * corresponding directories will be made automatically). There are
 * also FUSE operations on this structure provided, which can be used
 * to quickly implement FUSE filesystem.
 */
struct tree {
	/*!
	 * \brief Filename.
	 *
	 * Should be empty string in the root of the tree. Otherwise
	 * searching function won't work.
	 */
	char *name;

	/*!
	 * \brief Flag indicating that this a directory.
	 */
	char is_dir;

	/*!
	 * \brief Offset of the file inside of GRAF.
	 */
	off_t offset;

	/*!
	 * \brief Size of the file inside of GRAF.
	 */
	off_t size;

	/*!
	 * \brief Number of subdirectories.
	 *
	 * This is used to calculate correct number of hard links,
	 * which for directories should be 2 + number of
	 * subdirectories. This is especially needed to make find
	 * work. It will be incremented when inserting directories.
	 */
	int nsubdirs;

	/*!
	 * \brief Subdirectories.
	 *
	 * If the current file is a regular file this field is NULL. Otherwise
	 * it is a pointer to first element of directory.
	 */
	struct tree *sub;

	/*!
	 * \brief Next file.
	 *
	 * This is a pointer to the next file on the same directory
	 * level.
	 */
	struct tree *next;
};

/*!
 * \brief Insert path into representation of GRAF firectory tree.
 *
 * This function inserts given pathname into the \c tree structure,
 * which describes directory structure inside of GRAF. Every needed
 * subdirectories will be created by this function.
 *
 * \param root \c tree root, at which \c path should be inserted.
 * \param path path that should be inserted. This should be relative
 * name (no '/' in the beginning). Please note that it will be
 * destroyed.
 * \param length length of \c path (as sometimes it is not zero
 * terminated).
 * \param offset offset (from \c packentry) that should be stored in
 * the tree node corresponding to given \c path.
 * \param size size (from \c packentry) that should be stored in the
 * tree node correspoding to given \c path.
 */
void tree_insert(struct tree *root, const char *path, int length,
		 off_t offset, long size);

/*!
 * \brief Find given path in GRAF directory structure.
 *
 * \param root \c tree root, at which searching should be started.
 * \param path path to find, it should be exactly as provided by FUSE
 * (so it should start with '/').
 * \return pointer to node containing information about given \c path
 * or NULL if not found.
 */
struct tree *tree_find_entry(struct tree *root, const char *path);

/*!
 * \brief Free entire GRAF directory hierarchy structure.
 *
 * \param root root of the directory \c tree to be freed.
 */
void tree_free(struct tree *root);

/*!
 * \brief Create empty directory structure.
 *
 * This functions creates empty directory structures with all fields
 * set as needed.
 */
struct tree *tree_empty(void);

/*!
 * \brief FUSE getattr operation.
 *
 * This is FUSE compatible getattr operation which gets file
 * information from \c tree structure.
 * \param path file path.
 * \param stbuf stats will be stored here.
 * \param root tree root.
 * \param fd data file descriptor.
 * \return 0 on success, -errno otherwise.
 */
int tree_getattr(const char *path, struct stat *stbuf, struct tree *root,
		 int fd);

/*!
 * \brief FUSE open operation.
 *
 * This is FUSE compatible open operation which gets file
 * information from \c tree structure.
 * \param path file path.
 * \param fi FUSE file information.
 * \param root tree root.
 * \return 0 on success, -errno otherwise.
 */
int tree_open(const char *path, struct fuse_file_info *fi, struct tree *root);

/*!
 * \brief FUSE read operation.
 *
 * This is FUSE compatible read operation which gets file
 * information from \c tree structure.
 * \param path file path.
 * \param buf read buffer.
 * \param size size of \c buf.
 * \param offset read offset.
 * \param fi FUSE file information.
 * \param root tree root.
 * \param fd data file descriptor.
 * \param mutex file operations mutex related to \c fd.
 * \return number of bytes read on success, -errno otherwise.
 */
int tree_read(const char *path, char *buf, size_t size,
	      off_t offset, struct fuse_file_info *fi, struct tree *root,
	      int fd, pthread_mutex_t * mutex);

/*!
 * \brief FUSE opendir operation.
 *
 * This is FUSE compatible opendir operation which gets file
 * information from \c tree structure.
 * \param path directory path.
 * \param fi FUSE file information.
 * \param root tree root.
 * \return 0 on success, -errno otherwise.
 */
int tree_opendir(const char *path, struct fuse_file_info *fi,
		 struct tree *root);

/*!
 * \brief FUSE readdir operation.
 *
 * This is FUSE compatible readdir operation which gets file
 * information from \c tree structure.
 * \param path directory path.
 * \param buf output buffer.
 * \param filler filler function.
 * \param offset output offset.
 * \param fi FUSE file information.
 * \param root tree root.
 * \return 0 on success, -errno otherwise.
 */
int tree_readdir(const char *path, void *buf,
		 fuse_fill_dir_t filler, off_t offset,
		 struct fuse_file_info *fi, struct tree *root);

#endif				// _TREE_H_
