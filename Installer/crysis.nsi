!include "MUI2.nsh"
!define VERSION '0.5.0'

Name "Crysis VR Mod"
; should not need admin privileges as the install folder should be user writable, anyway
RequestExecutionLevel user

OutFile ".\crysis-vrmod-${VERSION}.exe"

!define MUI_ICON '.\Crysis_101.ico'

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP ".\header.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP ".\welcome.bmp"
!define MUI_WELCOMEPAGE_TITLE 'Crysis VR Mod v${VERSION}'
!define MUI_WELCOMEPAGE_TEXT 'This will install the Crysis VR Mod on your computer. \
Please be aware that this is an early development version, and bugs are to be expected. \
It is a seated-only experience for now with limited motion controller support. \
Also, it is Crysis - expect performance to not be great :)'

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

	SetOutPath $INSTDIR
	File .\assembly\system.vrsample.cfg

	; remove wrapper dlls from previous builds as they may now conflict
	Delete $INSTDIR\Bin64\d3d10.dll
	Delete $INSTDIR\Bin64\dxgi.dll
	Delete $INSTDIR\LaunchVRMod.bat

	WriteUninstaller "$INSTDIR\Uninstall_CrysisVR.exe"
SectionEnd

Section "Start menu shortcut"
	CreateShortcut "$SMPrograms\$(^Name).lnk" "$InstDir\Bin64\CrysisVR.exe"
SectionEnd

Section /o "Desktop shortcut"
	CreateShortcut "$Desktop\$(^Name).lnk" "$InstDir\Bin64\CrysisVR.exe"
SectionEnd

Section "Uninstall"
	SetOutPath $INSTDIR
	RMDir /r "$INSTDIR\Mods\VRMod"
	Delete "$INSTDIR\Bin64\CrysisVR.exe"
	Delete "$SMPrograms\$(^Name).lnk"
	Delete "$Desktop\$(^Name).lnk"
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
