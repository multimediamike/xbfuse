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
 * \file xbfs.h
 * \author Mike Melanson
 * \brief XBFS file support header file.
 */

#ifndef _XBFS_H_
#define _XBFS_H_

#define _GNU_SOURCE

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*!
 * \brief Basic information about one XBFS file.

 * This structure contains basic information about one XBFS file,
 * including information needed by FUSE system. It is stored as \c
 * private_data of FUSE context.
 */
struct xbfsfile {
	/*!
	 * \brief XBFS file descriptor.
	 */
	int fd;

	/*!
	 * \brief Disk operations mutex.
	 *
	 * This mutex is used to synchronize disk operations on XBFS
	 * file.  Whenever one need to read/write/lseek XBFS file, this
	 * mutex should be locked with \c pthread_mutex_lock(). After
	 * operation has finished (also after an error), this mutex
	 * should be unlocked with \c pthread_mutex_unlock().
	 */
	pthread_mutex_t mutex;

	/*!
	 * \brief Size of the XBFS file.
	 */
	off_t size;

	/*!
	 * \brief Directory tree of the XBFS file.
	 *
	 * This field contains directory tree of the XBFS file (which
	 * doesn't constain any directories, only files). This is
	 * filled by \c xbfs_init().
	 */
	struct tree *tree;
};

extern struct fuse_operations xbfs_operations;

extern int xbfs_fd;

//! Treat given memory address as a 16-bit big-endian integer.
#define BE_16(x)  ((((uint8_t*)(x))[0] << 8) | ((uint8_t*)(x))[1])

//! Treat given memory address as a 32-bit big-endian integer.
#define BE_32(x)  ((((uint8_t*)(x))[0] << 24) | \
                   (((uint8_t*)(x))[1] << 16) | \
                   (((uint8_t*)(x))[2] << 8) | \
                    ((uint8_t*)(x))[3])

//! Treat given memory address as a 16-bit little-endian integer.
#define LE_16(x)  ((((uint8_t*)(x))[1] << 8) | ((uint8_t*)(x))[0])

//! Treat given memory address as a 32-bit little-endian integer.
#define LE_32(x)  ((((uint8_t*)(x))[3] << 24) | \
                   (((uint8_t*)(x))[2] << 16) | \
                   (((uint8_t*)(x))[1] << 8) | \
                    ((uint8_t*)(x))[0])

#endif				// _XBFS_H_
