/*
 * file:        read-img.c
 * description: read and print summary of cs5600/cs7600 file system
 *              volume. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include "fsx600.h"

/* bitmap functions
 */
void bit_set(unsigned char *map, int i)
{
    map[i/8] |= (1 << (i%8));
}
void bit_clear(unsigned char *map, int i)
{
    map[i/8] &= ~(1 << (i%8));
}
int bit_test(unsigned char *map, int i)
{
    return map[i/8] & (1 << (i%8));
}

int main(int argc, char **argv)
{
    int i, j, fd = open(argv[1], O_RDONLY);
    if (fd < 0)
        perror("can't open"), exit(1);
    struct stat _sb;
    if (fstat(fd, &_sb) < 0)
        perror("fstat"), exit(1);
    int size = _sb.st_size;

    void *disk = malloc(size);
    if (read(fd, disk, size) != size)
        perror("read"), exit(1);
    unsigned char *blkmap = calloc(size/8192, 1);
    unsigned char *imap = calloc(size/8192, 1);

    struct fs_super *sb = (void*)disk;
    printf("superblock: magic:  %08x\n"
           "            imap:   %d blocks\n" 
           "            bmap:   %d blocks\n"
           "            inodes: %d blocks\n" 
           "            blocks: %d\n"
           "            root inode: %d\n\n", sb->magic, sb->inode_map_sz,
           sb->block_map_sz, sb->inode_region_sz, sb->num_blocks, sb->root_inode);

    printf("allocated inodes: ");
    unsigned char *inode_map = (void*)disk + FS_BLOCK_SIZE;
    char *comma = "";
    for (i = 0; i < sb->inode_map_sz * 8192; i++)
        if (bit_test(inode_map, i)) {
            printf("%s %d", comma, i);
            comma = ",";
        }
    printf("\n\n");

    printf("allocated blocks: ");
    unsigned char *block_map = (void*)inode_map + sb->inode_map_sz * FS_BLOCK_SIZE;
    for (comma = "", i = 0; i < sb->block_map_sz * 8192; i++)
        if (bit_test(block_map, i)) {
            printf("%s %d", comma, i);
            comma = ",";
        }
        printf("\n\n");

    struct fs_inode *inodes = (void*)block_map + sb->block_map_sz * FS_BLOCK_SIZE;

    int max_inodes = sb->inode_region_sz * INODES_PER_BLK;
    struct entry { int dir; int inum;} inode_list[max_inodes + 100];
    int head = 0, tail = 0;

    inode_list[head++] = (struct entry){.dir=1, .inum=1};
    bit_set(imap, 1);
    while (head != tail) {
        struct entry e = inode_list[tail++];
        struct fs_inode *in = inodes + e.inum;
        if (!e.dir) {
            printf("file: inode %d\n"
                   "      uid/gid %d/%d\n"
                   "      mode %08o\n"
                   "      size  %d\n",
                   e.inum, in->uid, in->gid, in->mode, in->size);
            printf("blocks: ");
            for (i = 0; i < 6; i++)
                if (in->direct[i]) {
                    printf("%d ", in->direct[i]);
                    bit_set(blkmap, in->direct[i]);
                    if (!bit_test(block_map, in->direct[i]))
                        printf("\n***ERROR*** block %d marked free\n", in->direct[i]);
                }
            if (in->indir_1) {
                int *buf = disk + in->indir_1 * FS_BLOCK_SIZE;
                for (i = 0; i < 256; i++)
                    if (buf[i]) {
                        printf("%d ", buf[i]);
                        bit_set(blkmap, buf[i]);
                        if (!bit_test(block_map, buf[i]))
                            printf("\n***ERROR*** block %d marked free\n", buf[i]);
                    }
            }
            if (in->indir_2) {
                int *buf2 = disk + in->indir_2 * FS_BLOCK_SIZE;
                for (i = 0; i < 256; i++) {
                    if (buf2[i])
                    {
                        int *buf = disk + buf2[i] * FS_BLOCK_SIZE;
                        for (j = 0; j < 256; j++) {
                            if (buf[j]) {
                                printf("%d ", buf[j]);
                                bit_set(blkmap, buf[j]);
                                if (!bit_test(block_map, buf[j]))
                                    printf("\n***ERROR*** block %d marked free\n", buf[j]);
                            }
                        }
                    }
                }
            }
            printf("\n\n");
        }
        else {
            if (!S_ISDIR(in->mode)) {
                printf("***ERROR*** inode %d not a directory\n", e.inum);
                continue;
            }
            printf("directory: inode %d (block %d)\n", e.inum, in->direct[0]);
            struct fs_dirent *de = disk + in->direct[0] * FS_BLOCK_SIZE;
            if (!bit_test(block_map, in->direct[0]))
                printf("\n***ERROR*** block %d marked free\n", in->direct[0]);
            bit_set(blkmap, in->direct[0]);
            
            for (i = 0; i < 32; i++)
                if (de[i].valid) {
                    printf("  %s %d %s\n", de[i].isDir ? "D" : "F", de[i].inode,
                           de[i].name);
                    int j = de[i].inode;
                    if (j < 0 || j >= sb->inode_region_sz * 16) {
                        printf("***ERROR*** invalid inode %d\n", j);
                        continue;
                    }
                    if (bit_test(imap, j)) {
                        printf("***ERROR*** loop found (inode %d)\n", e.inum);
                        goto fail;
                    }
                    bit_set(imap, j);
                    if (!bit_test(inode_map, j))
                        printf("***ERROR*** inode %d is marked free\n", j);
                    inode_list[head++] = (struct entry) {.dir = de[i].isDir, j};
                }
            printf("\n");
        }
    }

    printf("unreachable inodes: ");
    for (i = 1; i < sb->inode_region_sz * 16; i++)
        if (!bit_test(imap, i) && bit_test(inode_map, i))
            printf("%d ", i);
    printf("\n");

    printf("unreachable blocks: ");
    for (i = 1 + sb->inode_map_sz + sb->block_map_sz + sb->inode_region_sz;
         i < sb->num_blocks; i++)
        if (bit_test(blkmap, i) && !bit_test(block_map, i))
            printf("%d ", i);
    printf("\n");

fail:
    return 0;
}
