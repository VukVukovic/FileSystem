#include "bitvector.h"
#include <string.h>
#include "part.h"
#include "clustercache.h"

#include <iostream>

const unsigned long BitVector::BitsPerCluster = ClusterSize * 8;

void BitVector::claim(unsigned long i)
{
	if (i >= size) {
		return;
	}

	unsigned long j = i / 8;
	unsigned long k = i % 8;
	bit_vector[j] &= ~(1U << k);
}

void BitVector::free(unsigned long i)
{
	if (i >= size) {
		return;
	}

	unsigned long j = i / 8;
	unsigned long k = i % 8;
	bit_vector[j] |= (1U << k);
}

bool BitVector::isFree(unsigned long i) const
{
	if (i >= size) {
		return false;
	}

	unsigned long j = i / 8;
	unsigned long k = i % 8;
	return (bit_vector[j] & (1U << k)) != 0;
}

unsigned long BitVector::findClaim()
{
	ClusterNo newCluster = findFree();
	claim(newCluster);
	return newCluster;
}

unsigned long BitVector::findFree() const
{
	for (unsigned long i = 0; i < ClusterSize * cluster_num; i++) {
		if (bit_vector[i] != 0) {
			unsigned long free = i * 8;
			unsigned char byte = bit_vector[i];
			while ((byte & 1) == 0) {
				byte >>= 1;
				free++;
			}
			if (free >= size)
				return 0;
			return free;
		}
	}
	return 0;
}

void BitVector::freeAll()
{
	memset(bit_vector, 0xFF, ClusterSize * cluster_num);
}

void BitVector::init(unsigned long _size)
{
	size = _size;
	int new_cluster_num = _size / BitsPerCluster + (_size % BitsPerCluster != 0);

	if (bit_vector != nullptr && new_cluster_num != cluster_num) {
		delete[] bit_vector;
		bit_vector = nullptr;
	}

	cluster_num = new_cluster_num;

	if (bit_vector == nullptr) {
		bit_vector = new unsigned char[cluster_num * ClusterSize];
	}
}

bool BitVector::load(ClusterCache& cc)
{
	for (ClusterNo i = 0; i < cluster_num; i++) {
		CacheEntry e = cc.get(i);
		if (e.getBuffer() == nullptr) {
			return false;
		}
		memcpy(bit_vector + i * ClusterSize, e.getBuffer(), ClusterSize);
		e.unlock();
	}
	return true;
}

bool BitVector::store(ClusterCache& cc)
{
	for (ClusterNo i = 0; i < cluster_num; i++) {
		CacheEntry e = cc.get(i);
		if (e.getBuffer() == nullptr) {
			return false;
		}
		memcpy(e.getBuffer(), bit_vector + i * ClusterSize, ClusterSize);
		e.markDirty();
		e.unlock();
	}
	return true;
}

BitVector::~BitVector()
{
	if (bit_vector != nullptr) {
		delete[] bit_vector;
		bit_vector = nullptr;
	}
}
