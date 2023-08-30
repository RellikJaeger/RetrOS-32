/**
 * @file fat16.c
 * @author Joe Bayer (joexbayer)
 * @brief Main API for the FAT16 Filesystem.
 * @version 0.1
 * @date 2023-05-31
 * @see http://www.tavi.co.uk/phobos/fat.html
 * @see https://wiki.osdev.org/FAT
 * @copyright Copyright (c) 2023
 * 
 */
#include <stdint.h>
#include <fs/fat16.h>
#include <kutils.h>
#include <diskdev.h>
#include <memory.h>
#include <sync.h>


static struct fat_boot_table boot_table = {0};
static byte_t* fat_table_memory = NULL;  /* pointer to the in-memory FAT table */

/* Temporary "current" directory */
static uint16_t current_dir_block = 0;

/* locks for read / write and management */
static mutex_t fat16_table_lock; 
static mutex_t fat16_write_lock;
static mutex_t fat16_management_lock;

/* HELPER FUNCTIONS */

inline uint16_t get_fat_start_block()
{
    return BOOT_BLOCK + boot_table.reserved_blocks;  /* FAT starts right after the boot block and reserved blocks. */
}

inline uint16_t get_root_directory_start_block()
{
    return get_fat_start_block() + boot_table.fat_blocks;  /* Root directory starts after FAT */
}

inline uint16_t get_data_start_block()
{
    return get_root_directory_start_block()-1;  /* Data starts after Root directory */
}


uint16_t fat16_get_fat_entry(uint32_t cluster)
{
    if(fat_table_memory == NULL){
        return -1;
    }

    uint32_t fat_offset = cluster * 2;  /* Each entry is 2 bytes */
    return *(uint16_t*)(fat_table_memory + fat_offset);
}

void fat16_set_fat_entry(uint32_t cluster, uint16_t value)
{
    if(fat_table_memory == NULL){
        return;
    }

    acquire(&fat16_table_lock);

    uint32_t fat_offset = cluster * 2;  /* Each entry is 2 bytes */
    *(uint16_t*)(fat_table_memory + fat_offset) = value;

    release(&fat16_table_lock);
}

void fat16_sync_fat_table()
{
    if(fat_table_memory == NULL){
        return;
    }

    acquire(&fat16_table_lock);

    int start_block = get_fat_start_block();
    for (uint16_t i = 0; i < boot_table.fat_blocks; i++) {
        write_block(fat_table_memory + i * 512, start_block + i);
    }

    release(&fat16_table_lock);

    fat16_dump_fat_table();
}

/* wrapper functions TODO: inline replace */
inline void fat16_allocate_cluster(uint32_t cluster)
{
    fat16_set_fat_entry(cluster, 0xFFFF);  /* marking cluster as end of file */
}

inline void fat16_free_cluster(uint32_t cluster)
{
    fat16_set_fat_entry(cluster, 0x0000);  /* marking cluster as free */
}

uint32_t fat16_get_free_cluster()
{
    acquire(&fat16_table_lock);

    for (int i = 0; i < 65536; i++) {  /* Start from 2 as 0 and 1 are reserved entries */
        if (fat16_get_fat_entry(i) == 0x0000) {

            fat16_allocate_cluster(i);

            release(&fat16_table_lock);
            return i;
        }
    }

    release(&fat16_table_lock);
    return -1;  /* no free cluster found */
}

/**
 * Writes (synchronizes) a directory entry back to the root directory of the FAT16 filesystem.
 *
 * @param index: The index of the directory entry to be updated.
 * @param entry: Pointer to the directory entry data to be written.
 * @return 0 on success, negative value on error.
 */
static int fat16_sync_directory_entry(uint16_t block, uint32_t index, const struct fat16_directory_entry* entry)
{
    if(index >= boot_table.root_dir_entries)
        return -1;  /* index out of range */

    uint32_t offset = (index % ENTRIES_PER_BLOCK) * sizeof(struct fat16_directory_entry);
    write_block_offset((byte_t*)entry, sizeof(struct fat16_directory_entry), offset, block);

    return 0;  /* success */
}

/**
 * Read a root directory entry by index.
 * @param index The index of the root directory entry.
 * @param entry_out Pointer to the destination struct where the entry should be copied.
 * @return 0 on success, or a negative value on error.
 */
static int fat16_read_root_directory_entry(uint32_t index, struct fat16_directory_entry* entry_out)
{
    if(index >= boot_table.root_dir_entries)
        return -1;  /* index out of range */

    uint16_t block_num = get_root_directory_start_block() + (index / ENTRIES_PER_BLOCK);
    byte_t buffer[512];
    if(read_block(buffer, block_num) < 0){
        dbgprintf("Error reading block\n");
        return -2;  /* error reading block */
    }

    struct fat16_directory_entry* dir_entry = (struct fat16_directory_entry*)(buffer + index * sizeof(struct fat16_directory_entry));

    /* Copy the directory entry to the output */
    memcpy(entry_out, &buffer[(index % ENTRIES_PER_BLOCK) * sizeof(struct fat16_directory_entry)], sizeof(struct fat16_directory_entry));
    
    return 0;  /* success */
}

/* Will replace fat16_read_root_directory_entry eventually. */
static int fat16_read_entry(uint32_t block, uint32_t index, struct fat16_directory_entry* entry_out)
{
    if(index >= ENTRIES_PER_BLOCK)
        return -1;  /* index out of range */

    byte_t buffer[512];
    if(read_block(buffer, block) < 0){
        dbgprintf("Error reading block\n");
        return -2;  /* error reading block */
    }

    struct fat16_directory_entry* dir_entry = (struct fat16_directory_entry*)(buffer + index * sizeof(struct fat16_directory_entry));

    /* Copy the directory entry to the output */
    memcpy(entry_out, &buffer[(index % ENTRIES_PER_BLOCK) * sizeof(struct fat16_directory_entry)], sizeof(struct fat16_directory_entry));
    
    return 0;  /* success */
}

/**
 * @brief 
 * 
 * @param filename 
 * @param ext 
 * @param entry_out 
 * @return int index of directory
 */
static int fat16_find_entry(const char *filename, const char* ext, struct fat16_directory_entry* entry_out)
{
    /* Search the root directory for the file. */
    for (int i = 0; i < boot_table.root_dir_entries; i++) {
        struct fat16_directory_entry entry;

        fat16_read_entry(current_dir_block ,i, &entry);
        if (memcmp(entry.filename, filename, strlen(filename)) == 0 && memcmp(entry.extension, ext, 3) == 0) {
            if (entry_out) {
                *entry_out = entry;
            }
            dbgprintf("Found file at index %d\n", i);

            /* print file info */
            dbgprintf("Filename: %s.%s (%d bytes) Attributes: 0x%x Cluster: %d %s\n", entry.filename, entry.extension, entry.file_size, entry.attributes, entry.first_cluster, entry.attributes & 0x10 ? "<DIR>" : "");
            return i;  /* Found */
        }
    }
    return -1;  /* Not found */
}

/**
 * Adds a new root directory entry.
 * @param filename The name of the file (up to 8 characters, not including the extension).
 * @param extension The file extension (up to 3 characters).
 * @param attributes Attributes for the file (read-only, hidden, system, etc.).
 * @param start_cluster The first cluster of the file.
 * @param file_size The size of the file in bytes.
 * @return 0 on success, or a negative value on error.
 */
int fat16_add_entry(uint16_t block, char *filename, const char *extension, byte_t attributes, uint16_t start_cluster, uint32_t file_size)
{
    uint16_t root_start_block = block;
    //uint16_t root_blocks = (ENTRIES_PER_BLOCK * sizeof(struct fat16_directory_entry)) / 512;
    uint16_t root_blocks = 1;

    byte_t buffer[512];

    for (uint16_t block = 0; block < root_blocks; block++) {
        if(read_block(buffer, root_start_block + block) < 0){
            return -1;  /* error reading block */
        }

        for (int entry = 0; entry < ENTRIES_PER_BLOCK; entry++) {

            struct fat16_directory_entry* dir_entry = (struct fat16_directory_entry*)(buffer + entry * sizeof(struct fat16_directory_entry));
            if (dir_entry->filename[0] == 0x00 || dir_entry->filename[0] == 0xE5) {  /* empty or deleted entry */
                /* Fill in the directory entry */
                memset(dir_entry, 0, sizeof(struct fat16_directory_entry));  /* Clear the entry */
                memset(dir_entry->filename, ' ', 8);  /* Set the filename to spaces */
                memcpy(dir_entry->filename, filename, 8);
                memcpy(dir_entry->extension, extension, 3);
                dir_entry->attributes = attributes;
                dir_entry->first_cluster = start_cluster;
                dir_entry->file_size = file_size;

                fat16_set_date(&dir_entry->created_date, 2023, 5, 31);
                fat16_set_time(&dir_entry->created_time, 12, 0, 0);

                /* Write the block back to disk */
                write_block(buffer, root_start_block + block);
                return 0;  /* success */
            }
        }
    }

    return -1;  /* no empty slot found in the root directory */
}

int fat16_read_file(const char *filename, const char* ext, void *buffer, int buffer_length)
{
    struct fat16_directory_entry entry;
    int find_result = fat16_find_entry(filename, ext, &entry);
    if (find_result < 0) {
        dbgprintf("File not found\n");
        return -1;
    }  /* File not found */

    int ret = fat16_read(&entry, 0, buffer, buffer_length);
    return ret;  /* Return bytes read */
}

int fat16_create_file(const char *filename, const char* ext, const void *data, int data_length)
{
    int first_cluster = fat16_get_free_cluster();  
    if (first_cluster < 0) {
        dbgprintf("No free cluster found\n");
        return -1;  /* No free cluster found */
    }

    struct fat16_directory_entry entry = {
        .first_cluster = first_cluster,
    };

    fat16_write(&entry, 0, data, data_length);


    fat16_add_entry(current_dir_block, filename, ext, FAT16_FLAG_ARCHIVE, first_cluster, data_length);
    fat16_sync_fat_table();

    return 0;  /* Success */ 
}

void fat16_dump_fat_table()
{
    for (int i = 0; i < 10; i++) {
        uint16_t entry = fat16_get_fat_entry(i);
        dbgprintf("0x%x -> 0x%x\n", i, entry);
    }
}

void fat16_directory_entries(uint16_t block)
{
    for (int i = 0; i < ENTRIES_PER_BLOCK; i++) {
        struct fat16_directory_entry entry;
        struct fat16_directory_entry* dir_entry = &entry;

        fat16_read_entry(block, i, &entry);
        /* Check if the entry is used (filename's first byte is not 0x00 or 0xE5) */
        if (dir_entry->filename[0] != 0x00 && dir_entry->filename[0] != 0xE5) {
            /* Print the filename (you might need to format it further depending on your needs) */
            char name[9];
            memcpy(name, dir_entry->filename, 8);
            name[8] = '\0';

            char extension[4];
            memcpy(extension, dir_entry->extension, 3);
            extension[3] = '\0';

            /* print detailed info about the entry */
            dbgprintf("Filename: %s.%s (%d bytes) Attributes: 0x%x Cluster: %d %s\n", name, extension, dir_entry->file_size, dir_entry->attributes, dir_entry->first_cluster, dir_entry->attributes & 0x10 ? "<DIR>" : "");
        }
    }
}

int fat16_mbr_add_entry(uint8_t bootable, uint8_t type, uint32_t start, uint32_t size)
{
    byte_t mbr[512];
    if(read_block(mbr, 0) < 0){
        dbgprintf("Error reading block\n");
        return -1;  /* error reading block */
    }

    for(int i = 0; i < 4; i++) {
        struct mbr_partition_entry *entry = (struct mbr_partition_entry *)&mbr[446 + (i * sizeof(struct mbr_partition_entry))];
        if(entry->type == 0x00) {
            entry->status = bootable;
            entry->type = type;
            entry->lba_start = start;
            entry->num_sectors = size;

            dbgprintf("MBR entry added\n");
            write_block(mbr, 0);
            return 0;
        }
    }

    dbgprintf("No empty slot found in the MBR\n");

    return -1;
}

void fat16_print_root_directory_entries()
{
    fat16_directory_entries(current_dir_block);
}

void fat16_change_directory(const char* name)
{
    struct fat16_directory_entry entry;
    int find_result = fat16_find_entry(name, "   ", &entry);
    if (find_result < 0) {
        dbgprintf("Directory not found\n");
        return;
    }  /* Directory not found */

    if((entry.attributes & 0x10) == 0){
        dbgprintf("Not a directory\n");
        return;
    }

    current_dir_block = get_data_start_block()+ entry.first_cluster;
    dbgprintf("Changed directory to %s\n", name);
}

void fat16_bootblock_info()
{
        /* dbgprint out bootblock information: */
    dbgprintf("bootblock information:\n");
    dbgprintf("manufacturer: %s\n", boot_table.manufacturer);
    dbgprintf("bytes_per_plock: %d\n", boot_table.bytes_per_plock);
    dbgprintf("blocks_per_allocation: %d\n", boot_table.blocks_per_allocation);
    dbgprintf("reserved_blocks: %d\n", boot_table.reserved_blocks);
    dbgprintf("num_FATs: %d\n", boot_table.num_FATs);
    dbgprintf("root_dir_entries: %d\n", boot_table.root_dir_entries);
    dbgprintf("total_blocks: %d\n", boot_table.total_blocks);
    dbgprintf("media_descriptor: %d\n", boot_table.media_descriptor);
    dbgprintf("fat_blocks: %d\n", boot_table.fat_blocks);
    dbgprintf("file_system_identifier: %s\n", boot_table.file_system_identifier);

    dbgprintf("get_fat_start_block: %d\n", get_fat_start_block());
    dbgprintf("get_root_directory_start_block: %d\n", get_root_directory_start_block());
    dbgprintf("get_data_start_block: %d\n", get_data_start_block());
}

/**
 * Formats the disk with the FAT16 filesystem.
 * @param label The volume label (up to 11 characters). (NOT IMPLEMENTED)
 * @param reserved The number of reserved blocks (usually 1).
 * 
 * @warning This will erase all data on the disk.
 * @return 0 on success, or a negative value on error.
 */
int fat16_format(char* label, int reserved)
{
    if(disk_attached() == 0){
        dbgprintf("No disk attached\n");
        return -1;
    }

    int total_blocks = (disk_size()/512)-1;
    dbgprintf("Total blocks: %d (%d/512)\n", total_blocks, disk_size());

    int label_size = strlen(label);

    struct fat_boot_table new_boot_table = {
        .manufacturer = "NETOS   ",  /* This can be any 8-character string */
        .bytes_per_plock = 512,      /* Standard block size */
        .blocks_per_allocation = 1,  /* Usually 1 for small devices */
        .reserved_blocks = reserved,        /* The boot block, will also include kernel? */
        .num_FATs = 1,               /* Standard for FAT16 */
        .root_dir_entries = 16,     /* This means the root directory occopies 1 block TODO: Currently hardcoded*/
        .total_blocks = total_blocks,
        .media_descriptor = 0xF8,    /* Fixed disk  */
        .fat_blocks = (total_blocks*sizeof(uint16_t)/512),
        .volume_label = "VOLUME1    ",
        .volume_serial_number = 0x12345678,  /* TODO: Randomize */
        .extended_signature = 0x29,  /* Extended boot record signature */
        /* ... other fields ... */
        .file_system_identifier = "FAT16   ",
        .boot_signature = 0xAA55,
    };
    //memcpy(new_boot_table.volume_label, label, label_size <= 11 ? label_size : 11);

    /* Update the boot table */
    boot_table = new_boot_table;
    fat16_bootblock_info();

    /* Write the boot table to the boot block */
    if(write_block((byte_t* )&new_boot_table, BOOT_BLOCK) < 0){
        dbgprintf("Error writing boot block\n");
        return -2;
    }

    /* Clear out the FAT tables */
    byte_t zero_block[512];
    memset(zero_block, 0, sizeof(zero_block));
    for (uint16_t i = 0; i < new_boot_table.fat_blocks; i++) {
        write_block((byte_t*) zero_block, get_fat_start_block() + i);
    }


    /* Clear out the root directory area */
    for (uint16_t i = 0; i < new_boot_table.root_dir_entries * 32 / 512; i++) {
        write_block(zero_block, get_root_directory_start_block() + i);
    }

    fat16_mbr_add_entry(MBR_STATUS_ACTIVE, MBR_TYPE_FAT16_LBA, BOOT_BLOCK, total_blocks);

    dbgprintf("FAT16 formatted\n");

    return 0;  /* assume success */
}


void fat16_set_time(uint16_t *time, ubyte_t hours, ubyte_t minutes, ubyte_t seconds)
{
    /* Clear the existing time bits */
    *time &= 0xFC00;
    /* Set the hours bits */
    *time |= (hours & 0x1F) << 11;
    /* Set the minutes bits */
    *time |= (minutes & 0x3F) << 5;

    /* Set the seconds bits (converted to two-second periods) */
    ubyte_t twoSecondPeriods = seconds / 2;
    *time |= twoSecondPeriods & 0x1F;
}

void fat16_set_date(uint16_t *date, uint16_t year, ubyte_t month, ubyte_t day)
{
    /* Clear the existing date bits */
    *date &= 0xFE00;
    /* Set the year bits (offset from 1980) */
    *date |= ((year - 1980) & 0x7F) << 9;
    /* Set the month bits */
    *date |= (month & 0x0F) << 5;
    /* Set the day bits */
    *date |= (day & 0x1F);
}

int fat16_initialize()
{
    /* load the bootblock */
    read_block((byte_t*)&boot_table, BOOT_BLOCK);

    /* confirm that bootblock is correct */
    if (memcmp(boot_table.manufacturer, "NETOS   ", 8) != 0) {
        dbgprintf("Bootblock manufacturer is not NETOS\n");
        return -1;
    }

    fat16_bootblock_info();

    /* Load FAT table into memory. */
    fat_table_memory = (byte_t*)kalloc((boot_table.fat_blocks * 512));  /* Allocate memory for the FAT table */
    for (uint16_t i = 0; i < boot_table.fat_blocks; i++) {
        read_block(fat_table_memory + i * 512, get_fat_start_block() + i);
    }
    fat16_set_fat_entry(0, 0xFF00 | 0xF8); 
    fat16_allocate_cluster(1);
    fat16_add_entry(get_root_directory_start_block(), "VOLUME1 ", "   ", FAT16_FLAG_VOLUME_LABEL, 0, 0);

    fat16_dump_fat_table();

    /* init mutexes */
    mutex_init(&fat16_table_lock);
    mutex_init(&fat16_write_lock);
    mutex_init(&fat16_management_lock);

    current_dir_block = get_root_directory_start_block();

    dbgprintf("FAT16 initialized\n");

    return 0;
}

/* Open a file. Returns a file descriptor or a negative value on error. */
int fat16_open(const char *path);

/* Close an open file. Returns 0 on success, and a negative value on error. */
int fat16_close(int fd);

/* Create a new directory. Returns 0 on success, and a negative value on error. */
int fat16_mkdir(const char *path);

/* Delete a file or directory. Returns 0 on success, and a negative value on error. */
int fat16_remove(const char *path);

/* List the contents of a directory. */
int fat16_listdir(const char *path, void (*callback)(const char *name, int is_directory));
