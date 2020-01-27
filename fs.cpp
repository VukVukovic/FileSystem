#include "fs.h"
#include "kernelfs.h"
#include <mutex>
using namespace std;

KernelFS* FS::myImpl = nullptr;
mutex mtx;

FS::~FS()
{
	unique_lock<mutex> lck(mtx);
	if (myImpl != nullptr) {
		delete myImpl;
		myImpl = nullptr;
	}
}

char FS::mount(Partition* partition)
{
	unique_lock<mutex> lck(mtx);
	if (myImpl == nullptr)
		myImpl = new KernelFS();

	return myImpl->mount(partition);
}

char FS::unmount()
{
	unique_lock<mutex> lck(mtx);
	if (myImpl == nullptr)
		myImpl = new KernelFS();

	return myImpl->unmount();
}

char FS::format()
{
	unique_lock<mutex> lck(mtx);
	if (myImpl == nullptr)
		myImpl = new KernelFS();

	return myImpl->format();
}

FileCnt FS::readRootDir()
{
	unique_lock<mutex> lck(mtx);
	if (myImpl == nullptr)
		myImpl = new KernelFS();

	return myImpl->readRootDir();
}

char FS::doesExist(char* fname)
{
	unique_lock<mutex> lck(mtx);
	if (myImpl == nullptr)
		myImpl = new KernelFS();

	return myImpl->doesExist(fname);
}

File* FS::open(char* fname, char mode)
{
	unique_lock<mutex> lck(mtx);
	if (myImpl == nullptr)
		myImpl = new KernelFS();

	return myImpl->open(fname, mode);
}

char FS::deleteFile(char* fname)
{
	unique_lock<mutex> lck(mtx);
	if (myImpl == nullptr)
		myImpl = new KernelFS();

	return myImpl->deleteFile(fname);
}

FS::FS()
{
}
