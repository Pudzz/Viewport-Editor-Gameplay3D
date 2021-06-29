#include "Circularbuffers.h"

CircularBuffer::CircularBuffer(LPCWSTR bufferName, size_t bufferSize, ProcessType type)
{
    this->type = type;
    sharedMemory = new Memory(bufferName, bufferSize);  // Innehåller både memoryBuffer och ctrlBuffer
    mutex = new Mutex(L"MutexMap");

    messageData = sharedMemory->GetMemoryBuffer();

    head = sharedMemory->GetControlBuffer();
    tail = head + 1;
    freeMemory = tail + 1;

    std::cout << "MESSAGEDATA: " << messageData << std::endl;
    OutputDebugString((LPCWSTR)messageData);

    if (type == Producer)
    {
        *head = 0;
        *tail = 0;
        *freeMemory = bufferSize * (1 << 10);
    }
    else if (type == Consumer)
    {        
        *tail = 0;
    }
}

CircularBuffer::~CircularBuffer()
{
    if (mutex)
        delete mutex;

    if (sharedMemory)
        delete sharedMemory;
}

bool CircularBuffer::SendSectionHeader(SectionHeader* secHeader)
{
    bool result = false;
    mutex->Lock();

    size_t memoryLeft = sharedMemory->GetBufferSize() - *head;

    if (sizeof(SectionHeader) >= memoryLeft)
    {
        if (*tail != 0)
        {
            secHeader->messageSize = memoryLeft - sizeof(SectionHeader);
            secHeader->ID = 0;

            memcpy(messageData + *head, secHeader, sizeof(SectionHeader));

            *freeMemory -= (sizeof(SectionHeader));
            *head = 0;

            mutex->Unlock();
            result = false;

        }
        else
        {
            mutex->Unlock();
            result = false;
        }

    }

    else if (sizeof(SectionHeader) < *freeMemory - 1)
    {
        secHeader->ID = 1;

        // Print stuff
        memcpy(messageData + *head, secHeader, sizeof(SectionHeader));

        *freeMemory -= sizeof(SectionHeader);
        *head = (*head + sizeof(SectionHeader)) % sharedMemory->GetBufferSize();

        mutex->Unlock();
        result = true;
    }
    else
    {
        mutex->Unlock();
        result = false;
    }

    return result;
}

bool CircularBuffer::Send(char* message, SectionHeader* secHeader)
{
    bool result = false;
    mutex->Lock();

    size_t memoryLeft = sharedMemory->GetBufferSize() - *head;
    if (secHeader->messageSize + sizeof(SectionHeader) >= memoryLeft)
    {
        if (*tail != 0)
        {
            secHeader->messageSize = memoryLeft - sizeof(SectionHeader);
            secHeader->ID = 0;

            memcpy(messageData + *head, secHeader, sizeof(SectionHeader));

            *freeMemory -= (secHeader->messageSize + sizeof(SectionHeader));
            *head = 0;

            mutex->Unlock();
            result = false;

        }
        else
        {
            mutex->Unlock();
            result = false;
        }

    }

    else if (secHeader->messageSize  + sizeof(SectionHeader) < *freeMemory - 1)
    {
        secHeader->ID = 1;

        // Print stuff
        memcpy(messageData + *head, secHeader, sizeof(SectionHeader));
        memcpy(messageData + *head + sizeof(SectionHeader), message, secHeader->messageSize);

        *freeMemory -= (secHeader->messageSize + sizeof(SectionHeader));
        *head = (*head + secHeader->messageSize + sizeof(SectionHeader)) % sharedMemory->GetBufferSize();

        mutex->Unlock();
        result = true;
    }
    else
    {
        mutex->Unlock();
        result = false;
    }

    return result;
}

bool CircularBuffer::Recieve(char* message)
{
    mutex->Lock();
    bool result = false;

    size_t msgLength = 0;

    if (*freeMemory < sharedMemory->GetBufferSize())
    {
        if (*head != *tail)
        {
            //memcpy(sectionHeader, (SectionHeader*)messageData + *tail, sizeof(SectionHeader));
            sectionHeader = ((SectionHeader*)&messageData[*tail]);
            msgLength = sectionHeader->messageSize;

            if (sectionHeader->ID == 0)
            {
                *freeMemory += (msgLength + sizeof(SectionHeader));
                *tail = 0;

                mutex->Unlock();
                result = false;

            }
            else
            {
                memcpy(message, &messageData[*tail + sizeof(SectionHeader)], msgLength);

                *tail = (*tail + msgLength + sizeof(SectionHeader)) % sharedMemory->GetBufferSize();
                *freeMemory += (msgLength + sizeof(SectionHeader));

                mutex->Unlock();
                result = true;
            }
        }
        else
        {
            mutex->Unlock();
            result = false;
        }

    }
    else
    {
        mutex->Unlock();
        result = false;
    }

    std::cout << "Recieve works: " << std::endl;

    return result;
}
