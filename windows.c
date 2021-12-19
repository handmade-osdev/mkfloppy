
void typedef (dir_recurse_f)(
    void *user_data,
    char* rel_name, // relative to CWD
    char* short_name,
    int is_dir_flag
);

static void dir_recurse(void *ptr, char *fname, dir_recurse_f *user_recurse)
{
    // for each file call user_recurse
}
