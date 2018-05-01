#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>
#include <libgen.h>

#include <alloca.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#define ARGS_COUNT 4
#define BUFFER_SIZE 512
#define FOLDER_PERMISSIONS 0744

typedef enum {
    BY_NAME = 0,
    BY_SIZE = 1
} sortType;

typedef struct {
    char *path;
    char *name;
    off_t file_size;
} fileInfo;

typedef struct {
    fileInfo **files;
    size_t files_count;
} filesList;

typedef ssize_t (*compareFunc)(const fileInfo *, const fileInfo *);

char *module_name;

void print_error(const char *module_name, const char *error_msg, const char *file_name) {
    fprintf(stderr, "%s: %s %s\n", module_name, error_msg, file_name ? file_name : "");
}

ssize_t compare_files_by_name(const fileInfo *f1, const fileInfo *f2) {
    char *f1_name = f1->name, *f2_name = f2->name;
    size_t f1_len = strlen(f1_name), f2_len = strlen(f2_name);
    for (size_t i = 0, j = 0; i < f1_len && j < f2_len; i++, j++){
        while (!isalnum(f1_name[i]) && i < f1_len){
            i++;
        }
        while (!isalnum(f2_name[j]) && j < f2_len){
            j++;
        }

        if (toupper(f1_name[i]) == toupper(f2_name[j])){
            continue;
        }

        return toupper(f1_name[i]) - toupper(f2_name[j]);
    };
    return f1_len - f2_len;
}

ssize_t compare_files_by_size(const fileInfo *f1, const fileInfo *f2) {
    ssize_t result = f1->file_size - f2->file_size;
    if (!result){
        return compare_files_by_name(f1, f2);
    }
    return result;
}

void add_file(fileInfo *file, filesList *sorted_files, compareFunc compare_func) {
    int position;
    for (position = 0;
         (position < sorted_files->files_count) && (compare_func(file, sorted_files->files[position]) > 0); position++);

    sorted_files->files = realloc(sorted_files->files, sizeof(fileInfo *) * (++(sorted_files->files_count)));

    for (size_t i = sorted_files->files_count - 1; i > position; i--) {
        sorted_files->files[i] = sorted_files->files[i - 1];
    };

    sorted_files->files[position] = file;
}

void file_path(char *dest, const char *const path, const char *const name) {
    strcpy(dest, path);
    strcat(dest, "/");
    strcat(dest, name);
}

off_t file_size(const char *parent_path, const char *file_name) {
    struct stat file_info;
    char *full_path = alloca(strlen(parent_path) + strlen(file_name) + 2);

    file_path(full_path, parent_path, file_name);

    if (stat(full_path, &file_info) == -1) {
        print_error(module_name, strerror(errno), full_path);
        return 0;
    }

    return file_info.st_size;
}

void files_in_dir(const char *path, filesList *sorted_files, compareFunc compare_func) {
    DIR *dir_stream;
    struct dirent *dir_entry;
    if (!(dir_stream = opendir(path))) {
        print_error(module_name, strerror(errno), path);
        return;
    }

    errno = 0;
    while ((dir_entry = readdir(dir_stream)) != NULL) {
        char *entry_name = dir_entry->d_name;

        if (!strcmp(".", entry_name) || !strcmp("..", entry_name))
            continue;

        char *full_path = alloca(strlen(entry_name) + strlen(path) + 2);
        file_path(full_path, path, entry_name);
        struct stat entry_info;
        if (lstat(full_path, &entry_info) == -1) {
            print_error(module_name, strerror(errno), full_path);
            errno = 0;
            continue;
        }

        if (S_ISDIR(entry_info.st_mode)) {
            files_in_dir(full_path, sorted_files, compare_func);
        } else {
            if (S_ISREG(entry_info.st_mode)) {
                fileInfo *file = malloc(sizeof(fileInfo));

                file->name = malloc(strlen(entry_name) + 1);
                strcpy(file->name, entry_name);

                file->path = malloc(strlen(path) + 1);
                strcpy(file->path, path);

                file->file_size = file_size(path, entry_name);
                add_file(file, sorted_files, compare_func);
            }
        }
    }
    if (errno){
        print_error(module_name, strerror(errno), path);
    }

    if (closedir(dir_stream) == -1) {
        print_error(module_name, strerror(errno), path);
    }
}

void copy_file(const char *src_path, const char *dest_path) {
    char buffer[BUFFER_SIZE];
    int src, dest;

    if ((src = open(src_path, O_RDONLY)) == -1) {
        print_error(module_name, strerror(errno), src_path);
        return;
    }

    int copies_count = 0;
    char *tmp_dest_path = alloca(strlen(dest_path) + 10);

    strcpy(tmp_dest_path, dest_path);
    while ((dest = open(tmp_dest_path, O_WRONLY | O_CREAT | O_EXCL)) == -1) {
        if (errno == EEXIST) {
            sprintf(tmp_dest_path, "%s (%d)", dest_path, ++copies_count);
        } else {
            print_error(module_name, strerror(errno), tmp_dest_path);
            if (close(src) == -1) {
                print_error(module_name, strerror(errno), src_path);
            };
            return;
        }
    }

    ssize_t bytes_count = 0;
    while ((bytes_count = read(src, buffer, BUFFER_SIZE)) > 0 && bytes_count != -1) {
        if (write(dest, buffer, (size_t) bytes_count) == -1) {
            print_error(module_name, strerror(errno), dest_path);

            if (close(src) == -1){
                print_error(module_name, strerror(errno), src_path);
            };
            if (close(dest) == -1){
                print_error(module_name, strerror(errno), src_path);
            };

            return;
        }
    }

    if (close(src) == -1){
        print_error(module_name, strerror(errno), src_path);
    };
    if (close(dest) == -1){
        print_error(module_name, strerror(errno), src_path);
    };

    if (bytes_count == -1) {
        print_error(module_name, strerror(errno), src_path);
        return;
    }
}

void copy_files_list(filesList *files_to_copy, const char *dest_folder) {
    int i;
    char dest_path[PATH_MAX];
    char src_path[PATH_MAX];
    for (i = 0; i < files_to_copy->files_count; i++) {
        file_path(dest_path, dest_folder, files_to_copy->files[i]->name);
        file_path(src_path, files_to_copy->files[i]->path, files_to_copy->files[i]->name);
        copy_file(src_path, dest_path);
    }
}

int get_sort_type(const char *param_str) {
    if (strcmp(param_str, "1") == 0) {
        return BY_SIZE;
    }

    if (strcmp(param_str, "2") == 0) {
        return BY_NAME;
    }

    return -1;
}


int main(int argc, char *argv[]) {
    module_name = basename(argv[0]);

    if (argc != ARGS_COUNT) {
        print_error(module_name, "Wrong number of parameters", NULL);
        return 1;
    }

    struct stat out_dir_info;
    if (stat(argv[3], &out_dir_info) == -1) {
        if (errno != ENOENT) {
            print_error(module_name, strerror(errno), argv[3]);
            return 1;
        }
        if (mkdir(argv[3], FOLDER_PERMISSIONS) == -1) {
            print_error(module_name, strerror(errno), argv[3]);
            return 1;
        }
    } else {
        if (!S_ISDIR(out_dir_info.st_mode)) {
            print_error(module_name, "Not a directory", argv[3]);
            return 1;
        }
        if (access(argv[3], W_OK) == -1) {
            print_error(module_name, strerror(errno), argv[3]);
            return 1;
        }
    }

    char src_dir[PATH_MAX];
    char dest_dir[PATH_MAX];

    if (!realpath(argv[1], src_dir)) {
        print_error(module_name, strerror(errno), argv[1]);
        return 1;
    };
    if (!realpath(argv[3], dest_dir)) {
        print_error(module_name, strerror(errno), argv[3]);
        return 1;
    };

    sortType sort_type;
    if ((sort_type = (sortType) get_sort_type(argv[2])) == -1) {
        print_error(module_name, "Invalid sort type", NULL);
        return 1;
    }

    compareFunc compare_func = (sort_type == BY_NAME) ? compare_files_by_name : compare_files_by_size;

    filesList sorted_files;
    sorted_files.files_count = 0;
    sorted_files.files = NULL;

    files_in_dir(src_dir, &sorted_files, compare_func);
    copy_files_list(&sorted_files, dest_dir);

    for (int i = 0; i < sorted_files.files_count; i++) {
        free(sorted_files.files[i]->path);
        free(sorted_files.files[i]->name);
        free(sorted_files.files[i]);
    }
}
