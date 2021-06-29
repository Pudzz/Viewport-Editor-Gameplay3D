#include "Mutex.h"

Mutex::Mutex(LPCWSTR mutexName)
{
	this->mutexHandle = CreateMutex(nullptr, false, mutexName);
	if (mutexHandle == NULL)
		std::cout << "Failed to create mutex object\n";
	if (GetLastError() == ERROR_ALREADY_EXISTS)
		std::cout << "Mutex already exists - It's shared\n";
}

Mutex::~Mutex()
{
}

void Mutex::Lock()
{
	WaitForSingleObject(this->mutexHandle, INFINITE);
}

void Mutex::Unlock()
{
	ReleaseMutex(this->mutexHandle);
}
