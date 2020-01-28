#pragma once

#include "part.h"
#include "file.h"
#include <string>
#include <map>
#include <mutex>
#include <condition_variable>


class FCB {
	std::string name;
	ClusterNo index;
	BytesCnt size;

	bool exclusive = false;
	int users = 0, waiting=0;

	std::condition_variable cvOpen;

	friend class KernelFS;
public:
	FCB(std::string name, ClusterNo index, BytesCnt size);
	BytesCnt getSize() { return size; }
	void setSize(BytesCnt size) { this->size = size; }
	std::string getName() { return name; }
	ClusterNo getIndex() { return index; }
};