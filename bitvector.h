#pragma once

class ClusterCache;
#include <mutex>

class BitVector {
	std::mutex mtx;

	static const unsigned long BitsPerCluster;

	unsigned char* bit_vector = nullptr;
	unsigned long size = 0, cluster_num = 0;

	unsigned long _findFree() const; // returns 0 if full
	void _claim(unsigned long i);
	void _free(unsigned long i);

public:
	void claim(unsigned long i);
	void free(unsigned long i);
	unsigned long findClaim();
	
	unsigned long sizeInClusters() const { return cluster_num; }
	void freeAll();
	void init(unsigned long size);
	bool load(ClusterCache&);
	bool store(ClusterCache&);

	~BitVector();
};