#pragma once
#include "bitvector.h"
#include "part.h"
#include "fs.h"
#include "clustercache.h"
#include <condition_variable>
#include "FindDirResult.h"
#include "file.h"

class FCB;
class KernelFile;
struct DirEntry;

class KernelFS {
	static const unsigned long EntriesPerCluster;
	static const unsigned long FileEntriesPerCluster;

	static std::map<std::string, FCB*> opened_files;

	Partition* current_partition;
	BitVector bit_vector;
	ClusterNo root_index;
	ClusterCache cluster_cache;
	std::mutex mtx;
	std::condition_variable cvPartition, cvOpenFiles;
	bool started_format = false;

	void _formatRoot();
	DirResult _findDirEntry(char name[FNAMELEN], char extension[FEXTLEN]);
	static bool _split(const char* fname, char name[FNAMELEN], char ext[FEXTLEN]);
	DirResult _createFile(char name[FNAMELEN], char extension[FEXTLEN], const DirResult& r_info, bool inPlace);
	void _clearCluster(ClusterNo cluster);
	bool _removeEntry(const DirResult&);
	bool _deallocateFileClusters(ClusterNo cj, unsigned long k1);

	void _finishedOperation(FCB* fcb, char mode, std::unique_lock<std::mutex>&);
	KernelFile* _newOperation(char mode, std::string name, ClusterNo index, BytesCnt size, std::unique_lock<std::mutex>&);

	char _mount(Partition* partition, std::unique_lock<std::mutex>&);
	char _unmount(std::unique_lock<std::mutex>&);
	char _format(std::unique_lock<std::mutex>&);
	FileCnt _readRootDir();
	char _doesExist(char* fname);
	File* _open(char* fname, char mode, std::unique_lock<std::mutex>&);
	char _deleteFile(char* fname);

	friend class KernelFile;
	friend class FCB;
public:
	KernelFS() : cluster_cache(128) {}

	char mount(Partition* partition);
	char unmount();
	char format();
	FileCnt readRootDir();
	char doesExist(char* fname);
	File* open(char* fname, char mode);
	char deleteFile(char* fname);
	void finishedOperation(FCB* fcb, char mode);
};