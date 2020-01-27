#include "file.h"
#include "kernelfile.h"

File::~File()
{
	delete myImpl;
	myImpl = nullptr;
}

char File::write(BytesCnt bytes, char* buffer)
{
	return myImpl->write(bytes, buffer);
}

BytesCnt File::read(BytesCnt bytes, char* buffer)
{
	return myImpl->read(bytes, buffer);
}

char File::seek(BytesCnt cnt)
{
	return myImpl->seek(cnt);
}

BytesCnt File::filePos()
{
	return myImpl->filePos();
}

char File::eof()
{
	return myImpl->eof();
}

BytesCnt File::getFileSize()
{
	return myImpl->getFileSize();
}

char File::truncate()
{
	return myImpl->truncate();
}

File::File()
{
}