
#include "CallbackHandler.h"


MCallbackIdArray CallbackHandler::callBackIds;

char* CallbackHandler::meshDataToSend;

std::unordered_map<std::string, MObject> CallbackHandler::knownShaders;
std::unordered_map<std::string, MObject> CallbackHandler::deletedNodes;
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
inline MObject findShader(MObject& setNode)
{
	MFnDependencyNode fnNode(setNode);
	MPlug shaderPlug = fnNode.findPlug("surfaceShader");
	if (!shaderPlug.isNull())
	{
		MPlugArray connectedPlugs;
		bool asSrc = false;
		bool asDst = true;
		shaderPlug.connectedTo(connectedPlugs, asDst, asSrc);
		if (connectedPlugs.length() != 1)
			return MObject::kNullObj;
		else
			return connectedPlugs[0].node();

	}
}

inline bool GetMeshShaderName(MFnMesh& mesh, MString& nameContainer)
{

	MObjectArray sets;
	MObjectArray comps;
	unsigned int instanceNum = mesh.dagPath().instanceNumber();
	if (!mesh.getConnectedSetsAndMembers(instanceNum, sets, comps, true))
		return false;
	/*
	getConnectedSetsAndMembers()
	Returns all the sets connected to the specified instance of this DAG object.
	For each set in the "sets" array there is a corresponding entry in the "comps" array which are all the components in that set. If the entire object is in a set,
	then the corresponding entry in the comps array will have no elements in it.
	*/

	if (sets.length())
	{
		MObject set = sets[0];
		MObject comp = comps[0];


		MStatus status;
		MObject shaderNode = findShader(set);
		if (shaderNode != MObject::kNullObj)
		{

			
			nameContainer = MFnDependencyNode(shaderNode).name();
			if (nameContainer.asChar() > 0)
				return true;
			
		}
	}
	return false;

}



bool CallbackHandler::SendMesh(MFnMesh & mesh, char* materialName)
{
	MeshMessage meshMessage;
	string name = mesh.name().asChar();
	if (name == "")
		return false;

	if (materialName != nullptr)
		memcpy(meshMessage.materialName, materialName, 256);

	MString shaderName;
	if (GetMeshShaderName(mesh, shaderName))
	{
		if (knownShaders.find(shaderName.asChar()) != knownShaders.end())
		{
			std::cerr << " This shader is already known! : " << shaderName << std::endl;
			memcpy(meshMessage.materialName, shaderName.asChar(), shaderName.length());
		}
	}
	else
		return false;
	
	string command = "setAttr " + name + ".quadSplit 1";
	MGlobal::executeCommandStringResult(MString(command.c_str()));
	MFnTransform obj(mesh.parent(0));
	//MMatrix matrix       = obj.transformationMatrix();

	MFnDependencyNode depNode(obj.object());
	MFnMatrixData parentMatrix = depNode.findPlug("pm").elementByLogicalIndex(0).asMObject();
	MMatrix matrix = obj.transformationMatrix();
	matrix = matrix * parentMatrix.matrix();



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
	mesh.getUVs(U, V, 0); 
	//combtime new
	MIntArray triCount, TriIndices;
	mesh.getTriangleOffsets(triCount, TriIndices);
	MIntArray vertCount, vertList;
	mesh.getVertices(vertCount, vertList);
	MIntArray normCount, normalList;
	mesh.getNormalIds(normCount, normalList);
	MIntArray uvCount, uvIDs;
	mesh.getAssignedUVs(uvCount, uvIDs);
	unsigned int * indices = new unsigned int[meshMessage.indexCount];

	//mMesh.getAssignedUVs(uvCount, uvIds);
	//cheers!
	//
	//	vData[i].u = mU[uvIndex[offsetIndices[i].nr].nr];
	//vData[i].v = mV[uvIndex[offsetIndices[i].nr].nr];


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
		tempVert.uv.x = U[uvIDs[TriIndices[i]]];
		tempVert.uv.y = V[uvIDs[TriIndices[i]]] * -1;
		tempVert.logicalIndex = triangleVerts[i];
		tempVert.normalIndex =TriIndices[i];
		std::cerr << "ID: " << normalList[TriIndices[i]] << " Normal: " << tempVert.normal.x << " | " << tempVert.normal.y << " | " << tempVert.normal.z << endl;
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


bool CallbackHandler::SendMaterial(MaterialMessage* material, TextureFile* textures)
{
	if (material->numTextures > 0) // if there are textures
	{
		size_t msgSize = sizeof(MaterialMessage) + (sizeof(TextureFile) * material->numTextures);
		char* dataTosend = new char[msgSize];
		memcpy(dataTosend, material, sizeof(MaterialMessage));
		memcpy(dataTosend + sizeof(MaterialMessage),
			textures,
			sizeof(TextureFile)*material->numTextures);

		//std::cerr << "The texturepath in ""sendMaterial""  is : " << textures->texturePath << std::endl;
		bool result = MessageHandler::GetInstance()->SendNewMessage(dataTosend, MessageType::MATERIAL, msgSize);

		delete dataTosend;
		return result;
	}
	else
	{
		bool result = MessageHandler::GetInstance()->SendNewMessage((char*)material, MessageType::MATERIAL,sizeof(MaterialMessage));
		return result;
	}


	
}


bool CallbackHandler::ExtractAndSendMaterial(MObject materialNode)
{
	MaterialMessage material;
	TextureFile diffuse;
	
	if (materialNode != MObject::kNullObj)
	{
		MStatus status;
		float rgb[3];
		MString mMatName = MFnDependencyNode(materialNode).name();

		if (knownShaders.find(mMatName.asChar()) != knownShaders.end())
		{
			//std::cerr << " This shader is already known! : " << mMatName << std::endl;
		}
		else
		{
			//std::cerr << "New Shader discovered in sendMaterial! : " << mMatName << std::endl;
			knownShaders[mMatName.asChar()] = materialNode;
		}

		memcpy(material.matName, mMatName.asChar(), mMatName.length());
		std::cerr << "Shader name : " << material.matName << std::endl;
		MPlug colorPlug = MFnDependencyNode(materialNode).findPlug("color", &status);
		if (status != MS::kFailure)
		{
			MItDependencyGraph It(colorPlug, MFn::kFileTexture, MItDependencyGraph::kUpstream);
			if (!It.isDone())
			{
				// Get the filename
				MString filename;
				MFnDependencyNode(It.thisNode()).findPlug("fileTextureName").getValue(filename);
				if (filename.length())
				{
					if (filename.length() > 256)
					{
						std::cerr << "Texture path :" << filename.asChar() << "\n"
							<< " is too long! Consider changing the name" << std::endl;
						return false;
					}
					//A texture exists
					memcpy(diffuse.texturePath, filename.asChar(), filename.length());
					//std::cerr << "Texture map is : " << diffuse.texturePath << std::endl;
					diffuse.type = TextureTypes::DIFFUSE;
					material.numTextures += 1;
				}
			}
			else
			{
				//No texture exists, get the rgb values
				MObject data;
				colorPlug.getValue(data);
				MFnNumericData val(data);
				val.getData(rgb[0], rgb[1], rgb[2]);
				material.diffuse = Float3(rgb[0], rgb[1], rgb[2]);
			}
		}
		else
			return false;

		MPlug ambientPlug = MFnDependencyNode(materialNode).findPlug("ambientColor", &status);
		if (status != MS::kFailure)
		{
			MObject data;
			ambientPlug.getValue(data);
			MFnNumericData val(data);
			val.getData(rgb[0], rgb[1], rgb[2]);
			material.ambient = Float3(rgb[0], rgb[1], rgb[2]);
		}
		

		
		
			material.specularRGB = Float3(0.0f, 0.0f, 0.0f);
		if (materialNode.hasFn(MFn::kPhong))
		{
			MPlug cosinePlug = MFnDependencyNode(materialNode).findPlug("cosinePower", &status);
			if (status != MS::kFailure)
			{
				MObject data;
				float cosPower = 0.0f;
				cosinePlug.getValue(cosPower);
				material.specularVal = cosPower * 4.0f;
			}
			MPlug specColorPlug = MFnDependencyNode(materialNode).findPlug("specularColor", &status);
			if (status != MS::kFailure)
			{
				MObject data;
				specColorPlug.getValue(data);
				MFnNumericData val(data);
				val.getData(rgb[0], rgb[1], rgb[2]);
				material.specularRGB = Float3(rgb[0], rgb[1], rgb[2]);
			}
		}
		if (materialNode.hasFn(MFn::kBlinn))
		{
			MPlug cosinePlug = MFnDependencyNode(materialNode).findPlug("eccentricity", &status);
			if (status != MS::kFailure)
			{
				MObject data;
				float eccentricity = 0.0f;
				cosinePlug.getValue(eccentricity);
				material.specularVal = (eccentricity < 0.03125f) ? 128.0f : 4.0f / eccentricity;
			}
			MPlug specColorPlug = MFnDependencyNode(materialNode).findPlug("specularColor", &status);
			if (status != MS::kFailure)
			{
				MObject data;
				specColorPlug.getValue(data);
				MFnNumericData val(data);
				val.getData(rgb[0], rgb[1], rgb[2]);
				material.specularRGB = Float3(rgb[0], rgb[1], rgb[2]);
			}
		}
		return SendMaterial(&material, &diffuse);
	}
	else
		return false;
}

CallbackHandler::CallbackHandler()
{
}

CallbackHandler::~CallbackHandler()
{
	delete[] CallbackHandler::meshDataToSend;
	MMessage::removeCallbacks(callBackIds);
	knownShaders.clear();

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

	MItDependencyNodes shaderIt(MFn::kLambert, &result);
	
	for (; !shaderIt.isDone(); shaderIt.next())
	{
		MFnDependencyNode mat(shaderIt.item());
		const char* name=  mat.name().asChar();
		std::cerr << "New Material Found : " << name << std::endl;
		knownShaders[name] = shaderIt.item();
		ExtractAndSendMaterial(shaderIt.item());
		MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(shaderIt.item(), MaterialChanged, NULL, &result); //attr callback is for when anything has been changed (not REMOVED)

		if (result == MS::kSuccess)
			callBackIds.append(polyId);
	}
	MItDependencyNodes textureIT(MFn::kFileTexture, &result);
	for (; !textureIT.isDone(); textureIT.next())
	{
		

			MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(textureIT.item(), TextureChanged, NULL, &result); 

			if (result == MS::kSuccess)
				callBackIds.append(polyId);
			std::cerr << "A TEXTUREFILE IS ADDED!!!! " << std::endl;
		


	}

	
	MItDag meshIt(MItDag::kBreadthFirst, MFn::kMesh, &result);
	for (; !meshIt.isDone(); meshIt.next())
	{

		//MFnMesh thisMesh(meshIt.currentItem());

		MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(meshIt.currentItem(), VertChanged, NULL, &result); //attr callback is for when anything has been changed (not REMOVED)
		MFnMesh mesh(meshIt.currentItem());

		char materialName[256];
		memset(materialName, '\0', 256);

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

		
	
		CallbackHandler::SendMesh(mesh,materialName);
		
	}
	


	return true;
}

void CallbackHandler::VertChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug & otherPlug, void *)
{

	MFnDependencyNode depNode(plug.node());
	MPlug matPlug = depNode.findPlug("iog").elementByLogicalIndex(0);
	//First check is for material changes TO THE MESH. ie, new material, deleted material. etc
	if (matPlug == plug)
	{
		if (plug.node().hasFn(MFn::kMesh))
		{ 
	
			std::cerr << "Material Changed: " << depNode.name() << std::endl;
			MFnMesh mesh(plug.node());

			if (!CallbackHandler::SendMesh(mesh, nullptr))
			{
				queueMutex.lock();
				meshToSendQueue.push(plug.node());
				queueMutex.unlock();
			}
		}
	
		return;
	}

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
		if (!node.hasFn(MFn::kMesh))
			return;
		MFnMesh mesh = node;
		MFnTransform obj(mesh.parent(0));
		MString meshName = obj.name().asChar();

		MPointArray points;
		MIntArray logicalIDs;
		MFloatVectorArray normals, normals2;
		MFloatArray U, V;
		MIntArray normCount, normIDs;
		const float* rawNormals = mesh.getRawNormals(0);
		mesh.getUVs(V, U, 0);
		mesh.getNormals(normals2, MSpace::kObject);
		mesh.getNormalIds(normCount, normIDs);

		if (component.apiType() == MFn::kMeshPolygonComponent)
		{
			cerr << component.apiTypeStr() << endl;
			MItMeshPolygon	iteratorMeshPoly(item, component);
			iteratorMeshPoly.getPoints(points, MSpace::kObject);
			for (; !iteratorMeshPoly.isDone(); iteratorMeshPoly.next())
			{
				MPointArray temp;
				iteratorMeshPoly.getPoints(temp, MSpace::kObject);
				iteratorMeshPoly.getVertices(logicalIDs);
				for (size_t i = 0; i < temp.length(); i++)
				{
					points.append(temp[i]);
					logicalIDs.append(iteratorMeshPoly.vertexIndex(i));
				}
			}
		}
		else if (component.apiType() == MFn::kMeshEdgeComponent)
		{
			cerr << component.apiTypeStr() << endl;
			MItMeshEdge		iteratorMeshEdge(item, component);
			for (; !iteratorMeshEdge.isDone(); iteratorMeshEdge.next())
			{
				points.append(iteratorMeshEdge.point(0, MSpace::kObject));
				points.append(iteratorMeshEdge.point(1, MSpace::kObject));
				logicalIDs.append(iteratorMeshEdge.index(0));
				logicalIDs.append(iteratorMeshEdge.index(1));

			}
		}
		else if (component.apiType() == MFn::kMeshVertComponent)
		{
			cerr << component.apiTypeStr() << endl;
			MItMeshVertex	iteratorMeshVert(item, component);
			for (; !iteratorMeshVert.isDone(); iteratorMeshVert.next())
			{
				points.append(iteratorMeshVert.position());
				logicalIDs.append(iteratorMeshVert.index());
			}
		}
		else
		{
			cerr << "Fatal Error 0xfded " << endl;
			return;
		}

		unsigned int offset = sizeof(VertSegmentMessage);
		VertSegmentMessage vertSegMessasge;
		memcpy(vertSegMessasge.nodeName, meshName.asChar(), mesh.name().length());
		vertSegMessasge.nameLength = meshName.length();
		vertSegMessasge.nodeName[vertSegMessasge.nameLength] = '\0';
		vertSegMessasge.numVertices = points.length(); //dubbelkolla dupes
		vertSegMessasge.numNormals = normIDs.length();
		memcpy(meshDataToSend, &vertSegMessasge, sizeof(VertSegmentMessage));

		Float3* normalsToSend = new Float3[normIDs.length()];
		for (size_t i = 0; i < normIDs.length(); i++)
		{
				normalsToSend[i].x = rawNormals[normIDs[i] * 3];
				normalsToSend[i].y = rawNormals[normIDs[i] * 3 + 1];
				normalsToSend[i].z = rawNormals[normIDs[i] * 3 + 2];
		}
		memcpy(meshDataToSend + offset, normalsToSend, sizeof(Float3) * normIDs.length());
		offset += sizeof(Float3) * normIDs.length();
		delete[] normalsToSend;
		int* IDptr = new int[normIDs.length()];
		normIDs.get(IDptr);
		memcpy(meshDataToSend + offset, IDptr, sizeof(int) * normIDs.length());
		delete[] IDptr;
		offset += sizeof(int) * normIDs.length();

		for (size_t i = 0; i < points.length(); i++)
		{
			VertexMessage vertMessage;
			Vertex tempVert;

			tempVert.position.x = points[i].x;
			tempVert.position.y = points[i].y;
			tempVert.position.z = points[i].z;
			tempVert.logicalIndex = logicalIDs[i];
			vertMessage.indexId = logicalIDs[i];
			vertMessage.vert = tempVert;

			memcpy(meshDataToSend + offset, &vertMessage, sizeof(VertexMessage));
			offset += sizeof(VertexMessage);

		}
		MessageHandler::GetInstance()->SendNewMessage(meshDataToSend,
		MessageType::VERTSEGMENT, offset);
	}
}

void CallbackHandler::WorldMatrixChanged(MObject & transformNode, MDagMessage::MatrixModifiedFlags & modified, void * clientData)
{

	MStatus result; 
	MFnDependencyNode depNode(transformNode);
	MFnTransform obj(transformNode);
	TransformMessage header;

	header.nameLength = obj.name().length();
	memcpy(header.nodeName, obj.name().asChar(), header.nameLength);
	header.nodeName[header.nameLength] = '\0';


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
	
	std::cerr << "A TransformNode has changed!! |" << obj.name() << "   | " << modified << std::endl;
	
	MFloatMatrix floatMatrix(matrix.matrix); // convert it into float
	memcpy(header.matrix, floatMatrix.matrix, sizeof(float) * 16);

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
	MStatus result;
	if (node.hasFn(MFn::kTransform))
	{
		
		MCallbackId cbId = MNodeMessage::addNodeAboutToDeleteCallback(node, NodeDestroyed, NULL, &result);
		if (result == MS::kSuccess)
		{
			callBackIds.append(cbId);
			std::cerr << "Deletion callback added!  " << std::endl;
		}
	}
	if (node.hasFn(MFn::kLambert))
	{
		ExtractAndSendMaterial(node);
		MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(node, MaterialChanged, NULL, &result); //attr callback is for when anything has been changed (not REMOVED)
		std::cerr << " A new Material has been created!" << std::endl;
		if (result == MS::kSuccess)
			callBackIds.append(polyId);
	}
	if (node.hasFn(MFn::kFileTexture))
	{
		
		MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(node, TextureChanged, NULL, &result); //attr callback is for when anything has been changed (not REMOVED)
		
		if (result == MS::kSuccess)
			callBackIds.append(polyId);
		std::cerr << "A TEXTUREFILE IS ADDED!!!! " << std::endl;
	}
	


	if (node.hasFn(MFn::kMesh))
	{
		MFnMesh mesh(node);
		
		MFnDagNode nodeHandle(node);
		
		MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(node, VertChanged, NULL, &result);
		
		std::cerr << nodeHandle.fullPathName() << std::endl;
	
		if (!CallbackHandler::SendMesh(mesh,nullptr))
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
	static double lastKnownView[4][4]{0};
	static double lastKnownProj[4][4]{ 0 };
	//now i've done that! and my days were good. thank you
	
	MString focusedPanel = MGlobal::executeCommandStringResult("getPanel -wf");
	//std::cerr << "Panel callback : " << str.asChar()          << std::endl;
	if (str == focusedPanel)
	{
		M3dView viewport;//= M3dView::active3dView();
		M3dView::getM3dViewFromModelPanel(focusedPanel, viewport);
		//View Matrix
		MMatrix viewMatrixDouble;
		viewport.modelViewMatrix(viewMatrixDouble);		  // get the matrix as double
		MMatrix projMatrixDouble;
		viewport.projectionMatrix(projMatrixDouble);
		if (memcmp(lastKnownView, viewMatrixDouble.matrix, sizeof(double) * 16) == 0)
		{
			//now check if the projection has made any changes,
			if (memcmp(lastKnownProj, projMatrixDouble.matrix, sizeof(double) * 16) == 0)
			{
				//std::cerr << "The camera has not been updated, dont send" << std::endl;
				return;
			}
			else
				memcpy(lastKnownProj, projMatrixDouble.matrix, sizeof(double) * 16);
		}
		else
			memcpy(lastKnownView, viewMatrixDouble.matrix, sizeof(double) * 16);
		
		CameraMessage header;
		
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
		viewport.updateViewingParameters();

		double pos[3];
		MStatus stat = tempCamTransform.translation(MSpace::kTransform).get(pos);
		if(stat != MStatus::kFailure)
			header.camPos = Float3(pos);
		
		MFnCamera mCam(camera);
		float fov = (float)mCam.verticalFieldOfView();
		DirectX::XMMATRIX d3dProj;
		if (mCam.isOrtho())
		{
			float x = (float)mCam.orthoWidth();
			float y = (float)viewport.portHeight();
			float aspect = 800.0f / 600.0f;			//this is bad, but it works. don't copy this. it's an ugly fix
			float w = aspect * mCam.orthoWidth();
		
			d3dProj = DirectX::XMMatrixOrthographicLH(
				(float)w,
				(float)w,
				mCam.nearClippingPlane(),
				mCam.farClippingPlane());
		}
		else
		{
			d3dProj = DirectX::XMMatrixPerspectiveFovLH((float)mCam.verticalFieldOfView()
			,(float)mCam.aspectRatio(),
			mCam.nearClippingPlane(),
			mCam.farClippingPlane());

		}

	
		MFloatMatrix viewMatrix(viewMatrixDouble.matrix); // convert it into float
		memcpy(header.viewMatrix, viewMatrix.matrix, sizeof(float) * 16);
																		 
		memcpy(header.projMatrix, d3dProj.r->m128_f32, sizeof(float) * 16);


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
		bool send = true;
		while (send)
		{
			if (CallbackHandler::SendMesh(MFnMesh(meshToSendQueue.front(), nullptr)))
			{

				meshToSendQueue.pop();
			}
			else
			{
				MFnDagNode   object(meshToSendQueue.front());
				 // this is because, deletion caused a bug where we tried to send a deleted mesh
				// in  this infinite loop.
				if (deletedNodes.find(object.name().asChar()) == deletedNodes.end())
				{ //if the mesh to send is present in the deleted nodes. remove it from the queue
					std::cerr << " This mesh has been deleted and will not be send to the renderer" << std::endl;
					meshToSendQueue.pop();
					deletedNodes.erase(object.name().asChar());
				}

			}
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
	
		deletedNodes[object.name().asChar()] = node;
	}
}

void CallbackHandler::MaterialChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug& otherPlug, void *)
{

	//This callback is for material nodes,

	if (plug.node().hasFn(MFn::kLambert))
	{
		ExtractAndSendMaterial(plug.node());
		//std::cerr << "MATERIALS HAVE CHANGED WOHOOOO" << std::endl;
	}

}

void CallbackHandler::TextureChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug & otherPlug, void *)
{

	MFnDependencyNode depNode(plug.node());
	MPlug texPlug = depNode.findPlug("fileTextureName");

	if (texPlug == plug)
	{
		MPlugArray plugArray;
		MStatus result;
	
		MPlug outCol = depNode.findPlug("outColor");

		outCol.connectedTo(plugArray, false, true,&result);
		if (result != MStatus::kFailure)
		{
			if (plugArray.length() > 0)
			{
				//std::cerr << plugArray[0].name().asChar() << std::endl;

				
				MFnDependencyNode connectedNode(plugArray[0].node());
				std::cerr <<  connectedNode.name().asChar() << std::endl;
				ExtractAndSendMaterial(plugArray[0].node());

			}
			else
				std::cerr << "Could not find any connections to this texture" << std::endl;

		}
		MString texName;
		texPlug.getValue(texName);
		//std::cerr << "TEXTURE HAS CHANGED: " << texName.asChar() << std::endl;
		
		
		return;
	}

}
