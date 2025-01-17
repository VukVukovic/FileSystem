#include "bitvector.h"
#include <string.h>
#include "part.h"
#include "clustercache.h"

#include <iostream>
using namespace std;

const unsigned long BitVector::BitsPerCluster = ClusterSize * 8;

void BitVector::_claim(unsigned long i)
{
	unsigned long j = i / 8;
	unsigned long k = i % 8;
	bit_vector[j] &= ~(1U << k);
}

void BitVector::_free(unsigned long i)
{
	unsigned long j = i / 8;
	unsigned long k = i % 8;
	bit_vector[j] |= (1U << k);
}

void BitVector::claim(unsigned long i)
{
	if (i >= size)
		return;

	unique_lock<mutex> lck(mtx);
	_claim(i);
}

void BitVector::free(unsigned long i)
{
	if (i >= size)
		return;

	unique_lock<mutex> lck(mtx);
	_free(i);
}

unsigned long BitVector::_findFree() const
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

unsigned long BitVector::findClaim()
{
	unique_lock<mutex> lck(mtx);

	unsigned long newCluster = _findFree();
	if (newCluster == 0)
		return 0;

	unsigned long j = newCluster / 8;
	unsigned long k = newCluster% 8;
	bit_vector[j] &= ~(1U << k);
	return newCluster;
}

void BitVector::freeAll()
{
	unique_lock<mutex> lck(mtx);
	memset(bit_vector, 0xFF, ClusterSize * cluster_num);
}

void BitVector::init(unsigned long _size)
{
	unique_lock<mutex> lck(mtx);
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
	unique_lock<mutex> lck(mtx);
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
	unique_lock<mutex> lck(mtx);
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
