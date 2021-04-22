
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "fs.h"


int get_file_size(const char *file_path) {
    struct stat st;
    stat(file_path, &st);
    return (int)(st.st_size);
}


int fread_full(int fd, void *buffer, size_t size) {

    char *buff = (char *)buffer;
    int offset = 0;

    while(offset < size) {
        int bytes_read = read(fd, buff + offset, size - offset);
        if (bytes_read < 0) {
            perror("fread_full: error in reading file\n");
            return -1;
        }
        offset += bytes_read;
    }

    return offset;
}


int fwrite_full(int fd, void *buffer, size_t size) {
    
    char *buff = (char *)buffer;
    int offset = 0;

    while(offset < size) {
        int bytes_written  = write(fd, buff + offset, size - offset);
        if (bytes_written < 0) {
            perror("fwrite_full: error in writing file\n");
            return -1;
        }
        offset += bytes_written;
    }

    return offset;
}

int directory_exists(const char *dir_path) {

    DIR *dir = opendir(dir_path);
    if (dir) {
        return 1;
    } else {
        return 0;
    }
}

void make_dir(const char *dir_name) {
    int ret = mkdir(dir_name, 0777);
    if (ret != 0) {
        printf("make_dir failed\n");
    }
}