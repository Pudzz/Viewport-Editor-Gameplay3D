#include "Memory.h"

Memory::Memory(LPCWSTR bufferName, size_t bufferSize)
{
    this->ctrlbufferName = L"CtrlMap";

    this->bufferSize = bufferSize * 1048576; // (1 << 10);   // buffersize in mb
    this->controlbufferSize = sizeof(ControlHeader);

    InitializeFilemap(bufferName);
    InitializeFileview();
}

Memory::~Memory()
{
    UnmapViewOfFile(memoryData);
    UnmapViewOfFile(controlData);

    CloseHandle(memoryFilemap);
    CloseHandle(controlFilemap);
}

void Memory::InitializeFilemap(LPCWSTR buffername)
{
    /* Shared memory for msgbuffer */
    memoryFilemap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, bufferSize, buffername);
    if (memoryFilemap == NULL)
        std::cout << "Failed to create file mapping object\n";
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        std::cout << "File mapping object already exists - It's shared\n";

    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * /

     /* Shared memory for controlbuffer */
    controlFilemap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, controlbufferSize, ctrlbufferName);
    if (controlFilemap == NULL)
        std::cout << "Failed to create file mapping object\n";
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        std::cout << "File mapping object already exists - It's shared\n";

}

void Memory::InitializeFileview()
{
    /* Fileview of msgbuffer */
    memoryData = (char*)MapViewOfFile(memoryFilemap, FILE_MAP_ALL_ACCESS, 0, 0, bufferSize);
    if (memoryData == NULL)
        std::cout << "View of file mapping object for memorydata failed\n";

    /* Fileview of ctrlbuffer */
    controlData = (size_t*)MapViewOfFile(controlFilemap, FILE_MAP_ALL_ACCESS, 0, 0, controlbufferSize);
    if (controlData == NULL)
        std::cout << "View of file mapping object for controldata failed\n";
}