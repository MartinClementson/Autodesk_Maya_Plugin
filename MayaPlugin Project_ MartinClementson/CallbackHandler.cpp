
#include "CallbackHandler.h"

CallbackHandler::CallbackHandler()
{
}



CallbackHandler::~CallbackHandler()
{
	MMessage::removeCallbacks(callBackIds);

}

bool CallbackHandler::Init()
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

void CallbackHandler::VertChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug & otherPlug, void *)
{

	if (msg & MNodeMessage::AttributeMessage::kAttributeSet && !plug.isArray() && plug.isElement()) //if a specific vert has changed
	{
		MStringArray changes;
		MFnNumericData point(plug.attribute());


		plug.getSetAttrCmds(changes, MPlug::kChanged);
		if (changes.length() == 1)
		{


			float x, y, z;
			point.getData3Float(x, y, z);
			x = plug.child(0).asFloat();
			y = plug.child(1).asFloat();
			z = plug.child(2).asFloat();
			std::cerr << "A Vert has changed!! |" << x << "," << y << "," << z << "|  " << changes << std::endl;

		}
	}
}

void CallbackHandler::WorldMatrixChanged(MObject & transformNode, MDagMessage::MatrixModifiedFlags & modified, void * clientData)
{

	MFnTransform obj(transformNode);
	MMatrix matrix = obj.transformationMatrix();
	std::cerr << matrix.matrix[0][0] << " " << matrix.matrix[0][1] << " " << matrix.matrix[0][2] << " " << matrix.matrix[0][3] << std::endl;
	std::cerr << matrix.matrix[1][0] << " " << matrix.matrix[1][1] << " " << matrix.matrix[1][2] << " " << matrix.matrix[1][3] << std::endl;
	std::cerr << matrix.matrix[2][0] << " " << matrix.matrix[2][1] << " " << matrix.matrix[2][2] << " " << matrix.matrix[2][3] << std::endl;
	std::cerr << matrix.matrix[3][0] << " " << matrix.matrix[3][1] << " " << matrix.matrix[3][2] << " " << matrix.matrix[3][3] << std::endl;

	std::cerr << "A TransformNode has changed!! |" << obj.name() << "   | " << modified << std::endl;
}

void CallbackHandler::TopologyChanged(MObject & node, void * clientData)
{

	MFnMesh thisMesh(node);

	std::cerr << "Topology has changed |" << thisMesh.name() << std::endl;
}

void CallbackHandler::NodeCreated(MObject & node, void * clientData)
{

	if (node.hasFn(MFn::kMesh))
	{

		MFnDagNode nodeHandle(node);
		MStatus result;
		MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(node, VertChanged, NULL, &result);


		if (MS::kSuccess == result)
		{


			callBackIds.append(polyId);
			std::cerr << "A new node was created \n";
			std::cerr << "Callback triggered from  node : " << nodeHandle.name() << "\n"
				<< "Node Path : " << nodeHandle.fullPathName() << "\n" << std::endl;

			MDagPath path = MDagPath::getAPathTo(node);
			polyId = MDagMessage::addWorldMatrixModifiedCallback(path, WorldMatrixChanged, NULL, &result);
			if (result == MS::kSuccess)
			{
				callBackIds.append(polyId);
			}
			MCallbackId polyId = MPolyMessage::addPolyTopologyChangedCallback(node, TopologyChanged, NULL, &result);
			if (result == MS::kSuccess)
			{
				callBackIds.append(polyId);
				std::cerr << "Polychange callback added!  " << node.apiTypeStr() << std::endl;
			}

		}



	}



}
