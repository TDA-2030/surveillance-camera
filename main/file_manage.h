#ifndef FILE_MANAGE_H_
#define FILE_MANAGE_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include "esp_log.h"

#ifdef __cplusplus 
extern "C" {
#endif

typedef struct {
    char *base_path;
    char *label;

} fs_info_t;


esp_err_t fm_init(void);

esp_err_t fm_get_info(fs_info_t **info);

esp_err_t fm_file_table_create(char ***list_out, uint16_t *files_number);
esp_err_t fm_file_table_free(char ***list,uint16_t files_number);

int fm_mkdir(const char *path);

#ifdef __cplusplus 
}
#endif

#endif