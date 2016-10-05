#pragma once
#include "CircleBuffer.h"

#define IS_PRODUCER true
#define MS_DELAY    0
#define MEMORY_SIZE_IN_MB  100 ;
class EngineCommunicator
{
private:
	LPCWSTR msgFileName    = (LPCWSTR)TEXT("sharedMsgFile");	  // the file to be manipulated
	LPCWSTR msgMutexName   = (LPCWSTR)TEXT("sharedMsgMutex");	  // the the mutex to be used when synchronizing data							
	LPCWSTR infoFileName   = (LPCWSTR)TEXT("sharedInfoFile");	  // the file to be manipulated
	LPCWSTR infomutexName  = (LPCWSTR)TEXT("sharedInfoMutex");	  // the the mutex to be used when synchronizing data											
																  //LPCWSTR writeEventName  = (LPCWSTR)TEXT("writeEvent");		  // the event handle to the writing event
private:
	char* msg;
	SharedMemory::CircleBuffer circleBuffer;
public:
	bool PutMessageIntoBuffer(char* msg, size_t length);
	EngineCommunicator();
	~EngineCommunicator();
};

