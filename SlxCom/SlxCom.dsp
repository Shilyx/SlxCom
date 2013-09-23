# Microsoft Developer Studio Project File - Name="SlxCom" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=SlxCom - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SlxCom.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SlxCom.mak" CFG="SlxCom - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SlxCom - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "SlxCom - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SlxCom - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SLXCOM_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_UNICODE" /D "UNICODE" /D "_USRDLL" /D "SLXCOM_EXPORTS" /D _WIN32_WINNT=0x500 /D "ISOLATION_AWARE_ENABLED" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x804 /d "NDEBUG"
# ADD RSC /l 0x804 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /pdb:"../Release/SlxCom.pdb" /debug /machine:I386 /out:"../Release/SlxCom.dll"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "SlxCom - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SLXCOM_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_UNICODE" /D "UNICODE" /D "_USRDLL" /D "SLXCOM_EXPORTS" /D _WIN32_WINNT=0x500 /D "ISOLATION_AWARE_ENABLED" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x804 /d "_DEBUG"
# ADD RSC /l 0x804 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"../Debug/SlxCom.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "SlxCom - Win32 Release"
# Name "SlxCom - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\lib\charconv.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxCom.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxCom.def
# End Source File
# Begin Source File

SOURCE=.\SlxCom.rc
# ADD BASE RSC /l 0x804
# ADD RSC /l 0x804 /d "VC6"
# End Source File
# Begin Source File

SOURCE=.\SlxComConfig.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxComContextMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxComFactory.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxComOverlay.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxComPeTools.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxComTools.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxComWork.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxComWork_lvm.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxComWork_sd.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxFileHandles.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxManualCheckSignature.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxStringStatusCache.cpp
# End Source File
# Begin Source File

SOURCE=.\SlxUnlockFile.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\lib\charconv.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\SlxComContextMenu.h
# End Source File
# Begin Source File

SOURCE=.\SlxComFactory.h
# End Source File
# Begin Source File

SOURCE=.\SlxComOverlay.h
# End Source File
# Begin Source File

SOURCE=.\SlxComPeTools.h
# End Source File
# Begin Source File

SOURCE=.\SlxComTools.h
# End Source File
# Begin Source File

SOURCE=.\SlxComWork.h
# End Source File
# Begin Source File

SOURCE=.\SlxComWork_lvm.h
# End Source File
# Begin Source File

SOURCE=.\SlxComWork_sd.h
# End Source File
# Begin Source File

SOURCE=.\SlxFileHandles.h
# End Source File
# Begin Source File

SOURCE=.\SlxManualCheckSignature.h
# End Source File
# Begin Source File

SOURCE=.\SlxString.h
# End Source File
# Begin Source File

SOURCE=.\SlxStringStatusCache.h
# End Source File
# Begin Source File

SOURCE=.\SlxUnlockFile.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\AddToCopy.bmp
# End Source File
# Begin Source File

SOURCE=.\res\CopyPictureHtml.bmp
# End Source File
# Begin Source File

SOURCE=.\res\Default.jpg
# End Source File
# Begin Source File

SOURCE=.\res\default1.bin
# End Source File
# Begin Source File

SOURCE=.\res\Main.ico
# End Source File
# Begin Source File

SOURCE=.\res\Mark.ico
# End Source File
# End Group
# Begin Source File

SOURCE=.\SlxCom.manifest
# End Source File
# Begin Source File

SOURCE=.\WindowsXP.manifest
# End Source File
# End Target
# End Project
