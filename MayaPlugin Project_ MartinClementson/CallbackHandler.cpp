
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
	if (name == "")
		return false;
	string command = "setAttr " + name + ".quadSplit 1";
	MGlobal::executeCommandStringResult(MString(command.c_str()));
	MFnTransform obj(mesh.parent(0));
	MMatrix matrix       = obj.transformationMatrix();
	size_t sizeOfMessage = sizeof(MeshMessage);
	unsigned int offset  = sizeof(MeshMessage); //byte offset when storing the data to the char array
	for (size_t row = 0; row < 4; row++)
		for (size_t column = 0; column < 4; column++)
			meshMessage.worldMatrix[(4 * row) + column] = (float)matrix.matrix[row][column];

	MIntArray numTriangles;
	MIntArray triangleVerts;
	mesh.getTriangles(numTriangles, triangleVerts);

	meshMessage.indexCount = triangleVerts.length();

	meshMessage.nameLength = obj.name().length();
	memcpy(meshMessage.nodeName, obj.name().asChar(), meshMessage.nameLength);
	meshMessage.nodeName[meshMessage.nameLength] = '\0';
	meshMessage.vertexCount = triangleVerts.length();
	memcpy(meshDataToSend, &meshMessage, sizeof(MeshMessage));

	//Raw vertices
	const float* rawVertices = mesh.getRawPoints(0);
	//Raw normals
	const float* rawNormals = mesh.getRawNormals(0);
	//BiNormals
	vector<MFloatVector> biNormals;
	biNormals.reserve(mesh.numFaceVertices());
	MFloatVectorArray faceBiNormals;
	for (size_t i = 0; i < mesh.numPolygons(); i++)
	{
		mesh.getFaceVertexBinormals(i, faceBiNormals, MSpace::kObject);
		for (size_t j = 0; j < faceBiNormals.length(); j++)
		{
			biNormals.push_back(faceBiNormals[j]);
		}
	}
	//Tangents
	vector<MFloatVector> tangents;
	tangents.reserve(mesh.numFaceVertices());
	MFloatVectorArray faceTangents;
	for (size_t i = 0; i < mesh.numPolygons(); i++)
	{
		mesh.getFaceVertexTangents(i, faceTangents, MSpace::kObject);
		for (size_t j = 0; j < faceTangents.length(); j++)
		{
			tangents.push_back(faceTangents[j]);
		}
	}
	//UVs
	MFloatArray U, V;
	mesh.getUVs(V, U, 0); //changing U and V positions to align with D3D11. 0 stands for what UV set to use (default 0)
	//combtime new
	MIntArray triCount, TriIndices;
	mesh.getTriangleOffsets(triCount, TriIndices);
	MIntArray vertCount, vertList;
	mesh.getVertices(vertCount, vertList);
	MIntArray normCount, normalList;
	mesh.getNormalIds(normCount, normalList);
	unsigned int * indices = new unsigned int[meshMessage.indexCount];
	for (size_t i = 0; i < triangleVerts.length(); i++)
	{
		Vertex tempVert;
		tempVert.position.x = rawVertices[triangleVerts[i] * 3];
		tempVert.position.y = rawVertices[triangleVerts[i] * 3 + 1];
		tempVert.position.z = rawVertices[triangleVerts[i] * 3 + 2];
		tempVert.normal.x = rawNormals[normalList[TriIndices[i]] * 3];
		tempVert.normal.y = rawNormals[normalList[TriIndices[i]] * 3 + 1];
		tempVert.normal.z = rawNormals[normalList[TriIndices[i]] * 3 + 2];

		tempVert.binormal.x = biNormals[normalList[TriIndices[i]]].x;
		tempVert.binormal.y = biNormals[normalList[TriIndices[i]]].y;
		tempVert.binormal.z = biNormals[normalList[TriIndices[i]]].z;
		tempVert.tangent.x = tangents[normalList[TriIndices[i]]].x;
		tempVert.tangent.y = tangents[normalList[TriIndices[i]]].y;
		tempVert.tangent.z = tangents[normalList[TriIndices[i]]].z;
		tempVert.uv.x = U[vertList[TriIndices[i]]];
		tempVert.uv.y = V[vertList[TriIndices[i]]];
		tempVert.logicalIndex = triangleVerts[i];

		indices[i] = i;
		memcpy(meshDataToSend + offset, &tempVert, sizeof(Vertex));
		offset += sizeof(Vertex);
	}
	////Indices
	for (size_t i = 0; i < triangleVerts.length() / 3; i++)
	{
		int temp2 = indices[i * 3 + 2];
		indices[i * 3 + 2] = indices[i * 3 + 1];
		indices[i * 3 + 1] = temp2;				 //notice the shift, the order is different in DirectX, so we change it here
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

	MCallbackId tempId;
	tempId = MUiMessage::add3dViewPreRenderMsgCallback(MString("modelPanel1"), CameraUpdated, NULL, &result);
	callBackIds.append(tempId);
	tempId = MUiMessage::add3dViewPreRenderMsgCallback(MString("modelPanel2"), CameraUpdated, NULL, &result);
	callBackIds.append(tempId);
	tempId = MUiMessage::add3dViewPreRenderMsgCallback(MString("modelPanel3"), CameraUpdated, NULL, &result);
	callBackIds.append(tempId);
	tempId = MUiMessage::add3dViewPreRenderMsgCallback(MString("modelPanel4"), CameraUpdated, NULL, &result);
	callBackIds.append(tempId);


	id = MTimerMessage::addTimerCallback(0.1f, TimeCallback, NULL, &result);
	callBackIds.append(id);

	MItDag transformIt(MItDag::kBreadthFirst, MFn::kTransform, &result);
	for (; !transformIt.isDone(); transformIt.next())
	{

		MFnTransform thisTransform(transformIt.currentItem());
		MDagPath path = MDagPath::getAPathTo(thisTransform.child(0));

		MCallbackId cbId = MNodeMessage::addNodeAboutToDeleteCallback(transformIt.currentItem(), NodeDestroyed, NULL, &result); // topology call back is for when a vert has been removed
		if (result == MS::kSuccess)
		{
			callBackIds.append(cbId);

			std::cerr << "Deletion callback added!  " << thisTransform.fullPathName() << std::endl;
		}

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
		std::cerr << "Found Movement!" << std::endl;
		MSelectionList selectionList;
		MGlobal::getActiveSelectionList(selectionList);
		MItSelectionList iterator(selectionList);
		MDagPath item;
		MObject component, node;
		iterator.getDagPath(item, component);
		iterator.getDagPath(item);

		node = plug.node();
		MFnMesh mesh = node;
		MFnTransform obj(mesh.parent(0));
		MString meshName = obj.name().asChar();

		MFloatVectorArray normal;
		mesh.getNormals(normal, MSpace::kObject);

		MIntArray numTriangles;
		MIntArray triangleVerts;
		mesh.getTriangles(numTriangles, triangleVerts);


		MItMeshPolygon	iteratorMeshPoly(item, component);
		MItMeshEdge		iteratorMeshEdge(item, component);
		MItMeshVertex	iteratorMeshVert(item, component);

		MPoint point, sPoint;
		mesh.getPoint(plug.logicalIndex(), point);

		MPointArray points;

		if (iteratorMeshPoly.count() != 0)
		{
			cerr << component.apiTypeStr() << endl;
			iteratorMeshPoly.getPoints(points, MSpace::kObject);
			for (; !iteratorMeshPoly.isDone(); iteratorMeshPoly.next())
			{
				MPointArray temp;
				iteratorMeshPoly.getPoints(temp, MSpace::kObject);
				for (size_t i = 0; i < temp.length(); i++)
				{
					points.append(temp[i]);
				}
			}

		}
		else if (iteratorMeshEdge.count() != 0)
		{
			cerr << component.apiTypeStr() << endl;
			for (; !iteratorMeshEdge.isDone(); iteratorMeshEdge.next())
			{
				points.append(iteratorMeshEdge.point(0, MSpace::kObject));
				points.append(iteratorMeshEdge.point(1, MSpace::kObject));
			}
		}
		else if (iteratorMeshVert.count() != 0)
		{
			cerr << component.apiTypeStr() << endl;
			for (; !iteratorMeshVert.isDone(); iteratorMeshVert.next())
			{
				points.append(iteratorMeshVert.position());
			}
		}
		else
		{
			cerr << "fatal error" << endl;
			return;
		}

		//sPoint = points[0];

		//if (point == sPoint)
		{
			VertSegmentMessage vertSegMessasge;
			memcpy(vertSegMessasge.nodeName, meshName.asChar(), mesh.name().length());
			vertSegMessasge.nameLength = meshName.length();
			vertSegMessasge.nodeName[vertSegMessasge.nameLength] = '\0';
			vertSegMessasge.numVertices = iteratorMeshVert.count();
			memcpy(meshDataToSend, &vertSegMessasge, sizeof(VertSegmentMessage));
			unsigned int offset = sizeof(VertSegmentMessage);

			for (size_t i = 0; i < points.length(); i++)
			{
				VertexMessage vertMessage;
				Vertex tempVert;

				tempVert.position.x = points[i].x;
				tempVert.position.y = points[i].y;
				tempVert.position.z = points[i].z;
				tempVert.normal.x = normal[plug.logicalIndex()].x;	   //
				tempVert.normal.y = normal[plug.logicalIndex()].y;	   // Alla vertiser som delar denna point får nu dessa normaler. måste fixa så alla påverkade vertiser får de normaler de ska ha
				tempVert.normal.z = normal[plug.logicalIndex()].z;	   //
				tempVert.logicalIndex = triangleVerts[i];

				vertMessage.indexId = iteratorMeshVert.index();
				vertMessage.vert = tempVert;
				memcpy(meshDataToSend + offset, &vertMessage, sizeof(VertexMessage));
				offset += sizeof(VertexMessage);
			}

			//for (size_t i = 0; i < points.length(); i++)
			//{
			//	VertexMessage vertMessage;
			//	Vertex tempVert;

			//	tempVert.position.x = points[i].x;
			//	tempVert.position.y = points[i].y;
			//	tempVert.position.z = points[i].z;
			//	tempVert.normal.x = normal[plug.logicalIndex()].x;	   //
			//	tempVert.normal.y = normal[plug.logicalIndex()].y;	   // Alla vertiser som delar denna point får nu dessa normaler. måste fixa så alla påverkade vertiser får de normaler de ska ha
			//	tempVert.normal.z = normal[plug.logicalIndex()].z;	   //
			//	tempVert.logicalIndex = iteratorMeshVert.index();

			//	vertMessage.indexId = iteratorMeshVert.index();
			//	vertMessage.vert = tempVert;
			//	memcpy(meshDataToSend + offset, &vertMessage, sizeof(VertexMessage));
			//	offset += sizeof(VertexMessage);
			//}

			MessageHandler::GetInstance()->SendNewMessage(meshDataToSend,
			MessageType::VERTSEGMENT, offset);
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

	

	MFnMatrixData parentMatrix = depNode.findPlug("pm").elementByLogicalIndex(0).asMObject();
	MMatrix matrix = obj.transformationMatrix();
	matrix = matrix * parentMatrix.matrix();


	unsigned int numChildren = obj.childCount();
	for (int child = 0; child < numChildren; child++)
	{
		if (obj.child(child).hasFn(MFn::kCamera))
		{
			std::cerr << " Camera node : not sent" << std::endl;
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
	SendMesh(thisMesh);
}

void CallbackHandler::NodeCreated(MObject & node, void * clientData)
{

	if (node.hasFn(MFn::kTransform))
	{
		MStatus result;
		MCallbackId cbId = MNodeMessage::addNodeAboutToDeleteCallback(node, NodeDestroyed, NULL, &result);
		if (result == MS::kSuccess)
		{
			callBackIds.append(cbId);

			std::cerr << "Deletion callback added!  " << std::endl;
		}
	}


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
		}

		MDagPath path = MDagPath::getAPathTo(node);
		polyId = MDagMessage::addWorldMatrixModifiedCallback(path, WorldMatrixChanged, NULL, &result);
		if (result == MS::kSuccess)
		{
			callBackIds.append(polyId);
		}

		polyId = MPolyMessage::addPolyTopologyChangedCallback(node, TopologyChanged, NULL, &result);
		if (result == MS::kSuccess)
		{
			callBackIds.append(polyId);
			
			std::cerr << "Polychange callback added!  "  << std::endl;
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
		M3dView viewport;//= M3dView::active3dView();
		M3dView::getM3dViewFromModelPanel(focusedPanel, viewport);

		////// All this just to get the proper name/////////////
		MDagPath camera;									  //
		viewport.getCamera(camera);							  // 
		MFnTransform tempCamTransform(camera.transform());	  // 
															  //
		header.nameLength = tempCamTransform.name().length(); //
		memcpy(header.nodeName, tempCamTransform.name().asChar(), header.nameLength > 256 ?  256 : header.nameLength );
		header.nodeName[header.nameLength] = '\0';			  //
		///////////////////////////////////////////////////////

		MFnCamera mCam(camera);
		

		std::cerr << "Camera is   :" << tempCamTransform.name().asChar() << std::endl;
		viewport.updateViewingParameters();

		//View Matrix
		MMatrix viewMatrixDouble;
		viewport.modelViewMatrix(viewMatrixDouble);		  // get the matrix as double
		MFloatMatrix viewMatrix(viewMatrixDouble.matrix); // convert it into float
		memcpy(header.viewMatrix, viewMatrix.matrix, sizeof(float) * 16);
																		 
		//Proj Matrix												 
	//MMatrix projMatrixDouble;					
	//viewport.projectionMatrix(projMatrixDouble);
	//projMatrixDouble = projMatrixDouble.homogenize();
	//
	//MFloatMatrix projMatrix(viewMatrixDouble.matrix); // convert it into float

		MFloatMatrix projMatrix = mCam.projectionMatrix();
		memcpy(header.projMatrix, projMatrix.matrix, sizeof(float) * 16);


	

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

void CallbackHandler::NodeDestroyed(MObject &node, MDGModifier &modifier, void *clientData)
{
	


	if (node.hasFn(MFn::kTransform))
	{

		MFnTransform   object(node);
	
		std::cerr << "DESTROYED LIKE A PUNY MAGGOT : " << object.name().asChar() << std::endl;
		
		DeleteMessage newMessage;
		memset(&newMessage, '\0', sizeof(DeleteMessage));
		memcpy(newMessage.nodeName, object.name().asChar(), object.name().length());
		newMessage.nameLength = object.name().length();
		MessageHandler::GetInstance()->SendNewMessage((char*) &newMessage, MessageType::DELETION, sizeof(DeleteMessage));
	}
}
