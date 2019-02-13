/*
 * file:        mkfs-hw3.c
 * description: mkfs for cs5600 file system assignment
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#include "fsx600.h"

unsigned char *disk;

/* handle K/M
 */
int parseint(char *s)
{
    int n = strtol(s, &s, 0);
    if (tolower(*s) == 'k')
        return n * 1024;
    if (tolower(*s) == 'm')
        return n * 1024 * 1024;
    return n;
}

/* bitmap functions
 */
void bit_set(unsigned char *map, int i)
{
    map[i/8] |= (1 << (i%8));
}

/* usage: mkfs-hw3 [-size #] file.img
 * If file doesn't exist, create with size '#' (K and M suffixes allowed)
 */
int main(int argc, char **argv)
{
    int i, fd = -1, size = 0;
    if (!strcmp(argv[1], "-size") && argc >= 3) {
        size = parseint(argv[2]);
        argv += 2;
        argc -= 2;
    }

    if (argc == 2) {
        fd = open(argv[1], O_WRONLY | O_CREAT, 0777);
        if (fd >= 0 && size == 0) {
            struct stat sb;
            fstat(fd, &sb);
            size = sb.st_size;
        }
    }
    if (fd < 0) {
        printf("usage: mkfs-hw3 [-size #] file.img\n");
        exit(1);
    }

    if (size % FS_BLOCK_SIZE != 0)
        printf("WARNING: disk size not a multiple of block size: %d (0x%x)\n",
               size, size);
    if (size == 0) {
	printf("ERROR: you must specify a size when creating an image\n");
	exit(1);
    }

    /* calculate number of disk blocks, how many blocks for the inode
     * and block bitmaps, how many blocks for the inodes themselves.
     */
    int n_blks = size / FS_BLOCK_SIZE;
    int n_map_blks = DIV_ROUND_UP(n_blks, 8*FS_BLOCK_SIZE);
    int n_inos = n_blks / 4;
    int n_ino_map_blks = DIV_ROUND_UP(n_inos, 8*FS_BLOCK_SIZE);
    int n_ino_blks = DIV_ROUND_UP(n_inos*sizeof(struct fs_inode),
                                  FS_BLOCK_SIZE);

    /* create an image of the disk in memory and write it to disk when
     * we're done
     */
    disk = malloc(n_blks * FS_BLOCK_SIZE);
    memset(disk, 0, n_blks * FS_BLOCK_SIZE);

    struct fs_super *sb = (void*)disk;

    int inode_map_base = 1;
    unsigned char *inode_map = (disk + inode_map_base*FS_BLOCK_SIZE);

    int block_map_base = inode_map_base + n_ino_map_blks;
    unsigned char *block_map = (disk + block_map_base*FS_BLOCK_SIZE);
    
    int inode_base = block_map_base + n_map_blks;
    struct fs_inode *inodes = (void*)(disk + inode_base*FS_BLOCK_SIZE);

    int rootdir_base = inode_base + n_ino_blks;

    /* superblock */
    *sb = (struct fs_super){.magic = FS_MAGIC, .inode_map_sz = n_ino_map_blks,
                            .inode_region_sz = n_ino_blks,
                            .block_map_sz = n_map_blks,
                            .num_blocks = n_blks, .root_inode = 1};

    /* bitmaps */
    bit_set(inode_map, 0);
    bit_set(inode_map, 1);
    for (i = 0; i <= rootdir_base; i++)
        bit_set(block_map, i);

    int t  = time(NULL);
    inodes[1] = (struct fs_inode){.uid = 1001, .gid = 125, .mode = 0040777, 
                                  .ctime = t, .mtime = t, .size = 1024,
                                  .direct = {rootdir_base, 0, 0, 0, 0, 0},
                                  .indir_1 = 0, .indir_2 = 0};

    /* remember (from /usr/include/i386-linux-gnu/bits/stat.h)
     *    S_IFDIR = 0040000 - directory
     *    S_IFREG = 0100000 - regular file
     */
    /* block 0 - superblock  [layout for 1MB file]
     *       1 - inode map
     *       2 - block map
     *       3,4,5,6 - inodes
     *       7 - root directory (inode 1)
     */
                      

    assert(size == n_blks* FS_BLOCK_SIZE);
    write(fd, disk, size);
    close(fd);

    return 0;
}

