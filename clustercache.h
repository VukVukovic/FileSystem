#pragma once

#include <list>
#include <map>
#include <set>
#include "part.h"
#include <mutex>

class ClusterCache;

class CacheEntry {
	ClusterCache* cache;
	char* buffer;
	ClusterNo cluster;

public:
	CacheEntry(ClusterCache* cache, char* buffer, ClusterNo cluster) :
		cache(cache), buffer(buffer), cluster(cluster) {}
	void markDirty();
	char* getBuffer() { return buffer; }
	void unlock();
};

class ClusterCache {
	int size;

	std::list<ClusterNo> ref;
	std::map<ClusterNo, std::pair<char *, std::list<ClusterNo>::iterator>> map;
	std::map<ClusterNo, int> lockCnt;
	std::set<ClusterNo> dirty;
	std::list<char*> buffers;
	Partition* partition;
	std::mutex mtx;

	bool _getCluster(ClusterNo cluster);
	bool _flushDirtyCluster(ClusterNo cluster);

public:

	ClusterCache(int size);
	CacheEntry get(ClusterNo cluster);
	void setPartititon(Partition* partition);
	bool flush();
	void clear();
	~ClusterCache();

	friend class CacheEntry;
};