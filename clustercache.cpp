#include "clustercache.h"

#include <iostream>
using namespace std;

ClusterCache::ClusterCache(int size) : size(size) {
	for (int i = 0; i < size; i++)
		buffers.push_back(new char[ClusterSize]);
}

CacheEntry ClusterCache::get(ClusterNo cluster)
{
	char* retBuffer = nullptr;
	if (getCluster(cluster))
		retBuffer = map[cluster].first;
	return CacheEntry(this, retBuffer, cluster);
}

bool ClusterCache::getCluster(ClusterNo cluster) {
	if (map.find(cluster) != map.end()) {
		if (map[cluster].second != ref.end())
			ref.erase(map[cluster].second);

		map[cluster].second = ref.end();
		lockCnt[cluster]++;
	}
	else {
		if (map.size() == size) {
			ClusterNo last = ref.back();			
			if (!flushDirtyCluster(last))
				return false;
				
			ref.pop_back();
			buffers.push_back(map[last].first);
			map.erase(last);
		}

		char* my_buffer = buffers.front();
		buffers.pop_front();
		map[cluster] = { my_buffer, ref.end() };
		lockCnt[cluster]++;
		if (!partition->readCluster(cluster, my_buffer))
			return false;
	}

	return true;
}

bool ClusterCache::flushDirtyCluster(ClusterNo cluster)
{
	if (dirty.find(cluster) != dirty.end()) {
		if (!partition->writeCluster(cluster, map[cluster].first))
			return false;
		dirty.erase(cluster);
	}
	return true;
}

void ClusterCache::setPartititon(Partition* partition)
{
	this->partition = partition;
}

bool ClusterCache::flush()
{
	bool ret = true;
	for (ClusterNo cluster : ref)
		if (!flushDirtyCluster(cluster))
			ret = false;
	return ret;
}

void ClusterCache::clear()
{
	partition = nullptr;
	for (ClusterNo cluster : ref)
		buffers.push_back(map[cluster].first);
	ref.clear();
	map.clear();
	dirty.clear();
}

ClusterCache::~ClusterCache()
{
	clear();
	for (char* buffer : buffers)
		delete[] buffer;
}

void CacheEntry::markDirty()
{
	cache->dirty.insert(cluster);
}

void CacheEntry::unlock()
{
	if (--(cache->lockCnt[cluster]) == 0) {
		cache->lockCnt.erase(cluster);
		cache->ref.push_front(cluster);
		cache->map[cluster].second = cache->ref.begin();
	}
}
