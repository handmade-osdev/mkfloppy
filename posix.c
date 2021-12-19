
#include<sys/times.h>
#include<dirent.h>

void typedef (dir_recurse_f)(void*,char*,char*,int);

static void dir_recurse(void *ptr, char *fname, dir_recurse_f *user_recurse)
{
    DIR *dir_ptr = opendir(fname);
    if(dir_ptr == NULL) {
        fprintf(stderr, "Error opening %s\n", fname);
        return;
    }
    struct dirent *dirent_ptr;
    while ((dirent_ptr = readdir(dir_ptr)) != 0)
    {
        char *name = dirent_ptr->d_name;

        char new_fname[1024] = {0};

        int fname_len = strlen(fname);
        strcpy(new_fname, fname);
        new_fname[fname_len] = '/';

        int name_len = strlen(name);
        if(fname_len+1+name_len+1 > sizeof new_fname) {
            fprintf(stderr, "error: file in '%s': filename too long\n", fname);
            continue;
        }

        strcpy(new_fname+fname_len+1, name);

        if(strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            int dir = dirent_ptr->d_type == DT_DIR;
            user_recurse(ptr, new_fname, name, dir);
        }
    }
    closedir(dir_ptr);
}
