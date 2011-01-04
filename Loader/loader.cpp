/*
    Main unit for loader.  Handles:
    -   loader debugging log
    -   DLL entry point
    -   OBSE plugin interface        
    -   OBSE messaging interface

    The basic structure of this plugin is:
    -   A 'loader' dll, located in the OBSE/Plugins folder, which is loaded & processed by obse.
        The loader does the necessary version checks, registers obse interfaces & script commands, etc.
        It then loads ...
        -   ExportInjector.dll, which the player should have installed in the OBSE/Plugins/COEF folder.
            ExportInjector finds *.eed files (hich the player should also have installed with COEF), and uses them to
            build a table of exported functions and data objects for the Game/CS.
        Once ExportInjector is loaded (or if it's already been loaded by another plugin), then the loader loads a ...
        -   Submodule DLL, located in some subdirectory of Oblivion/Data.  This submodule contains the actual 'guts' of
            the plugin - all of the code that is based on COEF classes.
    The loader and communicates with the submodule using the submodules exported functions - Initialize(), COEFBasicTest(), etc.

*/
#include "obse/PluginAPI.h"             // for interfacing with obse
#define PLUGINVERSION 0x01000000        

/*--------------------------------------------------------------------------------------------*/
// includes & definitions for command registration
#include "obse/CommandTable.h"          
#include "obse/ParamInfos.h"
// Ported from CommandTable.cpp so we don't have to include the entire file
bool Cmd_Default_Execute(COMMAND_ARGS) {return true;} // nop command handler for script editor
bool Cmd_Default_Eval(COMMAND_ARGS_EVAL) {return true;} // nop command evaluator for script editor
// base opcode - this is not an officially assigned base; use for testing purposes only!
const int OPCODEBASE        = 0x5000;

/*--------------------------------------------------------------------------------------------*/
// global debugging log for the loader
HTMLTarget      _gLogFile("Data\\obse\\plugins\\" SOLUTIONNAME "\\" SOLUTIONNAME ".Loader.log.html", SOLUTIONNAME ".Loader.Log");
OutputLog       _gLog;
OutputLog&      gLog = _gLog;

/*--------------------------------------------------------------------------------------------*/
// global interfaces and handles
PluginHandle                    g_pluginHandle          = kPluginHandle_Invalid;    // identifier for this plugin, for internal use by obse
const OBSEInterface*            g_obseIntfc             = NULL; // master obse interface, for generating more specific intefaces below
OBSEMessagingInterface*         g_messagingIntfc        = NULL; // obse inter-plugin messaging interface
HMODULE                         g_hExportInjector       = 0;    // windows module handle for ExportInjector library
HMODULE                         g_hSubmodule            = 0;    // windows module handle for submodule dll

/*--------------------------------------------------------------------------------------------*/
// Messaging API
void OBSEMessageHandler(OBSEMessagingInterface::Message* msg)
{// registered during plugin load; recieves (event) messages from OBSE itself
    int x = 0;
	switch (msg->type)
	{
	case OBSEMessagingInterface::kMessage_ExitGame:
		_VMESSAGE("Received 'exit game' message");
		break;
	case OBSEMessagingInterface::kMessage_ExitToMainMenu:
		_VMESSAGE("Received 'exit game to main menu' message");
		break;
	case OBSEMessagingInterface::kMessage_PostLoad:
		_VMESSAGE("Received 'post-load' message"); 
		break;
	case OBSEMessagingInterface::kMessage_LoadGame:
	case OBSEMessagingInterface::kMessage_SaveGame:
		_VMESSAGE("Received 'save/load game' message with file path %s", msg->data);
		break;
	case OBSEMessagingInterface::kMessage_Precompile: 
		_VMESSAGE("Received 'pre-compile' message.");
		break;
	case OBSEMessagingInterface::kMessage_PreLoadGame:
		_VMESSAGE("Received 'pre-load game' message with file path %s", msg->data);
		break;
	case OBSEMessagingInterface::kMessage_ExitGame_Console:
		_VMESSAGE("Received 'quit game from console' message");
		break;
    case OBSEMessagingInterface::kMessage_PostLoadGame:
        _VMESSAGE("Received 'post-load game' message");
        break;
    case 9: // TODO - use appropriate constant
        _VMESSAGE("Received post-post-load message");
        break;
	default:
		_VMESSAGE("Received OBSE message type=%i from '%s' w/ data <%p> len=%04X", msg->type, msg->sender, msg->data, msg->dataLen);
		break;
	}
}

/*--------------------------------------------------------------------------------------------*/
// Test command definition
static ParamInfo kParams_ThreeOptionalStrings[3] =
{
	{	"stringA",	kParamType_String,	1 },
    {	"stringB",	kParamType_String,	1 },
    {	"stringC",	kParamType_String,	1 },
};
bool Cmd_coefBasicTest_Execute(COMMAND_ARGS)
{// handler for test command
    *result = 0; // initialize result
    char bufferA[0x200] = {{0}}; // initialize args
    char bufferB[0x200] = {{0}};
    char bufferC[0x200] = {{0}};
    ExtractArgs(PASS_EXTRACT_ARGS, bufferA, bufferB, bufferC);  // extract args from script environment
    // lookup execution code in submodule
    typedef void (* TestCmdFunc)(TESObjectREFR*,const char*, const char*, const char*);
    TestCmdFunc pCommand = (TestCmdFunc)GetProcAddress(g_hSubmodule,"COEFBasicTest");
    // call Submodule::COEFBasicTest() method
    if (pCommand) pCommand(thisObj,bufferA,bufferB,bufferC);   
    else _ERROR("Could not find Test command in submodule.");
    // done
    return true;
}
DEFINE_COMMAND_PLUGIN(coefBasicTest, "test command, accepts 3 optional string arguments", 0, 3, kParams_ThreeOptionalStrings)

/*--------------------------------------------------------------------------------------------*/
// OBSE plugin query
extern "C" bool _declspec(dllexport) OBSEPlugin_Query(const OBSEInterface* obse, PluginInfo* info)
{
    // attach html-formatted log file to loader output handler  
    gLog.AttachTarget(_gLogFile);     

	// fill out plugin info structure
	info->infoVersion = PluginInfo::kInfoVersion;   // info structure version
	info->name = SOLUTIONNAME;                      // plugin name
	info->version = PLUGINVERSION;                  // plugin version
    _MESSAGE("OBSE Query (CSMode = %i), v%08X",obse->isEditor,PLUGINVERSION);

    // check obse version
    g_obseIntfc = obse;
    if(obse->obseVersion < OBSE_VERSION_INTEGER)
	{
		_FATALERROR("OBSE version too old (got %08X expected at least %08X)", obse->obseVersion, OBSE_VERSION_INTEGER);
		return false;
	}
    
    // load ExportInjector library, which will automatically find *.eed files build an export table according to it's ini settings
    // ExportInjector *must* be loaded before the submodule, since loading the submodule will explicitly require the export table
    // Technically, only the first COEF-based plugin needs to perform this step; further LoadLibrary calls on ExportInjector.dll will do nothing
    _MESSAGE("Loading ExportInjector ...");
    g_hExportInjector = LoadLibrary("Data\\obse\\Plugins\\COEF\\API\\ExportInjector.dll");
    if (g_hExportInjector)
    {
        _DMESSAGE("ExportInjector loaded at <%p>", g_hExportInjector);
    }
    else
    {
        _FATALERROR("Could not load ExportInjector.dll.  Check that this file is installed correctly.");
        return false;
    }

    // load submodule 
    // the submodule dll contains the actual "meat" of the plugin, or as much of it as is based on COEF
    // in this example, there are two different submodules to choose from (CS vs Game, see Submodule.cpp for details)
    const char* modulename = obse->isEditor 
                            ? "Data\\obse\\plugins\\" SOLUTIONNAME "\\Submodule.CS.dll" 
                            : "Data\\obse\\plugins\\" SOLUTIONNAME "\\Submodule.Game.dll";
    _MESSAGE("Loading Submodule '%s' ...", modulename);
    g_hSubmodule = LoadLibrary(modulename);
    if (g_hSubmodule)
    {
        _DMESSAGE("Submodule loaded at <%p>", g_hSubmodule);
    }
    else
    {
        _FATALERROR("Could not load submodule '%s'.  Check that this file is installed correctly.", modulename);
        return false;
    }

	if(!obse->isEditor) // game-specific checks
	{		
        // check Game version
		if(obse->oblivionVersion != OBLIVION_VERSION)
		{
			_FATALERROR("Incorrect Oblivion version (got %08X need %08X)", obse->oblivionVersion, OBLIVION_VERSION);
			return false;
        }        
	}
	else    // CS specific checks
	{
		// check CS version
        if(obse->editorVersion != CS_VERSION)
		{
			_FATALERROR("TESCS version %08X, expected %08X)", obse->editorVersion, CS_VERSION);
			return false;
		}
	}

	// all checks pass, this plugin should continue to loading stage
	return true;
}

/*--------------------------------------------------------------------------------------------*/
// OBSE plugin load
extern "C" bool _declspec(dllexport) OBSEPlugin_Load(const OBSEInterface * obse)
{
	_MESSAGE("OBSE Load (CSMode = %i)",obse->isEditor);

    // get obse interface, plugin handle
    g_obseIntfc = obse;
	g_pluginHandle = obse->GetPluginHandle();

	// get messaging interface, register message listeners
	g_messagingIntfc = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);
	g_messagingIntfc->RegisterListener(g_pluginHandle, "OBSE", OBSEMessageHandler); // register to recieve messages from OBSE   

    // initialize submodule
    FARPROC pQuerySubmodule = GetProcAddress(g_hSubmodule,"Initialize"); // get pointer to Initialize() method exported by submodule
    if (pQuerySubmodule) pQuerySubmodule();   // call Submodule::Initialize() method
    else _ERROR("Could not initialize submodule.");

    // Register script commands
    _MESSAGE("Registering Commands from opcode base %04X ...", OPCODEBASE);
    g_obseIntfc->SetOpcodeBase(OPCODEBASE); // set opcode base
    g_obseIntfc->RegisterCommand(&kCommandInfo_coefBasicTest); // register test command

	return true;
}

/*--------------------------------------------------------------------------------------------*/
// Windows dll load
extern "C" BOOL WINAPI DllMain(HANDLE  hDllHandle, DWORD dwReason, LPVOID  lpreserved)
{// called when plugin is loaded into process memory, before obse takes over
	return true;
}