/*
 * file:        misc.c
 * description: various support functions for CS 5600/7600 file system
 *              startup argument parsing and checking, etc.
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, November 2016
 */

#define FUSE_USE_VERSION 27
#define _XOPEN_SOURCE 500
#define _ATFILE_SOURCE
#define _BSD_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <sys/types.h>
#include <fuse.h>

#include "fsx600.h"		/* only for FS_BLOCK_SIZE */

/*********** DO NOT MODIFY THIS FILE *************/

/* All disk I/O is accessed through these functions
 */
static int disk_fd;

/* read blocks from disk image. Returns -EIO if error, 0 otherwise
 */
int block_read(char *buf, int lba, int nblks)
{
    int len = nblks * FS_BLOCK_SIZE, start = lba * FS_BLOCK_SIZE;

    if (lseek(disk_fd, start, SEEK_SET) < 0)
        return -EIO;
    if (read(disk_fd, buf, len) != len)
        return -EIO;
    return 0;
}

/* write blocks from disk image. Returns -EIO if error, 0 otherwise
 */
int block_write(char *buf, int lba, int nblks)
{
    int len = nblks * FS_BLOCK_SIZE, start = lba * FS_BLOCK_SIZE;
    
    /* Seek to the *end* of the region being written, to make sure it
     * all fits on the disk image. Then seek to write location.
     */
    if (lseek(disk_fd, start + len, SEEK_SET) < 0)
        return -EIO;
    if (lseek(disk_fd, start, SEEK_SET) < 0)
        return -EIO;
    if (write(disk_fd, buf, len) != len)
        return -EIO;
    return 0;
}

void block_init(char *file)
{
    if (strlen(file) < 4 || strcmp(file+strlen(file)-4, ".img") != 0) {
        printf("bad image file (must end in .img): %s\n", file);
        exit(1);
    }
    if ((disk_fd = open(file, O_RDWR)) < 0) {
        printf("cannot open image file '%s': %s\n", file, strerror(errno));
        exit(1);
    }
}
    
/* All homework functions are accessed through the operations
 * structure.  
 */
extern struct fuse_operations fs_ops;

struct data {
    char *image_name;
    int   part;
    int   cmd_mode;
} _data;

/**************/

#ifdef REAL_FS
/*
 * See comments in /usr/include/fuse/fuse_opts.h for details of 
 * FUSE argument processing.
 * 
 *  usage: ./homework -image disk.img [-part #] directory
 *              disk.img  - name of the image file to mount
 *              directory - directory to mount it on
 */
static struct fuse_opt opts[] = {
    {"-image %s", offsetof(struct data, image_name), 0},
    FUSE_OPT_END
};

int main(int argc, char **argv)
{
    /* Argument processing and checking
     */
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    if (fuse_opt_parse(&args, &_data, opts, NULL) == -1)
	exit(1);

    block_init(_data.image_name);

    return fuse_main(args.argc, args.argv, &fs_ops, NULL);
}
#endif
