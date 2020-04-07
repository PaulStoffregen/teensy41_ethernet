#include "vfs.h"

#define 	isDirSeparator(c)   ((c) == '/')

#ifdef FTPD_DEBUG
#define dbg_printf Serial.printf
#else
#define dbg_printf(f, ...)
#endif

/* dirent that will be given to callers;
 * note: both APIs assume that only one dirent ever exists
 */
vfs_dirent_t dir_ent;
vfs_t *guard_for_the_whole_fs;
#define CWD_INC 256
static size_t cwd_size;
static char* cwd;

int vfs_read(void* buffer, int dummy, int len, vfs_file_t* file) {
    int bytesread = file->read(buffer, len);
    if (bytesread < 0) return 0;
    return bytesread;
}

vfs_dirent_t* vfs_readdir(vfs_dir_t* dir) {
    vfs_file_t file;
#if 0
    if (!file.openNext(dir, O_READ) || !file.getName(dir_ent.name, 255) || dir_ent.name[0] == 0)
        return NULL;
    return &dir_ent;
#endif
   return NULL;
}

int vfs_stat(vfs_t* vfs, const char* filename, vfs_stat_t* st) {
    vfs_file_t *f = new vfs_file_t();
    //if (!f->open(vfs_file_t::cwd(), filename, O_READ)) {
    if (! (*f = SD.open( filename, FILE_READ))) {
        free(f);
        return 1;
    }
    st->st_size = f->size();
    st->st_mode = 8; //f->fileAttr();
#if 0
    dir_t dst;
    if (f->dirEntry(&dst))
    {
        st->st_mtime.date = dst.lastWriteDate;
        st->st_mtime.time = dst.lastWriteTime;
    }
#endif
    free(f);
    return 0;
}

void vfs_close(vfs_file_t* file) {
    file->close();
    free(file);
}

void vfs_close(vfs_t* vfs) {
}

int vfs_write(void* buffer, int dummy, int len, vfs_file_t* file) {
    int byteswritten = file->write((char *)buffer, len);
    if (byteswritten < 0) return 0;
    return byteswritten;
}

vfs_t* vfs_openfs() {
	return guard_for_the_whole_fs;
}

vfs_file_t* vfs_open(vfs_t* vfs, const char* filename, const char* mode) {
    vfs_file_t *f = new vfs_file_t();
    uint8_t flags = 0;
    while (*mode != '\0') {
        if (*mode == 'r') flags |= O_READ;
        if (*mode == 'w') flags |= O_WRITE | O_CREAT;
        mode++;
    }
    //if (!f->open(vfs_file_t::cwd(), filename, flags)) {
    if (! ( *f = SD.open( filename, flags))) {
        free(f);
        return NULL;
    }
    return f;
}

bool check_cwd(size_t size) {
    if (cwd_size < size)
    {
        size = (size / CWD_INC + 1) * CWD_INC;
        void *tmp = realloc(cwd, size);
        if (!tmp) return false;
        cwd = (char*)tmp;
        cwd_size = size;
    }
    return true;
}

char* vfs_getcwd(vfs_t* vfs, void* dummy1, int dummy2) {
    return strdup(cwd);
}

vfs_dir_t* vfs_opendir(vfs_t* vfs, const char* path) {
#if 0
    vfs_dir_t *dir = new vfs_dir_t();
    *dir = *vfs_dir_t::cwd();
    dir->rewind();
    return dir;
#endif
  return NULL;
}

void vfs_closedir(vfs_dir_t* dir) {
    dir->close();
    free(dir);
}

int vfs_chdir(vfs_t* vfs, const char *path) {
    char *cpy = strdup(cwd), *p;
    bool res = false;
    if (strcmp(path, "..") == 0)
    {
        p = strrchr(cwd, '/');
        if (p) { if (p == cwd) p++; *p = 0; res = true; }
    }
    else if (check_cwd(strlen(path) + (isDirSeparator(*path) ? 1 : strlen(cwd) + 2)))
    {
        if (isDirSeparator(*path))
            strcpy(cwd, path);
        else
        {
            if (strcmp(cwd, "/") != 0)
                strcat(cwd, "/");
            strcat(cwd, path);
        }
        p = cwd + (strlen(cwd) - 1);
        if (isDirSeparator(*p) && p != cwd) *p = 0;
        res = true;
    }
#if 0
    if (res && vfs->chdir(cwd, true)) {
        free(cpy);
        return 0;
    }
#endif
    strcpy(cwd, cpy);
    free(cpy);
    return 1;
}

struct tm dummy = { 0, 0, 0, 1, 0, 70 };

struct tm* gmtime(vfs_time_t* c_t) {
    dummy.tm_year = FAT_YEAR(c_t->date) - 1900;
    dummy.tm_mon = FAT_MONTH(c_t->date);
    if (dummy.tm_mon) dummy.tm_mon--;
    dummy.tm_mday = FAT_DAY(c_t->date);
    dummy.tm_hour = FAT_HOUR(c_t->time);
    dummy.tm_min = FAT_MINUTE(c_t->time);
    return &dummy;
}

static void dateTime(uint16_t* p_date, uint16_t* p_time) {
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    *p_date = FAT_DATE(timeinfo->tm_year + 1900, timeinfo->tm_mon, timeinfo->tm_mday);
    *p_time = FAT_TIME(timeinfo->tm_hour, timeinfo->tm_min, 0);
}

void time(vfs_time_t *c_t) {
    dateTime(&c_t->date, &c_t->time);
}

void vfs_init(vfs_t* vfs) {
    guard_for_the_whole_fs = vfs;
    //vfs_file_t::dateTimeCallback(dateTime);
    check_cwd(2);
    strcpy(cwd, "/");
}
