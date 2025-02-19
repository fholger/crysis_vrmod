!include "MUI2.nsh"
!define VERSION '1.1.1'

Name "Crysis VR"
; should not need admin privileges as the install folder should be user writable, anyway
RequestExecutionLevel user

OutFile ".\crysis-vrmod-${VERSION}.exe"

!define MUI_ICON '.\Crysis_101.ico'

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP ".\header.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP ".\welcome.bmp"
!define MUI_WELCOMEPAGE_TITLE 'Crysis VR v${VERSION}'
!define MUI_WELCOMEPAGE_TEXT 'This will install the Crysis VR Mod on your computer. \
It will turn the game into a full 6DOF tracked VR experience with motion controls. \
But remember, it is Crysis - expect performance to not be great :)'

!define MUI_DIRECTORYPAGE_TEXT 'Please enter the location of your Crysis installation.'

!define MUI_FINISHPAGE_TITLE 'Installation complete.'
!define MUI_FINISHPAGE_TEXT 'You can launch the VR Mod by running Bin64\CrysisVR.exe from your Crysis install directory.'


!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "" 
	SetOutPath $INSTDIR\Mods\VRMod
	File /r .\assembly\Mods\VRMod\*

	SetOutPath $INSTDIR\Bin64
	File .\assembly\Bin64\*

	SetOutPath $INSTDIR\Bin32
	File .\assembly\Bin32\*

	WriteUninstaller "$INSTDIR\Uninstall_CrysisVR.exe"
SectionEnd

Section "Start menu shortcut"
	SetOutPath $INSTDIR\Bin64
	CreateShortcut "$SMPrograms\$(^Name) (64 bit).lnk" "$InstDir\Bin64\CrysisVR.exe"
	SetOutPath $INSTDIR\Bin32
	CreateShortcut "$SMPrograms\$(^Name) (32 bit).lnk" "$InstDir\Bin32\CrysisVR.exe"
SectionEnd

Section /o "Desktop shortcut"
	SetOutPath $INSTDIR\Bin64
	CreateShortcut "$Desktop\$(^Name) (64 bit).lnk" "$InstDir\Bin64\CrysisVR.exe"
	SetOutPath $INSTDIR\Bin32
	CreateShortcut "$Desktop\$(^Name) (32 bit).lnk" "$InstDir\Bin32\CrysisVR.exe"
SectionEnd

Section "Uninstall"
	SetOutPath $INSTDIR
	RMDir /r "$INSTDIR\Mods\VRMod"
	Delete "$INSTDIR\Bin64\CrysisVR.exe"
	Delete "$INSTDIR\Bin64\haptic_library.dll"
	Delete "$INSTDIR\Bin64\ForceTubeVR_API_x64.dll"
	Delete "$INSTDIR\Bin32\CrysisVR.exe"
	Delete "$INSTDIR\Bin32\haptic_library.dll"
	Delete "$INSTDIR\Bin32\ForceTubeVR_API_x32.dll"
	Delete "$SMPrograms\$(^Name) (64 bit).lnk"
	Delete "$SMPrograms\$(^Name) (32 bit).lnk"
	Delete "$Desktop\$(^Name) (64 bit).lnk"
	Delete "$Desktop\$(^Name) (32 bit).lnk"
	Delete "Uninstall_CrysisVR.exe"
SectionEnd

Function .onInit
	; try to look up install directory for the GOG.com version of Crysis
	SetRegView 32
	ReadRegStr $R0 HKLM "Software\GOG.com\Games\1809223221" "path"
	IfErrors lbl_checksteam 0
	StrCpy $INSTDIR $R0
	Return
	
	lbl_checksteam:
	; try to look up install directory for the Steam version of Crysis
	SetRegView 64
	ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 17300" "InstallLocation"
	IfErrors lbl_fallback 0
	StrCpy $INSTDIR $R0
	Return
	
	lbl_fallback:
	StrCpy $INSTDIR "$PROGRAMFILES32\Steam\steamapps\common\Crysis"
FunctionEnd
