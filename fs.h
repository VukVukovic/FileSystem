#pragma once

// File: fs.h
typedef long FileCnt;

const unsigned int FNAMELEN = 8;
const unsigned int FEXTLEN = 3;

class KernelFS;
class Partition;
class File;

class FS {
public:
    ~FS();
    static char mount(Partition* partition);
    static char unmount();
    static char format();
    static FileCnt readRootDir();
    static char doesExist(char* fname);

    static File* open(char* fname, char mode);
    static char deleteFile(char* fname);
protected:
    FS();
    static KernelFS* myImpl;
};