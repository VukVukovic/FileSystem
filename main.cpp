#include <iostream>
#include "part.h"
#include "fs.h"
#include "file.h"
#include "clustercache.h"

#include "kernelfs.h"
#include <thread>
#include <mutex>
using namespace std;

mutex printmutex;

void print(const string& str) {
	unique_lock<mutex> lck(printmutex);
	cout << str << endl;
}

using namespace std;

Partition* part;

void thr_func() {
	print("THREAD START");
	this_thread::sleep_for(chrono::microseconds(200));

	print("MOUNTING THREAD");
	FS::mount(part);
	print("MOUNTED THREAD");

	print("UNMOUNTING THREAD");
	FS::unmount();
	print("UNMOUNTED THREAD");
}

int main() {
	thread t(thr_func);

	
	char disk[] = "p1.ini";
	Partition partition(disk);
	part = &partition;

	print("MOUNTING MAIN");
	FS::mount(&partition);
	print("MOUNTED MAIN");

	FS::format();
	char naziv[] = "/pera.txt";
	char buffer[255];
	strcpy(buffer, "Ovo je moj prvi tekst fajla koji upisujem!");

	File * file = FS::open(naziv, 'w');
	print("WRITING ");
	file->write(43, buffer);
	delete file;
	
	file = FS::open(naziv, 'r');
	print("READING");
	delete file;

	print("STARTING TO SLEEP main");
	this_thread::sleep_for(chrono::microseconds(500));
	print("FINISHED SLEEPING main");

	print("UNMOUNTING main");
	FS::unmount(); 
	print("UNMOUNTED main");

	t.join();
	return 0;
}