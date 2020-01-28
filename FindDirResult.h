#pragma once

#include "part.h"

struct DirResult {
	bool success;
	unsigned long i;
	ClusterNo ci;
	unsigned long j;
	ClusterNo cj;
	unsigned long k;
};