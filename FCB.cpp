#include "FCB.h"
#include <string>
using namespace std;

FCB::FCB(string name, ClusterNo index, BytesCnt size) 
	: name(name), index(index), size(size)
{
}

