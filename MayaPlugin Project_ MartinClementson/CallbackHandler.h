#pragma once
#include "Callbacks.h"
#include "circular.h"
//
//void VertChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug& otherPlug, void *);
//
//
//void WorldMatrixChanged(MObject & transformNode, MDagMessage::MatrixModifiedFlags & modified, void * clientData);
//
//
//void  TopologyChanged(MObject & node, void * clientData);
//
//void NodeCreated(MObject & node, void * clientData);



class CallbackHandler
{


private:
	circular_buffer<char>*  localBuffer;
	 MCallbackIdArray callBackIdls;
	
public:
	CallbackHandler();
	void AddCallbackId(MCallbackId id) { callBackIds.append(id); };
	~CallbackHandler();
	bool Init(circular_buffer<char>*);



};

