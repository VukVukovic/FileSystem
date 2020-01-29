#include "kernelfile.h"
#include "clustercache.h"
#include "kernelfs.h"
#include <vector>
#include <iostream>
using namespace std;

const unsigned long KernelFile::ClustersPerLevel0Entry = ClusterSize / sizeof(ClusterNo);
const unsigned long KernelFile::EntriesPerCluster = ClusterSize / sizeof(ClusterNo);
const BytesCnt KernelFile::BytesPerLevel1Entry = ClusterSize;
const BytesCnt KernelFile::BytesPerLevel0Entry = BytesPerLevel1Entry * EntriesPerCluster;
const BytesCnt KernelFile::MaxFileSize = BytesPerLevel0Entry * EntriesPerCluster;


bool KernelFile::_allocate(unsigned long to_alloc, unsigned long level1_to_alloc)
{
	vector<ClusterNo> taken;
	for (int i = 0; i < to_alloc + level1_to_alloc; i++) {
		ClusterNo newCluster = kernelFS->bit_vector.findClaim();
		taken.push_back(newCluster);
		if (newCluster == 0) 
			break;
	}

	if (taken.size() != to_alloc + level1_to_alloc) {
		for (ClusterNo c : taken)
			kernelFS->bit_vector.free(c);
		return false;
	}


	unsigned long current_clusters = fcb->getSize() / BytesPerLevel1Entry + (fcb->getSize() % BytesPerLevel1Entry > 0 ? 1 : 0);
	unsigned long from0 = current_clusters / ClustersPerLevel0Entry;
	unsigned long to0 = (current_clusters + to_alloc) / ClustersPerLevel0Entry + 
		((current_clusters + to_alloc) % ClustersPerLevel0Entry > 0 ? 1 : 0);

	CacheEntry ce0 = kernelFS->cluster_cache.get(fcb->getIndex());
	ClusterNo* index0 = (ClusterNo*)ce0.getBuffer();
	bool dirty0 = false;

	for (unsigned long i = from0; i < to0; i++) {
		if (index0[i] == 0) {
			dirty0 = true;
			index0[i] = taken.back();
			taken.pop_back();
			kernelFS->_clearCluster(index0[i]);
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
			kernelFS->_clearCluster(index1[j]);
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

bool KernelFile::_deallocate(unsigned long to_dealloc, BytesCnt new_size)
{
	unsigned long new_clusters = new_size / BytesPerLevel1Entry + 
		(new_size % BytesPerLevel1Entry > 0 ? 1 : 0);
	unsigned long from0 = new_clusters / ClustersPerLevel0Entry;
	unsigned long to0 = (new_clusters + to_dealloc) / ClustersPerLevel0Entry +
		((new_clusters + to_dealloc) % ClustersPerLevel0Entry > 0 ? 1:0);

	CacheEntry ce0 = kernelFS->cluster_cache.get(fcb->getIndex());
	ClusterNo* index0 = (ClusterNo*)ce0.getBuffer();
	bool dirty0 = false;

	for (unsigned long i = from0; i < to0; i++) {
		unsigned long from1 = new_clusters - i * ClustersPerLevel0Entry;
		unsigned long to1 = from1 + to_dealloc;
		if (to1 > ClustersPerLevel0Entry)
			to1 = ClustersPerLevel0Entry;

		CacheEntry ce1 = kernelFS->cluster_cache.get(index0[i]);
		ClusterNo* index1 = (ClusterNo*)ce1.getBuffer();
		for (unsigned long j = from1; j < to1; j++) {
			kernelFS->bit_vector.free(index1[j]);
			index1[j] = 0;
			to_dealloc--;
			new_clusters++;
			from0++;
		}
		ce1.markDirty();
		ce1.unlock();

		if (from1 == 0) {
			kernelFS->bit_vector.free(index0[i]);
			index0[i] = 0;
			dirty0 = true;
		}
	}

	if (dirty0)
		ce0.markDirty();
	ce0.unlock();

	return true;
}

char KernelFile::_write(BytesCnt cnt, char* buffer)
{
	BytesCnt new_size = pos + cnt;
	if (new_size > MaxFileSize)
		return 0;

	unsigned long allocated_clusters = fcb->getSize() / BytesPerLevel1Entry + (fcb->getSize() % BytesPerLevel1Entry > 0 ? 1 : 0);
	unsigned long needed_clusters = new_size / BytesPerLevel1Entry + (new_size % BytesPerLevel1Entry > 0 ? 1 : 0);
	
	unsigned long needed_level1 = needed_clusters / ClustersPerLevel0Entry + (needed_clusters % ClustersPerLevel0Entry > 0 ? 1 : 0) - 
		(allocated_clusters / ClustersPerLevel0Entry + (allocated_clusters % ClustersPerLevel0Entry > 0 ? 1 : 0));

	if (needed_clusters > allocated_clusters) {
		if (!_allocate(needed_clusters - allocated_clusters, needed_level1))
			return 0;
	}

	fcb->setSize(new_size);

	BytesCnt to_write = cnt;
	BytesCnt succ_written = 0;

	CacheEntry ce0 = (kernelFS->cluster_cache).get(fcb->getIndex());
	ClusterNo* index0 = (ClusterNo*)ce0.getBuffer();

	unsigned long from0 = pos / BytesPerLevel0Entry;
	unsigned long to0 = (pos + to_write) / BytesPerLevel0Entry + ((pos + to_write) % BytesPerLevel0Entry > 0 ? 1 : 0);

	for (unsigned long i = from0; i < to0; i++) {
		CacheEntry ce1 = (kernelFS->cluster_cache).get(index0[i]);
		ClusterNo* index1 = (ClusterNo*)ce1.getBuffer();

		unsigned long pos_rel = pos - i * BytesPerLevel0Entry;
		unsigned long to_write_rel = to_write;
		if (pos_rel + to_write_rel > BytesPerLevel0Entry)
			to_write_rel = BytesPerLevel0Entry - pos_rel;

		unsigned long from1 = pos_rel / BytesPerLevel1Entry;
		unsigned long to1 = (pos_rel + to_write_rel) / BytesPerLevel1Entry + ((pos_rel + to_write_rel) % BytesPerLevel1Entry > 0 ? 1 : 0);

		for (unsigned long j = from1; j < to1; j++) {
			unsigned long start = pos_rel - j * BytesPerLevel1Entry;
			unsigned long size = to_write_rel;
			if (start + size > BytesPerLevel1Entry)
				size = BytesPerLevel1Entry - start;

			CacheEntry cc = (kernelFS->cluster_cache).get(index1[j]);
			char* data = cc.getBuffer();
			memcpy(data + start, buffer + succ_written, size);
			succ_written += size;
			to_write -= size;
			pos += size;
			pos_rel += size;
			cc.markDirty();
			cc.unlock();
		}
		ce1.unlock();
	}

	ce0.unlock();

	return 1;
}

BytesCnt KernelFile::_read(BytesCnt cnt, char* buffer)
{
	BytesCnt toRead = cnt;

	BytesCnt succRead = 0;
	CacheEntry ce0 = (kernelFS->cluster_cache).get(fcb->getIndex());
	ClusterNo* index0 = (ClusterNo*)ce0.getBuffer();

	unsigned long from0 = pos / BytesPerLevel0Entry;
	unsigned long to0 = (pos + toRead) / BytesPerLevel0Entry + ((pos + toRead) % BytesPerLevel0Entry > 0 ? 1 : 0);

	for (unsigned long i = from0; i < to0; i++) {
		CacheEntry ce1 = (kernelFS->cluster_cache).get(index0[i]);
		ClusterNo* index1 = (ClusterNo*)ce1.getBuffer();

		unsigned long pos_rel = pos - i * BytesPerLevel0Entry;
		unsigned long toRead_rel = toRead;
		if (pos_rel + toRead_rel > BytesPerLevel0Entry)
			toRead_rel = BytesPerLevel0Entry - pos_rel;

		unsigned long from1 = pos_rel / BytesPerLevel1Entry;
		unsigned long to1 = (pos_rel + toRead_rel) / BytesPerLevel1Entry + ((pos_rel + toRead_rel) % BytesPerLevel1Entry > 0 ? 1 : 0);

		for (unsigned long j = from1; j < to1; j++) {
			unsigned long start = pos_rel - j * BytesPerLevel1Entry;
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

void KernelFile::setKernelFS(KernelFS* kfs)
{
	kernelFS = kfs;
}

char KernelFile::write(BytesCnt cnt, char* buffer)
{
	if (mode == 'r')
		return 0;

	//unsigned long allocated = fcb->getSize() / BytesPerLevel1Entry + 
	//	(fcb->getSize() % BytesPerLevel1Entry > 0 ? 1 : 0);

	// invalid read buffer if appending to same area
	if (mode == 'a' && rd_buff_size > 0) {
		if ((pos >= rd_pos && pos < rd_pos+rd_buff_size) ||
			(pos+cnt >= rd_pos && pos+cnt < rd_pos+rd_buff_size))
			rd_buff_size = 0;
	}

	return _write(cnt, buffer);
}

BytesCnt KernelFile::read(BytesCnt cnt, char* buffer)
{
	if (mode == 'w' || eof())
		return 0;

	if (pos + cnt > fcb->getSize())
		cnt = fcb->getSize() - pos;

	if (pos >= rd_pos && cnt <= rd_buff_size - (pos - rd_pos)) {
		memcpy(buffer, rd_buffer + (pos - rd_pos), cnt);
		pos += cnt;
		return cnt;
	}

	if (cnt <= ClusterSize) {
		rd_pos = pos;
		rd_buff_size = _read(ClusterSize, rd_buffer);
		memcpy(buffer, rd_buffer, cnt);
		pos = rd_pos + cnt; // changed pos, fix
		return cnt;
	}

	return _read(cnt, buffer);
}



char KernelFile::seek(BytesCnt pos)
{
	if (pos >= fcb->getSize())
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
	return (pos == fcb->getSize()) ? 2 : 0;
}

BytesCnt KernelFile::getFileSize()
{
	return fcb->getSize();
}

char KernelFile::truncate()
{
	if (eof())
		return 0;

	BytesCnt new_size = pos;

	unsigned long allocated_clusters = fcb->getSize() / BytesPerLevel1Entry + (fcb->getSize() % BytesPerLevel1Entry > 0 ? 1 : 0);
	unsigned long needed_clusters = new_size / BytesPerLevel1Entry + (new_size % BytesPerLevel1Entry > 0 ? 1 : 0);

	if (allocated_clusters > needed_clusters)
		_deallocate(allocated_clusters - needed_clusters, new_size);

	fcb->setSize(new_size);
	return 1;
}

KernelFile::~KernelFile()
{
	//cout << "KERNEL FILE DEST " << endl;
	kernelFS->finishedOperation(fcb, mode);
	fcb = nullptr;
}
