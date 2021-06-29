#pragma once
#include <string>
#include "Memory.h"
#include "Headers.h"
#include "Mutex.h"

//struct MessageHeader
//{
//	
//	size_t msgSize = 0;
//	size_t ID = 0;
//};

enum ProcessType { Producer, Consumer };

class CircularBuffer
{
private:
	Mutex* mutex;
	Memory* sharedMemory;
	char* messageData;

	size_t* head;
	size_t* tail;
	size_t* freeMemory;

	SectionHeader* sectionHeader;
	ControlHeader* ctrler;
	ProcessType type;

public:
	CircularBuffer(LPCWSTR bufferName, size_t bufferSize, ProcessType type);
	~CircularBuffer();

	Memory* GetSharedMemory() { return sharedMemory; }

	bool Send(char* message, SectionHeader* secHeader);
	bool Recieve(char*& message, SectionHeader*& header);
};
