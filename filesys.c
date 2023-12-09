//Eli Bendavid

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

//For -r, -w, -rw
#define MODE_READ 1
#define MODE_WRITE 2
#define MODE_READ_WRITE 3
#define MODE_INVALID -1

//Source Used - FAT32 Spec File
typedef struct __attribute__((packed)){
        //BPB structure common to FAT12, FAT16, and FAT32 implementations 
        unsigned char BS_jmpBoot[3];
        unsigned char BS_OEMName[8];
        unsigned short BPB_BytsPerSec;
        unsigned char BPB_SecPerClus;
        unsigned short BPB_RsvdSecCnt;
        unsigned char BPB_NumberofFATS;
        unsigned short BPB_RootEntCnt;
        unsigned short BPB_TotSec16;
        unsigned char BPB_Media;
        unsigned short BPB_FATSz16;
        unsigned short BPB_SecPerTrk;
        unsigned short BPB_NumHeads;
        unsigned int BPB_HiddSec;
        unsigned int BPB_TotSec32;

        //Extended BPB structure for FAT32 volumes
        unsigned int BPB_FATSz32;
        unsigned short BPB_ExtFlags;
        unsigned short BPB_FSVer;
        unsigned int BPB_RootClus;
        unsigned short BPB_FSInfo;
        unsigned short BPB_BkBootSec;
        unsigned char BPB_Reserved[12];
        unsigned char BPB_DrvNum;
        unsigned char BS_Reserved1;
        unsigned char BS_BootSig;
        unsigned int BS_VolID;
        unsigned char BS_VolLab[11];
        unsigned char BS_FileSysType[8];
} BPB_Block;

//Source Used - Microsoft FAT Specification Section 6: Directory Structure
typedef struct __attribute__((packed)){
        unsigned char DIR_Name[11];
        unsigned char DIR_Attr;
        unsigned char DIR_NTRes;
        unsigned char DIR_CrtTimeTenth;
        unsigned short DIR_CrtTime;
        unsigned short DIR_CrtDate;
        unsigned short DIR_LstAccDate;
        unsigned short DIR_FstClusHi;
        unsigned short DIR_WrtTime;
        unsigned short DIR_WrtDate;
        unsigned short DIR_FstClusLo;
        unsigned int DIR_FileSize;
} DIR;

//Struct for open file
typedef struct {
    char fileName[13]; //Name of file
    int mode;          //Mode (read, write, read/write)
    int cluster;       //Starting cluster of the file
    long offset;       //Current offset in the file
    long fileSize;     //Size of the file
    char path[256];    //Path of file
} OpenFile;

//Max number of open files is 10
#define MAX_OPEN_FILES 10

OpenFile openFiles[MAX_OPEN_FILES];
int openFileCount = 0;

BPB_Block bpb;

//Function declarations
//Part 1:
bool mountImage(const char *imagePath, FILE **file);
void readBPB(FILE *file, BPB_Block *bpb);
void startShell(const char *imageName, FILE *file);
void info(FILE *file);

//Part 2:
int commandLS(FILE *file, int Directory);
void formatDirName(unsigned char *DIR_Name, char *formattedName);
long ClusterByteOffset(int cluster);
int GetNextCluster(FILE *file, int cluster);
int commandCD(FILE *file, int *currentDirectory, const char *dirName);

//Part 4:
int commandOpen(FILE *file, const char *fileName, const char *mode, const char *path);
int findFileCluster(FILE *file, const char *fileName);
int determineMode(const char *mode);
int closeFile(const char *fileName);
const char* modeToString(int mode);
void lsof();
int commandLseek(const char *fileName, long offset);
int readFile(FILE *file, const char *fileName, int size);
int getFileDirEntry(FILE *file, int cluster, DIR *dirEntry);


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./filesys [FAT32 ISO]\n");
        return 1;
    }

    const char *imagePath = argv[1];
    FILE *file;

    if (!mountImage(imagePath, &file)) {
        printf("Error: Unable to open the image file '%s'.\n", imagePath);
        return 1;
    }

    startShell(imagePath, file);

    if (file != NULL) {
        fclose(file);
    }

    return 0;
}

//Mounts the image
bool mountImage(const char *imagePath, FILE **file) {
    *file = fopen(imagePath, "rb");
    if (*file == NULL) {
        return false;
    }
    readBPB(*file, &bpb);
    return true;
}

void readBPB(FILE *file, BPB_Block *bpb) {
    //Start at the beggining of file
    fseek(file, 0, SEEK_SET);

    if (fread(bpb, sizeof(BPB_Block), 1, file) != 1) {
        printf("Error reading BPB.\n");
        return;
    }
}

//Come back to fix
void info(FILE *file) {
    //just added
    fseek(file, 0, SEEK_SET);
    unsigned char buffer[512];

    //Read boot sector into the buffer
    if (fread(buffer, sizeof(buffer), 1, file) != 1) {
        printf("Error reading boot sector\n");
        return;
    }

    //Information from boot sector
    //Calculate bytes per sector
    int bytes_per_sector = buffer[11] + (buffer[12] << 8);
    //Get sectors per cluster
    int sectors_per_cluster = buffer[13];
    //Calculate root cluster position
    int root_cluster = buffer[44] + (buffer[45] << 8) + (buffer[46] << 16) + (buffer[47] << 24); 
    //Calculate total sectors
    int total_sectors = buffer[32] + (buffer[33] << 8) + (buffer[34] << 16) + (buffer[35] << 24); 
    //Calculate # of entries in one fat
    int sectors_per_fat = buffer[36] + (buffer[37] << 8) + (buffer[38] << 16) + (buffer[39] << 24);
    int fat_size_bytes = sectors_per_fat * bytes_per_sector;
    int num_entries_in_fat = fat_size_bytes / 4;
    //Calculate total clusters in the data region
    int total_clusters = total_sectors / sectors_per_cluster;
    //Calculate the size of the image
    long size_of_image = (long)total_sectors * bytes_per_sector;

    //Print the extracted information
    printf("Position of Root Cluster: %d\n", root_cluster);
    printf("Bytes Per Sector: %d\n", bytes_per_sector);
    printf("Sectors Per Cluster: %d\n", sectors_per_cluster);
    printf("Total # of Clusters in Data Region: %d\n", total_clusters);
    printf("Number of entries in one FAT: %d\n", num_entries_in_fat);
    printf("Size of Image: %ld bytes\n", size_of_image);
}

//Starts the shell
void startShell(const char *imageName, FILE *file) {
    char currentPath[256] = "/";
    char command[256];
    int CurrentDirectory = bpb.BPB_RootClus;

    while (true) {
        printf("[%s]%s> ", imageName, currentPath);

        if (fgets(command, sizeof(command), stdin) == NULL) {
            printf("Error reading command. Exiting.\n");
            break;
        }

        command[strcspn(command, "\n")] = 0;

        if (strncmp(command, "cd", 2) == 0) {
            char dirName[256] = {0};
            //Parsing
            sscanf(command, "cd %s", dirName);

            if (strcmp(dirName, "..") == 0) {
                //Move up to the parent directory
                char *lastSlash = strrchr(currentPath, '/');
                if (lastSlash != NULL && lastSlash != currentPath) {
                    *lastSlash = '\0';
                } else {
                    printf("Already at the root directory.\n");
                    strcpy(currentPath, "/");
                    CurrentDirectory = 0x2;
                }
            } else if (strcmp(dirName, ".") != 0) {
                if (commandCD(file, &CurrentDirectory, dirName) == 0) {
                    //Append the directory name to the path for subdirectories
                    if (strlen(currentPath) > 1) {
                        strcat(currentPath, "/"); //Add slash if not at root
                    }
                    strcat(currentPath, dirName);
                } else {
                    printf("Directory not found: %s\n", dirName);
                }
            }
        } else if (strncmp(command, "info", 4) == 0) {
            info(file);
        } else if (strncmp(command, "ls", 2) == 0 && strlen(command) == 2) {
            commandLS(file, CurrentDirectory);
        } else if (strncmp(command, "open", 4) == 0) {
            char fileName[13];
            char mode[3];
            if (sscanf(command, "open %s %s", fileName, mode) == 2) {
                //Call openFile
                if (commandOpen(file, fileName, mode, currentPath) == 0) {
                    printf("File '%s' opened in mode '%s'.\n", fileName, mode);
                }
            } else {
                printf("Invalid command. Usage: open [FILENAME] [MODE]\n");
            }
        } else if (strncmp(command, "close", 5) == 0) {
            char fileName[13];
            if (sscanf(command, "close %s", fileName) == 1) {
                if (closeFile(fileName) == 0) {
                    printf("File '%s' closed.\n", fileName);
                } else {
                    printf("Error: File not found or not open.\n");
                }
            } else {
                printf("Invalid command. Usage: close [FILENAME]\n");
            }
        } else if (strncmp(command, "lsof", 4) == 0) {
            lsof();
        } else if (strncmp(command, "read", 4) == 0) {
            char fileName[13];
            int size;
            if (sscanf(command, "read %s %d", fileName, &size) == 2) {
                if (readFile(file, fileName, size) != 0) {
                    printf("Error occurred while reading the file.\n");
                }
            } else {
                printf("Invalid command. Usage: read [FILENAME] [SIZE]\n");
            }
        } else if (strncmp(command, "lseek", 5) == 0) {
            char fileName[13];
            long offset;
            if (sscanf(command, "lseek %s %ld", fileName, &offset) == 2) {
                if (commandLseek(fileName, offset) == 0) {
                    printf("Offset of file '%s' set to %ld.\n", fileName, offset);
                }
            } else {
                printf("Invalid command. Usage: lseek [FILENAME] [OFFSET]\n");
            }
        } else if (strncmp(command, "exit", 4) == 0) {
            break;
        }
    }
}

int commandLS(FILE *file, int Directory) {

    int next_cluster = Directory;
    DIR entry;
    int i;
    bool firstEntry = true;

    while (next_cluster < 0x0FFFFFF8) {  
        long offset = ClusterByteOffset(next_cluster);
        fseek(file, offset, SEEK_SET);

        for (i = 0; i < (bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus / sizeof(DIR)); i++) {
            fread(&entry, sizeof(DIR), 1, file);

            if (entry.DIR_Attr == 0x0F) {
                continue;    
            }
            if (entry.DIR_Name[0] == 0 || entry.DIR_Name[0] == 0xE5) {
                continue;
            }

            char formattedName[13];
            formatDirName(entry.DIR_Name, formattedName);

            //Print directory and file names on the same line
            if (firstEntry) {
                firstEntry = false;
            } else {
                printf("  ");
            }

            //Print directory name in blue
            if (entry.DIR_Attr & 0x10) {
                printf("\033[1;34m%s\033[0m", formattedName);
            } else {
                printf("%s", formattedName);
            }
        }

        next_cluster = GetNextCluster(file, next_cluster);
    }

    printf("\n");
    return 1;
}


void formatDirName(unsigned char *DIR_Name, char *formattedName) {
    int i, j;
    for (i = 0, j = 0; i < 8 && DIR_Name[i] != ' '; i++, j++) {
        formattedName[j] = DIR_Name[i];
    }

    if (DIR_Name[8] != ' ') {
        formattedName[j++] = '.';
    }

    for (i = 8; i < 11 && DIR_Name[i] != ' '; i++, j++) {
        formattedName[j] = DIR_Name[i];
    }

    formattedName[j] = '\0';
}

long ClusterByteOffset(int cluster) {
    long FirstDataSector = bpb.BPB_RsvdSecCnt + (bpb.BPB_NumberofFATS * bpb.BPB_FATSz32);
    long FirstSectorOfCluster = ((cluster - 2) * bpb.BPB_SecPerClus) + FirstDataSector;
    return FirstSectorOfCluster * bpb.BPB_BytsPerSec;
}

int GetNextCluster(FILE *file, int cluster) {
    long FATOffset = cluster * 4;
    int FATSecNum = bpb.BPB_RsvdSecCnt + (FATOffset / bpb.BPB_BytsPerSec);
    int FATEntOffset = FATOffset % bpb.BPB_BytsPerSec;

    unsigned char sector[bpb.BPB_BytsPerSec];
    fseek(file, FATSecNum * bpb.BPB_BytsPerSec, SEEK_SET);
    fread(&sector, bpb.BPB_BytsPerSec, 1, file);

    int nextCluster = *(int *)&sector[FATEntOffset] & 0x0FFFFFFF;
    return nextCluster;
}

int commandCD(FILE *file, int *currentDirectory, const char *dirName) {
    int next_cluster = *currentDirectory;
    DIR entry;
    bool found = false;

    while (next_cluster < 0x0FFFFFF8) {
        long offset = ClusterByteOffset(next_cluster);
        fseek(file, offset, SEEK_SET);

        for (int i = 0; i < (bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus / sizeof(DIR)); i++) {
            fread(&entry, sizeof(DIR), 1, file);

            if (entry.DIR_Name[0] == 0 || entry.DIR_Name[0] == 0xE5) {
                continue;
            }

            char formattedName[13];
            formatDirName(entry.DIR_Name, formattedName);

            if (strcmp(formattedName, dirName) == 0 && (entry.DIR_Attr & 0x10)) {
                *currentDirectory = ((int)entry.DIR_FstClusHi << 16) | entry.DIR_FstClusLo;
                found = true;
                break;
            }
        }

        if (found) break;
        next_cluster = GetNextCluster(file, next_cluster);
    }

    //Return 0 if directory found, -1 otherwise
    return found ? 0 : -1; 
}

int commandOpen(FILE *file, const char *fileName, const char *mode, const char *currentPath) {
    if (openFileCount >= MAX_OPEN_FILES) {
        printf("Error: Maximum number of open files reached.\n");
        return -1;
    }

    //Check if file exists and get its starting cluster
    int cluster = findFileCluster(file, fileName);
    if (cluster < 0) {
        printf("Error: File not found.\n");
        return -1;
    }

    //Check if the file is already open
    for (int i = 0; i < openFileCount; i++) {
        if (strcmp(openFiles[i].fileName, fileName) == 0) {
            printf("Error: File is already open.\n");
            return -1;
        }
    }

    //Set the mode
    int fileMode = determineMode(mode);
    if (fileMode == MODE_INVALID) {
        printf("Error: Invalid mode '%s'.\n", mode);
        return -1;
    }

    //Retrieve the file size
    long fileSize = 0;
    DIR dirEntry;
    if (getFileDirEntry(file, cluster, &dirEntry) == 0) {
        fileSize = dirEntry.DIR_FileSize;
    } else {
        printf("Error: Unable to retrieve file directory entry.\n");
        return -1;
    }

    //Construct the full path
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "fat32.img%s/%s", currentPath, fileName);

    //Add the file to the list
    strcpy(openFiles[openFileCount].fileName, fileName);
    openFiles[openFileCount].mode = fileMode;
    openFiles[openFileCount].cluster = cluster;
    openFiles[openFileCount].offset = 0;
    openFiles[openFileCount].fileSize = fileSize;
    strcpy(openFiles[openFileCount].path, fullPath);
    openFileCount++;

    return 0;
}




int determineMode(const char *mode) {
    if (strcmp(mode, "-r") == 0) {
        return MODE_READ;
    } else if (strcmp(mode, "-w") == 0) {
        return MODE_WRITE;
    } else if (strcmp(mode, "-rw") == 0 || strcmp(mode, "-wr") == 0) {
        return MODE_READ_WRITE;
    } else {
        return MODE_INVALID;
    }
}

int findFileCluster(FILE *file, const char *fileName) {
    int currentCluster = bpb.BPB_RootClus;
    DIR entry;

    while (currentCluster < 0x0FFFFFF8) {
        long offset = ClusterByteOffset(currentCluster);
        fseek(file, offset, SEEK_SET);

        for (int i = 0; i < (bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus / sizeof(DIR)); i++) {
            if (fread(&entry, sizeof(DIR), 1, file) != 1) {
                return -1;
            }

            if (entry.DIR_Name[0] == 0 || entry.DIR_Name[0] == 0xE5) {
                continue;
            }

            char formattedName[13];
            formatDirName(entry.DIR_Name, formattedName);

            if (strcmp(formattedName, fileName) == 0) {
                return ((int)entry.DIR_FstClusHi << 16) | entry.DIR_FstClusLo;
            }
        }

        currentCluster = GetNextCluster(file, currentCluster);
    }

    return -1;
}

int closeFile(const char *fileName) {
    for (int i = 0; i < openFileCount; i++) {
        if (strcmp(openFiles[i].fileName, fileName) == 0) {
            for (int j = i; j < openFileCount - 1; j++) {
                openFiles[j] = openFiles[j + 1]; 
            }
            openFileCount--;
            return 0;
        }
    }
    return -1; //File not found as open
}

const char* modeToString(int mode) {
    switch (mode) {
        case MODE_READ:
            return "-r";
        case MODE_WRITE:
            return "-w";
        case MODE_READ_WRITE:
            return "-rw/-wr";
        default:
            return "Unknown";
    }
}

void lsof() {
    if (openFileCount == 0) {
        printf("No files are currently open.\n");
        return;
    }

    printf("%-10s%-20s%-15s%-10s%-256s\n", "Index", "File Name", "Mode", "Offset", "Path");
    for (int i = 0; i < openFileCount; i++) {
        printf("%-10d%-20s%-15s%-10ld%-256s\n", 
               i, 
               openFiles[i].fileName, 
               modeToString(openFiles[i].mode),
               openFiles[i].offset,
               openFiles[i].path);
    }
}


//LSEEK HERE
int commandLseek(const char *fileName, long offset) {
    for (int i = 0; i < openFileCount; i++) {
        if (strcmp(openFiles[i].fileName, fileName) == 0) {
            if (offset > openFiles[i].fileSize) {
                printf("Error: Reached the end of the file.\n");
                return -1;
            }
            openFiles[i].offset = offset;
            return 0;
        }
    }
    printf("Error: File '%s' is not open.\n", fileName);
    return -1;
}

int readFile(FILE *file, const char *fileName, int size) {
    for (int i = 0; i < openFileCount; i++) {
        if (strcmp(openFiles[i].fileName, fileName) == 0) {
            //Open file found
            char *buffer = malloc(size);
            if (buffer == NULL) {
                printf("Memory allocation failed.\n");
                return -1;
            }

            //Seek to the current offset in the file
            fseek(file, ClusterByteOffset(openFiles[i].cluster) + openFiles[i].offset, SEEK_SET);
            
            //Read data into buffer
            size_t bytesRead = fread(buffer, 1, size, file);
            if (bytesRead < size) {
                printf("Could not read the requested number of bytes.\n");
                free(buffer);
                return -1;
            }

            printf("Data read from file: %.*s\n", size, buffer);

            openFiles[i].offset += bytesRead;

            free(buffer);
            return 0;
        }
    }

    printf("File not open: %s\n", fileName);
    return -1;
}

int getFileDirEntry(FILE *file, int cluster, DIR *dirEntry) {
    int currentCluster = cluster;

    while (currentCluster < 0x0FFFFFF8) {
        long offset = ClusterByteOffset(currentCluster);
        fseek(file, offset, SEEK_SET);

        for (int i = 0; i < (bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus / sizeof(DIR)); i++) {
            if (fread(dirEntry, sizeof(DIR), 1, file) != 1) {
                printf("Error reading directory entry.\n");
                return -1;
            }

            if (dirEntry->DIR_Name[0] == 0 || dirEntry->DIR_Name[0] == 0xE5) {
                continue;
            }

            return 0;
        }

        currentCluster = GetNextCluster(file, currentCluster);
    }

    //Directory entry not found
    return -1;
}
