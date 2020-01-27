#pragma once
#include "bitvector.h"
#include "part.h"
#include "fs.h"
#include "clustercache.h"

class FCB;

struct DirEntry;

class KernelFS {
	static const unsigned long EntriesPerCluster;
	static const unsigned long FileEntriesPerCluster;

	Partition* current_partition;
	BitVector bit_vector;
	ClusterNo root_index;
	ClusterCache cluster_cache;

	void formatRoot();
	bool findDirEntry(char name[FNAMELEN], char extension[FEXTLEN], 
		unsigned long& i1, ClusterNo& ci, unsigned long& j1, ClusterNo& cj, unsigned long& k1);
	static bool split(const char* fname, char name[FNAMELEN], char ext[FEXTLEN]);
	bool createFile(char name[FNAMELEN], char extension[FEXTLEN],
		unsigned long& i1, ClusterNo& ci, unsigned long& j1, ClusterNo& cj, unsigned long& k1,
		bool inPlace);
	void clearCluster(ClusterNo cluster);
	bool removeEntry(unsigned long i1, ClusterNo ci, unsigned long j1, ClusterNo cj, unsigned long k1);
	bool deallocate(ClusterNo cj, unsigned long k1);

	friend class KernelFile;
public:
	KernelFS() : cluster_cache(128) {}

	char mount(Partition* partition);
	char unmount();
	char format();
	FileCnt readRootDir();
	char doesExist(char* fname);
	File* open(char* fname, char mode);
	char deleteFile(char* fname);
};