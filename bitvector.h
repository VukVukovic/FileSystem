#pragma once

class ClusterCache;

class BitVector {
	static const unsigned long BitsPerCluster;

	unsigned char* bit_vector = nullptr;
	unsigned long size = 0, cluster_num = 0;

public:
	void claim(unsigned long i);
	void free(unsigned long i);
	bool isFree(unsigned long i) const;
	unsigned long findClaim();
	unsigned long findFree() const; // returns 0 if full
	unsigned long sizeInClusters() const { return cluster_num; }

	void freeAll();
	void init(unsigned long size);

	bool load(ClusterCache&);
	bool store(ClusterCache&);

	~BitVector();
};