#include "kernelfs.h"
#include "direntry.h"
#include "kernelfile.h"
#include "FCB.h"
#include <string>
#include <iostream>
using namespace std;

const unsigned long KernelFS::FileEntriesPerCluster =
					ClusterSize / sizeof(DirEntry);
const unsigned long KernelFS::EntriesPerCluster =
					ClusterSize / sizeof(ClusterNo);

map<string, FCB*> KernelFS::opened_files;

void KernelFS::_formatRoot()
{
	CacheEntry e = cluster_cache.get(root_index);
	memset(e.getBuffer(), 0, ClusterSize);
	e.markDirty();
	e.unlock();
}

DirResult KernelFS::_findDirEntry(char name[FNAMELEN], char extension[FEXTLEN])
{
	DirResult result;
	result.i = result.j = result.k = -1;

	CacheEntry e0 = cluster_cache.get(root_index);
	ClusterNo* index0 = (ClusterNo*)e0.getBuffer();
	bool found = false;

	for (unsigned long i = 0; i < EntriesPerCluster && index0[i] != 0; ++i) {
		result.i = i;
		result.ci = index0[i];
		CacheEntry e1 = cluster_cache.get(index0[i]);
		ClusterNo* index1 = (ClusterNo*)e1.getBuffer();

		for (unsigned long j = 0; j < EntriesPerCluster && index1[j] != 0; ++j) {
			result.j = j;
			result.cj = index1[j];
			CacheEntry entr = cluster_cache.get(index1[j]);
			DirEntry* entries = (DirEntry*)entr.getBuffer();
			
			for (unsigned long k = 0; k < FileEntriesPerCluster && !entries[k].empty(); ++k) {
				result.k = k;
				if (entries[k].equals(name, extension)) {
					found = true;
					break;
				}
			}
			entr.unlock();

			if (found)
				break;
		}
		e1.unlock();
	}
	
	e0.unlock();
	result.success = found;
	return result;
}

bool KernelFS::_removeEntry(const DirResult& r)
{
	CacheEntry ce = cluster_cache.get(r.cj);
	DirEntry* entries = (DirEntry*)ce.getBuffer();
	memset(entries + r.k, 0, sizeof(DirEntry));
	ce.markDirty();
	ce.unlock();

	if (r.k == 0) {
		CacheEntry ce1 = cluster_cache.get(r.ci);
		ClusterNo* index1 = (ClusterNo*)ce1.getBuffer();
		index1[r.j] = 0;
		ce1.markDirty();
		ce1.unlock();
		bit_vector.free(r.cj);

		if (r.j == 0) {
			CacheEntry ce0 = cluster_cache.get(root_index);
			ClusterNo* index0 = (ClusterNo*)ce0.getBuffer();
			index0[r.i] = 0;
			ce0.markDirty();
			ce0.unlock();
			bit_vector.free(r.ci);
		}
	}
	return true;
}

bool KernelFS::_deallocateFileClusters(ClusterNo cj, unsigned long k1)
{
	CacheEntry ce = cluster_cache.get(cj);
	DirEntry* entries = (DirEntry*)ce.getBuffer();
	ClusterNo ind0cluster = entries[k1].index;
	entries[k1].index = 0;
	ce.markDirty();
	ce.unlock();

	CacheEntry ceind0 = cluster_cache.get(ind0cluster);
	ClusterNo* index0 = (ClusterNo*)ceind0.getBuffer();
	for (unsigned long i = 0; i < EntriesPerCluster && index0[i] != 0; i++) {
		CacheEntry ceind1 = cluster_cache.get(index0[i]);
		ClusterNo* index1 = (ClusterNo*)ceind1.getBuffer();
		for (unsigned long j = 0; j < EntriesPerCluster && index1[j] != 0; j++) {
			bit_vector.free(index1[j]);
			index1[j] = 0;
		}
		ceind1.unlock();
		bit_vector.free(index0[i]);
		index0[i] = 0;
	}
	ceind0.unlock();
	bit_vector.free(ind0cluster);

	return true;
}

void KernelFS::_finishedOperation(FCB* fcb, char mode, std::unique_lock<std::mutex>&)
{
	//cout << "FINISHED OPERATION!" << endl;
	if (mode == 'r')
		fcb->users--;
	else
		fcb->exclusive = false;
	
	//cout << fcb->users << " " << fcb->exclusive << " " << fcb->waiting << endl;

	// notify waiting for file
	if (fcb->users == 0 && !fcb->exclusive)
		fcb->cvOpen.notify_all();

	if (fcb->users == 0 && !fcb->exclusive && fcb->waiting == 0) {
		opened_files.erase(fcb->name);

		char name[FNAMELEN] = { 0 }, extension[FEXTLEN] = { 0 };
		_split(fcb->name.c_str(), name, extension);
		DirResult result = _findDirEntry(name, extension);

		CacheEntry cdirentr = cluster_cache.get(result.cj);
		DirEntry* entries = (DirEntry*)cdirentr.getBuffer();
		entries[result.k].size = fcb->size;
		cdirentr.markDirty();
		cdirentr.unlock();

		// notify that all files are closed
		if (opened_files.size() == 0)
			cvOpenFiles.notify_all();

		delete fcb;
	}
}

KernelFile* KernelFS::_newOperation(char mode, std::string name, ClusterNo index, BytesCnt size, std::unique_lock<std::mutex>&  lck)
{
	FCB* fcb;
	if (opened_files.find(name) != opened_files.end())
		fcb = opened_files[name];
	else {
		fcb = new FCB(name, index, size);
		opened_files[name] = fcb;
	}

	fcb->waiting++;
	while ((mode == 'r' && fcb->exclusive) || (mode != 'r' && (fcb->exclusive || fcb->users > 0)))
		(fcb->cvOpen).wait(lck);
	fcb->waiting--;

	if (mode == 'r')
		fcb->users++;
	else
		fcb->exclusive = true;

	KernelFile* kfile = new KernelFile(mode, fcb);
	return kfile;
}

bool KernelFS::_split(const char* fname, char name[FNAMELEN], char ext[FEXTLEN])
{
	int len = strlen(fname);

	// shortest '/a.b'; longest '/abcdefgh.ijk'
	if (len < 4 || len > 2+ FNAMELEN + FEXTLEN)
		return false;

	//remove /
	fname++;
	len--;

	memset(name, '\0', FNAMELEN);
	memset(ext, '\0', FEXTLEN);

	char *dot = (char*)memchr(fname, '.', len);
	if (dot == nullptr)
		return false;


	int namelen = dot-fname, extlen = len-namelen-1;
	if (namelen > FNAMELEN || namelen == 0 || extlen > FEXTLEN || extlen == 0)
		return false;

	strncpy(name, fname, namelen);
	strncpy(ext, fname + namelen + 1, extlen);

	return true;
}

DirResult KernelFS::_createFile(char name[FNAMELEN], char extension[FEXTLEN], const DirResult& r_info, bool inPlace)
{
	DirResult r = r_info;
	bool noSpace = false, init0 = false, init1=false;
	if (!inPlace) {
		if (r.k == FileEntriesPerCluster - 1) {
			r.k = -1;
			r.j++;
			init1 = true;

			if (r.j == EntriesPerCluster) {
				r.j = -1;
				r.i++;
				init0 = true;

				if (r.i == EntriesPerCluster)
					noSpace = true;
			}
		}

		if (!noSpace && (r.i == -1 || init0)) {
			CacheEntry e0 = cluster_cache.get(root_index);
			ClusterNo* index0 = (ClusterNo*)e0.getBuffer();
			ClusterNo newCluster = bit_vector.findClaim();

			if (newCluster > 0) {
				if (r.i == -1) r.i = 0;
				index0[r.i] = newCluster;
				r.ci = newCluster;
				_clearCluster(newCluster);
				e0.markDirty();
			}
			else
				noSpace = true;

			e0.unlock();
		}

		if (!noSpace && (r.j == -1 || init1)) {
			CacheEntry e1 = cluster_cache.get(r.ci);
			ClusterNo* index1 = (ClusterNo*)e1.getBuffer();
			ClusterNo newCluster = bit_vector.findClaim();

			if (newCluster > 0) {
				if (r.j == -1) r.j = 0;
				index1[r.j] = newCluster;
				r.cj = newCluster;
				_clearCluster(newCluster);
				e1.markDirty();
			}
			else
				noSpace = true;

			e1.unlock();
		}
	}

	if (!noSpace) {
		if (!inPlace) {
			if (r.k == -1) r.k = 0;
			else r.k++;
		}

		CacheEntry entr = cluster_cache.get(r.cj);
		DirEntry* entries = (DirEntry*)entr.getBuffer();

		ClusterNo newCluster = bit_vector.findClaim();

		if (newCluster > 0) {
			//cout << name << endl;
			strncpy(entries[r.k].name, name, FNAMELEN);
			strncpy(entries[r.k].extension, extension, FEXTLEN);
			entries[r.k].size = 0;
			entries[r.k].index = newCluster;
			_clearCluster(newCluster);
			entr.markDirty();
		}
		else
			noSpace = true;
		entr.unlock();
	}

	r.success = !noSpace;
	return r;
}

void KernelFS::_clearCluster(ClusterNo cluster)
{
	CacheEntry e = cluster_cache.get(cluster);
	memset(e.getBuffer(), 0, ClusterSize);
	e.markDirty();
	e.unlock();
}

char KernelFS::_mount(Partition* partition, unique_lock<mutex>& lck)
{
	if (partition == nullptr)
		return 0;
	
	while (current_partition != nullptr)
		cvPartition.wait(lck);

	current_partition = partition;

	cluster_cache.clear();
	cluster_cache.setPartititon(current_partition);

	unsigned long cluster_num = current_partition->getNumOfClusters();
	bit_vector.init(cluster_num);
	if (!bit_vector.load(cluster_cache)) {
		partition = nullptr;
		return 0;
	}

	root_index = bit_vector.sizeInClusters();
	return 1;
}

char KernelFS::_unmount(std::unique_lock<std::mutex>& lck)
{
	if (current_partition == nullptr)
		return 0;

	// wait for all files to be closed
	while (opened_files.size() > 0) {
		//cout << "OPENED FILES " << opened_files.size() << endl;
		//for (auto p : opened_files)
		//	cout << p.first << endl;
		cvOpenFiles.wait(lck);
	}

	// store bit vector
	if (!bit_vector.store(cluster_cache))
		return 0;

	// flush cache
	if (!cluster_cache.flush())
		return 0;

	current_partition = nullptr;
	cvPartition.notify_all();
	return 1;
}

char KernelFS::_format(unique_lock<mutex>& lck)
{
	if (current_partition == nullptr)
		return 0;

	started_format = true;

	// wait for all files to be closed
	while (opened_files.size() > 0)
		cvOpenFiles.wait(lck);

	// clear bit vector
	bit_vector.freeAll();

	// claim bit_vector and dir index
	for (ClusterNo i = 0; i <= root_index; i++)
		bit_vector.claim(i);

	// store bit vector
	if (!bit_vector.store(cluster_cache)) {
		current_partition = nullptr;
		return 0;
	}
	
	// format dir index0 with 0
	_formatRoot();

	// flush cache
	cluster_cache.flush();
	started_format = false;
	return 1;
}

FileCnt KernelFS::_readRootDir()
{
	FileCnt cnt = 0;
	char name[FNAMELEN] = { 0 }, extension[FEXTLEN] = { 0 };
	
	DirResult result = _findDirEntry(name, extension);

	if (result.i == -1 || result.j == -1)
		return 0;

	return result.i*EntriesPerCluster*FileEntriesPerCluster + 
			result.j*FileEntriesPerCluster + result.k + 1;
}

char KernelFS::_doesExist(char* fname)
{
	char name[FNAMELEN] = { 0 }, extension[FEXTLEN] = { 0 };
	if (!_split(fname, name, extension))
		return 0;

	DirResult result = _findDirEntry(name, extension);
	return result.success;
}

File* KernelFS::_open(char* fname, char mode, std::unique_lock<std::mutex>& lck)
{
	if (fname == nullptr || started_format || (mode != 'r' && mode != 'w' && mode != 'a'))
		return nullptr;

	char name[FNAMELEN] = { 0 }, extension[FEXTLEN] = { 0 };
	if (!_split(fname, name, extension))
		return nullptr;

	string str_name = fname;

	DirResult result = _findDirEntry(name, extension);
	bool found = result.success;

	if ((mode == 'r' || mode == 'a') && !found)
		return nullptr;

	if (mode == 'w') {
		if (found) {
			while (opened_files.find(str_name) != opened_files.end())
				opened_files[str_name]->cvOpen.wait(lck);

			// if file was deleted while waiting for closure
			result = _findDirEntry(name, extension);
			found = result.success;

			if (found)
				_deallocateFileClusters(result.cj, result.k);
		}
			
		result = _createFile(name, extension, result, found); // override = found
		if (!result.success)
			return nullptr;
	}

	CacheEntry ce = cluster_cache.get(result.cj);
	DirEntry de = ((DirEntry*)ce.getBuffer())[result.k];
	ce.unlock();

	KernelFile* file = _newOperation(mode, str_name, de.index, de.size, lck);
	file->setKernelFS(this);
	File* f = new File();
	f->myImpl = file;

	return f;
}

char KernelFS::_deleteFile(char* fname)
{
	char name[FNAMELEN] = { 0 }, extension[FEXTLEN] = { 0 };
	if (!_split(fname, name, extension))
		return 0;

	DirResult result = _findDirEntry(name, extension);
	bool found = result.success;
	if (!found)
		return 0;

	char name0[FNAMELEN] = { 0 }, extension0[FEXTLEN] = { 0 };

	DirResult result_last = _findDirEntry(name0, extension0);

	_deallocateFileClusters(result.cj, result.k);
	if (!(result.i == result_last.i && result.j == result_last.j && result.k == result_last.k)) {
		CacheEntry ce_del = cluster_cache.get(result.cj);
		CacheEntry ce_last = cluster_cache.get(result_last.cj);

		DirEntry* entries_del = (DirEntry*)ce_del.getBuffer();
		DirEntry* entries_last = (DirEntry*)ce_last.getBuffer();

		memcpy(entries_del + result.k, entries_last + result_last.k, sizeof(DirEntry));

		ce_last.unlock();
		ce_del.markDirty();
		ce_del.unlock();
	}

	_removeEntry(result_last);
	return 1;
}

// PUBLIC INTERFACE

char KernelFS::mount(Partition* partition)
{
	unique_lock<mutex> lck(mtx);
	return _mount(partition, lck);
}

char KernelFS::unmount()
{
	unique_lock<mutex> lck(mtx);
	return _unmount(lck);
}

char KernelFS::format()
{
	unique_lock<mutex> lck(mtx);
	return _format(lck);
}

FileCnt KernelFS::readRootDir()
{
	unique_lock<mutex> lck(mtx);
	return _readRootDir();
}

char KernelFS::doesExist(char* fname)
{
	unique_lock<mutex> lck(mtx);
	return _doesExist(fname);
}

File* KernelFS::open(char* fname, char mode)
{
	unique_lock<mutex> lck(mtx);
	return _open(fname, mode, lck);
}

char KernelFS::deleteFile(char* fname)
{
	unique_lock<mutex> lck(mtx);
	return _deleteFile(fname);
}

void KernelFS::finishedOperation(FCB* fcb, char mode)
{
	unique_lock<mutex> lck(mtx);
	_finishedOperation(fcb, mode, lck);
}
