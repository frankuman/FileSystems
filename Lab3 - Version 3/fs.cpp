#include <iostream>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    boot();
}

FS::~FS()
{
    delete[] catalogs;
}

void FS::bootHelper(int readBlock)
{
    disk.read(readBlock, (uint8_t *)&catalogs[nrOfDirs++]);

    // nrOfDirs added when reading the disk, remember -1 during in loop/recursion call to avoid out of range
    for (int i = 2; i < catalogs[nrOfDirs - 1].nrOfSubDir; ++i)
    {
        if (catalogs[nrOfDirs - 1].dirs[i].type == TYPE_DIR)
        {
            bootHelper(catalogs[nrOfDirs - 1].dirs[i].first_blk);
        }
    }
}

void FS::boot()
{
    catalogs = new dir_helper[BLOCK_SIZE / 2];
    nrOfDirs = 0;

    // First read the FAT table from disk
    disk.read(FAT_BLOCK, (uint8_t *)fat);


    // Second check if root is -1, this means we have already formatted the disk once, boot as normal.
    if (fat[ROOT_BLOCK] == EOF)
    {
        disk.read(ROOT_BLOCK, (uint8_t *)&catalogs[ROOT_DIR]);
        workingDir = &catalogs[ROOT_DIR];
        nrOfDirs++;
        workingDirAsString = "~";

        for (int i = 2; i < catalogs[ROOT_DIR].nrOfSubDir; ++i)
        {
            // Recursive call to load in sub-folders from disk
            if (catalogs[ROOT_DIR].dirs[i].type == TYPE_DIR)
            {
                bootHelper(catalogs[ROOT_DIR].dirs[i].first_blk);
            }
        }
    }
    // Else we will format
    else
    {
        std::cout << "First time booting, formatting drive..." << std::endl;
        format();
    }
}

void FS::saveWorkingDir()
{
    workingDirPrevious = workingDir;
    stringWorkingDirPrevious = workingDirAsString;
}
void FS::resetWorkingDir()
{
    workingDir = workingDirPrevious;
    workingDirAsString = stringWorkingDirPrevious;
}

void FS::updateFat()
{
    disk.write(FAT_BLOCK, (uint8_t *)&fat);
}

int FS::dirExists(std::string dirName) const
{
    bool found = false;
    int index = -1;

    for (int i = 2; i < workingDir->nrOfSubDir && !found; ++i)
    {
        //Check if the names match and the type is a directory
        if (dirName == workingDir->dirs[i].file_name && workingDir->dirs[i].type == TYPE_DIR)
        {
            index = i;
            found = true;
        }
    }
    return index;
}

int FS::fileExists(std::string filename) const
{
    bool found = false;
    int index = -1;

    for (int i = 2; i < workingDir->nrOfSubDir && !found; ++i)
    {
        //Check if the names match and the type is a file
        if (filename == workingDir->dirs[i].file_name && workingDir->dirs[i].type == TYPE_FILE)
        {
            index = i;
            found = true;
        }
    }
    return index;
}

void FS::appendFile(int blocksToWrite, std::vector<std::string> &data, int appendIndex)
{
    int i = ROOT_BLOCK + 2;
    bool inserted = false;
    int previous = -1;

    for (int j = 0; j < data.size(); ++j)
    {
        if (j > 0)
        {
            for (i; i < (BLOCK_SIZE / 2) && !inserted; ++i)
            {
                if (fat[i] == FAT_FREE)
                {
                    if (fat[appendIndex] == EOF)
                    {
                        fat[appendIndex] = i;
                    }
                    if (previous != -1)
                    {
                        fat[previous] = i;
                    }
                    inserted = true;
                    fat[i] = EOF;
                    previous = i;
                    i--;
                }
            }
            disk.write(i, (uint8_t *)data[j].c_str());
            inserted = false;
        }
        else
        {
            disk.write(appendIndex, (uint8_t *)data[j].c_str());
        }
    }
}

void FS::writeFile(int blocksToWrite, std::vector<std::string> &data)
{
    int i = ROOT_BLOCK + 2;
    bool inserted = false;
    int previous = -1;

    for (int j = 0; j < blocksToWrite; ++j)
    {
        for (i; i < (BLOCK_SIZE / 2) && !inserted; ++i)
        {
            if (fat[i] == FAT_FREE)
            {
                if (workingDir->dirs[workingDir->nrOfSubDir].first_blk == FAT_FREE)
                {
                    workingDir->dirs[workingDir->nrOfSubDir].first_blk = i;
                }
                if (previous != -1)
                {
                    fat[previous] = i;
                }
                inserted = true;
                fat[i] = EOF;
                previous = i;
                i--;
            }
        }
        disk.write(i, (uint8_t *)data.at(j).c_str());
        inserted = false;
    }
}

std::string FS::getNameOfPath(std::string pathName)
{
    std::string paths;

    for (int i = 0; i < pathName.size(); ++i)
    {
        if (pathName[i] != '/')
        {
            paths += pathName[i];
        }
        else
        {
            paths.clear();
        }
    }

    return paths;
}

bool FS::goToPath(std::string newPath)
{
    std::string paths;
    bool found = false;
    bool validPath = true;

    for (int i = 0; i < newPath.size() && validPath; ++i)
    {
        if (newPath[i] != '/') //if the current char is / we know we have reached the end of this directory name
        {
            paths += newPath[i];
        }
        else
        {
            if (paths == ".") // If we have . we go to itself
            {
                paths.clear();
                found = true;
            }
            else if ((paths == "~") || (paths.empty() && i == 0)) //If we start / or have a ~ as the name, we need to go to root instead
            {
                workingDir = &catalogs[ROOT_DIR];
                workingDirAsString = "~";
                paths.clear();
                found = true;
            }
            else if (paths == "..") // If the user typed ".." that is translated to the parent name
            {
                paths = workingDir->dirs[PARENT].file_name;
            }
            for (int j = 1; j < workingDir->nrOfSubDir && !found; ++j) // loop through all sub-directories including parent
            {
                if (paths == workingDir->dirs[j].file_name)
                {
                    //If the workingdir has a subdir with the correct name
                    //find it in the array of all directories
                    for (int k = 0; k < nrOfDirs && !found; ++k)
                    {
                        if (paths == catalogs[k].dirs[ITSELF].file_name)
                        {
                            //When the directory is found, we need to increase the workingDirAsString with a '/'
                            //and the new name, as long as it isn't the parent
                            if (paths != workingDir->dirs[PARENT].file_name)
                            {
                                workingDirAsString.append("/" + paths);
                            }
                            //If the name is the parent and the name isn't 1 character long, we need to erase the current name plus the previous /
                            else if (workingDirAsString.size() > 1)
                            {
                                workingDirAsString.erase(workingDirAsString.size() - strlen(workingDir->dirs[ITSELF].file_name) - 1,
                                                        strlen(workingDir->dirs[ITSELF].file_name) + 1);
                            }
                            else
                            {
                                //This is an edge case if we are attempting to go back to root (as long as we aren't on root already)
                                if (k > 0)
                                {
                                    workingDirAsString.erase(workingDirAsString.size() - 1, 2);
                                }
                            }

                            workingDir = &catalogs[k];
                            found = true;
                            paths.clear();
                        }
                    }
                }
            }
            if (found == false)
            {
                std::cout << newPath << ": Invalid path!" << std::endl;
                validPath = false;
            }
        }
        found = false;
    }
    return validPath;
}

// formats the disk, i.e., creates an empty file system starting on an offset of 2
// disregarding ROOT_BLOCK and FAT_BLOCK
int FS::format()
{
    std::cout << "FS::format()\n";
    for (int i = ROOT_BLOCK + 2; i < (BLOCK_SIZE / 2); ++i)
    {
        fat[i] = FAT_FREE;
    }
    fat[FAT_BLOCK] = EOF;

    //We don't actually need to clear the disk. If we mark fat as free we can overwrite it later
    catalogs = new dir_helper[BLOCK_SIZE / 2];
    nrOfDirs = 0;

    //Recreate the root directory and write it to the disk
    strcpy(catalogs[nrOfDirs].dirs[ITSELF].file_name, "~");
    catalogs[ROOT_DIR].dirs[ITSELF].first_blk = ROOT_BLOCK;
    catalogs[ROOT_DIR].dirs[ITSELF].size = BLOCK_SIZE;
    catalogs[ROOT_DIR].dirs[ITSELF].type = TYPE_DIR;
    catalogs[ROOT_DIR].dirs[ITSELF].access_rights = READ | WRITE | EXECUTE;
    catalogs[ROOT_DIR].dirs[PARENT] = catalogs[nrOfDirs].dirs[ITSELF];
    fat[ROOT_BLOCK] = EOF;

    disk.write(ROOT_BLOCK, (uint8_t *)&catalogs[ROOT_BLOCK]);

    workingDir = &catalogs[nrOfDirs];
    workingDirAsString = "~";
    nrOfDirs++;
    updateFat();
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";

    std::string data, diskData;
    bool done = false;
    saveWorkingDir();
    if ((goToPath(filepath)) == true)
    {
        std::string fileName = getNameOfPath(filepath);
        int index = fileExists(fileName);
        if (workingDir->nrOfSubDir < MAX_DIRECTORIES && fileName.size() < MAX_NAME_SIZE)
        {

            // User shouldn't be able to create a file with same name as root anywhere in the system
            if (fileName == "~" || fileName.empty())
            {
                std::cout << fileName << ": Reserved name, aborting!" << std::endl;
            }
            else if (index > -1)
            {
                std::cout << fileName << ": Already exists!" << std::endl;
            }
            else
            {
                while (!done)
                {
                    std::getline(std::cin, data);
                    if (data.empty())
                    {
                        done = true;
                    }
                    else
                    {
                        data += '\n';
                        diskData.append(data);
                    }
                }

                if (diskData.size() == 0)
                {
                    std::cout << "No data entered." << std::endl;
                }
                else
                {

                    uint16_t noBlocks = (diskData.length() / BLOCK_SIZE) + 1;
                    //Create a new struct inside the current working dir sub-directories
                    dir_entry *newEntry = new dir_entry;
                    strcpy(newEntry->file_name, fileName.c_str());
                    newEntry->type = TYPE_FILE;
                    newEntry->first_blk = FAT_FREE;
                    newEntry->size = diskData.length();
                    newEntry->access_rights = workingDir->dirs[ITSELF].access_rights;

                    std::vector<std::string> dataVec;
                    uint16_t bytesLeft = diskData.size() % BLOCK_SIZE;
                    //Split up the data input from the user into substrings. Each index in the vector
                    //Corresponds to a block(which means their size is BLOCK_SIZE)
                    for (int k = 0; k < noBlocks; ++k)
                    {
                        if (k == noBlocks - 1 && bytesLeft != 0) // The last block is not necesserily BLOCK_SIZE large, so a special case is needed
                        {
                            dataVec.push_back(diskData.substr((k * BLOCK_SIZE), bytesLeft));
                        }
                        else
                        {
                            dataVec.push_back(diskData.substr((k * BLOCK_SIZE), BLOCK_SIZE));
                        }
                    }

                    workingDir->dirs[workingDir->nrOfSubDir] = *newEntry;
                    writeFile(noBlocks, dataVec);
                    workingDir->nrOfSubDir++;

                    updateFat();
                    updateFolder(*workingDir);
                }
            }
        }
        else
        {
            std::cout << "Permission denied: Limit reached\n";
        }
    }
    resetWorkingDir();
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";
    saveWorkingDir();
    std::string fileName = getNameOfPath(filepath);
    if (goToPath(filepath) == true)
    {
        int index = fileExists(fileName);
        if (index != EOF)
        {
            if ((decodeAccessRights(workingDir->dirs[index].access_rights).at(0)) == 'r')
            {
                std::vector<std::string> vec = readFile(workingDir->dirs[index].first_blk);
                for (int i = 0; i < vec.size(); i++)
                {
                    std::cout << vec[i];
                }
            }
            else
            {
                std::cout << "Permission denied: You do not have permission to read this file." << std::endl;
            }
        }
        else
        {
            std::cout << fileName << ": No such file!" << std::endl;
        }
    }
    resetWorkingDir();
    return 0;
}

std::string FS::decodeAccessRights(uint8_t accessRights)
{
    std::string decoded;

    //Since accessrights is a uint8_t between 1 and 7, we can simply check each possible scenario with a
    //switch case and change the 'decoded'
    switch (accessRights)
    {
    case 1:

        decoded = "--x";
        break;

    case 2:

        decoded = "-w-";
        break;

    case 3:

        decoded = "-wx";
        break;

    case 4:

        decoded = "r--";
        break;

    case 5:

        decoded = "r-x";
        break;

    case 6:

        decoded = "rw-";
        break;

    case 7:

        decoded = "rwx";
        break;

    default:

        decoded = "---";
        break;
    }

    return decoded;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls()
{
    std::cout << "FS::ls()\n";

    std::cout << "Filename:\t\tType:\t\tAccess-rights:\tSize:\tStart:\t" << std::endl;

    for (int i = 0; i < workingDir->nrOfSubDir; ++i)
    {
        //The first 2 ifs are special cases so it prints out "." and ".."
        //indicating itself and parent respectively
        if (i == ITSELF)
        {
            std::cout << "."
                    << "\t\t\t";
        }
        else if (i == PARENT)
        {
            std::cout << ".."
                    << "\t\t\t";
        }
        else
        {
            //One /t is about 8 spaces so we calculate how many spaces we need to enter
            int no_Spaces = TAB_SIZE * 3 - strlen(workingDir->dirs[i].file_name);
            //If no_Spaces is larger than 0, it means that we have to manually enter spaces
            if (no_Spaces > 0)
            {
                std::cout << workingDir->dirs[i].file_name;
                for (int j = 0; j < no_Spaces; j++)
                {
                    std::cout << " ";
                }
            }
            //If no_Spaces is less than 0, we need to shrink the printed name and we add (..) to indicate that the name is larger
            else
            {
                for (int j = 0; j < (TAB_SIZE * 3) - 4; j++)
                {
                    std::cout << workingDir->dirs[i].file_name[j];
                }
                std::cout << "(..)";
            }
        }

        //This if-else statement translates the type to directory or file for printing
        if (workingDir->dirs[i].type == TYPE_FILE)
        {
            std::cout << "File\t\t";
        }
        else
        {
            std::cout << "Directory\t";
        }
        std::cout << decodeAccessRights(workingDir->dirs[i].access_rights) << "\t\t" << workingDir->dirs[i].size << "\t"
                  << workingDir->dirs[i].first_blk << std::endl;
    }
    return 0;
}
//Helper function to read a file
std::vector<std::string> FS::readFile(int startBlock)
{
    int iterator = startBlock;
    char *buffer;
    std::vector<std::string> data;
    //Simply keep reading until we reach the end of the datablocks
    //Each iteration we read the current block and then navigate towards the next block in the fat-table
    //Pushing to a final vector each time
    while (iterator != EOF)
    {
        buffer = new char[BLOCK_SIZE];
        disk.read(iterator, (uint8_t *)buffer);
        iterator = fat[iterator];
        data.push_back(buffer);
        delete[] buffer;
    }
    return data;
}

// cp <sourcefilepath> <destfilepath> makes an exact copy of the file
// <sourcefilepath> to a new file <destfilepath>
int FS::cp(std::string sourcefilepath, std::string destfilepath)
{
    std::cout << "FS::cp(" << sourcefilepath << "," << destfilepath << ")\n";
    saveWorkingDir();

    std::string fileName = getNameOfPath(sourcefilepath);
    if (goToPath(sourcefilepath) == true)
    {
        int index = fileExists(fileName);
        if (index == -1)
        {
            std::cout << fileName << ": No such file!" << std::endl;
        }
        if (index > -1)
        {
            //If we have access, we need to create a new identical copy
            if (decodeAccessRights(workingDir->dirs[index].access_rights).at(0) == 'r')
            {

                dir_entry copyEntry;
                strcpy(copyEntry.file_name, fileName.c_str());
                copyEntry.access_rights = workingDir->dirs[index].access_rights;
                copyEntry.size = workingDir->dirs[index].size;
                copyEntry.type = workingDir->dirs[index].type;
                copyEntry.first_blk = FAT_FREE;

                int noBlocks = (workingDir->dirs[index].size / BLOCK_SIZE) + 1;
                std::vector<std::string> data = readFile(workingDir->dirs[index].first_blk);

                resetWorkingDir();

                if (destfilepath[destfilepath.size() - 1] != '/')
                {
                    destfilepath.append("/");
                }
                //Now that we have a struct to insert, we navigate to the destination
                if (goToPath(destfilepath) == true)
                {
                    index = fileExists(fileName);
                    if (index == -1)
                    {
                        workingDir->dirs[workingDir->nrOfSubDir] = copyEntry;

                        writeFile(noBlocks, data);
                        workingDir->nrOfSubDir++;
                        updateFat();
                        updateFolder(*workingDir);
                    }
                    else
                    {
                        std::cout << "Permission denied: " << fileName << " already exist!" << std::endl;
                    }
                }
            }
            else
            {
                std::cout << "Permission denied: "
                          << "You do not have permission to copy this file." << std::endl;
            }
        }
        else if (index == -1)
        {
            std::cout << "Permission denied: " << fileName << " doesn't exist!" << std::endl;
        }
    }
    resetWorkingDir();
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    saveWorkingDir();
    std::string fileName = getNameOfPath(sourcepath);
    if (goToPath(sourcepath) == true)
    {
        std::string sourcePathString = workingDirAsString;
        dir_helper *tempDir2 = workingDir;
        int index = fileExists(fileName);

        if (index == -1)
        {
            std::cout << fileName << ": No such file!" << std::endl;
        }
        else
        {
            dir_entry temp = workingDir->dirs[index];
            resetWorkingDir();
            fileName = getNameOfPath(destpath);
            if (destpath == "/")
            {
                destpath.clear();
            }
            if (goToPath(destpath + '/') == true)
            {
                fileName = getNameOfPath(destpath);
                workingDir->dirs[workingDir->nrOfSubDir++] = tempDir2->dirs[index];
                for (int i = index; i < tempDir2->nrOfSubDir; i++)
                {
                    tempDir2->dirs[i] = tempDir2->dirs[i + 1];
                }
                tempDir2->nrOfSubDir--;
            }
            else
            {
                if (workingDirAsString == sourcePathString)
                {
                    if (fileName.empty() || fileName == "~")
                    {
                        std::cout << "Permission denied: Rename error!" << std::endl;
                    }
                    else
                    {
                        std::cout << "Same directory, renaming...\n";
                        strcpy(workingDir->dirs[index].file_name, fileName.c_str());
                    }
                }
            }
            updateFat();
            updateFolder(*workingDir);
            updateFolder(*tempDir2);
        }
    }
    resetWorkingDir();
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    saveWorkingDir();
    std::string fileName = getNameOfPath(filepath);
    if (goToPath(filepath) == true)
    {
        int index = fileExists(fileName);

        if (index == -1)
        {
            std::cout << fileName << ": No such file!" << std::endl;
        }
        else
        {
            int iterator = workingDir->dirs[index].first_blk;
            int previous = workingDir->dirs[index].first_blk;
            while (iterator != EOF)
            {
                iterator = fat[iterator];
                fat[previous] = FAT_FREE;
                previous = iterator;
            }

            fat[iterator] = FAT_FREE;
            //we need to move everything in the array, starting at the index of the removed file
            for (int i = index; i < workingDir->nrOfSubDir; ++i)
            {
                workingDir->dirs[i] = workingDir->dirs[i + 1];
            }

            workingDir->nrOfSubDir--;
            updateFat();
            updateFolder(*workingDir);
        }
    }
    resetWorkingDir();
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    saveWorkingDir();

    std::string fileName = getNameOfPath(filepath2);
    if (goToPath(filepath2) == true)
    {
        int index2 = fileExists(fileName);
        if (decodeAccessRights(workingDir->dirs[index2].access_rights).at(1) == 'w')
        {
            resetWorkingDir();

            fileName = getNameOfPath(filepath1);
            if (goToPath(filepath1) == true)
            {
                int index1 = fileExists(fileName);
                int appendIndex = -1;
                if (decodeAccessRights(workingDir->dirs[index1].access_rights).at(0) == 'r')
                {
                    if (index1 != NOT_FOUND && index2 != NOT_FOUND)
                    {
                        std::string newData;
                        std::string tempData;
                        std::vector<std::string> file = readFile(workingDir->dirs[index1].first_blk);
                        int appendSize = workingDir->dirs[index1].size;
                        for (int i = 0; i < file.size(); ++i)
                        {
                            tempData.append(file[i]);
                        }
                        resetWorkingDir();
                        if (goToPath(filepath2) == true)
                        {
                            fileName = getNameOfPath(filepath2);
                            file.clear();
                            file = readFile(workingDir->dirs[index2].first_blk);

                            newData.append(file[file.size() - 1]);
                            newData.append(tempData);

                            int iterator2 = workingDir->dirs[index2].first_blk;

                            while (iterator2 != EOF)
                            {
                                //Only increment appendIndex if we're not at EOF, so that we can start append other files FAT-indexes.
                                if (iterator2 != EOF)
                                {
                                    appendIndex = iterator2;
                                }
                                iterator2 = fat[iterator2];
                            }

                            uint32_t noBlocks = (newData.length() / BLOCK_SIZE) + 1;
                            uint32_t lastBlock = (newData.length() % BLOCK_SIZE);

                            file.clear();

                            for (int i = 0; i < noBlocks; ++i)
                            {
                                if (i == noBlocks - 1)
                                {
                                    file.push_back(newData.substr((i * BLOCK_SIZE), lastBlock));
                                }
                                else
                                {
                                    file.push_back(newData.substr((i * BLOCK_SIZE), BLOCK_SIZE));
                                }
                            }
                            workingDir->dirs[index2].size += appendSize;
                            appendFile(noBlocks, file, appendIndex);
                            updateFat();
                            updateFolder(*workingDir);
                        }
                    }
                    else
                    {
                        //If one or both of the files don't exist, we print out different errors to indicate the problem
                        if (index1 == NOT_FOUND && index2 == NOT_FOUND)
                        {
                            std::cout << "'" << filepath1 << "' and '" << filepath2 << "' does not exist!" << std::endl;
                        }
                        else if (index1 == NOT_FOUND && index2 != NOT_FOUND)
                        {
                            std::cout << "'" << filepath1 << "' does not exist!" << std::endl;
                        }
                        else if (index1 != NOT_FOUND && index2 == NOT_FOUND)
                        {
                            std::cout << "'" << filepath2 << "' does not exist!" << std::endl;
                        }
                    }
                }
                else
                {
                    std::cout << "Permission denied: " << filepath1 << " do not have read access!" << std::endl;
                }
            }
        }
        else
        {
            std::cout << "Permission denied: "
                      << "'" << filepath2 << "' do not have write access!" << std::endl;
        }
    }
    resetWorkingDir();
    return 0;
}

void FS::writeDir(dir_helper &catalog)
{
    bool inserted = false;
    for (int i = ROOT_BLOCK + 2; i < (BLOCK_SIZE / 2) && !inserted; ++i)
    {
        //We simply find a free block and write the struct to it
        if (fat[i] == FAT_FREE)
        {
            catalog.dirs[ITSELF].first_blk = i;
            disk.write(i, (uint8_t *)&catalog);
            inserted = true;
            fat[i] = EOF;
        }
    }
}

void FS::updateFolder(dir_helper &catalog)
{
    disk.write(catalog.dirs[ITSELF].first_blk, (uint8_t *)&catalog);
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    saveWorkingDir();

    std::string dirName = getNameOfPath(dirpath);

    if (goToPath(dirpath) == true)
    {
        bool found = false;
        int index = dirExists(dirName);

        if (dirName == "~" || dirName.empty())
        {
            std::cout << dirName << ": Reserved name, aborting!" << std::endl;
        }
        else if (index > -1)
        {
            std::cout << dirName << ": Directory already exists!" << std::endl;
        }
        else
        {
            if (workingDir->nrOfSubDir < MAX_DIRECTORIES && dirName.size() < MAX_NAME_SIZE)
            {
                // Creating new meta-data for directory.
                dir_helper newDir;
                strcpy(newDir.dirs[ITSELF].file_name, dirName.c_str());
                newDir.dirs[ITSELF].size = BLOCK_SIZE;
                newDir.dirs[ITSELF].access_rights = workingDir->dirs[ITSELF].access_rights;
                newDir.dirs[ITSELF].type = TYPE_DIR;
                newDir.dirs[PARENT] = workingDir->dirs[ITSELF];

                writeDir(newDir);

                // Working directory receives a reference to the new sub-directory
                workingDir->dirs[workingDir->nrOfSubDir] = newDir.dirs[ITSELF];
                catalogs[nrOfDirs++] = newDir;
                workingDir->nrOfSubDir++;
                updateFolder(*workingDir);
                updateFat();
            }
            else
            {
                std::cout << "Permission denied: Limit reached\n";
            }
        }
    }
    resetWorkingDir();
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    saveWorkingDir();

    if (dirpath[dirpath.size() - 1] != '/')
    {
        dirpath.append("/");
    }
    if (goToPath(dirpath) == false)
    {
        //If we couldn't go to the path, reset the working
        resetWorkingDir();
    }

    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int FS::pwd()
{
    std::cout << "FS::pwd()\n";
    //Since workingDirAsString is a variable for the entire class that changes during run-time, we can simply just print it
    std::cout << workingDirAsString << std::endl;
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int FS::chmod(std::string accessrights, std::string filepath){
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    saveWorkingDir();

    if (goToPath(filepath) == true)
    {
        std::string fileName = getNameOfPath(filepath);

        int index = fileExists(fileName);

        if (index != NOT_FOUND)
        {
            //Check if the character is a digit
            if (isdigit(accessrights[0]) != 0)
            {
                int digit = atoi(&accessrights[0]);
                if (digit > -1 && digit < 8)
                {
                    //If the character is a digit, perfom XOR operation to change the relevant rights
                    workingDir->dirs[index].access_rights = workingDir->dirs[index].access_rights ^ digit;
                    updateFolder(*workingDir);
                }
                else
                {
                    std::cout << "Permission denied: Invalid input!" << std::endl;
                }
            }
            else
            {
                std::cout << "Usage: chmod <number> <file>" << std::endl;
            }
        }
        else
        {
            std::cout << filepath << ": No such file or directory!" << std::endl;
        }
    }
    resetWorkingDir();
    return 0;
}