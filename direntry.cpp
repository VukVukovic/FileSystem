#include "direntry.h"
#include <iostream>
using namespace std;

bool DirEntry::equals(char name[FNAMELEN], char extension[FEXTLEN])
{
	/*
	cout << "COMPARING ";
	for (int i = 0; i < FNAMELEN; i++)
		cout << this->name[i];
	for (int i = 0; i < FEXTLEN; i++)
		cout << this->extension[i];
	cout << "-";
	for (int i = 0; i < FNAMELEN; i++)
		cout << name[i];
	for (int i = 0; i < FEXTLEN; i++)
		cout << extension[i];
	cout << endl; */
	return strncmp(this->name, name, FNAMELEN) == 0 
		&& strncmp(this->extension, extension, FEXTLEN) == 0;
}
