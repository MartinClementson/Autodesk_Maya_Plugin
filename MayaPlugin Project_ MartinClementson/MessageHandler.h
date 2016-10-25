#pragma once
#include "EngineCommunicator.h"
#include "SharedMemHeaders.h"
#include <iostream>

class MessageHandler
{
private :
	EngineCommunicator engineCommunicator;
	MessageHandler();
public:
	static MessageHandler* GetInstance() { static MessageHandler instance; return &instance; };

	bool SendNewMessage(char* msg,MessageType type, size_t length = 0);
	~MessageHandler();
};

