#ifndef INCLUDE_VFS_H
#define INCLUDE_VFS_H

#include <SD.h>
#include <time.h>
//#define FTPD_DEBUG 1

#define bcopy(src, dest, len) memmove(dest, src, len)

typedef struct {
	uint16_t date;
	uint16_t time;
} vfs_time_t;
typedef File vfs_dir_t;
typedef File vfs_file_t;
typedef struct {
	long st_size;
	char st_mode;
	vfs_time_t st_mtime;
} vfs_stat_t;
typedef struct {
	char name[256];
} vfs_dirent_t;
typedef SDClass vfs_t;

#define vfs_eof(fil) (fil->available() == 0)
#define VFS_ISDIR(st_mode) ((st_mode) & (DIR_ATT_DIRECTORY | 0X20 | 0X40))
#define VFS_ISREG(st_mode) ((st_mode) & 0X08)
#define vfs_rename(vfs, from, to) !vfs->rename(from, to)
#define VFS_IRWXU 0
#define VFS_IRWXG 0
#define VFS_IRWXO 0
#define vfs_mkdir(vfs, name, mode) !vfs->mkdir(name)
#define vfs_rmdir(vfs, name) !vfs->rmdir(name)
#define vfs_remove(vfs, name) !vfs->remove(name)
char* vfs_getcwd(vfs_t* vfs, void*, int dummy);
int vfs_read (void* buffer, int dummy, int len, vfs_file_t* file);
int vfs_write (void* buffer, int dummy, int len, vfs_file_t* file);
vfs_dirent_t* vfs_readdir(vfs_dir_t* dir);
vfs_file_t* vfs_open(vfs_t* vfs, const char* filename, const char* mode);
vfs_t* vfs_openfs();
void vfs_close(vfs_file_t* file);
void vfs_close(vfs_t* vfs);
int vfs_stat(vfs_t* vfs, const char* filename, vfs_stat_t* st);
void vfs_closedir(vfs_dir_t* dir);
vfs_dir_t* vfs_opendir(vfs_t* vfs, const char* path);
int vfs_chdir(vfs_t* vfs, const char *path);
void time(vfs_time_t *c_t);
struct tm* gmtime(vfs_time_t *c_t);
void vfs_init(vfs_t* vfs);

#define vfs_load_plugin(x) vfs_init((vfs_t*)(x))

#endif /* INCLUDE_VFS_H */
