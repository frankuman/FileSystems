/**
 * @file fs.h
 * @author Oliver Bölin & Philippe Van Daele
 * @brief 
 * @version 1
 * @date 2023-08-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */


#include <iostream>
#include <cstdint>
#include <string>
#include "disk.h"

#ifndef __FS_H__
#define __FS_H__

#define ROOT_BLOCK 0
#define FAT_BLOCK 1
#define FAT_FREE 0
#define FAT_EOF -1

#define TYPE_FILE 0
#define TYPE_DIR 1
#define READ 0x04
#define WRITE 0x02
#define EXECUTE 0x01
#define OLD 0
#define NEW 1
#define DOT_INDEX 1
#define DOUBLE_DOT_INDEX 1

const std::string PARENT_DIR = "..";

struct dir_entry {
    char file_name[56]; // name of the file / sub-directory
    uint32_t size; // size of the file in bytes
    uint16_t first_blk; // index in the FAT for the first block of the file
    uint8_t type; // directory (1) or file (0)
    uint8_t access_rights; // read (0x04), write (0x02), execute (0x01)
};

const unsigned MAX_DIR_ENTRIES = (BLOCK_SIZE / sizeof(dir_entry));

struct dir_info {
    int block;  // disk block number where the dir 'entries' are stored
    int index;  // index to actual dir_entry in the 'entries' array
    dir_entry entries[MAX_DIR_ENTRIES]; // all directory entries in a block
};

class FS {
private:
    Disk disk;
    std::string working_directory = "..";
    // size of a FAT entry is 2 bytes
    int16_t fat[BLOCK_SIZE/2];
    //----------- OWN FUNCTIONS -----------
    int ReadFromFAT();
    int writeToFAT();
    int findFreeBlock();
    uint16_t curr_blk = ROOT_BLOCK;
    int FindingFileEntry(std::string filepath, uint8_t newOrExisting, dir_info& dir, uint8_t access_rights);
    int FileEntry(int dir_block, std::string filepath, int& dir_index, dir_entry* dir_entries, uint8_t NewOrOld, uint8_t accessrights);
    int GetDirectoryBlock(std::string filepath, int& dir_block, uint8_t accessRights);
    int get_file_string(std::string filepath,std::string &text);
    int create_with_string(std::string filepath,std::string line);
    int get_free_blocks(int* free_blocks,int amount_blocks,int start_block);
    int write_block(std::string text, int block_amount, int* free_blocks);
    int update_FAT(int* free_block, int block_amount);
    int get_dir_name(std::string path, std::string &name, std::string &absolute_path);
    void goHome();
    void removeTrailingSlash(std::string& str);
    bool accessread(dir_entry dir_entries);
    bool accesswrite(dir_entry dir_entries);
    int path_handler(std::string &path, bool &is_absolute_path, std::string &dirpath);
    std::string getFileName(std::string filepath);
    std::string getDirPath(std::string dirpath);
    int dotdot_remover(std::string &dirpath);
public:
    FS();
    ~FS();
    // formats the disk, i.e., creates an empty file system
    int format();
    // create <filepath> creates a new file on the disk, the data content is
    // written on the following rows (ended with an empty row)
    int create(std::string filepath);
    // cat <filepath> reads the content of a file and prints it on the screen
    int cat(std::string filepath);
    // ls lists the content in the current directory (files and sub-directories)
    int ls();

    // cp <sourcepath> <destpath> makes an exact copy of the file
    // <sourcepath> to a new file <destpath>
    int cp(std::string sourcepath, std::string destpath);
    // mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
    // or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
    int mv(std::string sourcepath, std::string destpath);
    // rm <filepath> removes / deletes the file <filepath>
    int rm(std::string filepath);
    // append <filepath1> <filepath2> appends the contents of file <filepath1> to
    // the end of file <filepath2>. The file <filepath1> is unchanged.
    int append(std::string filepath1, std::string filepath2);

    // mkdir <dirpath> creates a new sub-directory with the name <dirpath>
    // in the current directory
    int mkdir(std::string dirpath);
    // cd <dirpath> changes the current (working) directory to the directory named <dirpath>
    int cd(std::string dirpath);
    // pwd prints the full path, i.e., from the root directory, to the current
    // directory, including the current directory name
    int pwd();

    // chmod <accessrights> <filepath> changes the access rights for the
    // file <filepath> to <accessrights>.
    int chmod(std::string accessrights, std::string filepath);
};

#endif // __FS_H__
