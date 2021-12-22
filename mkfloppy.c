#include<stdint.h>
#include<stdio.h>
#include<assert.h>
#include<stdlib.h>
#include<string.h>

#define FLAG_DIR 0x01
#define FLAG_ERR 0x02

#if defined(__GNUC__)
    #define packed __attribute__((packed))
#endif

#if defined(P_USE_POSIX)
    #include"posix.c"
#elif defined(P_USE_WINDOWS)
    #include"windows.c"
#else
    #error "bad platform"
#endif

#define FLOPPY_SIZE  (1440*1024)
#define FAT_NUM_ENTS 3072
#define FAT_SIZE     (FAT_NUM_ENTS/2*3)
#define DATA_OFFS    (512+FAT_SIZE+FAT_SIZE)
#define DATA_SIZE    (FLOPPY_SIZE-DATA_OFFS)

#define ATTR_RO  0x01
#define ATTR_HID 0x02
#define ATTR_SYS 0x04
#define ATTR_VOL 0x08
#define ATTR_DIR 0x10
#define ATTR_ARC 0x20

static uint8_t boot[512] = {0};
static uint16_t fat[FAT_SIZE] = {0};
static uint8_t data[DATA_SIZE] = {0};
static int first_free_clus = 0;
static int root_node_count = 0;

struct fat_node typedef fat_node;
struct fat_node {
    char name[11];
    uint8_t attr;
    uint8_t resv;
    uint8_t ctime_s;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    uint16_t clus_hi;
    uint16_t mtime;
    uint16_t mdate;
    uint16_t clus_lo;
    uint32_t size;
} packed;

struct dirinfo typedef dirinfo;
struct dirinfo {
    dirinfo *parent;
    char *name;
    fat_node *fileinfo;
    int first_clus;
    int last_clus;
};

static uint16_t fat_find_next_free(uint16_t clus)
{
    do {
        clus++;
    } while(fat[clus] != 0);
    return clus;
}

// Note: when what is `first_free_clus`, this function
// allocates a chain of 1 cluster.
static uint16_t fat_alloc_link(uint16_t what)
{
    uint16_t new = first_free_clus;
    first_free_clus = fat_find_next_free(first_free_clus);
    fat[what] = new;
    fat[new] = 0xfff;
    return new;
}

static uint8_t *get_data_for_clus(uint16_t clus)
{
    return data + 512*(14+(clus-2));
}

static int write_filename(char buffer[11], char *fn)
{
    char *f = fn;
    int fncount = 0;
    while(!(*f == '.' || *f == 0)) {
        fncount++;
        f++;
    }
    int extcount = 0;
    if(*f == '.') {
        ++f;
        while(*f != 0) {
            f++;
            extcount++;
        }
    }
    char *name = fn;
    char *ext = fn+fncount+1;
    if(fncount > 8) {
        return 1;
    }
    if(extcount > 3) {
        return 1;
    }  
    int i = 0;
    for(;i<fncount; ++i) buffer[i]=name[i];
    for(;i<8;       ++i) buffer[i]=' ';
    int j = 0;
    for(;j<extcount;++i) buffer[8+j]=ext[j];
    for(;j<3;       ++i) buffer[8+j]=' ';
    
    return 0;
}

static int write_file_from_disk_to_floppy(fat_node *fnode, char *filename)
{
    FILE *file = fopen(filename, "rb");
    if(!file) {
        fprintf(stderr, "error opening %s for reading\n", filename);
        return 0;
    }
    fseek(file, 0, SEEK_END);
    int filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    int sectors_required = filesize==0 ? 1 : ((filesize+511)/512);
    if(first_free_clus-2+sectors_required > DATA_SIZE/512) {
        fprintf(stderr, "error: sorry this disk is out of space\n");
        return 0;
    }

    int first_clus = first_free_clus;

    fnode->size = filesize;
    fnode->clus_lo = first_clus;
    fnode->clus_hi = 0;

    int clus = first_free_clus;

    int written = 0;
    do {
        clus = fat_alloc_link(clus);
        uint8_t *dest = get_data_for_clus(clus);
        memset(dest, 0, 512);
        fread(dest, 1, 512, file);
        written += 1;
    } while(written != sectors_required);
    
    fclose(file);
    return 1;
}

static void write_fat_entries(dirinfo *parent, char *fname, char *sname, int dirflag)
{
    printf("Processing: %s\n", fname);

    // make space for the directory node
    fat_node *pnode = parent->fileinfo;
    int flist_offset = pnode->size;
    if(flist_offset != 0 && flist_offset%512 == 0) {
        int clus = fat_alloc_link(parent->last_clus);
        parent->last_clus = clus;
    }
    uint8_t *data = get_data_for_clus(parent->last_clus) + (pnode->size%512);
    pnode->size += sizeof(fat_node);

    fat_node *node = (fat_node *)data;

    if(dirflag) {
        dirinfo this;
        this.parent = parent;
        this.name = sname;
        dir_recurse(&this, fname, (dir_recurse_f *)write_fat_entries);
    }

    if(write_filename(node->name, sname)) {
        fprintf(stderr, "filename exceeds 8.3 limit\n");
        return;
    }

    node->attr |= dirflag?ATTR_DIR:0;

    if(!dirflag) {
        if(!write_file_from_disk_to_floppy(node, fname)) {
            return;
        }
    }
    else {
        int clus = fat_alloc_link(first_free_clus);
        node->clus_lo = clus;
        node->clus_hi = 0;
        node->size = 0;

        dirinfo this;
        this.parent = parent;
        this.name = sname;
        this.fileinfo = node;
        this.last_clus = clus;

        dir_recurse(&this, fname, (dir_recurse_f *)write_fat_entries);
    }
}

static void write_root_entries(dirinfo *parent, char *fname, char *sname, int dirflag)
{
    printf("Processing: %s\n", fname);

    if(root_node_count >= 224) {
        fprintf(stderr, "error: >224 entries in root directory\n");
        return;
    }

    fat_node *fat_nodes = (fat_node *)data;
    fat_node *node = &fat_nodes[root_node_count++];

    if(write_filename(node->name, sname)) {
        fprintf(stderr, "filename exceeds 8.3 limit\n");
        return;
    }

    node->attr |= dirflag?ATTR_DIR:0;

    if(!dirflag) {
        if(!write_file_from_disk_to_floppy(node, fname)) {
            return;
        }
    }
    else {
        int clus = fat_alloc_link(first_free_clus);
        node->clus_lo = clus;
        node->clus_hi = 0;
        node->size = 0;

        dirinfo this;
        this.parent = parent;
        this.name = sname;
        this.fileinfo = node;
        this.last_clus = clus;

        dir_recurse(&this, fname, (dir_recurse_f *)write_fat_entries);
    }
}

int main(int argc, char **argv)
{
    assert(sizeof(fat_node) == 32);
    // Parse arguments
    char *out_fn = 0;
    char *boot_fn = 0;
    char *dir_fn = 0;
    for(int i = 1; i<argc; ++i) {
        char *arg = argv[i];
        if(arg[0] != '-') {
            if(out_fn == 0) out_fn = arg;
            else {
                fprintf(stderr,
                        "Output filename specified more than once:\n\t%s\n\t%s\n",
                        out_fn,
                        arg);
                exit(1);
            }
        }
        else if(strcmp("-b", arg)==0) {
            arg = argv[++i];
            if(boot_fn == 0) boot_fn = arg;
            else {
                fprintf(stderr,
                        "Boot image specified more than once:\n\t%s\n\t%s\n",
                        boot_fn,
                        arg);
                exit(1);
            }

        }
        else if(strcmp("-d", arg)==0) {
            arg = argv[++i];
            if(dir_fn==0) dir_fn = arg;
            else {
                fprintf(stderr,
                        "Directory specified more than once:\n\t%s\n\t%s\n",
                        dir_fn,
                        arg);
                exit(1);
            }
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            exit(1);
        }
    }
    if(out_fn == 0) {
        fprintf(stderr, "Output filename not specified\n");
        exit(1);
    }

    // Write boot sector
    if(boot_fn != 0) {
        FILE *boot_file = fopen(boot_fn, "r");
        if(boot_file == NULL) {
            fprintf(stderr, "error: unable to open the boot file\n");
            exit(1);
        }
        fread(boot, 1, 512, boot_file);
        fclose(boot_file);
    }

    // Initialize FAT
    fat[0] = 0xff0;
    fat[1] = 0xfff;
    first_free_clus = 2;

    // Write directories
    if(dir_fn != 0) {
        dirinfo startdir;
        startdir.parent = 0;
        startdir.name = dir_fn;
        dir_recurse(&startdir, startdir.name, (dir_recurse_f *)write_root_entries);
    }

    FILE *out = fopen(out_fn, "w");
    if(out == NULL) {
        fprintf(stderr, "error: unable to open image file\n");
        exit(1);
    }
    // Write boot sector
    fwrite(boot, 1, 512, out);
    // Write FAT
    for(int pi = 0; pi != FAT_NUM_ENTS/2; ++pi) {
        int fe0 = fat[2*pi+0];
        int fe1 = fat[2*pi+1];
        // Nibble layout of pair of entries in a FAT
        // assuming 0x123, 0x456 following each other
        //   [32] [61] [54]
        int b0 = fe0 & 0xff;
        int b1 = (fe0>>8) | ((fe1&0xff) << 4);
        int b2 = fe1 >> 4;
        fputc(b0, out);
        fputc(b1, out);
        fputc(b2, out);
    }
    // Copy reserve FAT
    for(int pi = 0; pi != FAT_NUM_ENTS/2; ++pi) {
        int fe0 = fat[2*pi+0];
        int fe1 = fat[2*pi+1];
        int b0 = fe0 & 0xff;
        int b1 = (fe0>>8) | ((fe1&0xff) << 4);
        int b2 = fe1 >> 4;
        fputc(b0, out);
        fputc(b1, out);
        fputc(b2, out);
    }
    // Write data
    for(int i = 0; i != DATA_SIZE;++i) {
        fputc(data[i], out);
    }
    fclose(out);
    return 0;
}
