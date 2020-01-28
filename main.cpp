#include <iostream>
#include "part.h"
#include "fs.h"
#include "file.h"

using namespace std;

int main() {
	char disk[] = "p1.ini";


	Partition partition(disk);
	FS::mount(&partition);

	FS::format();

	char naziv[20];
	for (int i = 0; i < 50; i++) {
		sprintf(naziv, "/f%d.txt", i);
		File *f = FS::open(naziv, 'w');
		cout << "KREIRAN " << naziv << endl;
		delete f;
		cout << "ZATVOREN " << naziv << endl;
	}
	
	FS::unmount();
	return 0;
}