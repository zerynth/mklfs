/*
 * Copyright (C) 2015 - 2020, IBEROXARXA SERVICIOS INTEGRALES, S.L.
 * Copyright (C) 2015 - 2020, Jaume Olivé Petrus (jolive@whitecatboard.org)
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *     * The WHITECAT logotype cannot be changed, you can remove it, but you
 *       cannot change it in any way. The WHITECAT logotype is:
 *
 *          /\       /\
 *         /  \_____/  \
 *        /_____________\
 *        W H I T E C A T
 *
 *     * Redistributions in binary form must retain all copyright notices printed
 *       to any local or remote output device. This include any reference to
 *       Lua RTOS, whitecatboard.org, Lua, and other copyright notices that may
 *       appear in the future.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Lua RTOS, a tool for make a LFS file system image
 *
 */

#include "lfs/lfs.h"

#include <ctype.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#if !defined(__CYGWIN__) && defined(TARGET_OS) && TARGET_OS == WINDOWS
#include "dirent.h"
#else
#include <dirent.h>
#endif
#include <sys/types.h>

static struct lfs_config cfg;
static lfs_t lfs;
static uint8_t *data;

static int lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    memcpy(buffer, data + (block * c->block_size) + off, size);
    return 0;
}

static int lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    memcpy(data + (block * c->block_size) + off, buffer, size);
    return 0;
}

static int lfs_erase(const struct lfs_config *c, lfs_block_t block) {
    memset(data + (block * c->block_size), 0, c->block_size);
    return 0;
}

static int lfs_sync(const struct lfs_config *c) {
    return 0;
}

static void create_dir(char *src) {
    char *path;
    int ret;

    path = strchr(src, '/');
    if (path) {
        fprintf(stdout, "%s\r\n", path);

        if ((ret = lfs_mkdir(&lfs, path)) < 0) {
            fprintf(stderr,"can't create directory %s: error=%d\r\n", path, ret);
            exit(1);
        }
    }
}

static void create_file(char *src) {
    char *path;
    int ret;

    path = strchr(src, '/');
    if (path) {
        fprintf(stdout, "%s\r\n", path);

        // Open source file
        FILE *srcf = fopen(src,"rb");
        if (!srcf) {
            fprintf(stderr,"can't open source file %s: errno=%d (%s)\r\n", src, errno, strerror(errno));
            exit(1);
        }

        // Open destination file
        lfs_file_t dstf;
        if ((ret = lfs_file_open(&lfs, &dstf, path, LFS_O_WRONLY | LFS_O_CREAT)) < 0) {
            fprintf(stderr,"can't open destination file %s: error=%d\r\n", path, ret);
            exit(1);
        }

        char c = fgetc(srcf);
        while (!feof(srcf)) {
            ret = lfs_file_write(&lfs, &dstf, &c, 1);
            if (ret < 0) {
                fprintf(stderr,"can't write to destination file %s: error=%d\r\n", path, ret);
                exit(1);
            }
            c = fgetc(srcf);
        }

        // Close destination file
        ret = lfs_file_close(&lfs, &dstf);
        if (ret < 0) {
            fprintf(stderr,"can't close destination file %s: error=%d\r\n", path, ret);
            exit(1);
        }

        // Close source file
        fclose(srcf);
    }
}
static long file_size(char *src) {
    char *path;
    long sz=0;

    path = strchr(src, '/');
    if (path) {
        // Open source file
        FILE *srcf = fopen(src,"rb");
        if (!srcf) {
            fprintf(stderr,"can't open source file %s: errno=%d (%s)\r\n", src, errno, strerror(errno));
            exit(1);
        }

        fseek(srcf, 0, SEEK_END);
        sz = ftell(srcf);
        if (sz == -1) {
            fprintf(stderr,"can't get source file size %s: errno=%d (%s)\r\n", src, errno, strerror(errno));
            exit(1);
        }

        // Close source file
        fclose(srcf);

        fprintf(stdout, "%s:%ld\r\n", path, sz);
    }
    return sz;
}


static void compact(char *src) {
    DIR *dir;
    struct dirent *ent;
    char curr_path[PATH_MAX+1];

    dir = opendir(src);
    if (dir) {
        while ((ent = readdir(dir))) {
            // Skip . and .. directories
            if ((strcmp(ent->d_name,".") != 0) && (strcmp(ent->d_name,"..") != 0)) {
                // Update the current path
                strncpy(curr_path, src, PATH_MAX-strlen(curr_path));
                strncat(curr_path, "/", PATH_MAX-strlen(curr_path));
                strncat(curr_path, ent->d_name, PATH_MAX-strlen(curr_path));

                if (ent->d_type == DT_DIR) {
                    create_dir(curr_path);
                    compact(curr_path);
                } else if (ent->d_type == DT_REG) {
                    create_file(curr_path);
                }
            }
        }

        closedir(dir);
    }
}

static int dir_size(char *src) {
    DIR *dir;
    struct dirent *ent;
    char curr_path[PATH_MAX+1];
    int sz=0;

    dir = opendir(src);
    if (dir) {
        while ((ent = readdir(dir))) {
            // Skip . and .. directories
            if ((strcmp(ent->d_name,".") != 0) && (strcmp(ent->d_name,"..") != 0)) {
                // Update the current path
                strncpy(curr_path, src, PATH_MAX-strlen(curr_path));
                strncat(curr_path, "/", PATH_MAX-strlen(curr_path));
                strncat(curr_path, ent->d_name, PATH_MAX-strlen(curr_path));

                if (ent->d_type == DT_DIR) {
                    sz+=dir_size(curr_path);
                } else if (ent->d_type == DT_REG) {
                    sz+=file_size(curr_path);
                }
            }
        }

        closedir(dir);
    }
    return sz;
}

void usage() {
    fprintf(stdout, "Zerynth LittleFS Make\n");
    fprintf(stdout, "---------------------\n");
    fprintf(stdout, "usage: mklfs -c <pack-dir> -b <block-size> -r <read-size> -p <prog-size> -s <filesystem-size> -o <image-file-path> -h <cache-size> -l <lookahead-size> -w <block-wear> -k <shrink>\n");
    fprintf(stdout, "       <pack-dir>         :: directory to use as filesystem content\n");
    fprintf(stdout, "       <image-file-path>  :: output file for the lfs image\n");
    fprintf(stdout, "       <block-size>       :: size of flash block (default 4096)\n");
    fprintf(stdout, "       <read-size>        :: size of a read operation (default 1024)\n");
    fprintf(stdout, "       <prog-size>        :: size of a prog operation (default 1024)\n");
    fprintf(stdout, "       <filesystem-size>  :: total size of filesystem\n");
    fprintf(stdout, "       <lookahead-size>   :: size of the lookahead buffer in bytes, each byte of RAM can track 8 blocks. Must be a multiple of 8 (default 16)\n");
    fprintf(stdout, "       <cache-size>       :: per file cache-size (must be a multiple of read and prog size, default 1024)\n");
    fprintf(stdout, "       <block-wear>       :: dynamic wear leveling for metadata (default 1000, -1 to disable)\n");
    fprintf(stdout, "       <shrinked>         :: if non-zero, shrinks final image (default 0)\n");
}

static int is_number(const char *s) {
    const char *c = s;

    while (*c) {
        if ((*c < '0') || (*c > '9')) {
            return 0;
        }
        c++;
    }

    return 1;
}

static int is_hex(const char *s) {
    const char *c = s;

    if (*c++ != '0') {
        return 0;
    }

    if (*c++ != 'x') {
        return 0;
    }

    while (*c) {
        if (((*c < '0') || (*c > '9')) && ((*c < 'A') || (*c > 'F')) && ((*c < 'a') || (*c > 'f'))) {
            return 0;
        }
        c++;
    }

    return 1;
}

static int to_int(const char *s) {
    if (is_number(s)) {
        return atoi(s);
    } else if (is_hex(s)) {
        return (int)strtol(s, NULL, 16);
    }

    return -1;
}

int main(int argc, char **argv) {
    char *src = NULL;        // Source directory
    char *dst = NULL;        // Destination image
    char *dirc, *basec;      // Temporary copy of src
    char *dname, *bname;     // Source dirname and basename
    int c;                   // Current option
    int block_size = 4096;   // Block size
    int read_size = 1024;    // Read size
    int prog_size = 1024;    // Prog size
    int fs_size = 0;         // File system size
    int cache_size = 1024;   // Cache size
    int lookahead_size = 16; // Look ahead buffer size (optional)
    int block_wear = 1000;
    int shrinked = 0;
    int err;

    while ((c = getopt(argc, argv, "c:o:b:p:r:s:h:w:k:l:")) != -1) {
        switch (c) {
        case 'c':
            src = optarg;
            break;

        case 'o':
            dst = optarg;
            break;

        case 'w':
            block_wear = to_int(optarg);
            break;

        case 'k':
            shrinked = to_int(optarg);
            break;

        case 'b':
            block_size = to_int(optarg);
            break;

        case 'p':
            prog_size = to_int(optarg);
            break;

        case 'r':
            read_size = to_int(optarg);
            break;

        case 'l':
            lookahead_size = to_int(optarg);
            break;

        case 's':
            fs_size = to_int(optarg);
            break;
        case 'h':
            cache_size = to_int(optarg);
            break;
        }
    }

    if ((src == NULL) || (dst == NULL) || (block_size <= 0) || (prog_size <= 0) ||
        (read_size <= 0) || (fs_size <= 0)) {
            usage();
        exit(1);
    }

    // dirname() and basename() may modify src
    dirc = strdup(src);
    basec = strdup(src);
    dname = dirname(dirc);
    bname = basename(basec);

    if (chdir(dname) != 0) {
        fprintf(stderr, "cannot chdir into %s: error=%d (%s)\r\n", src, errno, strerror(errno));
        return -1;
    }

    int total_size = dir_size(bname)+block_size*16;
    fprintf(stderr, "Total size %d\n",total_size);
    // Mount the file system
    cfg.read  = lfs_read;
    cfg.prog  = lfs_prog;
    cfg.erase = lfs_erase;
    cfg.sync  = lfs_sync;

    cfg.block_size  = block_size;
    cfg.read_size   = read_size;
    cfg.prog_size   = prog_size;
    cfg.cache_size  = cache_size;
    cfg.block_count = total_size / cfg.block_size;
    cfg.block_cycles = block_wear;
    cfg.lookahead_size  = lookahead_size;
    cfg.context     = NULL;

    // data = calloc(1, fs_size);
    data = calloc(1, total_size);
    if (!data) {
        fprintf(stderr, "no memory for mount\r\n");
        return -1;
    }

    err = lfs_format(&lfs, &cfg);
    if (err < 0) {
        fprintf(stderr, "format error: error=%d\r\n", err);
        return -1;
    }

    err = lfs_mount(&lfs, &cfg);
    if (err < 0) {
        fprintf(stderr, "mount error: error=%d\r\n", err);
        return -1;
    }

    compact(bname);

    FILE *img = fopen(dst, "wb+");

    if (!img) {
        fprintf(stderr, "can't create image file: errno=%d (%s)\r\n", errno, strerror(errno));
        return -1;
    }

    if (!shrinked) {
        lfs_superblock_t sb;
        //get current superblock
        lfs_z_get_superblock(&lfs,&sb,0);
        //search superblock in data
        int i;
        for(i=0;i<total_size;i++){
            if(memcmp(data+i,(uint8_t*)&sb,sizeof(lfs_superblock_t))==0) {
                //found superblock!
                //modify it with fs_size
                lfs_z_get_superblock(&lfs,&sb,fs_size/block_size);
                memcpy(data+i,(uint8_t*)&sb,sizeof(lfs_superblock_t));
                break;

            }
        }
    }
    fprintf(stderr, "image size: %d, fs size %d\n",total_size, fs_size);

    fwrite(data, 1, total_size, img);

    fclose(img);

    return 0;
}
