#include <iostream>
#include <cstring>
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
    //std::cout << "DEBUG: Trying to read... status: " << status << std::endl;
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

    //initialize blocks to all zeroes
    uint8_t root_block[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        root_block[i] = 0;
    }

    int status = disk.write(ROOT_BLOCK, root_block);
    if(!status){
        std::cout << "DEBUG: Disk formatted successfully!" << std::endl;
    }
    return status;
}

int FS::FindingFileEntry(std::string filepath, uint8_t newOrExisting, dir_info &dir, uint8_t access_rights){
    
    int status = GetDirectoryBlock(filepath, dir.block);
    if(status) return status;

    return FileEntry(dir.block, filepath, dir.index, dir.entries, newOrExisting);
}

int FS::GetDirectoryBlock(std::string filepath, int &dir_block){

    //std::cout << "Current filepath: " << filepath << std::endl;
    //std::cout << "Current dir_block: " << dir_block << std::endl; 

    dir_block = ROOT_BLOCK;


    return 0;
}

int FS::FileEntry(int dir_block, std::string filepath, int &index, dir_entry *dir_entries, uint8_t NewOrOld)
{

    int status = disk.read(dir_block, (uint8_t*)dir_entries);
    if (status){
        return status;
    }
    
    index = -1;
    if(NewOrOld == 1){
        for(int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
            if (dir_entries[i].file_name[0] == 0){ //file here
                if(index < 0){
                    index = i;
                }
            }
        }
    }
    else{
        for(int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
            if (strcmp(dir_entries[i].file_name, filepath.c_str()) == 0){ //Detta behöver skrivas om angående fulla filnamn
                index = i;
            }
        }
    }
    
    //std::cout << "DEBUG: Found Index for filename at " << index << std::endl;
    // 1. If (Check if a file name with that name already exists)
    // 2. Check access rights (IGNORE FOR NOW)
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

    std::string nameOfFile = filepath;
    dir_info dir;
    
    status = FileEntry(dir.block,filepath,dir.index,dir.entries, NEW);
    int current_block = findFreeBlock();

    if(current_block == -1){
        std::cout << "ERROR: No free blocks\n" << std::endl;
        return 1;
    }

    fat[current_block] = FAT_EOF;
    int first_block = current_block;
    char data[BLOCK_SIZE] = {int(0)}; //Data container  memset(data, 0, BLOCK_SIZE);


    std::string line = ".";
    uint32_t size = 0;      // Current block size counter
    uint32_t tot_size = 0;  // File size counter
    //To find the next block
    while(!line.empty()){
        std::getline(std::cin, line);
        if((line.length() + 1 + size) > BLOCK_SIZE){
            int length = BLOCK_SIZE - size;
            tot_size += length;
            memcpy(&data[size], line.c_str(), length);
            std::cout << "DEBUG: Placing " << line.substr(length) << " at block " << current_block << std::endl;

            status = disk.write(current_block, (uint8_t*)data);
            
            if(status){
                return 1;
            }
            memset(data,0,BLOCK_SIZE);
            int new_block = findFreeBlock();
                if (new_block == -1) {
                    std::cout << "ERROR: No free blocks\n";
                    return 1;
                }
            std::cout << "DEBUG: Old block: " << current_block << " New block: " << new_block << std::endl;

            fat[new_block] = FAT_EOF;
            fat[current_block] = new_block;
            current_block = new_block;
            size = 0;
            line = line.substr(length);
            // 0000000/0
        }
        else{
            std::cout << "DEBUG: Placing " << line << " at block " << current_block << std::endl;
            memcpy(&data[size], line.c_str(), line.length() + 1);
            size += line.length() + 1;
            tot_size += line.length();
            line.clear();
        }
    }
    tot_size++;
    status = disk.write(current_block, (uint8_t*)data);
    if(status){
        return status;
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
    int status = ReadFromFAT();
    if(status){
        return status;
    }

    dir_info dir;
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = FileEntry(dir.block,filepath,dir.index,dir.entries,OLD);
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
        std::cout << &data[size] << "\n";
        size += length;
        tot_size += length;
        size++;
        tot_size++;
        std::cout << "\n";
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
        
    std::cout << "Name \t size" << std::endl; 
    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
        if (dir_entries[i].file_name[0] != 0){
            std::string blocks = std::to_string(curr_blk);
            
            int curr_blk = dir_entries[i].first_blk;
            std::cout << dir_entries[i].file_name << "\t " << dir_entries[i].size << std::endl;
        }
    }

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath){
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";

    int status = ReadFromFAT();
    if (status){
        return status;
    }

    //check if the desitnation file already exists
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = disk.read(curr_blk, (uint8_t*)dir_entries);
    if (status){
        return status;
    }

    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
    if (dir_entries[i].file_name[0] != 0){
        std::string blocks = std::to_string(curr_blk);
        
            if(dir_entries[i].file_name == destpath){
                std::cout << "Error: The destination file already exists!" << std::endl;
                return 0;
            }
        }
    }

    dir_info source;
    dir_info destination;

    status = FindingFileEntry(sourcepath, OLD, source, READ);
    if(status) return status;
    status = FindingFileEntry(destpath, NEW, destination, WRITE);
    if(status) return status;

    char data[BLOCK_SIZE];
    memset(data, 0, BLOCK_SIZE);

    int first_block = -1;
    int previous_block = -1;
    int source_block = source.entries[source.index].first_blk;
    while(source_block != FAT_EOF){

        //finding FAT free entry
        int current_block = findFreeBlock();
        if(current_block < 0){
            std::cout << "Error: There isn't anymore free blocks in FAT" << std::endl;
            return 1;
        }

        if(first_block == -1){
            first_block = current_block;
        }
        if(previous_block != -1){
            fat[previous_block] = current_block;
        }
        fat[current_block] = FAT_EOF;

        status = disk.read(source_block, (uint8_t*)data);
        if(status) return status;
        status = disk.write(current_block, (uint8_t*)data);
        if(status) return status;

        previous_block = current_block;
        source_block = fat[source_block];
    }

    std::cout << "Destination index: " << destination.index << std::endl;

    //copying the file entry info from source to destination
    memcpy(&destination.entries[destination.index], &source.entries[source.index], sizeof(dir_entry));
    
    //set the new filename and the new first-block
    memcpy(destination.entries[destination.index].file_name, destpath.c_str(), destpath.length() + 1);
    destination.entries[destination.index].first_blk = first_block;

    status = disk.write(destination.block, (uint8_t*)destination.entries);
    if(status) return status;

    status = writeToFAT();
    if(status) return status;

    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";

    dir_info source;
    dir_info destination;
    
    int status = FindingFileEntry(sourcepath, OLD, source, READ);
    if(status) return status;
    status = FindingFileEntry(destpath, NEW, destination, WRITE);
    if(status) return status;

    //check if the desitnation file already exists
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = disk.read(curr_blk, (uint8_t*)dir_entries);
    if (status){
        return status;
    }

    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
        if (dir_entries[i].file_name[0] != 0){
            std::string blocks = std::to_string(curr_blk);

            if(dir_entries[i].file_name == destpath){
                std::cout << "Error: The destination file already exists!" << std::endl;
                return 0;
            }
        }
    }

    //since we are only working at root level, I don't need to handle with directories (Yet) But I would add the check here
    // --> HERE

    if(destination.block == source.block){ // The source file exists on the same level as the destination file
        memcpy(source.entries[source.index].file_name, destpath.c_str(), destpath.length() + 1);

        status = disk.write(source.block, (uint8_t*) source.entries);
        if(status){
            return status;
        }
    }

    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath){
    std::cout << "FS::rm(" << filepath << ")\n";
    
    int status = ReadFromFAT();
    if(status) return status;
    
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
    status = FileEntry(dir.block,filepath,dir.index,dir.entries,OLD);
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

int FS::create_with_string(std::string filepath,std::string text){

    return 0;
}
// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2){
    int status = ReadFromFAT();

    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    std::string filepath1_text = ".";
    std::string filepath2_text = ".";
    get_file_string(filepath1,filepath1_text);
    get_file_string(filepath2,filepath2_text);
    std::cout << "filepath1_text: " << filepath1_text << std::endl; 
    std::cout << "filepath2_text: " << filepath2_text << std::endl; 
    
    
    std::string full_string = filepath2_text.substr(1,filepath2_text.length()-1) + filepath1_text.substr(1,filepath1_text.length()-1);
    std::cout << "append text: " << full_string << std::endl; 

    rm(filepath2);

    //------------------------------copied from create()------------------------------
    
    status = ReadFromFAT();
    if(status){
        return status;
    }
    std::string line = full_string;
    std::string filepath = filepath2;
    std::string nameOfFile = filepath;
    dir_info dir;
    
    status = FileEntry(dir.block,filepath,dir.index,dir.entries, NEW);
    int current_block = findFreeBlock();
    std::cout << "current_block: " << current_block << std::endl; 

    if(current_block == -1){
        std::cout << "ERROR: No free blocks\n" << std::endl;
        return 1;
    }

    fat[current_block] = FAT_EOF;
    int first_block = current_block;
    char data[BLOCK_SIZE] = {int(0)}; //Data container  memset(data, 0, BLOCK_SIZE);


    uint32_t size = 0;      // Current block size counter
    uint32_t tot_size = 0;  // File size counter
    //To find the next block
    while(!line.empty()){
        if((line.length() + 1 + size) > BLOCK_SIZE){
            int length = BLOCK_SIZE - size;
            tot_size += length;
            memcpy(&data[size], line.c_str(), length);
            std::cout << "DEBUG: Placing " << line.substr(length) << " at block " << current_block << std::endl;

            status = disk.write(current_block, (uint8_t*)data);
            
            if(status){
                return 1;
            }
            memset(data,0,BLOCK_SIZE);
            int new_block = findFreeBlock();
                if (new_block == -1) {
                    std::cout << "ERROR: No free blocks\n";
                    return 1;
                }
            std::cout << "DEBUG: Old block: " << current_block << " New block: " << new_block << std::endl;

            fat[new_block] = FAT_EOF;
            fat[current_block] = new_block;
            current_block = new_block;
            size = 0;
            line = line.substr(length);
            // 0000000/0
        }
        else{
            std::cout << "DEBUG: Placing " << line << " at block " << current_block << std::endl;
            memcpy(&data[size], line.c_str(), line.length() + 1);
            size += line.length() + 1;
            tot_size += line.length();
            line.clear();
        }
    }
    tot_size++;
    status = disk.write(current_block, (uint8_t*)data);
    if(status){
        return status;
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

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int FS::pwd()
{
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}


//----------------- OWN FUNCTIONS -----------------

int FS::writeToFAT(){
    //writes to FAT
    int status = disk.write(FAT_BLOCK, (uint8_t*)fat);
    return status;
}
