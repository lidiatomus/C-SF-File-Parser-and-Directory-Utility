#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


//constants for the program
#define VARIANT 59592
#define MAX_PATH_LENGTH 1024
#define MAX_NAME_LENGTH 256
#define BUFFER_SIZE 4096
#define MAX_SECTIONS 16
#define MAX_LINE_LENGTH 1024
#define SECTION_HEADER_SIZE 17

#define MAGIC_VALUE 'z'
#define MIN_VERSION 84
#define MAX_VERSION 163
#define VALID_SECTION_TYPES_COUNT 5

//valid section types
const int VALID_SECTION_TYPES[VALID_SECTION_TYPES_COUNT] = {80, 43, 38, 23, 81};

//section structure : name, type, offset, size
typedef struct {
    char name[8];
    unsigned short type;
    unsigned int offset;
    unsigned int size;
} SectionHeader;

//header structure : version, no_of_sections, sections, header_size, magic
typedef struct {
    unsigned int version;
    unsigned char no_of_sections;
    SectionHeader sections[MAX_SECTIONS];
    unsigned short header_size;
    char magic;
} SFHeader;

//functions prototypes
void displayVariant(); //display variant number
void processDirectory(char *basePath, char *currentPath, int recursive, char *filterOption, char *filterValue, int findAllMode); //process directory
int readSFHeader(int fd, SFHeader *header); //read header from file
int countLinesInSection(int fd, SFHeader *header, int sectionIndex); //count lines in a section
int hasRequiredSections(char *filePath); //check if file has required sections
void parseFile(char *filePath); //parse file
void extractLine(char *filePath, int sectionNumber, int lineNumber); //extract line from file
void findAllSpecialFiles(char *dirPath); //find all special files
void listDirectory(char *dirPath, int recursive, char *filterOption, char *filterValue); //list directory
int readSFHeaderWithoutMessages(int fd, SFHeader *header); //read header silently (without error messages)
int hasRequiredSectionsWithoutMessages(char *filePath); //check if file has required sections silently (without error messages)


//function to display variant number
void displayVariant() {
    printf("%d\n", VARIANT);
}

//function to process directory: traverse the directory and process files/filter them based on the options
void processDirectory(char *basePath, char *currentPath, int recursive, char *filterOption, char *filterValue, int findAllMode) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char path[MAX_PATH_LENGTH];

    //open the directory
    dir = opendir(currentPath);
    if (dir == NULL) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) { //read each entry in the directory
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; //skip the current and parent directory
        }

        //create the full path
        snprintf(path, sizeof(path), "%s/%s", currentPath, entry->d_name);

        if (stat(path, &statbuf) == -1) {
            continue;
        }

        //check if the file has required sections using the hasRequiredSections function and find all mode
        if (findAllMode) {
            if (S_ISREG(statbuf.st_mode) && hasRequiredSections(path)) {
                printf("%s\n", path);//write the path to the output if the file has the required sections
            }
        } else {
            int match = 1;

            //check if the file matches the filter options
            if (filterOption != NULL && filterValue != NULL) {
                if (strcmp(filterOption, "size_smaller") == 0) {
                    if (S_ISREG(statbuf.st_mode)) {
                        long size_limit = atol(filterValue);
                        match = statbuf.st_size < size_limit;
                    } else {
                        match = 0;
                    }
                } else if (strcmp(filterOption, "name_starts_with") == 0) {
                    match = strncmp(entry->d_name, filterValue, strlen(filterValue)) == 0;
                }
            }

            if (match) {
                printf("%s\n", path);
            }
        }

        //if the file is a directory, and we are in the recursive mode, process the directory
        if (recursive && S_ISDIR(statbuf.st_mode)) {
            processDirectory(basePath, path, recursive, filterOption, filterValue, findAllMode);
        }
    }

    closedir(dir);
}

//function to list the content of a directory
void listDirectory(char *dirPath, int recursive, char *filterOption, char *filterValue) {
    DIR *dir = opendir(dirPath);
    if (dir == NULL) {
        printf("ERROR\n invalid directory path");
        return;
    }
    closedir(dir);

    printf("SUCCESS\n");

    processDirectory(dirPath, dirPath, recursive, filterOption, filterValue, 0);
}

//function to parse file and validate it
void parseFile(char *filePath) {
    int fd = open(filePath, O_RDONLY); //open the file
    if (fd == -1) {
        printf("ERROR\n invalid file");
        return;
    }

    SFHeader header;
    if (!readSFHeader(fd, &header)) { //read the header from the file
        close(fd);
        return;
    }

    printf("SUCCESS\n");
    printf("version=%u\n", header.version);
    printf("nr_sections=%u\n", header.no_of_sections);

    for (int i = 0; i < header.no_of_sections; i++) {
        printf("section%d: %s %u %u\n", i + 1, header.sections[i].name, header.sections[i].type, header.sections[i].size);
    }

    close(fd);
}

//function to read header from file and validate it
int readSFHeader(int fd, SFHeader *header) {
    struct stat st;
    if (fstat(fd, &st) == -1) {
        printf("ERROR\n invalid file"); //write error message if the file is invalid
        return 0;
    }

    if (st.st_size < 3) {
        printf("ERROR\n invalid file");
        return 0;
    }

    //read magic value from the end of the file
    lseek(fd, -1, SEEK_END);
    if (read(fd, &header->magic, 1) != 1) {
        printf("ERROR\n invalid file");
        return 0;
    }

    //check if the magic value is correct
    if (header->magic != MAGIC_VALUE) {
        printf("ERROR\n wrong magic");
        return 0;
    }

    //read header size from the byte before the magic value
    lseek(fd, -3, SEEK_END);
    if (read(fd, &header->header_size, 2) != 2) {
        printf("ERROR\n invalid file");
        return 0;
    }

    //look for the start of the header
    lseek(fd, -header->header_size, SEEK_END);

    //read version from the header
    if (read(fd, &header->version, 4) != 4) {
        printf("ERROR\n invalid file");
        return 0;
    }

    //check if the version is correct
    if (header->version < MIN_VERSION || header->version > MAX_VERSION) {
        printf("ERROR\n wrong version");
        return 0;
    }

    //read number of sections from the header
    if (read(fd, &header->no_of_sections, 1) != 1) {
        printf("ERROR\n invalid file");
        return 0;
    }

    //check if the number of sections is correct
    if (header->no_of_sections != 2 && (header->no_of_sections < 6 || header->no_of_sections > 14)) {
        printf("ERROR\n wrong sect_nr");
        return 0;
    }

    //read each section headers
    for (int i = 0; i < header->no_of_sections; i++) {
        if (read(fd, header->sections[i].name, 7) != 7) {
            printf("ERROR\n invalid file");
            return 0;
        }
        header->sections[i].name[7] = '\0'; //null terminate the name

        if (read(fd, &header->sections[i].type, 2) != 2) {
            printf("ERROR\n invalid file");
            return 0;
        }

        int valid_type = 0;
        for (int j = 0; j < VALID_SECTION_TYPES_COUNT; j++) {
            if (header->sections[i].type == VALID_SECTION_TYPES[j]) {
                valid_type = 1; //check if the section type is valid
                break;
            }
        }

        if (!valid_type) {
            printf("ERROR\n wrong sect_types");
            return 0;
        }

        if (read(fd, &header->sections[i].offset, 4) != 4) {
            printf("ERROR\n invalid file");
            return 0;
        }

        if (read(fd, &header->sections[i].size, 4) != 4) {
            printf("ERROR\n invalid file");
            return 0;
        }
    }

    return 1;
}

//function to count lines in a section
int countLinesInSection(int fd, SFHeader *header, int sectionIndex) {
    SectionHeader *section = &header->sections[sectionIndex];

    lseek(fd, section->offset, SEEK_SET); //look for the start of the section

    char *sectionContent = (char *)malloc(section->size + 1); //allocate memory for the section content
    if (sectionContent == NULL) {
        return 0;
    }

    if (read(fd, sectionContent, section->size) != section->size) {
        free(sectionContent); //free the memory if the read fails
        return 0;
    }
    sectionContent[section->size] = '\0'; //null terminate the section content

    int lineCount = 0;
    for (unsigned int i = 0; i < section->size; i++) {
        if (sectionContent[i] == '\n') {
            lineCount++; //count the lines
        }
    }
    lineCount++;

    free(sectionContent); //free the memory
    return lineCount;
}

//function to check if file has required sections
int hasRequiredSections(char *filePath) {
    int fd = open(filePath, O_RDONLY); //open the file
    if (fd == -1) {
        return 0;
    }

    //read the header from the file
    SFHeader header;
    if (!readSFHeader(fd, &header)) {
        close(fd);
        return 0;
    }

    int sectionsWithThirteenLines = 0;
    for (int i = 0; i < header.no_of_sections; i++) {
        int lineCount = countLinesInSection(fd, &header, i);
        if (lineCount == 13) {
            sectionsWithThirteenLines++; //count the sections with 13 lines
        }
    }

    close(fd);
    return sectionsWithThirteenLines >= 2; //return 1 if there are at least 2 sections with 13 lines
}

//function to read header silently (without error messages)
int readSFHeaderWithoutMessages(int fd, SFHeader *header) {
    struct stat st;
    if (fstat(fd, &st) == -1) {
        return 0;
    }

    if (st.st_size < 3) {
        return 0;
    }

    lseek(fd, -1, SEEK_END);
    if (read(fd, &header->magic, 1) != 1) {
        return 0;
    }

    if (header->magic != MAGIC_VALUE) {
        return 0;
    }

    lseek(fd, -3, SEEK_END);
    if (read(fd, &header->header_size, 2) != 2) {
        return 0;
    }

    lseek(fd, -header->header_size, SEEK_END);

    if (read(fd, &header->version, 4) != 4) {
        return 0;
    }

    if (header->version < MIN_VERSION || header->version > MAX_VERSION) {
        return 0;
    }

    if (read(fd, &header->no_of_sections, 1) != 1) {
        return 0;
    }

    if (header->no_of_sections != 2 && (header->no_of_sections < 6 || header->no_of_sections > 14)) {
        return 0;
    }

    for (int i = 0; i < header->no_of_sections; i++) {
        if (read(fd, header->sections[i].name, 7) != 7) {
            return 0;
        }
        header->sections[i].name[7] = '\0';

        if (read(fd, &header->sections[i].type, 2) != 2) {
            return 0;
        }

        int valid_type = 0;
        for (int j = 0; j < VALID_SECTION_TYPES_COUNT; j++) {
            if (header->sections[i].type == VALID_SECTION_TYPES[j]) {
                valid_type = 1;
                break;
            }
        }
        if (!valid_type) {
            return 0;
        }

        if (read(fd, &header->sections[i].offset, 4) != 4) {
            return 0;
        }

        if (read(fd, &header->sections[i].size, 4) != 4) {
            return 0;
        }
    }

    return 1;
}

//function to check if file has required sections silently (without error messages)
int hasRequiredSectionsWithoutMessages(char *filePath) {
    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    SFHeader header;
    if (!readSFHeaderWithoutMessages(fd, &header)) { // <- silentErrors = 1
        close(fd);
        return 0;
    }

    int sectionsWithThirteenLines = 0;
    for (int i = 0; i < header.no_of_sections; i++) {
        int lineCount = countLinesInSection(fd, &header, i);
        if (lineCount == 13) {
            sectionsWithThirteenLines++;
        }
    }

    close(fd);
    return sectionsWithThirteenLines >= 2;
}


// recursive directory traversal for findall, find all files with required sections
void traverseAndFindAll(const char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        return;
    }

    struct dirent *entry;
    char fullPath[MAX_PATH_LENGTH];

    while ((entry = readdir(dir)) != NULL) { //read each entry in the directory
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        //create the full path
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

        struct stat statbuf;
        if (stat(fullPath, &statbuf) == -1) {
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            traverseAndFindAll(fullPath); //recursively traverse the directory
        } else if (S_ISREG(statbuf.st_mode)) {
            if (hasRequiredSectionsWithoutMessages(fullPath)) {
                printf("%s\n", fullPath); //write the path to the output if the file has the required sections
            }
        }
    }

    closedir(dir);
}

// function to find all special files
void findAllSpecialFiles(char *dirPath) {
    DIR *dir = opendir(dirPath);
    if (dir == NULL) {
        printf("ERROR\n invalid directory path");
        return;
    }
    closedir(dir);

    printf("SUCCESS\n");

    traverseAndFindAll(dirPath); //traverse the directory and find all files with required sections
}

// function to extract line from file
void extractLine(char *filePath, int sectionNumber, int lineNumber) {
    int fd = open(filePath, O_RDONLY); //open the file
    if (fd == -1) {
        printf("ERROR\n invalid file");
        return;
    }

    SFHeader header; //read the header from the file
    if (!readSFHeader(fd, &header)) {
        close(fd);
        return;
    }

    //check if the section number is valid
    if (sectionNumber < 1 || sectionNumber > header.no_of_sections) {
        close(fd);
        printf("ERROR\n invalid section");
        return;
    }

    //get the section header
    SectionHeader *section = &header.sections[sectionNumber - 1];

    //allocate memory for the section content
    char *content = (char *)malloc(section->size + 1);
    if (content == NULL) {
        close(fd);
        printf("ERROR\n memory error");
        return;
    }

    //read the section content
    lseek(fd, section->offset, SEEK_SET);
    if (read(fd, content, section->size) != section->size) {
        free(content);
        close(fd);
        printf("ERROR\n invalid section");
        return;
    }
    content[section->size] = '\0';

    // count total lines
    int totalLines = 0;
    for (unsigned int i = 0; i < section->size; i++) {
        if (content[i] == '\n') {
            totalLines++;
        }
    }

    if (section->size > 0 && content[section->size - 1] != '\n') {
        totalLines++; // Last line without newline
    }

    // Check if line number is valid
    if (lineNumber < 1 || lineNumber > totalLines) {
        free(content);
        close(fd);
        printf("ERROR\n invalid line");
        return;
    }

    // calculate correct line index (from start)
    int targetLine = totalLines - lineNumber + 1;

    // find and print target line
    int currentLine = 1;
    char *start = content;
    for (unsigned int i = 0; i <= section->size; i++) {
        if (content[i] == '\n' || content[i] == '\0') {
            if (currentLine == targetLine) {
                content[i] = '\0';

                // output only ok ascii characters
                char cleanLine[MAX_LINE_LENGTH];
                int pos = 0;
                for (char *p = start; *p != '\0' && pos < MAX_LINE_LENGTH - 1; p++) {
                    if (*p >= 32 && *p <= 126) {
                        cleanLine[pos++] = *p;
                    }
                }
                cleanLine[pos] = '\0';

                printf("SUCCESS\n%s\n", cleanLine);
                free(content);
                close(fd);
                return;
            }
            currentLine++;
            start = content + i + 1;
        }
    }

    free(content);
    close(fd);
    printf("ERROR\n invalid line");
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("ERROR\n invalid command");
        return 1;
    }

    if (strcmp(argv[1], "variant") == 0) {
        displayVariant();
    } else if (strcmp(argv[1], "list") == 0) {
        char *dirPath = NULL;
        int recursive = 0;
        char *filterOption = NULL;
        char *filterValue = NULL;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "recursive") == 0) {
                recursive = 1;
            } else if (strncmp(argv[i], "path=", 5) == 0) {
                dirPath = argv[i] + 5;
            } else if (strncmp(argv[i], "size_smaller=", 13) == 0) {
                filterOption = "size_smaller";
                filterValue = argv[i] + 13;
            } else if (strncmp(argv[i], "name_starts_with=", 17) == 0) {
                filterOption = "name_starts_with";
                filterValue = argv[i] + 17;
            }
        }

        if (dirPath == NULL) {
            printf("ERROR\n invalid directory path");
            return 1;
        }

        listDirectory(dirPath, recursive, filterOption, filterValue);
    } else if (strcmp(argv[1], "parse") == 0) {
        char *filePath = NULL;

        for (int i = 2; i < argc; i++) {
            if (strncmp(argv[i], "path=", 5) == 0) {
                filePath = argv[i] + 5;
                break;
            }
        }

        if (filePath == NULL) {
            printf("ERROR\n invalid file");
            return 1;
        }

        parseFile(filePath);
    } else if (strcmp(argv[1], "extract") == 0) {
        char *filePath = NULL;
        int sectionNumber = -1;
        int lineNumber = -1;

        for (int i = 2; i < argc; i++) {
            if (strncmp(argv[i], "path=", 5) == 0) {
                filePath = argv[i] + 5;
            } else if (strncmp(argv[i], "section=", 8) == 0) {
                sectionNumber = atoi(argv[i] + 8);
            } else if (strncmp(argv[i], "line=", 5) == 0) {
                lineNumber = atoi(argv[i] + 5);
            }
        }

        if (filePath == NULL || sectionNumber < 0 || lineNumber < 0) {
            printf("ERROR\n invalid file|section|line");
            return 1;
        }

        extractLine(filePath, sectionNumber, lineNumber);
    } else if (strcmp(argv[1], "findall") == 0) {
        char *dirPath = NULL;

        for (int i = 2; i < argc; i++) {
            if (strncmp(argv[i], "path=", 5) == 0) {
                dirPath = argv[i] + 5;
                break;
            }
        }

        if (dirPath == NULL) {
            printf("ERROR\n invalid directory path");
            return 1;
        }

        findAllSpecialFiles(dirPath);
    } else {
        printf("ERROR\n invalid command");
        return 1;
    }

    return 0;
} FOR THIS
