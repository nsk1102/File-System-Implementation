/*
 * file:        homework.c
 * description: skeleton file for CS 5600 file system
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, November 2018
 */

#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64
#define MAX_PATH_LEN 10
#define MAX_NAME_LEN 27
#define ROOT_INODE 1

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <libgen.h>

#include "fsx600.h"

/* disk access. All access is in terms of 1024-byte blocks; read and
 * write functions return 0 (success) or -EIO.
 */
extern int block_read(void *buf, int lba, int nblks);
extern int block_write(void *buf, int lba, int nblks);

struct fs_super sb;
unsigned char *inode_map;
unsigned char *block_map;
struct fs_inode *inodes;

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

/* init - this is called once by the FUSE framework at startup. Ignore
 * the 'conn' argument.
 * recommended actions:
 *   - read superblock
 *   - allocate memory, read bitmaps and inodes
 */
void* fs_init(struct fuse_conn_info *conn)
{
    if (block_read(&sb, 0, 1) < 0)
        exit(1);

    /* your code here */
    inode_map = (unsigned char *) malloc(sb.inode_map_sz * FS_BLOCK_SIZE);
    block_map = (unsigned char *) malloc(sb.block_map_sz * FS_BLOCK_SIZE);
    inodes = (struct fs_inode *) malloc(sb.inode_region_sz * FS_BLOCK_SIZE);

    if (block_read(inode_map, 1, sb.inode_map_sz) < 0)
        exit(1);

    if (block_read(block_map, 1 + sb.inode_map_sz, sb.block_map_sz) < 0)
        exit(1);

    if (block_read(inodes, 1 + sb.inode_map_sz + sb.block_map_sz, sb.inode_region_sz) < 0)
        exit(1);

    return NULL;
}

/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path doesn't exist
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */


/* note on splitting the 'path' variable:
 * the value passed in by the FUSE framework is declared as 'const',
 * which means you can't modify it. The standard mechanisms for
 * splitting strings in C (strtok, strsep) modify the string in place,
 * so you have to copy the string and then free the copy when you're
 * done. One way of doing this:
 *
 *    char *_path = strdup(path);
 *    int inum = translate(_path);
 *    free(_path);
 */
int parse(char *path, char **argv)
{
    int i;
    for (i = 0; i < MAX_PATH_LEN; i++) {
        if ((argv[i] = strtok(path, "/")) == NULL)
            break;
        if (strlen(argv[i]) > MAX_NAME_LEN)
            argv[i][MAX_NAME_LEN] = 0;        // truncate to 27 characters
        path = NULL;
    }
    return i;
}

int lookup(int inum, char *name)
{
    if (!S_ISDIR(inodes[inum].mode))
        return -ENOTDIR;
    struct fs_dirent *fd;
    fd = (struct fs_dirent *) malloc(FS_BLOCK_SIZE);
    if (block_read(fd, inodes[inum].direct[0], 1) < 0)
        exit(1);
    int i, result;
    for (i = 0; i < 32; i++)
    {
        if (fd[i].valid && strcmp(fd[i].name, name) == 0)
        {    
            result = fd[i].inode;
            break;
        }
        if (i == 31)
            result = -ENOENT;
    }
    free(fd);
    return result;

}

int translate(char *path)    // need to copy path *before* calling
{
    int i, inum = ROOT_INODE;  // 1
    char *names[MAX_PATH_LEN];

    int pathlen = parse(path, names);
    for (i = 0; i < pathlen && inum > 0; i++) {
        if (i == 0 && strcmp(names[i], ".") == 0)
            continue;
        inum = lookup(inum, names[i]);
    }
    return inum;
}

void fill_stat(struct stat * sb, int inum)
{
    memset(sb, 0, sizeof(*sb));
    sb->st_ino = inum;
    sb->st_mode = inodes[inum].mode;
    sb->st_uid = inodes[inum].uid;
    sb->st_gid = inodes[inum].gid;
    sb->st_size = inodes[inum].size;
    sb->st_ctime = inodes[inum].ctime;
    sb->st_mtime = inodes[inum].mtime;
    sb->st_atime = sb->st_mtime;
    sb->st_nlink = 1;
}

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - fields not provided in fsx600 are:
 *    st_nlink - always set to 1
 *    st_atime - set to same value as st_mtime
 *
 * errors - path translation, ENOENT
 */
static int fs_getattr(const char *path, struct stat *sb)
{
    char *_path = strdup(path);
    int inum = translate(_path); 
    if (inum < 0)
        return inum;
    fill_stat(sb, inum);
    return 0;
}

/* readdir - get directory contents.
 *
 * for each entry in the directory, invoke the 'filler' function,
 * which is passed as a function pointer, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a pointer to struct stat, just like in getattr.
 *
 * Errors - path resolution, ENOTDIR, ENOENT
 */
static int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
    char *_path = strdup(path);
    int inum = translate(_path); 
    if (inum < 0)
        return inum;
    if (!S_ISDIR(inodes[inum].mode))
        return -ENOTDIR;

    struct fs_dirent *fd;
    fd = (struct fs_dirent *) malloc(FS_BLOCK_SIZE);
    struct stat sb;
    if (block_read(fd, inodes[inum].direct[0], 1) < 0)
        exit(1);
    int i;
    for (i = 0; i < 32; i++)
    {
        if (fd[i].valid)
        {    
            fill_stat(&sb, fd[i].inode);
            filler(ptr, fd[i].name, &sb, 0);
        }
    }
    free(fd);
    return 0;
}

void split_path(const char *path, char *parent, char *leaf)
{
    char *_path = strdup(path);
    strcpy(leaf, basename(_path));
    if (strlen(leaf) > MAX_NAME_LEN)
        leaf[MAX_NAME_LEN] = 0; 
    _path = strdup(path);
    strcpy(parent, dirname(_path));
}

int get_next_inode()
{
    int i, num_inodes = sb.inode_region_sz * FS_BLOCK_SIZE / sizeof(struct fs_inode);
    for (i = 0; i < num_inodes; i++) {
        if (!bit_test(inode_map, i)) {
            return i;
        }
    }
    return -ENOSPC;
}

int get_next_block()
{
    int i;
    for (i = 0; i < sb.num_blocks; i++) {
        if (!bit_test(block_map, i)) {
            return i;
        }
    }
    return -ENOSPC;

}

void update_disk()
{
    if (block_write(inode_map, 1, sb.inode_map_sz) < 0)
        exit(1);

    if (block_write(block_map, 1 + sb.inode_map_sz, sb.block_map_sz) < 0)
        exit(1);

    if (block_write(inodes, 1 + sb.inode_map_sz + sb.block_map_sz, sb.inode_region_sz) < 0)
        exit(1);
}

int create(const char *path, mode_t mode, int isDir)
{
    char *parent = malloc(1000 * sizeof(char));
    char *leaf = malloc(28 * sizeof(char));

    split_path(path, parent, leaf);     
    int inum = translate(parent);
    if (inum < 0)
        return inum;
    if (!S_ISDIR(inodes[inum].mode))
        return -ENOTDIR;
    char *_path = strdup(path);
    if (translate(_path) > 0)
        return -EEXIST;

    int i;
    struct fs_dirent *fd;
    fd = (struct fs_dirent *) malloc(FS_BLOCK_SIZE);
    if (block_read(fd, inodes[inum].direct[0], 1) < 0)
        exit(1);
    for (i = 0; i < 32; i++)
    {
        if (!fd[i].valid)
            break;
        if (i == 31)
        {
            free(fd);
            return -ENOSPC;
        }
    }

    int next_inode = get_next_inode();
    int next_block = get_next_block();
    if (next_inode < 0 || next_block < 0)
        return -ENOSPC;

    bit_set(inode_map, next_inode);

    struct fuse_context *ctx = fuse_get_context();
    inodes[next_inode].uid = ctx->uid;
    inodes[next_inode].gid = ctx->gid;
    inodes[next_inode].mode = mode;
    inodes[next_inode].ctime = time(NULL);
    inodes[next_inode].mtime = inodes[next_inode].ctime;
    inodes[next_inode].size = 0;

    fd[i].valid = 1;
    fd[i].isDir = isDir;
    fd[i].inode = next_inode;
    strcpy(fd[i].name, leaf);
    if (block_write(fd, inodes[inum].direct[0], 1) < 0) {
        exit(1);
    }

    if (isDir)
    {
        bit_set(block_map, next_block);
        inodes[next_inode].direct[0] = next_block;
        inodes[next_inode].size = FS_BLOCK_SIZE;
    }

    update_disk();
    free(fd);
    return 0;
}

/* create - create a new file with specified permissions
 *
 * Errors - path resolution, EEXIST
 *          in particular, for create("/a/b/c") to succeed,
 *          "/a/b" must exist, and "/a/b/c" must not.
 *
 * Note that 'mode' will already have the S_IFREG bit set, so you can
 * just use it directly.
 *
 * If a file or directory of this name already exists, return -EEXIST.
 * If there are already 32 entries in the directory (i.e. it's filled an
 * entire block), you are free to return -ENOSPC instead of expanding it.
 */
static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    return create(path, mode | S_IFREG, 0);
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create. 
 *
 * If this would result in >32 entries in a directory, return -ENOSPC
 * Note that (unlike the 'create' case) 'mode' needs to be OR-ed with S_IFDIR.
 *
 * Note that you may want to combine the logic of fs_create and
 * fs_mkdir. 
 */ 
static int fs_mkdir(const char *path, mode_t mode)
{
    return create(path, mode | S_IFDIR, 1);
}


/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * Errors - path resolution, ENOENT.
 */
static int fs_chmod(const char *path, mode_t mode)
{
    char *_path = strdup(path);
    int inum = translate(_path); 
    if (inum < 0)
        return inum;
    inodes[inum].mode = S_ISDIR(inodes[inum].mode) ? mode | S_IFDIR : mode | S_IFREG;
    update_disk();
    return 0;

}

int fs_utime(const char *path, struct utimbuf *ut)
{
    char *_path = strdup(path);
    int inum = translate(_path); 
    if (inum < 0)
        return inum;
    inodes[inum].mtime = ut->modtime;
    update_disk();
    return 0;
}

void read_helper(char *data, char *buf, size_t *len, off_t offset, int times, 
    int size, int *offset_cnt, int *cnt, uint32_t *addr)
{
    if (*offset_cnt + FS_BLOCK_SIZE * times < offset)
    {
        *(offset_cnt) += FS_BLOCK_SIZE * times;
        return;
    }
    int i, j;
    for (i = 0; i < times; i++, addr++)
    {
        if (*offset_cnt + FS_BLOCK_SIZE < offset)
        {
            *(offset_cnt) += FS_BLOCK_SIZE;
            continue;
        }
        if (block_read(data, *addr, 1) < 0)
            exit(1);
        for (j = 0; j < FS_BLOCK_SIZE; j++)
        {
            if (*offset_cnt < offset) 
            {
                (*offset_cnt)++;
                continue;
            }     
            if (offset + *cnt >= size || *len <= 0) 
                break;
            *buf++ = *(data + j);
            (*cnt)++;
            (*len)--;
            
        }
        if (offset + *cnt >= size || *len <= 0) 
            break;
    }
}

/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= file len, return 0
 *   - if offset+len > file len, return bytes from offset to EOF
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
static int fs_read(const char *path, char *buf, size_t len, off_t offset,
		    struct fuse_file_info *fi)
{
    char *_path = strdup(path);
    int inum = translate(_path); 
    if (inum < 0)
        return inum;
    if (S_ISDIR(inodes[inum].mode))
        return -EISDIR;
    int size = inodes[inum].size;
    if (offset >= size)
        return 0;

    int i, offset_cnt = 0, cnt = 0;
    char *data = malloc(FS_BLOCK_SIZE);
    // read direct blocks
    read_helper(data, buf, &len, offset, 6, size, &offset_cnt, &cnt, inodes[inum].direct);
    // read indirect_1 blocks
    if (offset + cnt < size && len > 0) 
    {
        uint32_t *indir_1 = malloc(FS_BLOCK_SIZE);
        if (block_read(indir_1, inodes[inum].indir_1, 1) < 0)
            exit(1);
        read_helper(data, buf + cnt, &len, offset, 256, size, &offset_cnt, &cnt, indir_1);
        // read indrect_2 blocks
        if (offset + cnt < size && len > 0) 
        {
            uint32_t *indir_2 = malloc(FS_BLOCK_SIZE);
            if (block_read(indir_2, inodes[inum].indir_2, 1) < 0)
                exit(1);
            for (i = 0; i < 256; i++)
            {
                if (offset_cnt + 256 * FS_BLOCK_SIZE < offset)
                {
                    offset_cnt += 256 * FS_BLOCK_SIZE;
                    continue;
                }
                if (block_read(indir_1, indir_2[i], 1) < 0)
                    exit(1);
                read_helper(data, buf + cnt, &len, offset, 256, size, &offset_cnt, &cnt, indir_1);
                if (offset + cnt >= size || len <= 0) 
                    break;
            }
            free(indir_2);
        }
        free(indir_1);
    }
    free(data);
    return cnt;
}

int write_helper(char *data, const char *buf, size_t *len, off_t offset, int times, 
    int size, int *offset_cnt, int *cnt, uint32_t *addr)
{
    if (*offset_cnt + FS_BLOCK_SIZE * times < offset)
    {
        *(offset_cnt) += FS_BLOCK_SIZE * times;
        return 0;
    }
    int i, j;
    for (i = 0; i < times && *len > 0; i++, addr++)
    {
        if (*offset_cnt + FS_BLOCK_SIZE < offset)
        {
            *(offset_cnt) += FS_BLOCK_SIZE;
            continue;
        } 
        if (*addr == 0)
        {
            int next_block = get_next_block();
            if (next_block < 0)
                return -ENOSPC;
            bit_set(block_map, next_block);
            *addr = next_block;
        }
        if (block_read(data, *addr, 1) < 0)
            exit(1);
        for (j = 0; j < FS_BLOCK_SIZE && *len > 0; j++)
        {
            if (*offset_cnt < offset) 
            {
                (*offset_cnt)++;
                continue;
            }
            *(data + j) = *buf++;
            (*cnt)++;
            (*len)--;
        }
        if (block_write(data, *addr, 1) < 0)
            exit(1);
    }
    return 0;
}

/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them, 
 *   but we don't)
 */
static int fs_write(const char *path, const char *buf, size_t len,
		     off_t offset, struct fuse_file_info *fi)
{
    char *_path = strdup(path);
    int inum = translate(_path); 
    if (inum < 0)
        return inum;
    if (S_ISDIR(inodes[inum].mode))
        return -EISDIR;
    int size = inodes[inum].size;
    if (offset > size)
        return -EINVAL;

    int i, offset_cnt = 0, cnt = 0;
    char *data = malloc(FS_BLOCK_SIZE);
    // read direct blocks
    if (write_helper(data, buf, &len, offset, 6, size, &offset_cnt, &cnt, inodes[inum].direct) < 0)
    {    
        free(data);
        return -ENOSPC;
    }
    // read indirect_1 blocks
    if (len > 0) 
    {
        if (inodes[inum].indir_1 == 0)
        {
            int next_block = get_next_block();
            if (next_block < 0)
            {
                free(data);
                return -ENOSPC;
            }
            bit_set(block_map, next_block);
            inodes[inum].indir_1 = next_block;
        }
        uint32_t *indir_1 = malloc(FS_BLOCK_SIZE);
        if (block_read(indir_1, inodes[inum].indir_1, 1) < 0)
            exit(1);
        if (write_helper(data, buf + cnt, &len, offset, 256, size, &offset_cnt, &cnt, indir_1) < 0)
        {
            free(data);
            free(indir_1);
            return -ENOSPC;
        }
        // read indrect_2 blocks
        if (offset_cnt + cnt > size && block_write(indir_1, inodes[inum].indir_1, 1) < 0)
            exit(1);
        if (len > 0) 
        {
            if (inodes[inum].indir_2 == 0)
            {
                int next_block = get_next_block();
                if (next_block < 0)
                {
                    free(data);
                    free(indir_1);
                    return -ENOSPC;
                }
                bit_set(block_map, next_block);
                inodes[inum].indir_2 = next_block;
            }
            uint32_t *indir_2 = malloc(FS_BLOCK_SIZE);
            if (block_read(indir_2, inodes[inum].indir_2, 1) < 0)
                exit(1);
            for (i = 0; i < 256 && len > 0; i++)
            {
                if (offset_cnt + 256 * FS_BLOCK_SIZE < offset)
                {
                    offset_cnt += 256 * FS_BLOCK_SIZE;
                    continue;
                }
                if (indir_2[i] == 0)
                {
                    int next_block = get_next_block();
                    if (next_block < 0)
                    {
                        free(data);
                        free(indir_1);
                        free(indir_2);
                        return -ENOSPC;
                    }
                    bit_set(block_map, next_block);
                    indir_2[i] = next_block;
                }
                if (block_read(indir_1, indir_2[i], 1) < 0)
                    exit(1);
                if (write_helper(data, buf + cnt, &len, offset, 256, size, &offset_cnt, &cnt, indir_1) < 0)
                {
                    free(data);
                    free(indir_1);
                    free(indir_2);
                    return -ENOSPC;
                }
                if (offset_cnt + cnt > size && block_write(indir_1, indir_2[i], 1) < 0)
                    exit(1);
            }
            if (offset_cnt + cnt > size && block_write(indir_2, inodes[inum].indir_2, 1) < 0)
                exit(1);
            free(indir_2);
        }
        free(indir_1);
    }
    free(data);
    inodes[inum].size = offset + cnt > size ? offset + cnt : size;
    inodes[inum].mtime = time(NULL);
    update_disk();
    return cnt;;
}


/* truncate - truncate file to exactly 'len' bytes
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
static int fs_truncate(const char *path, off_t len)
{
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
    if (len != 0)
	   return -EINVAL;		/* invalid argument */

    char *_path = strdup(path);
    int inum = translate(_path); 
    if (inum < 0)
        return inum;
    if (S_ISDIR(inodes[inum].mode))
        return -EISDIR;

    int i, block_num;
    void *block = malloc(FS_BLOCK_SIZE);
    memset(block, 0, FS_BLOCK_SIZE);
    for (i = 0; i < 6; i++)
    {
        block_num = inodes[inum].direct[i];
        if (block_num == 0)
            break;
        bit_clear(block_map, block_num);
        block_write(block, block_num, 1);
        inodes[inum].direct[i] = 0;
    }

    uint32_t *indir_1 = malloc(FS_BLOCK_SIZE);
    uint32_t *indir_2 = malloc(FS_BLOCK_SIZE);
    if (inodes[inum].indir_1 != 0)
    {
        if (block_read(indir_1, inodes[inum].indir_1, 1) < 0)
            exit(1);
        for (i = 0; i < 256; i++)
        {
            block_num = indir_1[i];
            if (block_num == 0)
                break;
            bit_clear(block_map, block_num);
            block_write(block, block_num, 1);
        }
        bit_clear(block_map, inodes[inum].indir_1);
        block_write(block, inodes[inum].indir_1, 1);
        inodes[inum].indir_1 = 0;
    }

    if (inodes[inum].indir_2 != 0)
    {
        if (block_read(indir_2, inodes[inum].indir_2, 1) < 0)
            exit(1);
        for (i = 0; i < 256; i++)
        {
            int indir_num = indir_2[i];
            if (indir_num == 0)
                break;
            if (block_read(indir_1, indir_num, 1) < 0)
                exit(1);
            for (i = 0; i < 256; i++)
            {
                block_num = indir_1[i];
                if (block_num == 0)
                    break;
                bit_clear(block_map, block_num);
                block_write(block, block_num, 1);
            }
            bit_clear(block_map, indir_num);
            block_write(block, indir_num, 1);
        }
        bit_clear(block_map, inodes[inum].indir_2);
        block_write(block, inodes[inum].indir_2, 1);
        inodes[inum].indir_2 = 0;
    }

    free(block);
    free(indir_1);
    free(indir_2);
    inodes[inum].size = 0;
    inodes[inum].mtime = time(NULL);
    update_disk();
    return 0;
}

/* unlink - delete a file
 *  Errors - path resolution, ENOENT, EISDIR
 * Note that you have to delete (i.e. truncate) all the data.
 */
static int fs_unlink(const char *path)
{
    int num = fs_truncate(path, 0);
    if (num < 0)
        return num;

    char *parent = malloc(1000 * sizeof(char));
    char *leaf = malloc(28 * sizeof(char));

    split_path(path, parent, leaf); 
    int inum = translate(parent);

    int i;
    struct fs_dirent *fd;
    fd = (struct fs_dirent *) malloc(FS_BLOCK_SIZE);
    if (block_read(fd, inodes[inum].direct[0], 1) < 0)
        exit(1);
    for (i = 0; i < 32; i++)
    {
        if (fd[i].valid && strcmp(fd[i].name, leaf) == 0)
        {  
            fd[i].valid = 0;
            if (block_write(fd, inodes[inum].direct[0], 1) < 0)
                exit(1);
            bit_clear(inode_map, fd[i].inode);
            inodes[inum].mtime = time(NULL);
            update_disk();
        }
    }
    free(fd);
    free(parent);
    free(leaf);
    return 0;
}

/* rmdir - remove a directory
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
static int fs_rmdir(const char *path)
{
    char *_path = strdup(path);
    int inum = translate(_path); 
    if (inum < 0)
        return inum;
    if (!S_ISDIR(inodes[inum].mode))
        return -ENOTDIR;

    struct fs_dirent *fd;
    fd = (struct fs_dirent *) malloc(FS_BLOCK_SIZE);
    if (block_read(fd, inodes[inum].direct[0], 1) < 0)
        exit(1);
    int i;
    for (i = 0; i < 32; i++)
        if (fd[i].valid)
            return -ENOTEMPTY;

    void *block = malloc(FS_BLOCK_SIZE);
    memset(block, 0, FS_BLOCK_SIZE);
    block_write(block, inodes[inum].direct[0], 1);
    free(block);

    bit_clear(block_map, inodes[inum].direct[0]);
    inodes[inum].direct[0] = 0;
    bit_clear(inode_map, inum);

    char *parent = malloc(1000 * sizeof(char));
    char *leaf = malloc(28 * sizeof(char));
    split_path(path, parent, leaf); 
    inum = translate(parent);

    if (block_read(fd, inodes[inum].direct[0], 1) < 0)
        exit(1);
    for (i = 0; i < 32; i++)
    {
        if (fd[i].valid && strcmp(fd[i].name, leaf) == 0)
        {  
            fd[i].valid = 0;
            if (block_write(fd, inodes[inum].direct[0], 1) < 0)
                exit(1);
            bit_clear(inode_map, fd[i].inode);
            inodes[inum].mtime = time(NULL);
        }
    }
    update_disk();
    free(fd);
    free(parent);
    free(leaf);
    return 0;
}


/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. 
 */
static int fs_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - metadata
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * this should work fine, but you may want to add code to
     * calculate the correct values later.
     */
    int i, res = 0;
    for (i = 0; i < sb.num_blocks; i++)
    {
        if (!bit_test(block_map, i))
            res++;
    }

    st->f_bsize = FS_BLOCK_SIZE;
    st->f_blocks = sb.num_blocks;           /* probably want to */
    st->f_bfree = res;            /* change these */
    st->f_bavail = st->f_bfree;           /* values */
    st->f_namemax = 27;
    return 0;
}


/* rename - rename a file or directory
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
static int fs_rename(const char *src_path, const char *dst_path)
{
    char *_path = strdup(src_path);
    int inum = translate(_path); 
    if (inum < 0)
        return -ENOENT;

    _path = strdup(dst_path);
    inum = translate(_path); 
    if (inum > 0)
        return -EEXIST;

    char *parent = malloc(1000 * sizeof(char));
    char *src_leaf = malloc(28 * sizeof(char));
    char *dst_leaf = malloc(28 * sizeof(char));
    split_path(src_path, parent, src_leaf); 
    inum = translate(parent);

    split_path(dst_path, parent, dst_leaf); 
    if (inum != translate(parent))
        return -EINVAL;

    int i;
    struct fs_dirent *fd;
    fd = (struct fs_dirent *) malloc(FS_BLOCK_SIZE);
    if (block_read(fd, inodes[inum].direct[0], 1) < 0)
        exit(1);
    for (i = 0; i < 32; i++)
    {
        if (fd[i].valid && strcmp(fd[i].name, src_leaf) == 0)
        {  
            strcpy(fd[i].name, dst_leaf);
            if (block_write(fd, inodes[inum].direct[0], 1) < 0)
                exit(1);
            uint32_t t = time(NULL);
            inodes[fd[i].inode].mtime = t;
            inodes[inum].mtime = t;
            update_disk();
        }
    }
    free(fd);
    free(parent);
    free(src_leaf);
    free(dst_leaf);
    return 0;
}


/* operations vector. Please don't rename it, as the code in
 * misc.c and libhw3.c assumes it is named 'fs_ops'.
 */
struct fuse_operations fs_ops = {
    .init = fs_init,
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .create = fs_create,
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .rename = fs_rename,
    .chmod = fs_chmod,
    .utime = fs_utime,
    .truncate = fs_truncate,
    .read = fs_read,
    .write = fs_write,
    .statfs = fs_statfs,
};

