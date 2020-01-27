#include "kernelfs.h"
#include "direntry.h"
#include "kernelfile.h"

#include <iostream>
using namespace std;

const unsigned long KernelFS::FileEntriesPerCluster =
					ClusterSize / sizeof(DirEntry);
const unsigned long KernelFS::EntriesPerCluster =
					ClusterSize / sizeof(ClusterNo);


void KernelFS::formatRoot()
{
	CacheEntry e = cluster_cache.get(root_index);
	memset(e.getBuffer(), 0, ClusterSize);
	e.markDirty();
	e.unlock();
}

bool KernelFS::findDirEntry(char name[FNAMELEN], char extension[FEXTLEN],
	unsigned long& i1, ClusterNo& ci, 
	unsigned long& j1, ClusterNo& cj, 
	unsigned long& k1)
{
	i1 = j1 = k1 = -1;
	CacheEntry e0 = cluster_cache.get(root_index);
	ClusterNo* index0 = (ClusterNo*)e0.getBuffer();
	bool found = false;

	for (unsigned long i = 0; i < EntriesPerCluster && index0[i] != 0; ++i) {
		i1 = i;
		ci = index0[i];
		CacheEntry e1 = cluster_cache.get(index0[i]);
		ClusterNo* index1 = (ClusterNo*)e1.getBuffer();

		for (unsigned long j = 0; j < EntriesPerCluster && index1[j] != 0; ++j) {
			j1 = j;
			cj = index1[j];
			CacheEntry entr = cluster_cache.get(index1[j]);
			DirEntry* entries = (DirEntry*)entr.getBuffer();
			
			for (unsigned long k = 0; k < FileEntriesPerCluster && !entries[k].empty(); ++k) {
				k1 = k;
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
	return found;
}

bool KernelFS::removeEntry(unsigned long i1, ClusterNo ci, unsigned long j1, ClusterNo cj, unsigned long k1)
{
	CacheEntry ce = cluster_cache.get(cj);
	DirEntry* entries = (DirEntry*)ce.getBuffer();
	memset(entries + k1, 0, sizeof(DirEntry));
	ce.markDirty();
	ce.unlock();

	if (k1 == 0) {
		CacheEntry ce1 = cluster_cache.get(ci);
		ClusterNo* index1 = (ClusterNo*)ce1.getBuffer();
		index1[j1] = 0;
		ce1.markDirty();
		ce1.unlock();
		bit_vector.free(cj);

		if (j1 == 0) {
			CacheEntry ce0 = cluster_cache.get(root_index);
			ClusterNo* index0 = (ClusterNo*)ce0.getBuffer();
			index0[i1] = 0;
			ce0.markDirty();
			ce0.unlock();
			bit_vector.free(ci);
		}
	}
	return true;
}

bool KernelFS::deallocate(ClusterNo cj, unsigned long k1)
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

bool KernelFS::split(const char* fname, char name[FNAMELEN], char ext[FEXTLEN])
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

bool KernelFS::createFile(char name[FNAMELEN], char extension[FEXTLEN], 
	unsigned long& i1, ClusterNo& ci, unsigned long& j1, ClusterNo& cj, 
	unsigned long& k1, bool inPlace)
{
	bool noSpace = false, init0 = false, init1=false;
	if (!inPlace) {
		if (k1 == FileEntriesPerCluster - 1) {
			k1 = -1;
			j1++;
			init1 = true;

			if (j1 == EntriesPerCluster) {
				j1 = -1;
				i1++;
				init0 = true;

				if (i1 == EntriesPerCluster)
					noSpace = true;
			}
		}

		if (!noSpace && (i1 == -1 || init0)) {
			CacheEntry e0 = cluster_cache.get(root_index);
			ClusterNo* index0 = (ClusterNo*)e0.getBuffer();
			ClusterNo newCluster = bit_vector.findClaim();

			if (newCluster > 0) {
				if (i1 == -1) i1 = 0;
				index0[i1] = newCluster;
				ci = newCluster;
				clearCluster(newCluster);
				e0.markDirty();
			}
			else
				noSpace = true;

			e0.unlock();
		}

		if (!noSpace && (j1 == -1 || init1)) {
			CacheEntry e1 = cluster_cache.get(ci);
			ClusterNo* index1 = (ClusterNo*)e1.getBuffer();
			ClusterNo newCluster = bit_vector.findClaim();

			if (newCluster > 0) {
				if (j1 == -1) j1 = 0;
				index1[j1] = newCluster;
				cj = newCluster;
				clearCluster(newCluster);
				e1.markDirty();
			}
			else
				noSpace = true;

			e1.unlock();
		}
	}

	if (!noSpace) {
		if (!inPlace) {
			if (k1 == -1) k1 = 0;
			else k1++;
		}

		CacheEntry entr = cluster_cache.get(cj);
		DirEntry* entries = (DirEntry*)entr.getBuffer();

		ClusterNo newCluster = bit_vector.findClaim();

		if (newCluster > 0) {
			//cout << name << endl;
			strncpy(entries[k1].name, name, FNAMELEN);
			strncpy(entries[k1].extension, extension, FEXTLEN);
			entries[k1].size = 0;
			entries[k1].index = newCluster;
			clearCluster(newCluster);
			entr.markDirty();
		}
		else
			noSpace = true;
		entr.unlock();
	}

	return !noSpace;
}

void KernelFS::clearCluster(ClusterNo cluster)
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
	FCB::setKernelFS(this);
	return 1;
}

char KernelFS::_unmount()
{
	if (current_partition == nullptr)
		return 0;

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

char KernelFS::_format()
{
	if (current_partition == nullptr)
		return 0;

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
	formatRoot();

	// flush cache
	cluster_cache.flush();
	return 1;
}

FileCnt KernelFS::_readRootDir()
{
	FileCnt cnt = 0;
	char name[FNAMELEN] = { 0 }, extension[FEXTLEN] = { 0 };
	unsigned long i1, _, j1, __, k1;
	findDirEntry(name, extension, i1, _, j1, __, k1);

	if (i1 == -1 || j1 == -1)
		return 0;

	return i1*EntriesPerCluster*FileEntriesPerCluster + j1*FileEntriesPerCluster + k1 + 1;
}

char KernelFS::_doesExist(char* fname)
{
	char name[FNAMELEN] = { 0 }, extension[FEXTLEN] = { 0 };
	if (!split(fname, name, extension)) {
		return 0;
	}
	 
	unsigned long _1,_2,_3,_4,_5;
	bool found = findDirEntry(name, extension, _1, _2, _3, _4, _5);
	cout << " (" << _2 << " " << _4 << ") " << endl;
	return found;
}

File* KernelFS::_open(char* fname, char mode)
{
	if (fname == nullptr || (mode != 'r' && mode != 'w' && mode != 'a'))
		return nullptr;

	char name[FNAMELEN] = { 0 }, extension[FEXTLEN] = { 0 };
	if (!split(fname, name, extension))
		return nullptr;

	string str_name = fname;

	unsigned long int i1, j1, k1;
	ClusterNo ci, cj;
	bool found = findDirEntry(name, extension, i1, ci, j1, cj, k1);

	if ((mode == 'r' || mode == 'a') && !found)
		return nullptr;

	if (mode == 'w') {
		if (found)
			deallocate(cj, k1); // ako je otvoren, blokiraj se?

		bool created = createFile(name, extension, i1, ci, j1, cj, k1, found); // override = found
		if (!created)
			return nullptr;
	}

	CacheEntry ce = cluster_cache.get(cj);
	DirEntry de = ((DirEntry*)ce.getBuffer())[k1];
	ce.unlock();

	KernelFile* file = FCB::newOperation(mode, str_name, de.index, de.size);
	file->setKernelFS(this);
	File* f = new File();
	f->myImpl = file;

	return f;
}

char KernelFS::_deleteFile(char* fname)
{
	char name[FNAMELEN] = { 0 }, extension[FEXTLEN] = { 0 };
	if (!split(fname, name, extension))
		return 0;

	unsigned long int i1, j1, k1;
	ClusterNo ci, cj;
	bool found = findDirEntry(name, extension, i1, ci, j1, cj, k1);
	if (!found)
		return 0;

	char name0[FNAMELEN] = { 0 }, extension0[FEXTLEN] = { 0 };
	unsigned long int li1, lj1, lk1;
	ClusterNo lci, lcj;
	bool last_found = findDirEntry(name0, extension0, li1, lci, lj1, lcj, lk1);

	deallocate(cj, k1);

	if (!(i1 == li1 && j1 == lj1 && lk1 == k1)) {
		CacheEntry ce_del = cluster_cache.get(cj);
		CacheEntry ce_last = cluster_cache.get(lcj);

		DirEntry* entries_del = (DirEntry*)ce_del.getBuffer();
		DirEntry* entries_last = (DirEntry*)ce_last.getBuffer();

		memcpy(entries_del + k1, entries_last + lk1, sizeof(DirEntry));

		ce_last.unlock();
		ce_del.markDirty();
		ce_del.unlock();
	}

	removeEntry(li1, lci, lj1, lcj, lk1);
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
	return _unmount();
}

char KernelFS::format()
{
	unique_lock<mutex> lck(mtx);
	return _format();
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
	return _open(fname, mode);
}

char KernelFS::deleteFile(char* fname)
{
	unique_lock<mutex> lck(mtx);
	return _deleteFile(fname);
}
