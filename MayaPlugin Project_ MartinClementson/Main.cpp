#include "MayaIncludes.h"
#include "CallbackHandler.h"

//CallbackHandler* callbackHandler = nullptr;

//circular_buffer<char>* localBuffer;


EXPORT MStatus initializePlugin(MObject obj) {
	std::cout.rdbuf(std::cerr.rdbuf());


	MStatus result = MS::kSuccess;

	// Set plugin registration info: Author, plugin-version and Maya version needed.
	MFnPlugin plugin(obj, "Martin_Clementson", "1.0", "2016", &result);

	if (MFAIL(result))
	{
		CHECK_MSTATUS(result);
		return result;
	}

	//localBuffer		= new circular_buffer<char>;
	//callbackHandler = new CallbackHandler();
	//callbackHandler->Init(localBuffer);

	MGlobal::displayInfo("Plugin is loaded");
	return MS::kSuccess;
}

// Gets called when the plugin is unloaded from Maya.
EXPORT MStatus uninitializePlugin(MObject obj)
{
	MFnPlugin plugin(obj);


	// Print to show the plugin was unloaded.
	std::cout << "In uninitializePlugin()\n" << std::endl;

	MGlobal::displayInfo("Plugin is unloaded");
	//delete callbackHandler;
	//delete localBuffer;
	return MS::kSuccess;
}