#pragma once

#include <list>
#include <map>
#include <set>
#include "part.h"

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

	bool getCluster(ClusterNo cluster);
	bool flushDirtyCluster(ClusterNo cluster);

public:

	ClusterCache(int size);
	CacheEntry get(ClusterNo cluster);
	void setPartititon(Partition* partition);
	bool flush();
	void clear();
	~ClusterCache();

	int getUnlocked() { return ref.size(); }

	friend class CacheEntry;
};