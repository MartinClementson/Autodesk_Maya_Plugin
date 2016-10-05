#include "EngineCommunicator.h"
#include <iostream>


bool EngineCommunicator::PutMessageIntoBuffer(char * msg, size_t length)
{
	
	if (!circleBuffer.Push(msg, length))
		std::cerr << "Error sending message " << std::endl;
	else
		std::cerr << "Message is SENT " << std::endl;
	return true;
}

EngineCommunicator::EngineCommunicator()
{
	msg = new char[20 * (1 << 20)];
	SharedMemory::CommandArgs arg;
	arg.memorySize = MEMORY_SIZE_IN_MB;
	arg.producer   = IS_PRODUCER;
	arg.msDelay     = MS_DELAY;

	if (!this->circleBuffer.Init(arg,
		msgFileName, msgMutexName,
		infoFileName, infomutexName))
	{
		std::cerr << "COULD NOT INITIALIZE CIRCLEBUFFER " << std::endl;
	}
}


EngineCommunicator::~EngineCommunicator()
{
}
