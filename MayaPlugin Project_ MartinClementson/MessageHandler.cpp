#include "MessageHandler.h"



MessageHandler::MessageHandler()
{
}


bool MessageHandler::SendNewMessage(char * msg, MessageType type, size_t length)
{

	MainMessageHeader mainHead;
	bool result = false;
	switch (type)
	{
	case MESH:		
	{

		char * newMessage = new char[sizeof(MainMessageHeader) + length];
		mainHead.messageType = MESH;
		mainHead.msgSize     = length;
		memcpy(newMessage, &mainHead, sizeof(MainMessageHeader));						 //merge the message and the header
		memcpy(newMessage + sizeof(MainMessageHeader), msg, length);					 //merge the message and the header
		result = engineCommunicator.PutMessageIntoBuffer(newMessage, sizeof(MainMessageHeader) + length);
		delete newMessage;
		break;			 
	}
	case VERTSEGMENT:	 
		break;			 
	case VERTEX:		 
		break;			 
	case CAMERA:	
	{

		mainHead.messageType = CAMERA;
		mainHead.msgSize     = sizeof(CameraMessage);
		char newMessage[sizeof(MainMessageHeader) + sizeof(CameraMessage)];
		memcpy(newMessage, &mainHead, sizeof(MainMessageHeader));						 //merge the message and the header
		memcpy(newMessage + sizeof(MainMessageHeader), msg, sizeof(CameraMessage));	     //merge the message and the header
		result = engineCommunicator.PutMessageIntoBuffer(newMessage, sizeof(MainMessageHeader) + sizeof(CameraMessage));
		break;			 
	}
	case TRANSFORM:	
	{

		mainHead.messageType = TRANSFORM;
		mainHead.msgSize     = sizeof(TransformMessage);
		char newMessage[sizeof(MainMessageHeader) + sizeof(TransformMessage)];

		memcpy(newMessage, &mainHead, sizeof(MainMessageHeader));						 //merge the message and the header
		memcpy(newMessage+ sizeof(MainMessageHeader), msg, sizeof(TransformMessage));	 //merge the message and the header

		result = engineCommunicator.PutMessageIntoBuffer(newMessage, sizeof(MainMessageHeader) + sizeof(TransformMessage));
		break;			 
	}
	case MATERIAL:		
	{
		
		mainHead.messageType = MATERIAL;
		mainHead.msgSize	 = sizeof(MaterialMessage);
		MaterialMessage* matMsg = (MaterialMessage*)msg;
		if (matMsg->numTextures > 0)
		{
			size_t msgSize = sizeof(MainMessageHeader) + length;
			char* newMessage = new char[msgSize];
			memcpy(newMessage, &mainHead, sizeof(MainMessageHeader));
			memcpy(newMessage + sizeof(MainMessageHeader), msg, length);	 //merge the message and the header
			
			result = engineCommunicator.PutMessageIntoBuffer(newMessage, msgSize);
			delete newMessage;

			TextureFile* texture = (TextureFile*)(msg + sizeof(MaterialMessage));
			std::cerr << "The texture that was sent had the path : " << texture->texturePath << std::endl;

		}
		else
		{
			char newMessage [sizeof(MainMessageHeader) + sizeof(MaterialMessage)];
			memcpy(newMessage, &mainHead, sizeof(MainMessageHeader));
			memcpy(newMessage + sizeof(MainMessageHeader), msg, sizeof(MaterialMessage));	 //merge the message and the header

			result = engineCommunicator.PutMessageIntoBuffer(newMessage, sizeof(MainMessageHeader) + sizeof(MaterialMessage));

		}
		break;	
	}

	case DELETION:
	{
		mainHead.messageType = DELETION;
		mainHead.msgSize	 = sizeof(DeleteMessage);

		char newMessage[sizeof(MainMessageHeader) + sizeof(DeleteMessage)];

		memcpy(newMessage, &mainHead, sizeof(MainMessageHeader));						 //merge the message and the header
		memcpy(newMessage + sizeof(MainMessageHeader), msg, sizeof(DeleteMessage));	     //merge the message and the header

		result = engineCommunicator.PutMessageIntoBuffer(newMessage, sizeof(MainMessageHeader) + sizeof(DeleteMessage));
	}


	default:			 
		break;			 
	}
	return result;
}

MessageHandler::~MessageHandler()
{

}
