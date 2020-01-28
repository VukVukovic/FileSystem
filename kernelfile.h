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

    char rd_buffer[ClusterSize];
    BytesCnt rd_pos = -1; 
    unsigned long rd_buff_size = 0;
    //char wrBuffer[ClusterSize];

    bool _allocate(unsigned long to_alloc, unsigned long level1_to_alloc);
    bool _deallocate(unsigned long to_dealloc, BytesCnt new_size);
    char _write(BytesCnt cnt, char* buffer);
    BytesCnt _read(BytesCnt cnt, char* buffer);

public:
    KernelFile(char mode, FCB* fcb) : mode(mode), fcb(fcb) { if (mode == 'a') pos = fcb->getSize(); }
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