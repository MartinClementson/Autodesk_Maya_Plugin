
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

inline MMatrix GetAccumulatedMatrix(MFnTransform& obj)
{
	//Recursive.

	MMatrix matrix = obj.transformationMatrix();

	for (size_t i = 0; i < obj.parentCount(); i++) // get the parent transformnodes and multiply them in.
	{
		if (obj.parent(i).hasFn(MFn::kTransform))	 // parented object is found!
		{
			MFnTransform parent(obj.parent(i));
			matrix = matrix* GetAccumulatedMatrix(parent);
			


		}
		
	}

		return matrix;

}

void CallbackHandler::SendMesh(MFnMesh & mesh)
{
	MeshMessage meshMessage;


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
		return;
	}
	
	meshMessage.meshName    = string(obj.name().asChar()); //use the transformnode name, since that is the id in the renderer
	meshMessage.vertexCount = mesh.numVertices();
	meshMessage.indexCount  = mesh.numPolygons() * 6;

	memcpy(meshDataToSend, &meshMessage, sizeof(MeshMessage));

	for (size_t i = 0; i < meshMessage.vertexCount; i++)
	{
		Vertex tempVert;
		tempVert.position.x =vertices[i].x;
		tempVert.position.y =vertices[i].y;
		tempVert.position.z =vertices[i].z;
		memcpy(meshDataToSend + offset, &tempVert, sizeof(Vertex));
		offset += sizeof(Vertex);
		std::cerr << "x: " << vertices[i].x << " y: " << vertices[i].y << " z: " << vertices[i].z << " "  << std::endl;
	}

	
	unsigned int * indices = new unsigned int[ mesh.numPolygons() * 6];
	
	for (size_t polygon    = 0; polygon < mesh.numPolygons(); polygon++)
	{
		MIntArray polyIndices;

		for (size_t tris = 0; tris < 2; tris++)
		{
			int verts[3];
			mesh.getPolygonTriangleVertices(polygon,tris, verts);
		
				std::cerr << "Amount of indices on this polygon : " << polyIndices.length() << std::endl;


				indices[(polygon * 6)+ 3 * tris]     = verts[0];
				indices[(polygon * 6)+ 3 * tris + 1] = verts[2];	 //notice the shift, the order is different in DirectX, so we change it here
				indices[(polygon * 6)+ 3 * tris + 2] = verts[1];	 //notice the shift, the order is different in DirectX, so we change it here
				std::cerr << "tris# "<< tris << " " << verts[0] << " " << verts[1] << "  " << verts[2] << " \n" << std::endl;
		}
		
	
		
		

	}

	//meshDataToSend
	
	memcpy(meshDataToSend + offset , indices, sizeof(unsigned int) *meshMessage.indexCount);
	


	MessageHandler::GetInstance()->SendNewMessage(meshDataToSend, MessageType::MESH, offset + (sizeof(unsigned int) *meshMessage.indexCount));
	std::cerr << "MESSAGE SHOULD BE SENT NOW " << std::endl;
	delete indices;
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

		//MFnMesh thisMesh(meshIt.currentItem());

		MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(meshIt.currentItem(), VertChanged, NULL, &result); //attr callback is for when anything has been changed (not REMOVED)
		MFnMesh mesh(meshIt.currentItem());
		//thisMesh.attribute("quadSplit",) <---- IMPORTANT! find out how to!

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
	header.nodeName = obj.name().asChar();
	header.matrix;



	MStatus result; 

	

	MMatrix matrix = GetAccumulatedMatrix(obj);
	
	

	
	std::cerr << matrix.matrix[0][0] << " " << matrix.matrix[0][1] << " " << matrix.matrix[0][2] << " " << matrix.matrix[0][3] << std::endl;
	std::cerr << matrix.matrix[1][0] << " " << matrix.matrix[1][1] << " " << matrix.matrix[1][2] << " " << matrix.matrix[1][3] << std::endl;
	std::cerr << matrix.matrix[2][0] << " " << matrix.matrix[2][1] << " " << matrix.matrix[2][2] << " " << matrix.matrix[2][3] << std::endl;
	std::cerr << matrix.matrix[3][0] << " " << matrix.matrix[3][1] << " " << matrix.matrix[3][2] << " " << matrix.matrix[3][3] << std::endl;
	unsigned int numChildren = obj.childCount();
	for (int child = 0; child < numChildren; child++)
	{
		if (obj.child(child).hasFn(MFn::kCamera))
		{

		//std::cerr << " THIS IS A CAMERA" << modified << std::endl;
		//M3dView viewport = M3dView::active3dView();
		//MMatrix viewMatrix;
		//viewport.modelViewMatrix(viewMatrix);
		//matrix = matrix.inverse();
		//double3 translationPoint = { matrix[3][0], matrix[3][1], matrix[3][2] };
		//matrix = matrix * viewMatrix;
		////matrix[3][0] = translationPoint[0];//matrix[0][3];
		////matrix[3][1] = translationPoint[1];//matrix[1][3];
		////matrix[3][2] = translationPoint[2];//matrix[2][3];
		//
		//


			MFnCamera cam(obj.child(child));
			MPoint lookAt = cam.centerOfInterestPoint(MSpace::kWorld).homogenize();
			MPoint eye = cam.eyePoint(MSpace::kWorld).homogenize();
			
			MVector zAxis = eye - lookAt;

			matrix[0][2] = zAxis.x;
			matrix[1][2] = zAxis.y;
			matrix[2][2] = zAxis.z;
		break; //Camera is found so break the loop
		}

	}
	

	std::cerr << "A TransformNode has changed!! |" << obj.name() << "   | " << modified << std::endl;
	
	

	for (size_t row = 0; row < 4; row++)
	{
		for (size_t column = 0; column < 4; column++)
		{
			header.matrix[(4 * row) + column] = (float)matrix.matrix[row][column]; 

		}

	}

	//M3dView view // use this to get view matrix

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
		MFnMesh meshTwo(mesh.parent(0));
		MPointArray vertices;
		std::cerr << meshTwo.name() <<  ":   DETTA LETAR VI EFTER" <<std::endl;
		
		MFnDagNode nodeHandle(node);
		MStatus result;
		MCallbackId polyId = MNodeMessage::addAttributeChangedCallback(node, VertChanged, NULL, &result);
		
		std::cerr << nodeHandle.fullPathName() << std::endl;
	//result = mesh.getPoints(vertices, MSpace::kObject);
	//if (MStatus::kFailure == result)
	//{
	//	
	//	MGlobal::displayError("MFnMesh::getPoints");
	//	std::cerr << result.errorString()<< std::endl;
	//	return ;
	//}
	//CallbackHandler::SendMesh(mesh);
	//std::cerr << "THE SENDMESH FUNCTION WAS CALLED  " << std::endl;

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
