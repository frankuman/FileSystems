# FileSystems
Laboration 3, a project by Philippe Van Daele & Oliver Bölin during a university course at Blekinge Tekniska Högskola


**Properties of the system**
In this laboration we designed a file system based on a disk block size of 4096 bytes. It used organization as a file allocation table (FAT). By implementing FAT entries as 2 bytes each in the FAT we can address 4096 / 2 = 2048 disk blocks in on of our partitions. Therefore we can have FAT * 4 kB per block  8 MB per partition.

In the properties of this laboratory system, a file or directory entry in different disk blocks. If, a file is for example 9432 bytes, the system will store it in ceil(9432 / 4096) = 3 disk blocks, which will have 4096 + 4096 + 9432 %  4096 respectively. 

**Basic functionality and one-level directory**
In this part we formatted the disk by initiating the FAT and assigning all the blocks as free. We were also able to do simple tasks such as creating a file and then reading from the file. We also added the functionality of listing the content of the directory. 
Following functionalities were added in this section:
**format**
First we initialised both the root block and the FAT block as empty and then initialised the rest as FAT free, indicating that they are ready for use again. We then set the root block to all zeros so that we could reset the blocks to be used again. Then we write to the disk saying its empty. 

**create <filename>**
The create function starts off calling ReadFromFAT, it then checks with the function FindingFileEntry, which basically iterates through the filepath that the user inputs, and by using some smart algorithms it finds out if it's an absolute path or not. The function returns a struct with fileinformation. Depending if the input is for a new or old file, it either finds the file or the file entry for the file in question. After that it acquires an available FAT block which the content can be written at. It then takes the user input and places it on the block. If the content of the file is larger than the remaining block starts storing the rest of the content in a new block. Later it assigns the entries with useful information such as the first block of the content, total size, filetype and if it is read or write. If all is well it then writes the content to the disk.

**cat <filename>**
Cat function needs to read the content of a file and print it out to the user. To do this we first start by reading the FAT block (1) so we initialize the FAT. After that we need to find where the file is, if it exists. To do this we use a userdefined function called FindingFileEntry,. After that cat uses the struct to find what index the file has, gets the file and prints out its contents. Because lines can span over multiple lines the code here checks for block boundaries, and if it spans it checks the next block.

**ls**
The function ls was implemented by reading the FAT block (1), therefore it reads all the directory entries for the current directory or working directory, so that it gets an updated struct. When the struct is updated, it iterates over all the 64 possible dir entries in the struct or dir.
After that is just smart string writing and pretty printing to show the user what is apparent in the directory.

**cp <sourcefilename><destfilename>**
For the system to make an exact copy of one file to a new file, we have to first do a FindingFileEntry of the old file, and then do the same for the new entry. The programming hereafter is not so difficult to understand, but in simple terms we use the same method for the create to make the new file.

**mv <sourcefilename><destfilename>**
Move has to rename the sourcefile to the destinationfilename. But there is also another requirement. If the destfilename is a directory, it has to move the sourcefile into that directory. This proved a difficult challenge to implement. By implementing the requirement that if the destfilename is a directory, it has to start with a ‘/’ character, we could solve the issue. First we check for the slash character, and if its a folder we can simply find where it should be. After this we used the same method as rm and create to remove the old file and create the new file. If the destfilename is simply a new name for the sourcefile, we just find the old file and change its name in the struct before writing it to the disk.

**rm <filename>**
The function calls on the function FindingFileEntry to see if the content previously exists. When found, we then assign that block as empty and then write back to the disk, and telling the disk that this is empty and ready for usage.

**append <filename1><filename2>**
We first start by finding the first block of both paths, and find the latest fat from disk. We take the text from file1 and the text from file2 and add them together, which we can now do a quick calculation with. int block_amount = ceil((append_text.length()+1) / (float) BLOCK_SIZE)
This gives us the amount of blocks needed to append into the end of file2. We use some functions to get the next free blocks available, write to those blocks with the amount of blocks we had and then write it to the disk.
Hierarchical directories

**mkdir <dirname>**
First we check if the new directory doesn’t have the same name as the parent directory. We move on to call on the function FindingFileEntry, verifying that there doesn’t exist a directory with the same name. We then assign the block as a directory with assigned values, such as where it starts in the FAT, it’s size, etc. When all the required values are assigned we later write it to the disk.

**cd <dirname>**
Firstly a global variable with the current working directory had to be assigned, so that when the users goes into a new directory the working directory gets changed.
The function cd has to first check if the user wants to go back to the root block by checking the input. After that it just simply uses FindingFileEntry to find the directory and if it exists. After the input has been formatted correctly and checked for necessary substrings, it changes the current block to the directory block imputed and the global working directory string into the function inputted string.

**pwd**
Everytime we use the function cd, we assign the working directory with the directory path. I.e, working_directory += “/” + dirpath. 

This updates the variable holding the current working directory and is able to easily be called on by the function pwd.

**Access rights**
All functions got updates by using FindingFileEntry to check the file for the correct access rights. As an example, we want to Cat(examplefile), which we need read rights for. It is simply used in FindingFileEntry that reads right what we have to have for this file, when the function finds the file it checks what access rights the file has. By using switch cases we can then print out if the user is missing any access rights and return.

**chmod <accessrights><filepath>**
With chmod we update the files access rights by assigning it an integer value between 0-7, indicating the permissions. It first checks if the entry is a file or a directory. If a directory it will give an error message indicating that it cannot be changed. It then calls the function FindingFileEntry to acquire the right block to edit. When found we only update the access rights of that content and then write the updated value to the disk. 

