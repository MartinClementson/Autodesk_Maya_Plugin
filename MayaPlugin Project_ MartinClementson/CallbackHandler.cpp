
#include "CallbackHandler.h"
inline void UpdateChildren(MFnTransform& transformNode)
{
	// recursive

	for (size_t i = 0; i < transformNode.childCount(); i++) // update the children meshes
	{

		if (transformNode.child(i).hasFn(MFn::kTransform))	 // child transform object is found!
		{
			MDagMessage::MatrixModifiedFlags modified = MDagMessage::MatrixModifiedFlags::kAll;
			CallbackHandler::WorldMatrixChanged(transformNode.child(i), modified, NULL);

			MFnTransform childN(transformNode.child(i));
			UpdateChildren(childN);
		}

	}



}


bool CallbackHandler::SendMesh(MFnMesh & mesh)
{
	MeshMessage meshMessage;
	string name = mesh.name().asChar();
	string command = "setAttr " + name + ".quadSplit 1";

	MGlobal::executeCommandStringResult(MString(command.c_str()));

	MFnTransform obj(mesh.parent(0));
	MMatrix matrix       = obj.transformationMatrix();
	size_t sizeOfMessage = sizeof(MeshMessage);
	unsigned int offset  = sizeof(MeshMessage); //byte offset when storing the data to the char array

	MPointArray vertices;

	for (size_t row = 0; row < 4; row++)
	{
		for (size_t column = 0; column < 4; column++)
		{
			meshMessage.worldMatrix[(4 * row) + column] = (float)matrix.matrix[row][column];
		}
	}
	
	if(MStatus::kFailure == mesh.getPoints(vertices,MSpace::kObject))
	{	
		MGlobal::displayError("MFnMesh::getPoints");
		std::cerr << "ERROR GETTING POINTS  " << std::endl;
		return false;
	}
	
	MIntArray numTriangles;
	MIntArray triangleVerts;
	mesh.getTriangles(numTriangles, triangleVerts);

	meshMessage.indexCount = triangleVerts.length();

	meshMessage.nameLength = obj.name().length();
	memcpy(meshMessage.nodeName, obj.name().asChar(), meshMessage.nameLength);
	meshMessage.nodeName[meshMessage.nameLength] = '\0';
	 //use the transformnode name, since that is the id in the renderer
	meshMessage.vertexCount = mesh.numVertices();
	

	memcpy(meshDataToSend, &meshMessage, sizeof(MeshMessage));

	for (size_t i = 0; i < meshMessage.vertexCount; i++)
	{
		Vertex tempVert;
		tempVert.position.x =	vertices[i].x;
		tempVert.position.y =	vertices[i].y;
		tempVert.position.z =	vertices[i].z;
		memcpy(meshDataToSend + offset, &tempVert, sizeof(Vertex));
		offset += sizeof(Vertex);
	//	std::cerr << "x: " << vertices[i].x << " y: " << vertices[i].y << " z: " << vertices[i].z << " "  << std::endl;
	}
	
	unsigned int * indices = new unsigned int[meshMessage.indexCount];

	for (size_t i = 0; i < triangleVerts.length() / 3; i++)
	{
		
		indices[i * 3]     = triangleVerts[i*3 + 0];
		indices[i * 3 + 1] = triangleVerts[i*3 + 2];	 //notice the shift, the order is different in DirectX, so we change it here
		indices[i * 3 + 2] = triangleVerts[i*3 + 1];

	}

	memcpy(meshDataToSend + offset, indices, sizeof(unsigned int) *meshMessage.indexCount);





bool result =	MessageHandler::GetInstance()->SendNewMessage(meshDataToSend, 
		MessageType::MESH, 
		offset + ( sizeof(unsigned int) * meshMessage.indexCount));

	delete indices;
	std::cerr << "Result returns : " << result << std::endl;
	return result;
}

CallbackHandler::CallbackHandler()
{
}



CallbackHandler::~CallbackHandler()
{
	delete[] CallbackHandler::meshDataToSend;
	MMessage::removeCallbacks(callBackIds);

}

bool CallbackHandler::Init()
{


	CallbackHandler::meshDataToSend = new char[sizeof(Vertex) * 1000000 + sizeof(MeshMessage)]; //this is really risky. try to find another way
	MStatus result = MS::kSuccess;

	MCallbackId id;
	id = MDGMessage::addNodeAddedCallback(NodeCreated, "dependNode", NULL, &result);

	if (MFAIL(result))
		return false;

	callBackIds.append(id);

	MCallbackId tempId = MUiMessage::add3dViewPreRenderMsgCallback(MString("modelPanel4"), CameraUpdated, NULL, &result);

	callBackIds.append(tempId);

	//MCallbackId tempId = MUiMessage::add3dViewPreRenderMsgCallback(MString("modelPanel1"), CameraUpdated, NULL, &result);
	//MCallbackId tempId = MUiMessage::add3dViewPreRenderMsgCallback(MString("modelPanel1"), CameraUpdated, NULL, &result);
	id = MTimerMessage::addTimerCallback(0.1f, TimeCallback, NULL, &result);
	callBackIds.append(id);

	MItDag transformIt(MItDag::kBreadthFirst, MFn::kTransform, &result);
	for (; !transformIt.isDone(); transformIt.next())
	{

		MFnTransform thisTransform(transformIt.currentItem());
		MDagPath path = MDagPath::getAPathTo(thisTransform.child(0));

		if (transformIt.currentItem().hasFn(MFn::kCamera))
		{
			
			continue;
		}
		else
		{


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
	}


	MItDag meshIt(MItDag::kBreadthFirst, MFn::kMesh, &result);
	for (; !meshIt.isDone(); meshIt.next())
	{

		//MFnMesh thisMesh(meshIt.currentItem());

		MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(meshIt.currentItem(), VertChanged, NULL, &result); //attr callback is for when anything has been changed (not REMOVED)
		MFnMesh mesh(meshIt.currentItem());
		

		string name = mesh.name().asChar();
		string command = "setAttr " + name +".quadSplit 1";
		MGlobal::executeCommandStringResult(MString(command.c_str()));

		if (result == MS::kSuccess)
		{
			callBackIds.append(polyId);
			std::cerr << "Polychange callback added!  " << mesh.fullPathName() << std::endl;
		}
		else
			std::cerr << "error adding topologychange CB :" << meshIt.currentItem().apiTypeStr() << std::endl;


		polyId = MPolyMessage::addPolyTopologyChangedCallback(meshIt.currentItem(), TopologyChanged, NULL, &result); // topology call back is for when a vert has been removed
		if (result == MS::kSuccess)
		{
			callBackIds.append(polyId);
			
			std::cerr << "Topology callback added!  " << mesh.fullPathName() << std::endl;
		}
	
		CallbackHandler::SendMesh(mesh);
		
	}
	


	return true;
}

void CallbackHandler::VertChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug & otherPlug, void *)
{

	if (msg & MNodeMessage::AttributeMessage::kAttributeSet && !plug.isArray() && plug.isElement()) //if a specific vert has changed
	{
		MStringArray changes;
		MFnNumericData point(plug.attribute());
		std::cerr << plug.info() << std::endl;

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

	MFnDependencyNode depNode(transformNode);
	MFnTransform obj(transformNode);
	TransformMessage header;

	header.nameLength = obj.name().length();
	memcpy(header.nodeName, obj.name().asChar(), header.nameLength);
	header.nodeName[header.nameLength] = '\0';




	MStatus result; 

	std::cerr << "parent matrix is type :  " << depNode.attribute("pm").apiTypeStr() << std::endl;;

	MFnMatrixData parentMatrix = depNode.findPlug("pm").elementByLogicalIndex(0).asMObject();
	MMatrix matrix = obj.transformationMatrix();
	matrix = matrix * parentMatrix.matrix();


	unsigned int numChildren = obj.childCount();
	for (int child = 0; child < numChildren; child++)
	{
		if (obj.child(child).hasFn(MFn::kCamera))
		{
			//if This belongs to a camera, return. 
			return;
		}
	
	}
	
		std::cerr << matrix.matrix[0][0] << " " << matrix.matrix[0][1] << " " << matrix.matrix[0][2] << " " << matrix.matrix[0][3] << std::endl;
		std::cerr << matrix.matrix[1][0] << " " << matrix.matrix[1][1] << " " << matrix.matrix[1][2] << " " << matrix.matrix[1][3] << std::endl;
		std::cerr << matrix.matrix[2][0] << " " << matrix.matrix[2][1] << " " << matrix.matrix[2][2] << " " << matrix.matrix[2][3] << std::endl;
		std::cerr << matrix.matrix[3][0] << " " << matrix.matrix[3][1] << " " << matrix.matrix[3][2] << " " << matrix.matrix[3][3] << std::endl;

	std::cerr << "A TransformNode has changed!! |" << obj.name() << "   | " << modified << std::endl;
	

	for (size_t row = 0; row < 4; row++)
	{
		for (size_t column = 0; column < 4; column++)
		{
			header.matrix[(4 * row) + column] = (float)matrix.matrix[row][column]; 
		}
	}

	char newHeader[sizeof(TransformMessage)];

	memcpy(newHeader, &header, sizeof(TransformMessage));

	MessageHandler::GetInstance()->SendNewMessage(newHeader, MessageType::TRANSFORM);

	UpdateChildren(obj);
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
		MFnMesh mesh(node);
		
		MFnDagNode nodeHandle(node);
		MStatus result;
		MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(node, VertChanged, NULL, &result);
		
		std::cerr << nodeHandle.fullPathName() << std::endl;
	

		if (!CallbackHandler::SendMesh(mesh))
		{
			queueMutex.lock();
			meshToSendQueue.push(node);
			queueMutex.unlock();
		}
		
	
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
				
				std::cerr << "Polychange callback added!  "  << std::endl;
			}

		}

	}


}

void CallbackHandler::CameraUpdated( const MString &str, void *clientData)
{

	//This is called for every frame, Create an if here that checks if the cam has been updated
	//until then, have a good day


		MString focusedPanel = MGlobal::executeCommandStringResult("getPanel -wf");
		std::cerr << "Panel callback : " << str.asChar()          << std::endl;
		std::cerr << "Active panel   :"  << focusedPanel.asChar() << std::endl;
	if (str == focusedPanel)
	{
		
		CameraMessage header;

		M3dView viewport = M3dView::active3dView();

		////// All this just to get the proper name////////////
		MDagPath camera;									 //
		viewport.getCamera(camera);							 // 
		MFnTransform tempCamTransform(camera.transform());	 // 
		///////////////////////////////////////////////////////

		header.nameLength = tempCamTransform.name().length();
		memcpy(header.nodeName, tempCamTransform.name().asChar(), header.nameLength > 256 ?  256 : header.nameLength );
		header.nodeName[header.nameLength] = '\0';



		std::cerr << "Camera is   :" << tempCamTransform.name().asChar() << std::endl;
		viewport.updateViewingParameters();
		MMatrix viewMatrix;
		viewport.modelViewMatrix(viewMatrix);
		//matrix = matrix.inverse();
		//double3 translationPoint = { matrix[3][0], matrix[3][1], matrix[3][2] };
		
		for (size_t row = 0; row < 4; row++)
		{
			for (size_t column = 0; column < 4; column++)
			{
				header.viewMatrix[(4 * row) + column] = (float)viewMatrix.matrix[row][column];
			}
		}

		std::cerr << viewMatrix.matrix[0][0] << " " << viewMatrix.matrix[0][1] << " " << viewMatrix.matrix[0][2] << " " << viewMatrix .matrix[0][3] << std::endl;
		std::cerr << viewMatrix.matrix[1][0] << " " << viewMatrix.matrix[1][1] << " " << viewMatrix.matrix[1][2] << " " << viewMatrix .matrix[1][3] << std::endl;
		std::cerr << viewMatrix.matrix[2][0] << " " << viewMatrix.matrix[2][1] << " " << viewMatrix.matrix[2][2] << " " << viewMatrix .matrix[2][3] << std::endl;
		std::cerr << viewMatrix.matrix[3][0] << " " << viewMatrix.matrix[3][1] << " " << viewMatrix.matrix[3][2] << " " << viewMatrix .matrix[3][3] << std::endl;

		

		char newHeader[sizeof(CameraMessage)];
		memset(newHeader, '\0', sizeof(CameraMessage));
		
		memcpy(newHeader, &header, sizeof(CameraMessage));
	
		MessageHandler::GetInstance()->SendNewMessage(newHeader, MessageType::CAMERA);
	}

}

void CallbackHandler::TimeCallback(float elapsedTime, float lastTime, void * clientData)
{

	queueMutex.lock();
	if (!meshToSendQueue.empty())
	{
		std::cerr << "Time Callback: trying to send mesh" << std::endl;
		while (CallbackHandler::SendMesh(MFnMesh(meshToSendQueue.front())))
		{
			meshToSendQueue.pop();
			if (meshToSendQueue.empty())
				return;
		}
	}
	queueMutex.unlock();

}
