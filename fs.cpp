#include "fs.h"
#include "kernelfs.h"
#include <mutex>
using namespace std;

KernelFS* FS::myImpl = nullptr;
mutex mtx;

FS::~FS()
{
	// will not be called
}

FS::FS()
{
	// will not be called
}

char FS::mount(Partition* partition)
{
	mtx.lock();
	if (myImpl == nullptr)
		myImpl = new KernelFS();
	mtx.unlock();

	return myImpl->mount(partition);
}

char FS::unmount()
{
	mtx.lock();
	if (myImpl == nullptr)
		myImpl = new KernelFS();
	mtx.unlock();

	return myImpl->unmount();
}

char FS::format()
{
	mtx.lock();
	if (myImpl == nullptr)
		myImpl = new KernelFS();
	mtx.unlock();

	return myImpl->format();
}

FileCnt FS::readRootDir()
{
	mtx.lock();
	if (myImpl == nullptr)
		myImpl = new KernelFS();
	mtx.unlock();

	return myImpl->readRootDir();
}

char FS::doesExist(char* fname)
{
	mtx.lock();
	if (myImpl == nullptr)
		myImpl = new KernelFS();
	mtx.unlock();

	return myImpl->doesExist(fname);
}

File* FS::open(char* fname, char mode)
{
	mtx.lock();
	if (myImpl == nullptr)
		myImpl = new KernelFS();
	mtx.unlock();

	return myImpl->open(fname, mode);
}

char FS::deleteFile(char* fname)
{
	mtx.lock();
	if (myImpl == nullptr)
		myImpl = new KernelFS();
	mtx.unlock();

	return myImpl->deleteFile(fname);
}
