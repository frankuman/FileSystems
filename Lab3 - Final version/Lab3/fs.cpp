/**
 * @file fs.cpp
 * @author Oliver Bölin & Philippe Van Daele
 * @brief 
 * @version 1
 * @date 2023-08-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

/*
FÖRORD
------------------------------------------------------------------------------------------------
Root heter ".." och inte "/"

--Designval--
Om man ska göra move eller copy till en directory, måste man skriva cp(file, /dir) eller mv(file,/dir)
Man kan inte skriva cd(d1) om man är i d1/d2, då det inte funkar så vanligtvist i t.ex Linux.

--Angående uppgiften--
Denna uppgift tog alldeles för lång tid och ger alldeles för lite högskolepoäng för att vara logisk. 
Det är möjligt att vi har övertänkt allt och gjort det svårt för oss själva, men detta var verkligen inte enkelt.
Det finns möjlighet att vi kan göra korden snabbare, kortare och smidigare men vi har verkligen inte tiden för att göra detta, 
med tanke på den enorma tiden vi har lagt på något som egentligen ska vara 1 hp = ca 35-40h ...
Hoppas detta räcker. Lycka till med rättningen!
------------------------------------------------------------------------------------------------
*/

#include <iostream>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
}

FS::~FS()
{

}

//Userdefined functions
int FS::ReadFromFAT(){
    int status = disk.read(FAT_BLOCK, (uint8_t*)fat);
    return status;    
}

int FS::findFreeBlock() {
    int fatIndex;
    for (fatIndex = 2; fatIndex < (BLOCK_SIZE / 2); fatIndex++){
        if (fat[fatIndex] == FAT_FREE){
            break;
        }
    }
    if (fatIndex == (BLOCK_SIZE / 2)) {
        fatIndex = -1;
    }

    return fatIndex;
}

// formats the disk, i.e., creates an empty file system
int FS::format(){
    std::cout << "FS::format()\n";

    // initializes FAT
    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK] = FAT_EOF;

    for (int i = 2; i < (BLOCK_SIZE / 2); i++)
    {
        fat[i] = FAT_FREE;
    }
    int status = writeToFAT();
    if(status){
        return status;
    }
    //initialize blocks to all zeroes
    uint8_t root_block[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        root_block[i] = 0;
    }

    status = disk.write(ROOT_BLOCK, root_block);
    goHome();
    return status;
}

int FS::FindingFileEntry(std::string filepath, uint8_t newOrExisting, dir_info &dir, uint8_t accessrights){

    int status = GetDirectoryBlock(filepath, dir.block, accessrights);
    if(status) return status;

    return FileEntry(dir.block, filepath, dir.index, dir.entries, newOrExisting, accessrights);
}

std::string FS::getDirPath(std::string filepath)
{
    std::string::size_type idx;

    idx = filepath.rfind('/');

    std::string dirpath;
    if (idx != std::string::npos)
        dirpath = filepath.substr(0, idx);
    else
        dirpath = "";
    return dirpath;
}

int FS::GetDirectoryBlock(std::string filepath, int &dir_block, uint8_t accessrights){

    std::string dirpath = getDirPath(filepath);

    if (dirpath.empty()) {

        if (filepath.front() == '/') {
            // absolute path - starts at the root dir
            dir_block = ROOT_BLOCK;
        }
        else {
            dir_block = curr_blk;
        }

    }
    else {
        
        if (dirpath.front() == '/') {
            // absolute path - starts at the root dir
            dir_block = ROOT_BLOCK;
            dirpath = dirpath.substr(1);
        }
        else {
            // relative path - starts at the current dir
            dir_block = curr_blk;
        }

        // traverse dirpath to find the correct dir block
        while (!dirpath.empty()) {
            std::string::size_type idx = dirpath.find('/');
            std::string dirname;
            if (idx != std::string::npos) {
                dirname = dirpath.substr(0, idx);
                dirpath = dirpath.substr(idx + 1);
            }
            else {
                dirname = dirpath;
                dirpath = "";
            }

            int dir_index;
            dir_entry dir_entries[MAX_DIR_ENTRIES];
            
            int sts = FileEntry(dir_block, dirname, dir_index,dir_entries, OLD, accessrights);
            if (sts)
                return sts;

            if (dir_entries[dir_index].type != TYPE_DIR) {
                std::cout << "Error: Filepath is not a directory: " << dirname << "\n";
                return 1;
            }
            dir_block = dir_entries[dir_index].first_blk;
        }
    }

    return 0;

}

std::string FS::getFileName(std::string filepath)
{
    std::string::size_type idx;

    idx = filepath.rfind('/');

    if (idx != std::string::npos)
    {
        filepath = filepath.substr(idx + 1);
    }
    return filepath;
}

int FS::FileEntry(int dir_block, std::string filepath, int &index, dir_entry *dir_entries, uint8_t NewOrOld, uint8_t accessrights){

    int status = disk.read(dir_block, (uint8_t*)dir_entries);
    if (status)
        return status;

    std::string filename = getFileName(filepath);
    index = -1;
    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++)
    {

        if (dir_entries[i].file_name[0] == 0)
        {
            if (NewOrOld == NEW) {
                // We're looking for a free entry - and this is a free dir entry
                if (index < 0)
                    index = i;
            }
        }
        else if (strcmp(dir_entries[i].file_name, filename.c_str()) == 0) {
            
            if (NewOrOld == NEW) {
                index = -2;
            }
            else {
                index = i;
            }
            break;
        }
    }

    if (index < 0) {
        if (index == -2) {
            std::cout << "Error: File already exists: " << filename << "\n";
        }
        else {
            if (NewOrOld == NEW){
                std::cout << "Error: No more free directory entries\n";
            }
            else{
                std::cout << "Error: File not found: " << filename << "\n";
            }
        }
        status = 1;
    }

    if (accessrights > 0) {

    // Check file access
        if (NewOrOld == OLD) {
            // Existing file: check the requested access to the file
            bool access = (dir_entries[index].access_rights & accessrights) == accessrights;
            
            //std::cout << "DEBUG (access): " << (int) accessrights << std::endl;
            if (!access) {
                switch ((int)dir_entries[index].access_rights){ 
                case 1:
                    std::cout << "Error 1: EXE access is only permitted\n";

                    break;
                case 2:
                    std::cout << "Error 2: Write access is only permitted\n";
                    break;
                case 3:
                    std::cout << "Error 3: Execute & Write access is only permitted \n";
                    break;
                case 4:
                    std::cout << "Error 4: Read access is only permitted \n";
                    break;
                case 5:
                    std::cout << "Error 5: Read & Executute access is only permitted \n";
                    break;
                case 6:
                    std::cout << "Error 6: Read & Write access is only permitted \n";
                    break;
                case 7:
                    std::cout << "Error 7: Read & Write & Execute access is only permitted \n";
                    break;
                default:
                    
                    break;
                }
                return -1; 
            }
        }
    }
    return status;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int FS::create(std::string filepath){

    std::cout << "FS::create(" << filepath << ")\n";
    int status = ReadFromFAT();
    if(status){
        return status;
    }
    if(filepath.length() > 55){
        std::cout << "Error: Name can't be longer than 55\n" << std::endl;
        return 1;
    }
    std::string nameOfFile = filepath;
    dir_info dir; 

    //status = FileEntry(dir.block,filepath,dir.index,dir.entries, NEW);
    status = FindingFileEntry(filepath,NEW,dir,WRITE);
    if(status){
        return status;
    }
    
    int curr_blk = findFreeBlock();
    if(curr_blk == -1){
        std::cout << "Error: No free blocks\n" << std::endl;
        return 1;
    }
    //std::cout << "CHECK: " << working_directory << dir.entries[dir.index].access_rights << std::endl;

    fat[curr_blk] = FAT_EOF;
    int first_block = curr_blk;
    char data[BLOCK_SIZE] = {int(0)}; //Data container  memset(data, 0, BLOCK_SIZE);
    memset(data,0,BLOCK_SIZE);

    std::string line = ".";
    uint32_t size = 0;      // Current block size counter
    uint32_t tot_size = 0;  // File size counter
    //To find the next block
    bool done = false;
    while(!done){
        std::getline(std::cin, line); // max is 4096
        //std::cout << "block: " << (line.length() + 1 + size) << std::endl;
        if(line.empty()){
            //std::cout << "Done " << std::endl;
            done = true;
            status = disk.write(curr_blk, (uint8_t*)data);
            if (status){
                return status;
            }   
        }
        else if((line.length() + 1 + size) > BLOCK_SIZE){
            int length = BLOCK_SIZE - size;
            tot_size += length;
            memcpy(&data[size], line.c_str(), length);
            //std::cout << "DEBUG: Placing " << line.substr(length) << " at block " << curr_blk << std::endl;

            status = disk.write(curr_blk, (uint8_t*)data);
            
            if(status){
                return 1;
            }
            memset(data,0,BLOCK_SIZE);
            int new_block = findFreeBlock();
                if (new_block == -1) {
                    std::cout << "Error: No free blocks\n";
                    return 1;
                }
            //std::cout << "DEBUG: Old block: " << curr_blk << " New block: " << new_block << std::endl;

            fat[new_block] = FAT_EOF;
            fat[curr_blk] = new_block;
            curr_blk = new_block;
            size = 0;
            line = line.substr(length);
            //tot_size++;
            tot_size++;
            
        }
        else{
            //std::cout << "DEBUG: Placing " << line << " at block " << curr_blk << std::endl;
            memcpy(&data[size], line.c_str(), line.length() + 1);
            size += line.length();
            tot_size += line.length();
            //tot_size++;
            tot_size++;
            line.clear();
        }
    }
    status = disk.write(curr_blk, (uint8_t*)data);
    if(status){
        return status;
    }
    if(dir.index == -1){
        std::cout << "Error: The directory is full" << std::endl;
        return 1;
    }
    memcpy(dir.entries[dir.index].file_name, nameOfFile.c_str(), nameOfFile.length() + 1);
    dir.entries[dir.index].first_blk = first_block;
    dir.entries[dir.index].size = tot_size;
    dir.entries[dir.index].type = TYPE_FILE;
    dir.entries[dir.index].access_rights = READ | WRITE;

    status = disk.write(dir.block, (uint8_t*)dir.entries);
    if (status){
        return status;
    }

    status = writeToFAT();
    if (status){
        return status;
    }
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    // read FAT from disk to memory
    int sts = ReadFromFAT();
    if (sts) return sts;

    // Find the directory index for the passed file and read directory block into memory.
    dir_info dir;
    sts = FindingFileEntry(filepath, OLD, dir, READ);
    if (sts) return sts;

    if (dir.entries[dir.index].type == TYPE_DIR) {
        std::cout << "Error: '" << filepath << "' is a directory\n";
        return 1;
    }

    int file_blk = dir.entries[dir.index].first_blk;
    uint32_t file_size = dir.entries[dir.index].size;

    uint32_t size = 0;
    uint32_t tot_size = 0;
    char data[BLOCK_SIZE];

    sts = disk.read(file_blk, (uint8_t*)data);
    if (sts)
        return sts;

    while (tot_size < file_size) {

        // A line can potentially span over multiple blocks, so check block boundaries.
        int len = 0;
        for (int i = size; i < BLOCK_SIZE; i++, len++)
            if (data[i] == '\0') break;

        printf("%-*.*s", len, len, &data[size]);
        size += len;
        tot_size += len;
        if (size == BLOCK_SIZE) {
            // line continues in next block
            size = 0;
            file_blk = fat[file_blk];
            if (file_blk == FAT_EOF) {
                printf("Programming error ... unexpected EOF detected\n");
                return 1;
            }

            sts = disk.read(file_blk, (uint8_t*)data);
            if (sts)
                return sts;

        }
        else {
            size++;
            tot_size++;
            printf("\n");
        }
    }

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls(){

    std::cout << "FS::ls()\n";

    // read FAT from disk to memory
    int status = ReadFromFAT();
    if (status){
        return status;
    }
    
    
    // Get the directory entries for the current dir
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = disk.read(curr_blk, (uint8_t*)dir_entries);
    if (status){
        return status;
    }
    std::string cwd = working_directory;

    std::cout << "Name \t Size \t Access rights \t Type " << std::endl;
    std::cout << "---- \t ---- \t ------------- \t ---- " << std::endl;
    
    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
        if (dir_entries[i].file_name[0] != 0){
            std::string blocks = std::to_string(curr_blk);
            std::string filetype;
            std::string access_right = "";
        
            if (dir_entries[i].access_rights & READ) {
                access_right += "r";
            } else {
                access_right += "-";
            }

            if (dir_entries[i].access_rights & WRITE) {
                access_right += "w";
            } else {
                access_right += "-";
            }

            if (dir_entries[i].access_rights & EXECUTE) {
                access_right += "x";
            } else {
                access_right += "-";
            }

            int curr_blk = dir_entries[i].first_blk;
            if (curr_blk != ROOT_BLOCK){
                if(dir_entries[i].type == 0){
                    std::cout << dir_entries[i].file_name << "\t " << dir_entries[i].size << "\t " << access_right << "\t" << "\t" << " file" << std::endl;
                }
                else{
                    std::cout << dir_entries[i].file_name << "\t " << " - " << "\t " << access_right << "\t" << "\t" << " dir" << std::endl;
                }
            }

        }
    }
    std::cout << "\n"; // Just too make some spaceing for estetics

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath){
    int status = ReadFromFAT();
    if (status){
        return status;
    }

    dir_info source_test;
    status = FindingFileEntry(sourcepath,OLD,source_test,READ);
    if(status){
        return status;
    }
    //check if the desitnation file already exists
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = disk.read(curr_blk, (uint8_t*)dir_entries);
    if (status){
        return status;
    }

    int checker = -1;
    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
        if (dir_entries[i].file_name[0] != 0){
            std::string blocks = std::to_string(curr_blk);

                if(dir_entries[i].file_name == sourcepath){
                    checker = 1;   
                }
            }
    }
    if(checker == -1){
        std::cout << "Error: The source file doesn't exist!" << std::endl;
        return 0;
    }
    if(destpath == ".."){ 
        std::string temp_working_dir = working_directory;
        temp_working_dir = temp_working_dir.substr(2,temp_working_dir.length());
        size_t pos = temp_working_dir.find_last_of("/\\");
        if(pos != std::string::npos){
            temp_working_dir = temp_working_dir.substr(0,pos);
            destpath = temp_working_dir;
            //std::cout << "[DESTPATH CP]: "<< destpath << std::endl;
        }
    }
    dir_info source;
    dir_info destination;
    bool bool_dir = false;
    bool dir_exists = true;
    if(destpath.front() == '/'){
        ///std::cout << "1 [DESTPATH CP]: "<< destpath << std::endl;
        status = FindingFileEntry(destpath, OLD, destination, WRITE);
        if(status){
            return status;
        }
        status = disk.read(destination.block, (uint8_t*)dir_entries);
        if(status){
            return status;
        }
        destpath = destpath.substr(1,destpath.length());

        dir_exists = false;
        bool_dir = true;
        for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
            if (dir_entries[i].file_name[0] != 0){
                std::string blocks = std::to_string(curr_blk);
                    if(dir_entries[i].file_name == destpath && dir_entries[i].type == TYPE_DIR){
                        dir_exists = true; 
                    }
            }
        }
    }
    if(!dir_exists){
        std::cout << "Error: The destination directory doesn't exist!" << std::endl;
        return 0;
    }

    std::string path = destpath;
    status = FindingFileEntry(sourcepath, OLD, source, READ);
    if(status) return status;

    if(bool_dir){
        path = "/" + destpath + "/" + sourcepath;
        //std::cout << "2 [PATH CP]: "<< path << std::endl;

        status = FindingFileEntry(path, NEW, destination, WRITE);
        if(status) return status;
    }
    else{
        path = destpath;
        status = FindingFileEntry(path, NEW, destination, WRITE);
        if(status) return status;
    }

    char data[BLOCK_SIZE];
    memset(data, 0, BLOCK_SIZE);

    int first_block = -1;
    int previous_block = -1;
    int source_block = source.entries[source.index].first_blk;
    while(source_block != FAT_EOF){

        //finding FAT free entry
        int curr_blk = findFreeBlock();
        if(curr_blk < 0){
            std::cout << "Error: There isn't anymore free blocks in FAT" << std::endl;
            return 1;
        }

        if(first_block == -1){
            first_block = curr_blk;
        }
        if(previous_block != -1){
            fat[previous_block] = curr_blk;
        }
        fat[curr_blk] = FAT_EOF;

        status = disk.read(source_block, (uint8_t*)data);
        if(status) return status;
        status = disk.write(curr_blk, (uint8_t*)data);
        if(status) return status;

        previous_block = curr_blk;
        source_block = fat[source_block];
    }


    //copying the file entry info from source to destination
    memcpy(&destination.entries[destination.index], &source.entries[source.index], sizeof(dir_entry));
    
    //set the new filename and the new first-block
    if(bool_dir){
        memcpy(destination.entries[destination.index].file_name, sourcepath.c_str(), sourcepath.length() + 1);
    }
    else{
        memcpy(destination.entries[destination.index].file_name, destpath.c_str(), destpath.length() + 1);
    }
    destination.entries[destination.index].first_blk = first_block;

    status = disk.write(destination.block, (uint8_t*)destination.entries);
    if(status) return status;

    status = writeToFAT();
    if(status) return status;

    return 0;
}


// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
int FS::mv(std::string sourcepath, std::string destpath){ // cp and rm combined
    int status = ReadFromFAT();
    if (status){
        return status;
    }

    dir_info source_test;
    status = FindingFileEntry(sourcepath,OLD,source_test,READ);
    if(status){
        return status;
    }

    //check if the desitnation file already exists
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = disk.read(curr_blk, (uint8_t*)dir_entries);
    if (status){
        return status;
    }

    int checker = -1;
    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
        if (dir_entries[i].file_name[0] != 0){
            std::string blocks = std::to_string(curr_blk);

                if(dir_entries[i].file_name == sourcepath){
                    checker = 1;   
                }
            }
    }
    if(checker == -1){
        std::cout << "Error: The source file doesn't exist!" << std::endl;
        return 0;
    }
    if(destpath == ".."){ 
        std::string temp_working_dir = working_directory;
        temp_working_dir = temp_working_dir.substr(2,temp_working_dir.length());
        size_t pos = temp_working_dir.find_last_of("/\\");
        if(pos != std::string::npos){
            temp_working_dir = temp_working_dir.substr(0,pos);
            destpath = temp_working_dir;
        }
    }

    dir_info source;
    dir_info destination;
    bool bool_dir = false;
    bool dir_exists = true;

    if(destpath.front() == '/'){
        status = FindingFileEntry(destpath, OLD, destination, WRITE);
        if(status){
            return status;
        }

        status = disk.read(destination.block, (uint8_t*)dir_entries);
        if(status){
            return status;
        }
        destpath = destpath.substr(1,destpath.length());

        dir_exists = false;
        bool_dir = true;
        for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
            if (dir_entries[i].file_name[0] != 0){
                std::string blocks = std::to_string(curr_blk);
                    if(dir_entries[i].file_name == destpath && dir_entries[i].type == TYPE_DIR){
                        dir_exists = true; 
                    }
            }
        }
    }
    if(!dir_exists){
        std::cout << "Error: The destination directory doesn't exist!" << std::endl;
        return 0;
    }

    std::string path = destpath;
    status = FindingFileEntry(sourcepath, OLD, source, READ);
    if(status) return status;

    if(bool_dir){
        path = "/" + destpath + "/" + sourcepath;        
        status = FindingFileEntry(path, NEW, destination, WRITE);
        if(status) return status;
    }
    else{
        path = destpath;
        status = FindingFileEntry(path, NEW, destination, READ);

        if(status) return status;
    }

    char data[BLOCK_SIZE];
    memset(data, 0, BLOCK_SIZE);

    int first_block = -1;
    int previous_block = -1;
    int source_block = source.entries[source.index].first_blk;
    while(source_block != FAT_EOF){

        //finding FAT free entry
        int curr_blk = findFreeBlock();
        if(curr_blk < 0){
            std::cout << "Error: There isn't anymore free blocks in FAT" << std::endl;
            return 1;
        }

        if(first_block == -1){
            first_block = curr_blk;
        }
        if(previous_block != -1){
            fat[previous_block] = curr_blk;
        }
        fat[curr_blk] = FAT_EOF;

        status = disk.read(source_block, (uint8_t*)data);
        if(status) return status;
        status = disk.write(curr_blk, (uint8_t*)data);
        if(status) return status;

        previous_block = curr_blk;
        source_block = fat[source_block];
    }


    //copying the file entry info from source to destination
    
    //set the new filename and the new first-block
    if(bool_dir){
        memcpy(&destination.entries[destination.index], &source.entries[source.index], sizeof(dir_entry));

        memcpy(destination.entries[destination.index].file_name, sourcepath.c_str(), sourcepath.length() + 1);
        destination.entries[destination.index].first_blk = first_block;

        status = disk.write(destination.block, (uint8_t*)destination.entries);
        if(status) return status;
        
        int disk_block = source.entries[source.index].first_blk;
        while(disk_block != FAT_EOF){
            int next_block = fat[disk_block];
            fat[disk_block] = FAT_FREE;
            disk_block = next_block;
        }

        //writing the empty block to disk
        memset(&source.entries[source.index], 0, sizeof(dir_entry));
        status = disk.write(source.block, (uint8_t*)source.entries);
        if (status) return status;
    }
    else{
        status = FindingFileEntry(sourcepath, OLD, source, READ);
        if(status) return status;
        //std::cout << "Check (rename) source.entries[source.index].file_name = "<< source.entries[source.index].file_name << std::endl;
        memcpy(source.entries[source.index].file_name, destpath.c_str(), destpath.length() + 1);

        status = disk.write(source.block, (uint8_t*)source.entries);
        if (status) return status;
    }
    
    status = writeToFAT();
    if(status) return status;

    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath){
    std::cout << "FS::rm(" << filepath << ")\n";
    
    int status = ReadFromFAT();
    if(status) return status;
    
       //check if the desitnation file already exists
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = disk.read(curr_blk, (uint8_t*)dir_entries);
    if (status){
        return status;
    }
    
    int checker = -1;
    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
        if (dir_entries[i].file_name[0] != 0){
            std::string blocks = std::to_string(curr_blk);
                if((dir_entries[i].file_name == filepath)){
                    checker = 1;
                    if((dir_entries[i].file_name == filepath) && dir_entries[i].type == TYPE_DIR){
                        checker = -2;
                    }
                }
                
            }
    }
    if(checker == -1){
        std::cout << "Error: The file or directory doesn't exist!" << std::endl;
        return 1;
    }
    if(checker == -2){
        std::cout << "Error: You can't remove a directory!" << std::endl;
        return 1;
    }

    dir_info source;
    status = FindingFileEntry(filepath, OLD, source, WRITE);
    if(status) return status;

    
    int disk_block = source.entries[source.index].first_blk;
    while(disk_block != FAT_EOF){
        int next_block = fat[disk_block];
        fat[disk_block] = FAT_FREE;
        disk_block = next_block;
    }

    //writing the empty block to disk
    memset(&source.entries[source.index], 0, sizeof(dir_entry));
    status = disk.write(source.block, (uint8_t*)source.entries);
    if (status) return status;
    
    status = writeToFAT();
    if (status) return status;
    
    return 0;
}

int FS::get_file_string(std::string filepath,std::string &text){
    //This function works the same as cat but modifies the reference text to be the text of the filepath. 
    int status = ReadFromFAT();
    if(status){
        return status;
    }

    dir_info dir;
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    
    status = FindingFileEntry(filepath,OLD,dir,READ);
    if(status){
        return status;
    }
    
    int file_block = dir.entries[dir.index].first_blk;
    

    uint32_t size = 0;
    uint32_t tot_size = 0;

    char data[BLOCK_SIZE];
    

    status = disk.read(file_block, (uint8_t*)data);
    if(status){
        return status;
    }
    int length = 0;

    while(tot_size < dir.entries[dir.index].size){
        for (int i = size; i < BLOCK_SIZE; i++, length++){
            if (data[i] == '\0'){
                break;
            } 
        }
        text += &data[size];
        size += length;
        tot_size += length;
        size++;
        tot_size++;
        
    }
    return 0;

}

int FS::get_free_blocks(int* free_blocks,int amount_blocks,int start_block){
    //get new
    int status = disk.read(FAT_BLOCK, (uint8_t*) &fat);
    if(status){
        return status;
    }

    int free_blocks_counter = 0;
    //calculates free blocks
    for(int i = 2; i < BLOCK_SIZE/2 && free_blocks_counter < amount_blocks; i++){
        if(fat[i] == FAT_EOF){
            free_blocks[free_blocks_counter+start_block] = i;
            free_blocks_counter++;
        }
    }
    //std::cerr << "DEBUG: Free blocks: " << *free_blocks << std::endl;

    return 0;
}
int FS::update_FAT(int* free_block, int block_amount){
    // WARNING THIS IS COPYPASTED
    if (disk.read(FAT_BLOCK, (uint8_t*) &fat) < 0 ) {
        std::cerr << "Error: FAT encountered disk Error" << std::endl;
        return -1;
    }

	// link FAT
	for (int i = 0; i < block_amount-1; i++){
        // if no free blocks in FAT was reached (incase other function than get_free_blocks was used)
        if (i >= BLOCK_SIZE/2) {
            std::cerr << "Error: Disk is full" << std::endl;
            return -1;
        }

		fat[free_block[i]] = free_block[i+1];
	}
	fat[free_block[block_amount-1]] = FAT_EOF;

	// FAT update
	if (disk.write(FAT_BLOCK, (uint8_t*) &fat) != 0){
		std::cerr << "Error: FAT encountered disk Error" << std::endl;
		return -1;
	}
    //std::cerr << "DEBUG: No Error - Disk updated" << std::endl;
	return 0;
}
int FS::write_block(std::string text, int block_amount, int* free_blocks){
    int status = update_FAT(free_blocks,block_amount);
    if(status) return status;
    char c_write[BLOCK_SIZE];
    
    for(int i = 0; i < block_amount -1; i++){
        text.copy(c_write, BLOCK_SIZE, i*BLOCK_SIZE);
        //std::cout << "DEBUG- text: " << text << std::endl;

        if(disk.write(free_blocks[i], (uint8_t*) &c_write) != 0){
            std::cerr << "Error: Couldnt write" << std::endl;

        }
        
    }
    //Write the last block
    text.copy(c_write,text.length()%BLOCK_SIZE,(block_amount-1)*BLOCK_SIZE);
    c_write[text.length()%BLOCK_SIZE] = '\0';
    disk.write(free_blocks[block_amount-1],(uint8_t*) &c_write);
    //std::cout << "DEBUG- c_write: " << c_write << std::endl;
    return 0;
}
// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string sourcepath, std::string destinationpath){
    int status = ReadFromFAT();
    if(status) return status;

    std::cout << "FS::append(" << sourcepath << "," << destinationpath << ")\n";
    std::string temp_append_text;
    

    get_file_string(sourcepath,temp_append_text);
    
    std::string append_text = "\n" + temp_append_text;
    
    //Find first block of paths
    dir_info source;
    dir_info destination;
    status = FindingFileEntry(sourcepath, OLD, source, READ);
    if(status) return status;
    
    status = FindingFileEntry(destinationpath, OLD, destination, WRITE);
    if(status) return status;
    

    if (!(source.entries[source.index].access_rights & READ)) {
        std::cout << "Error: You do not have access rights to read from source! :(\n";
        return 0;
    }
    if (!(destination.entries[destination.index].access_rights & WRITE)) {
        std::cout << "Error: You do not have access rights to write to destination! :(\n";
        return 0;
    }
    int source_block = source.entries[source.index].first_blk;
    int destination_block = destination.entries[destination.index].first_blk;
    int source_size = source.entries[source.index].size;
    int destination_size = destination.entries[destination.index].size;
    int tot_size = destination_size + source_size;

    //We need the latest fat from disk

    if(disk.read(FAT_BLOCK,(uint8_t*) &fat)< 0){
        return -1;
    }
    while(fat[destination_block] != FAT_EOF){
        destination_block = fat[destination_block];
    }

    char text_buffer[BLOCK_SIZE];
    if(disk.read(destination_block,(uint8_t*) &text_buffer)<0){
        return -1;
    }
    append_text = text_buffer + append_text;
    //Calculation of how many blocks file will be
    int block_amount = ceil((append_text.length()+1) / (float) BLOCK_SIZE);
    if(block_amount < 0){
        return -1;
    }
    destination.entries[destination.index].size = tot_size;


    int free_blocks[block_amount+1];

    free_blocks[0] = destination_block;
    
    status = (get_free_blocks(free_blocks,block_amount-1,1));    
    if(status){
        return status;
    }
    
    status = write_block(append_text,block_amount,free_blocks);
    if(status){
        return status;
    }

    status = disk.write(destination.block, (uint8_t*)destination.entries);
    if(status){
        return status;
    }
    status = writeToFAT();
    if(status){
        return status;
    }
    return 0;

}
int FS::get_dir_name(std::string path, std::string &last_dir, std::string &absolute_path){
    //We should extract the path name here
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos){
        return -1;
    }
    absolute_path = path.substr(0,pos);
    last_dir = path.substr(pos + 1);
    return 0;
}

int FS::path_handler(std::string &test, bool &is_absolute_path, std::string &dirpath){
    is_absolute_path = false;
    std::string temp_working_directory = working_directory;

    if(dirpath == ".."){
        size_t split_wd = temp_working_directory.find_last_of("/\\");
        if(split_wd != std::string::npos){
            // m1/m2... 
            temp_working_directory = std::string(&temp_working_directory[3], &temp_working_directory[split_wd]);

            //temp_working_directory = temp_working_directory.substr(3,size);
            test = "/" + temp_working_directory + "/" + dirpath;
            is_absolute_path = true;
            return 0;
        }
    }
    std::string last_dir;
    std::string absolute_path;
    int status = get_dir_name(dirpath, last_dir, absolute_path);
    test = dirpath;
    if(status == -1){ //We're currently in the working directory where dir should be placed.
        test = working_directory + "/" + dirpath;
        return 0;
        
    }
    if(status == 0){ //We either have absolute path or just a mkdir /subdir1 in cwd
        if(absolute_path.length() == 0){
            dirpath = last_dir;
            test = working_directory + "/" + dirpath;
            return 0;
        }
        else{ //d1/d2/../d3
            is_absolute_path = true;
            dirpath = last_dir;
            
            if(absolute_path == ".." || dirpath == ".."){ 
                size_t split_wd = temp_working_directory.find_last_of("/\\");
                if(split_wd != std::string::npos){
                    // m1/m2... 
                    temp_working_directory = std::string(&temp_working_directory[3], &temp_working_directory[split_wd]);

                    //temp_working_directory = temp_working_directory.substr(3,size);
                    test = "/" + temp_working_directory + "/" + dirpath;
                }
            }
            std::string dotdotpath = test;
            size_t pos = dotdotpath.find_last_of("..\\");
            size_t pos_slash = temp_working_directory.find_last_of("/\\");
            size_t pos1;
            bool root_dir = false;
            if(pos != std::string::npos){
                while(pos != std::string::npos){ //Algoritm for excluding dotdot in path...    ../dir1 /////Note, fortsätt här.
                    // d1/d2/d3
                    // ../../../ND
                    
                    temp_working_directory = temp_working_directory.substr(0,pos_slash); // d1/d2
                    //std::cout << "$1 DEBUG: WORKING DIR -> "  << temp_working_directory << std::endl;

                    dotdotpath = dotdotpath.substr(0,pos-3);
                    //std::cout << "1 DEBUG: dotdotpath -> "  << dotdotpath << std::endl;

                    pos1 = dotdotpath.find_last_of("/\\");
                    dotdotpath = dotdotpath.substr(0,pos1-1);
                    // std::cout << "1 DEBUG: dotdotpath -> "  << dotdotpath << std::endl;
                    dotdotpath = dotdotpath + "/" + last_dir;
                    // std::cout << "2 DEBUG: dotdotpath -> "  << dotdotpath << std::endl;
                    pos_slash = temp_working_directory.find_last_of("/\\");
                    pos = dotdotpath.find_last_of("..\\");
                    if(dotdotpath == ("../" + dirpath) || dotdotpath == ("/" + dirpath) ){
                        temp_working_directory = "/";
                        root_dir = true;
                        break;
                    }
                }
                if(!root_dir){
                    temp_working_directory = temp_working_directory.substr(2,temp_working_directory.length()-1);
                    test = temp_working_directory + "/" + dirpath;
                    return 0;

                }
                else{
                    test = temp_working_directory + dirpath;
                    dirpath = test;
                    return 0;
                }
            }
        return 0;    
        }
    }
    return 1;

}
int FS::dotdot_remover(std::string &dirpath){
    
    size_t pos = dirpath.find_last_of("..\\");
    std::string last_dir;
    std::string absolute_path;
    int status = get_dir_name(dirpath, last_dir, absolute_path);
    size_t pos1;
    if(pos != std::string::npos){
        while(pos != std::string::npos){ //Algoritm for excluding dotdot in path...  

            dirpath = dirpath.substr(0,pos-3);
            pos1 = dirpath.find_last_of("/\\");
            dirpath = dirpath.substr(0,pos1);
            dirpath = dirpath + "/" + last_dir;
            pos = dirpath.find_last_of("..\\");
            if(dirpath == ("../" + dirpath)){
                dirpath = "/" + last_dir;
                break;
            }
        }
    }
    return 0;
}

int FS::mkdir(std::string dirpath)
{

    std::cout << "FS::mkdir(" << dirpath << ")\n";
    int status = ReadFromFAT();
    dir_info dir;

    std::string test = dirpath;
    bool is_absolute_path = false;
    status = dotdot_remover(dirpath);
    if(status){
        std::cerr << "Error: mkdir " << dirpath << " went bad" << std::endl;
        return status;
    }
    status = path_handler(test, is_absolute_path, dirpath);
    if(status){
        std::cerr << "Error: mkdir " << dirpath << " went bad" << std::endl;
        return status;
    }
    //check if the desitnation file already exists
    dir_info cwd;

    if(dirpath == PARENT_DIR){
        std::cerr << "Error:" << PARENT_DIR << " is for the parent dir" << std::endl;
    }
    
    if(status){
        return status;
    }
    //status = FindingFileEntry(test,OLD,cwd,WRITE);
    // if(status){
    //     return status;
    // }
    
    if(is_absolute_path == true){

        status = FindingFileEntry(test,NEW,dir,WRITE);
    }
    else{
        status = FindingFileEntry(dirpath,NEW,dir,WRITE);
    }
    
    int temp_block = dir.block;

    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = ReadFromFAT();
    status = disk.read(temp_block, (uint8_t*)dir_entries); // change to the block we are at, no the current_blk
    if (status){
        return status;
    }
    
    int checker = -1;
    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){

        if (dir_entries[i].file_name[0] != 0){
            
                if((dir_entries[i].file_name == dirpath)){
                    checker = 1;
                    if((dir_entries[i].file_name == dirpath) && dir_entries[i].type == TYPE_DIR){
                        checker = -2;
                    }
                }
                
            }
    }
    
    if(checker == -2){
        std::cout << "Error: The file or directory already exist!" << std::endl;
        return 1;
    }
    if(status){
        return status;
    }
    int free_block = findFreeBlock();
    fat[free_block] = FAT_EOF;
    memcpy(dir.entries[dir.index].file_name, dirpath.c_str(), dirpath.length() + 1);
    dir.entries[dir.index].first_blk = free_block;
    dir.entries[dir.index].size = 0;
    dir.entries[dir.index].type = TYPE_DIR;
    dir.entries[dir.index].access_rights = READ | WRITE | EXECUTE;
    status = disk.write(dir.block, (uint8_t*)dir.entries);
    if(status){
        return status;
    }
    memset(dir.entries, 0, BLOCK_SIZE);
    dir.index = 0;
    memcpy(dir.entries[dir.index].file_name, PARENT_DIR.c_str(), PARENT_DIR.length() + 1);
    dir.entries[dir.index].first_blk = dir.block;
    dir.entries[dir.index].size = 0;
    dir.entries[dir.index].type = TYPE_DIR;
    dir.entries[dir.index].access_rights = READ | WRITE | EXECUTE;

    status = disk.write(free_block, (uint8_t*)dir.entries);
    if(status){
        return status;
    }
    status = writeToFAT();
    if(status){
        return status;
    }
    return 0;
}

int FS::cd(std::string dirpath) {
    if (dirpath == "/" || dirpath == PARENT_DIR) {
        goHome();
        return 0;
    }
    dir_info dir;
    int status = ReadFromFAT();
    if(status) return status;
    removeTrailingSlash(dirpath);

    status = FindingFileEntry(dirpath, OLD, dir, READ);
    if (status != 0) {
        std::cout << ("Error: '" + dirpath + "' is not a directory") << std::endl;
        return status;
    }

    if (dir.entries[dir.index].type != TYPE_DIR) {
        std::cout << ("Error: '" + dirpath + "' is not a directory") << std::endl;
        return -1;
    }
    if(dirpath.front() == '/'){
        dirpath = dirpath.substr(1,dirpath.length());
    }

    std::string cwd;
    if(working_directory != ".."){
        cwd = "/" + working_directory.substr(3,working_directory.length()) + "/" + dirpath;
        dir_info test_dir;

        status = FindingFileEntry(cwd,OLD,test_dir,READ);
        if(status){
            std::cout << ("Error: '" + cwd + "' is not a directory") << std::endl;
            return -1;
        }

    }
    
    working_directory = working_directory + "/" +dirpath;
    curr_blk = dir.entries[dir.index].first_blk;
    return 0;
}

void FS::goHome() {
    curr_blk = ROOT_BLOCK;
    working_directory = "..";
}

void FS::removeTrailingSlash(std::string& str) {
    if (!str.empty() && str.back() == '/') {
        str.pop_back();
    }
}

// hmh
//  pwd prints the full path, i.e., from the root directory, to the current
//  directory, including the currect directory name
int FS::pwd(){
    std::cout << "FS::pwd()\n";
    std::cout << working_directory << std::endl;

    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
        
    dir_info dir;
    int status = ReadFromFAT();
    if (status){
        return status;
    }
    status = FindingFileEntry(filepath, OLD, dir, 0);
    if(status){
        std::cout << "Error: That doesn't exist" << std::endl;

    }
    std::string pathname;
    std::string absolute_path;
    get_dir_name(filepath,pathname, absolute_path);
    if(pathname == PARENT_DIR){ 
        std::cout << "Error: You can't modify the parent directory of " << PARENT_DIR << std::endl;
        return 1;
    }
    uint8_t accessInt = std::stoi(accessrights);
    dir.entries[dir.index].access_rights = accessInt;
    status = disk.write(dir.block, (uint8_t*)dir.entries);
    if (status){
        return status;
    }


    return 0;

}
//----------------- OWN FUNCTIONS -----------------

int FS::writeToFAT(){
    //writes to FAT
    int status = disk.write(FAT_BLOCK, (uint8_t*)fat);
    return status;
}



