#pragma once
#include "MayaIncludes.h"
#include "MessageHandler.h"
#include <queue>
#include <maya\MMutexLock.h>
#include <unordered_map>
////#include "circular.h"
////
static std::queue<MObject> meshToSendQueue;
static MMutexLock queueMutex;

 class CallbackHandler 
{

private:
	static std::unordered_map<std::string, MObject> deletedNodes;
	static std::unordered_map<std::string, MObject> knownShaders;
	static char* meshDataToSend; //temporary to store the message we send, we keep this just to avoid reallocating memory
	static MCallbackIdArray callBackIds;
	//circular_buffer<char>*  localBuffer;
	
public:
	
	
	//void AddCallbackId(MCallbackId id) { callBackIds.append(id); };
	CallbackHandler();
	~CallbackHandler();
	bool Init();
	static CallbackHandler* GetInstance() { static CallbackHandler instance; return &instance; }
	static bool SendMesh(MFnMesh& mesh, char* materialName = nullptr);
	static bool SendMaterial(MaterialMessage* material, TextureFile* textures);
	static bool GetMaterialFromMesh(MFnMesh& mesh, char* matName = nullptr);
	static bool SendMaterial(MObject materialNode);
#pragma region Callback functions
	static void VertChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug& otherPlug, void *);
	static void WorldMatrixChanged(MObject & transformNode, MDagMessage::MatrixModifiedFlags & modified, void * clientData);
	static void TopologyChanged(MObject & node, void * clientData);
	static void NodeCreated(MObject & node, void * clientData);
	static void CameraUpdated(const MString &str, void *clientData);
	static void TimeCallback(float elapsedTime, float lastTime, void *clientData);
	static void NodeDestroyed(MObject &node, MDGModifier &modifier, void *clientData);
	static void MaterialChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug& otherPlug, void *);
	static void TextureChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug& otherPlug, void *);

#pragma endregion
};

