#include "CallbackHandler.h"



CallbackHandler::CallbackHandler()
{
}


CallbackHandler::~CallbackHandler()
{
	MMessage::removeCallbacks(callBackIds);

}


bool CallbackHandler::Init(circular_buffer<char>*)
{
	MStatus result = MS::kSuccess;

	MCallbackId id;
	id = MDGMessage::addNodeAddedCallback(NodeCreated, "dependNode", NULL, &result);
	if (MFAIL(result))
		return false;
	callBackIds.append(id);


	MItDag transformIt(MItDag::kBreadthFirst, MFn::kTransform, &result);
	for (; !transformIt.isDone(); transformIt.next())
	{

		MFnTransform thisTransform(transformIt.currentItem());


		MDagPath path = MDagPath::getAPathTo(thisTransform.child(0));

		MCallbackId newId = MDagMessage::addWorldMatrixModifiedCallback(path, WorldMatrixChanged, NULL, &result);
		if (result == MS::kSuccess)
		{
			if (callBackIds.append(newId) == MS::kSuccess)
				std::cerr << "Transform callback added!  " << path.fullPathName() << std::endl;
			else
				std::cerr << "Could not add worldMatrix callback , Path: " << path.fullPathName() << std::endl;

		}
		else
			std::cerr << "Could not add worldMatrix callback , Path: " << path.fullPathName() << std::endl;
	}


	MItDag meshIt(MItDag::kBreadthFirst, MFn::kMesh, &result);
	for (; !meshIt.isDone(); meshIt.next())
	{

		MFnMesh thisMesh(meshIt.currentItem());
		
		MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(meshIt.currentItem(), VertChanged, NULL, &result);

		//thisMesh.attribute("quadSplit",) <---- IMPORTANT! find out how to!

		if (result == MS::kSuccess)
		{
			callBackIds.append(polyId);
			std::cerr << "Polychange callback added!  " << meshIt.currentItem().apiTypeStr() << std::endl;
		}
		else
			std::cerr << "error adding topologychange CB :" << meshIt.currentItem().apiTypeStr() << std::endl;


		polyId = MPolyMessage::addPolyTopologyChangedCallback(meshIt.currentItem(), TopologyChanged, NULL, &result);
		if (result == MS::kSuccess)
		{
			callBackIds.append(polyId);
			std::cerr << "Polychange callback added!  " << meshIt.currentItem().apiTypeStr() << std::endl;
		}
	}


	return true;
}
