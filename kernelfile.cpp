#include "kernelfile.h"
#include "clustercache.h"
#include "kernelfs.h"
#include <vector>
using namespace std;

const unsigned long KernelFile::ClustersPerLevel0Entry = ClusterSize / sizeof(ClusterNo);
const unsigned long KernelFile::EntriesPerCluster = ClusterSize / sizeof(ClusterNo);
const BytesCnt KernelFile::BytesPerLevel1Entry = ClusterSize;
const BytesCnt KernelFile::BytesPerLevel0Entry = BytesPerLevel1Entry * EntriesPerCluster;
const BytesCnt KernelFile::MaxFileSize = BytesPerLevel0Entry * EntriesPerCluster;


bool KernelFile::allocate(unsigned long to_alloc)
{
	vector<ClusterNo> taken;
	for (unsigned long i = 0; i < to_alloc; i++) {
		ClusterNo new_cluster = kernelFS->bit_vector.findClaim();
		if (new_cluster == 0)
			break;
		taken.push_back(new_cluster);
	}

	if (taken.size() != to_alloc) {
		for (ClusterNo c : taken)
			kernelFS->bit_vector.free(c);
		return false;
	}

	unsigned long current_clusters = fcb->size / BytesPerLevel1Entry + (fcb->size % BytesPerLevel1Entry > 0) ? 1 : 0;
	unsigned long from0 = current_clusters / ClustersPerLevel0Entry;
	unsigned long to0 = (current_clusters + to_alloc) / ClustersPerLevel0Entry + 
		(current_clusters + to_alloc) % ClustersPerLevel0Entry;

	CacheEntry ce0 = kernelFS->cluster_cache.get(fcb->index);
	ClusterNo* index0 = (ClusterNo*)ce0.getBuffer();
	bool dirty0 = false;

	for (unsigned long i = from0; i < to0; i++) {
		if (index0[i] == 0) {
			dirty0 = true;
			index0[i] = taken.back();
			taken.pop_back();
			kernelFS->clearCluster(index0[i]);
		}

		unsigned long from1 = current_clusters - i * ClustersPerLevel0Entry;
		unsigned long to1 = from1 + to_alloc;
		if (to1 > ClustersPerLevel0Entry)
			to1 = ClustersPerLevel0Entry;

		CacheEntry ce1 = kernelFS->cluster_cache.get(index0[i]);
		ClusterNo* index1 = (ClusterNo*)ce1.getBuffer();
		for (unsigned long j = from1; j < to1; j++) {
			index1[j] = taken.back();
			taken.pop_back();
			kernelFS->clearCluster(index1[j]);
			to_alloc--;
			current_clusters++;
			from0++;
		}
		ce1.markDirty();
		ce1.unlock();
	}

	if (dirty0)
		ce0.markDirty();
	ce0.unlock();

	return true;
}

bool KernelFile::deallocate(unsigned long clusterNum)
{
	return true;
}

void KernelFile::setKernelFS(KernelFS* kfs)
{
	kernelFS = kfs;
}

char KernelFile::write(BytesCnt cnt, char* buffer)
{
	BytesCnt new_size = pos + cnt;
	if (new_size > MaxFileSize)
		return 0;

	unsigned long allocated_clusters = fcb->size / BytesPerLevel1Entry + (fcb->size % BytesPerLevel1Entry > 0)?1:0;
	unsigned long needed_clusters = new_size / BytesPerLevel1Entry + (new_size % BytesPerLevel1Entry > 0) ? 1 : 0;

	if (needed_clusters > allocated_clusters) {
		if (!allocate(needed_clusters - allocated_clusters))
			return false;
	}

	fcb->size = new_size;
	// kao i read...
	return 1;
}

BytesCnt KernelFile::read(BytesCnt cnt, char* buffer)
{
	if (eof())
		return 0;

	BytesCnt toRead = cnt;
	if (pos + toRead > fcb->size)
		toRead = fcb->size - pos;

	BytesCnt succRead = 0;
	CacheEntry ce0 = (kernelFS->cluster_cache).get(fcb->index);
	ClusterNo* index0 = (ClusterNo*)ce0.getBuffer();

	unsigned long from0 = pos / BytesPerLevel0Entry;
	unsigned long to0 = (pos + toRead) / BytesPerLevel0Entry + ((pos+toRead) %BytesPerLevel0Entry>0)?1:0;

	for (unsigned long i = from0; i < to0; i++) {
		CacheEntry ce1 = (kernelFS->cluster_cache).get(index0[i]);
		ClusterNo* index1 = (ClusterNo*)ce1.getBuffer();

		unsigned long pos_rel = pos - i * BytesPerLevel0Entry;
		unsigned long toRead_rel = toRead;
		if (pos_rel + toRead_rel > BytesPerLevel0Entry)
			toRead_rel = BytesPerLevel0Entry - pos_rel;

		unsigned long from1 = pos_rel / BytesPerLevel1Entry;
		unsigned long to1 = (pos_rel+toRead_rel) / BytesPerLevel1Entry + ((pos_rel + toRead_rel) % BytesPerLevel1Entry > 0) ? 1 : 0;

		for (unsigned long j = from1; j < to1; j++) {
			unsigned long start = pos_rel - j*BytesPerLevel1Entry;
			unsigned long size = toRead;
			if (start + size > BytesPerLevel1Entry)
				size = BytesPerLevel1Entry - start;

			CacheEntry cc = (kernelFS->cluster_cache).get(index1[j]);
			char* data = cc.getBuffer();
			memcpy(buffer + succRead, data + start, size);
			succRead += size;
			toRead -= size;
			pos += size;
			pos_rel += size;
			cc.unlock();
		}
		ce1.unlock();
	}

	ce0.unlock();
	return succRead;
}

char KernelFile::seek(BytesCnt pos)
{
	if (pos >= fcb->size)
		return 0;

	this->pos = pos;
	return 1;
}

BytesCnt KernelFile::filePos()
{
	return pos;
}

char KernelFile::eof()
{
	return (pos == fcb->size) ? 2 : 0;
}

BytesCnt KernelFile::getFileSize()
{
	return fcb->size;
}

char KernelFile::truncate()
{
	if (eof())
		return 0;

	BytesCnt new_size = pos;

	unsigned long allocated_clusters = fcb->size / BytesPerLevel1Entry + (fcb->size % BytesPerLevel1Entry > 0) ? 1 : 0;
	unsigned long needed_clusters = new_size / BytesPerLevel1Entry + (new_size % BytesPerLevel1Entry > 0) ? 1 : 0;

	if (allocated_clusters > needed_clusters)
		deallocate(allocated_clusters - needed_clusters);

	fcb->size = new_size;
	return 1;
}

KernelFile::~KernelFile()
{
	FCB::finishedOperation(mode, fcb);
	fcb = nullptr;
}