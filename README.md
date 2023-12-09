# FAT32 File System Interface

## Division of Labor

**Team Member:**
- Eli Bendavid

### Part 1: Mount the Image File
- `mount` - Eli
- `info` - Eli
- `exit` - Eli

### Part 2: Navigation
- `cd` - Eli
- `ls` - Eli

### Part 4: Read
- `open` - Eli
- `close` - Eli
- `lsof` - Eli
- `lseek` - Eli
- `read` - Eli

## File Listing
- `fat32.c` - Main source file for the FAT32 interface.
- `Makefile` - Makefile for compiling the program.
- `fat32.img` - FAT32 image file for testing.

## How to Compile and Run
You have two options to compile and run the program: using the Makefile or using the command line directly.

### Using the Makefile

1. Clean previous builds to ensure a fresh start:
`make clean`
2. Now make the program:
`make`
3. assuming you have the fat.32 img in your directory, simply type:
`make run`
4. alternitively, you can run
`./filesys fat32.img`

### Using the Command Line
1. `gcc -o filesys filesys.c`
2. `./filesys fat32.img`

## Additional Notes

- The program is designed to interact with FAT32 image files.
- Make sure the `fat32.img` file is in the same directory as the executable when running the program.
- It is recommended to check the FAT32 specification for any specific requirements or limitations when interacting with the file system.
