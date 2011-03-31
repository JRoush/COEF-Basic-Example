The 'COEF Basic Example' plugin demonstrates the following concepts for COEF projects:
-   "1 loader, 2 submodule" project structure
-   Loading ExportInjector to enable dynamic linking to Oblivion.exe & TESConstructionSet.exe
-   Loading a submodule and communicating with it via LoadLibrary() and GetProcAddress()
-   Separating the execution of script commands from the extraction & validation of arguments
-   The basic layout for DllMain() & CWinApp
-   Basic useage of the OutputLog utility

Requirements for Developers:
============================
1.  Common Oblivion Engine Framework
2.  Oblivion Script Extender, v19 or later
3.  A TR1 compliant version of Visual Studio (2008 with TR1 extras installed, or 2010 or later)

Setup Instructions for Developers:
==================================
1.  Extract the archive to a working directory for development
2.  Open COEF_BasicExample.sln in a text editor and find the line beginning with
        Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "COEF"
    Enter the path (relative or absolute) to COEF.vcproj, found in your COEF directory    
3.  Open EnvPaths.vsprops in a text editor, and change the paths it contains:
        COEFPATH - the path (relative or absolute) of your COEF directory
        OBSEPATH - the path (relative or absolute) to the OBSE source code directory containing 'Common' and 'OBSE' subdirectories
        OBLIVIONPATH - the path to your Oblivion\ directory
4.  Create an Oblivion\Data\OBSE\Plugins\COEF_BasicExample folder
5.  In your working directory, open COEF_BasicExample.sln in Visual Studio and begin development.  You may get an error the first time you
    compile the solution, compile it at least twice before troubleshooting.
NOTE: you can change the name of your project, and the resulting plugin, by renaming COEF_BasicExample.sln.  Make sure that you create
a corresponding folder for the output in Oblivion\Data\OBSE\Plugins\.