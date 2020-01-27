#pragma once

#include "fs.h"
#include "part.h"
#include "file.h"
#include <string>

struct DirEntry {
	char name[FNAMELEN] = { 0 };
	char extension[FEXTLEN] = { 0 };
	char _ = 0;
	ClusterNo index = 0;
	BytesCnt size = 0;
	char __[12] = { 0 };

	bool empty() { return name[0] == 0; }
	bool equals(char name[FNAMELEN], char extension[FEXTLEN]);

	std::string getStrName() {
		std::string strName = "/";
		for (int i = 0; i < FNAMELEN && name[i] != '\0'; i++) 
			strName += name[i];
		strName += '.';
		for (int i = 0; i < FEXTLEN && extension[i] != '\0'; i++) 
			strName += extension[i];
		return strName;
	}
};