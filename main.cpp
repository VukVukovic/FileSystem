#include <iostream>
#include "part.h"
#include "fs.h"
#include "file.h"
#include "clustercache.h"

#include "kernelfs.h"

using namespace std;


int main() {
	char disk[] = "p1.ini";
	Partition partition(disk);


	FS::mount(&partition);
	FS::format();
	char naziv[20] = { 0 };
	for (int i = 0; i < 200; i++) {
		sprintf(naziv, "/f%d.txt", i);
		FS::open(naziv, 'w');
		cout << naziv << (FS::doesExist(naziv)==1) << endl;
	}

	/*
	for (int i = 0; i < 200; i++) {
		sprintf(naziv, "/f%d.txt", i);
		FS::open(naziv, 'w');
		cout << naziv << (FS::doesExist(naziv) == 1) << endl;
	}*/

	cout << FS::readRootDir() << endl;
	FS::unmount();
	return 0;
}