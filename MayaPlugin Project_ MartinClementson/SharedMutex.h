#pragma once
#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
namespace SharedMemory
{

	class SharedMutex
	{
	private:
		HANDLE mutexHandle;			 // mutex Handle
		SharedMutex();
	public:
		SharedMutex(LPCWSTR mutexName);
		bool Lock(DWORD waitTime = INFINITE);
		void Unlock();
		virtual ~SharedMutex();
	};
}

