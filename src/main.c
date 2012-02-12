/*
 *  Copyright (C) 2006 Mike Melanson (mike at multimedia.cx)
 *    based on code by:
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
 * \file main.c
 * \author Mike Melanson
 * \brief Main program function.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//! FUSE library compliance level.
#define FUSE_USE_VERSION 25
#include <fuse.h>

#include "xdvdfs.h"

/*!
 * \brief Flag indicating wheter we should run in quiet mode (0 - no,
 * all other values - yes).
 */
int quiet;

/*!
 * \brief Main function.
 */
int main(int argc, char *argv[])
{
	char **nargv;
	int nargc, i, j;

	if (argc < 3) {
		fprintf
		    (stderr,
		     "Usage: %s <archive_file> <mount_point> [<options>] [<FUSE library options>]\n\n",
		     argv[0]);
		fprintf(stderr, "Available options:\n");
		fprintf(stderr,
			"\t-q - quiet mode (print only error messages)\n");
		exit(EXIT_FAILURE);
	}

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-q")) {
			quiet = 1;
			for (j = i; j < argc - 1; j++)
				argv[j] = argv[j + 1];
			argc--;
			break;
		}
	}

	nargc = argc - 1;
	nargv = (char **)malloc(nargc * sizeof(char *));

	// skip the resource filename
	nargv[0] = argv[0];
	for (i = 1; i < nargc; i++)
		nargv[i] = argv[i + 1];

	// try to open the file
	xbfs_fd = open(argv[1], O_RDONLY);
	if (xbfs_fd < 0) {
		perror(argv[1]);
		exit(EXIT_FAILURE);
	}

	return fuse_main(nargc, nargv, &xbfs_operations);
}
