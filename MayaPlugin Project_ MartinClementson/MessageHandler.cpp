#include "MessageHandler.h"



MessageHandler::MessageHandler()
{
}


bool MessageHandler::SendNewMessage(char * msg, MessageType type, size_t length)
{

	MainMessageHeader mainHead;

	switch (type)
	{
	case MESH:		
	{

		char * newMessage = new char[sizeof(MainMessageHeader) + length];
		mainHead.messageType = MESH;
		mainHead.msgSize     = length;
		memcpy(newMessage, &mainHead, sizeof(MainMessageHeader));						 //merge the message and the header
		memcpy(newMessage + sizeof(MainMessageHeader), msg, length);					 //merge the message and the header
		engineCommunicator.PutMessageIntoBuffer(newMessage, sizeof(MainMessageHeader) + length);
		delete newMessage;
		break;			 
	}
	case VERTSEGMENT:	 
		break;			 
	case VERTEX:		 
		break;			 
	case CAMERA:		 
		break;			 
	case TRANSFORM:	
		mainHead.messageType = TRANSFORM;
		mainHead.msgSize     = sizeof(sizeof(TransformMessage));
		char newMessage[sizeof(MainMessageHeader) + sizeof(TransformMessage)];

		memcpy(newMessage, &mainHead, sizeof(MainMessageHeader));						 //merge the message and the header
		memcpy(newMessage+ sizeof(MainMessageHeader), msg, sizeof(TransformMessage));	 //merge the message and the header

		engineCommunicator.PutMessageIntoBuffer(newMessage, sizeof(MainMessageHeader) + sizeof(TransformMessage));
		break;			 
	case MATERIAL:		 
		break;			 
	default:			 
		break;			 
	}
	return true;
}

MessageHandler::~MessageHandler()
{
}
