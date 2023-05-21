#ifndef __FS_H
#define __FS_H

#define FS_START_OFFSET

#include <fs/inode.h>

int init_fs();
void fs_create_file_system();

inode_t fs_open(char* name);
inode_t fs_open_from_directory(char* name, inode_t i);
void fs_close(inode_t inode);
int fs_read(inode_t i, void* buf, int size);
int fs_write(inode_t i, void* buf, int size);
inode_t change_directory(char* path);
inode_t fs_create_directory(char* name, inode_t);
int fs_create(char* name);

int fs_seek(inode_t i, int pos, int opt);

int fs_size(inode_t i);

inode_t fs_get_current_dir();
inode_t fs_get_root();

void fs_stats();

#endif /* __FS_H */
