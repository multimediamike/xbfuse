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
 * \file tree.c
 * \author Janusz Dziemidowicz
 * \brief Directory hierarchy abstraction file.
 */

#include "tree.h"

void tree_insert(struct tree *root, const char *path, int length,
		 off_t offset, long size)
{
	char *pos;
	struct tree *node;

	if (!path || !*path)
		return;

	if ((pos = memchr(path, '/', length))) {
		// Path contains directory.
		*pos = '\0';
		node = root->sub;

		// Check if this directory was already inserted by earlier calls.
		while (node) {
			if (!strcmp(node->name, path)) {
				tree_insert(node, pos + 1,
					    length - (pos + 1 - path),
					    offset, size);
				return;
			}
			node = node->next;
		}

		// Create new directory.
		node = (struct tree *)malloc(sizeof(struct tree));
		node->name = strdup(path);
		node->is_dir = 1;
		node->offset = 0;
		node->size = 0;
		node->nsubdirs = 0;
		node->sub = NULL;
		node->next = root->sub;

		// Connect new directory under current directory.
		root->sub = node;
		root->nsubdirs++;

		// Insert remaining parts of path in new directory.
		tree_insert(node, pos + 1, length - (pos + 1 - path),
			    offset, size);
	} else {
		// No more directories in path. Just create new file
		// under current directory.
		node = (struct tree *)malloc(sizeof(struct tree));
		node->name = strndup(path, length);
		node->is_dir = 0;
		node->offset = offset;
		node->size = size;
		node->nsubdirs = 0;
		node->sub = NULL;
		node->next = root->sub;
		root->sub = node;
	}
}

struct tree *tree_find_entry(struct tree *root, const char *path)
{
	struct tree *node, *ret;
	const char *next;

	if (!root)
		return NULL;

	if (!strcmp(path, root->name))
		return root;

	if (strncmp(path, root->name, strlen(root->name)))
		return NULL;

	next = path + strlen(root->name);
	if (*next != '/')
		return NULL;
	next++;

	if (!*next)
		return root;

	node = root->sub;
	while (node) {
		ret = tree_find_entry(node, next);
		if (ret)
			return ret;

		node = node->next;
	}

	return NULL;
}

void tree_free(struct tree *root)
{
	struct tree *node, *next;

	node = root->sub;
	while (node) {
		next = node->next;
		tree_free(node);
		node = next;
	}

	free(root->name);
	free(root);
}

struct tree *tree_empty(void)
{
	struct tree *ret;

	ret = (struct tree *)malloc(sizeof(struct tree));
	ret->name = strdup("");
	ret->is_dir = 1;
	ret->offset = 0;
	ret->size = 0;
	ret->nsubdirs = 0;
	ret->sub = NULL;
	ret->next = NULL;

	return ret;
}

int tree_getattr(const char *path, struct stat *stbuf, struct tree *root,
		 int fd)
{
	struct tree *node;
	struct stat st;

	node = tree_find_entry(root, path);
	if (node) {
		memset(stbuf, 0, sizeof(struct stat));
		// Set UID and GID to current user
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();
		if (node->is_dir) {
			stbuf->st_mode =
			    S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR
			    | S_IXGRP | S_IXOTH;
			// Directory should have number of links set
			// to 2 + number of subdirectories (not
			// files), this makes find work.
			stbuf->st_nlink = 2 + node->nsubdirs;
		} else {
			stbuf->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
			stbuf->st_nlink = 1;
			stbuf->st_size = node->size;
		}

		// Set all times to the values from PACK file.
		fstat(fd, &st);
		stbuf->st_atime = st.st_atime;
		stbuf->st_mtime = st.st_mtime;
		stbuf->st_ctime = st.st_ctime;
		return 0;
	} else
		return -ENOENT;
}

int tree_open(const char *path, struct fuse_file_info *fi, struct tree *root)
{
	struct tree *node;

	if (fi->flags & O_WRONLY)
		return -EROFS;

	node = tree_find_entry(root, path);

	if (!node)
		return -ENOENT;

	return 0;
}

int tree_read(const char *path, char *buf, size_t size,
	      off_t offset, struct fuse_file_info *fi, struct tree *root,
	      int fd, pthread_mutex_t * mutex)
{
	struct tree *node;
	int ret;

	node = tree_find_entry(root, path);
	if (!node)
		return -ENOENT;

	if (node->is_dir)
		return -EISDIR;

	if (offset >= node->size)
		return 0;
	if (offset + size > node->size)
		size = node->size - offset;

	// This is needed to ensure, that no other thread will change
	// seek point before we call read().
	pthread_mutex_lock(mutex);

	lseek(fd, node->offset + offset, SEEK_SET);
	ret = read(fd, buf, size);

	pthread_mutex_unlock(mutex);

	return ret;
}

int tree_opendir(const char *path, struct fuse_file_info *fi, struct tree *root)
{
	struct tree *node = tree_find_entry(root, path);

	if (!node)
		return -ENOENT;

	if (!node->is_dir)
		return -ENOTDIR;

	return 0;
}

int tree_readdir(const char *path, void *buf,
		 fuse_fill_dir_t filler, off_t offset,
		 struct fuse_file_info *fi, struct tree *root)
{
	struct tree *node;

	node = tree_find_entry(root, path);

	if (!node)
		return -ENOENT;

	if (!node->is_dir)
		return -ENOTDIR;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	node = node->sub;
	while (node) {
		filler(buf, node->name, NULL, 0);
		node = node->next;
	}

	return 0;
}
