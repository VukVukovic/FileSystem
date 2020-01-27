#include "FCB.h"
#include <string>
#include "kernelfile.h"
#include "clustercache.h"
#include "kernelfs.h"
#include "direntry.h"
using namespace std;

map<string, FCB*> FCB::opened_files;
mutex FCB::mtx;
condition_variable FCB::cv;
KernelFS* FCB::kernelFS = nullptr;

FCB::FCB(string name, ClusterNo index, BytesCnt size) 
	: name(name), index(index), size(size)
{
}

bool FCB::isOpen(string name)
{
	unique_lock<mutex> lck(mtx);
	return opened_files.find(name) != opened_files.end();
}

bool FCB::isAnyOpen()
{
	unique_lock<mutex> lck(mtx);
	return !opened_files.empty();
}

KernelFile* FCB::newOperation(char mode, std::string name, ClusterNo index, BytesCnt size)
{
	unique_lock<mutex> lck(mtx);

	FCB* fcb;
	if (opened_files.find(name) != opened_files.end())
		fcb = opened_files[name];
	else {
		fcb = new FCB(name, index, size);
		opened_files[name] = fcb;
	}

	fcb->waiting++;
	while ((mode == 'r' && fcb->exclusive) || (mode != 'r' && (fcb->exclusive || fcb->users > 0)))
		cv.wait(lck);
	fcb->waiting--;

	if (mode == 'r')
		fcb->users++;
	else
		fcb->exclusive = true;

	KernelFile *kfile = new KernelFile(mode, fcb);
	return kfile;
}

void FCB::finishedOperation(char mode, FCB* fcb)
{
	unique_lock<mutex> lck(mtx);

	if (mode == 'r')
		fcb->users--;
	else
		fcb->exclusive = false;

	if (fcb->users == 0 && fcb->waiting==0) {
		opened_files.erase(fcb->name);
		
		char name[FNAMELEN] = { 0 }, extension[FEXTLEN] = { 0 };
		KernelFS::split(fcb->name.c_str(), name, extension);

		unsigned long int i1, j1, k1;
		ClusterNo ci, cj;
		bool found = kernelFS->findDirEntry(name, extension, i1, ci, j1, cj, k1);

		CacheEntry cdirentr = kernelFS->cluster_cache.get(cj);
		DirEntry* entries = (DirEntry*)cdirentr.getBuffer();
		entries[k1].size = fcb->size;
		cdirentr.markDirty();
		cdirentr.unlock();

		delete fcb;
	}
	else
		cv.notify_all();
}

