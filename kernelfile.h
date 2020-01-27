#pragma once
#include "file.h"
#include "FCB.h"

class KernelFS;

class KernelFile {
    static const unsigned long EntriesPerCluster;
    static const unsigned long ClustersPerLevel0Entry;
    static const BytesCnt BytesPerLevel0Entry;
    static const BytesCnt BytesPerLevel1Entry;
    static const BytesCnt MaxFileSize;

    char mode;
    FCB* fcb;
    BytesCnt pos=0;
    KernelFS* kernelFS;

    bool allocate(unsigned long to_alloc);
    bool deallocate(unsigned long to_dealloc, BytesCnt new_size);

public:
    KernelFile(char mode, FCB* fcb) : mode(mode), fcb(fcb) { if (mode == 'a') pos = fcb->size; }
    void setKernelFS(KernelFS* kfs);
    char write(BytesCnt cnt, char* buffer);
    BytesCnt read(BytesCnt cnt, char* buffer);
    char seek(BytesCnt pos);
    BytesCnt filePos();
    char eof();
    BytesCnt getFileSize();
    char truncate();
    ~KernelFile();
};