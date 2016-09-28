#pragma once
#include "MayaIncludes.h"
////#include "circular.h"
////


 class CallbackHandler 
{

private:
	static MCallbackIdArray callBackIds;
	//circular_buffer<char>*  localBuffer;

	
public:
	CallbackHandler();
	//void AddCallbackId(MCallbackId id) { callBackIds.append(id); };
	~CallbackHandler();
	bool Init();

#pragma region Callback functions
static void VertChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug& otherPlug, void *);
static void WorldMatrixChanged(MObject & transformNode, MDagMessage::MatrixModifiedFlags & modified, void * clientData);
static void TopologyChanged(MObject & node, void * clientData);
static void NodeCreated(MObject & node, void * clientData);
#pragma endregion
};

