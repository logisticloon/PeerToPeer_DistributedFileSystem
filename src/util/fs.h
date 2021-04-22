
#ifndef _FS_H_
#define _FS_h_

#include <sys/types.h>
#include <stdint.h>

/* reads the entire file */
int get_file_size(const char *file_path);

int fread_full(int fd, void *buf, size_t size);

int fwrite_full(int fd, void *buf, size_t size);

/*directory*/
int directory_exists(const char *dir_path);

void make_dir(const char *dir_name);


#endif