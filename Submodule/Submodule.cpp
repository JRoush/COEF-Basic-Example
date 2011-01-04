/*
    Main unit for submodules.  Handles:
    -   Initialization (writing hooks & patches)
    -   submodule debugging log
    -   Actual code (called by hooks)

    This example plugin has two submodule DLLs - one for the CS, and one for the game.  
    This is necessary because the game and CS use slightly different definitions for many COEF classes.
    However, there is (usually) a lot of overlap between the code for the two.
    The best all-around solution, to minimize redefinition and the hassle of similar but separate VS projects,
    is to use a single 'Submodule' project that compiles as the CS submodule under the 'Debug_CS' and 'Release_CS' 
    configurations, but compiles as the Game submodule under the 'Debug_Game' and 'Release_Game' configurations.

    IMPORTANT: this project must be compiled *twice*, once using a 'CS' configuration, and once using a 'Game'
    configuration.  One will generate a 'CS' dll, and the other a 'Game' dll. 
*/

/*--------------------------------------------------------------------------------------------*/
// macro for strings that use "CS" or "Game" depending on compiler state
#ifdef OBLIVION
    #define TARGETNAME "Game"
#else
    #define TARGETNAME "CS"
#endif

/*--------------------------------------------------------------------------------------------*/
// global debugging log for the submodule
HTMLTarget _gLogFile("Data\\obse\\plugins\\" SOLUTIONNAME "\\" SOLUTIONNAME "." TARGETNAME ".log.html", SOLUTIONNAME "." TARGETNAME ".Log");
OutputLog  _gLog;
OutputLog& gLog = _gLog;

/*--------------------------------------------------------------------------------------------*/
// global interfaces and handles
HMODULE hModule = 0;    // windows module handle ("instance") for this dll
char    sModuleName[0x100] = {{0}}; // name of this dll

/*--------------------------------------------------------------------------------------------*/
// submodule initialization
extern "C" _declspec(dllexport) void Initialize()
{   
    // begin initialization  
    _MESSAGE("Initializing Submodule {%p} '%s' ...", hModule, sModuleName); 

    // ... Perform hooks & patches here
    
    // initialization complete
    _DMESSAGE("Submodule initialization completed sucessfully");
}

/*--------------------------------------------------------------------------------------------*/
// Execution code for the lonely script function, 'coefBasicTest"
#include "API/TESForms/TESObjectREFR.h"
#include "API/BSTypes/BSStringT.h"
extern "C" _declspec(dllexport) void COEFBasicTest(TESObjectREFR* thisObj, const char* argA, const char* argB, const char* argC)
{/*
    This function is a stub, just to demonstrate the concept.  It uses the COEF API (note the #include statements above) to 
    generate a description string for the passed TESObjectREF.  This would be impossible if this submodule dll was not 
    dynamically linked to Oblivion.exe/TESConstructionSet.exe.  That's why this code must go here, in the submodule, rather than
    in the loader.
*/
    BSStringT desc;
    if (thisObj) thisObj->GetDebugDescription(desc);
    _DMESSAGE("Test command on <%p> '%s' w/ args A='%s' B='%s' C='%s'",thisObj,desc.c_str(),argA,argB,argC);
}

/*--------------------------------------------------------------------------------------------*/
// submodule loading
#ifndef MFC
// Project uses standard windows libraries, define an entry point for the DLL to handle loading/unloading
BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved)
{
    switch(dwReason)
    {
    case DLL_PROCESS_ATTACH:    // dll loaded
        // get submodule handle, name
        hModule = (HMODULE)hDllHandle;
        GetModuleFileName(hModule,sModuleName,sizeof(sModuleName));
        // attach log file to output handler
        gLog.AttachTarget(_gLogFile);        
        // done loading
        _MESSAGE("Attaching Submodule {%p} '%s' ...", hModule, sModuleName); 
        break;
    case DLL_PROCESS_DETACH:    // dll unloaded
        _MESSAGE("Detaching Submodule {%p} '%s' ...", hModule, sModuleName);      
        gLog.DetachTarget(_gLogFile);
        break;
    }   
    return true;
}
#else
// Project uses MFC, we define here an instance of CWinApp to make this a 'well-formed' DLL
class CSubmoduleApp : public CWinApp
{
public:
    virtual BOOL InitInstance()
    {// dll loaded
        // get submodule handle, name
        hModule = m_hInstance;
        GetModuleFileName(hModule,sModuleName,sizeof(sModuleName));
        // attach log file to output handler
        gLog.AttachTarget(_gLogFile);      
        // done loading
        _MESSAGE("Initializing Submodule {%p} '%s' ...", hModule, sModuleName); 
        return true;
    }
    virtual int ExitInstance() 
    {// dll unloaded
       _MESSAGE("Exiting Submodule {%p} '%s' ...", hModule, sModuleName);      
        gLog.DetachTarget(_gLogFile);
       return CWinApp::ExitInstance();
    }
} gApp;
#endif