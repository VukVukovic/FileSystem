#pragma once

#include "part.h"

struct FindDirResult {
	bool found;
	unsigned long i;
	ClusterNo ci;
	unsigned long j;
	ClusterNo cj;
	unsigned long k;
};