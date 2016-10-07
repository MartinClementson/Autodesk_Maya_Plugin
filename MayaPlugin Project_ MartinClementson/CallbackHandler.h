#pragma once
#include "MayaIncludes.h"
#include "MessageHandler.h"
////#include "circular.h"
////


 class CallbackHandler 
{

private:

	static char* meshDataToSend; //temporary to store the message we send, we keep this just to avoid reallocating memory
	static MCallbackIdArray callBackIds;
	//circular_buffer<char>*  localBuffer;
	
	
public:
	//void AddCallbackId(MCallbackId id) { callBackIds.append(id); };
	CallbackHandler();
	~CallbackHandler();
	bool Init();
	static CallbackHandler* GetInstance() { static CallbackHandler instance; return &instance; }
	static void SendMesh(MFnMesh& mesh);
#pragma region Callback functions
static void VertChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug& otherPlug, void *);
static void WorldMatrixChanged(MObject & transformNode, MDagMessage::MatrixModifiedFlags & modified, void * clientData);
static void TopologyChanged(MObject & node, void * clientData);
static void NodeCreated(MObject & node, void * clientData);
static void CameraUpdated(const MString &str, void *clientData);

#pragma endregion
};

