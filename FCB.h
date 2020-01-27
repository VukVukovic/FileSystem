#pragma once

#include "part.h"
#include "file.h"
#include <string>
#include <map>
#include <mutex>
#include <condition_variable>

class KernelFile;
class ClusterCache;

class FCB {
	friend class KernelFile;

	std::string name;
	ClusterNo index;
	BytesCnt size;

	bool exclusive = false;
	int users = 0, waiting=0;

	static std::map<std::string, FCB*> opened_files;
	static std::mutex mtx;
	static std::condition_variable cv;
	static KernelFS* kernelFS;

	FCB(std::string name, ClusterNo index, BytesCnt size);
	
public:
	static void setKernelFS(KernelFS* kfs) { kernelFS = kfs; }
	static bool isOpen(std::string name);
	static bool isAnyOpen();
	static KernelFile* newOperation(char mode, std::string name, ClusterNo index, BytesCnt size);
	static void finishedOperation(char mode, FCB* fcb);
};