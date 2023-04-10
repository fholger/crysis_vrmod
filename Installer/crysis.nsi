!include "MUI2.nsh"
!define VERSION '0.1.2'

Name "Crysis VR Mod"

OutFile ".\crysis-vrmod-${VERSION}.exe"

!define MUI_ICON '.\Crysis_101.ico'

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP ".\header.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP ".\welcome.bmp"
!define MUI_WELCOMEPAGE_TITLE 'Crysis VR Mod v${VERSION}'
!define MUI_WELCOMEPAGE_TEXT 'This will install the Crysis VR Mod on your computer. \
Please be aware that this is an early development version, and bugs are to be expected. \
It is a seated-only experience for now without any motion controller support. \
Also, it is Crysis - expect performance to not be great :)'

!define MUI_DIRECTORYPAGE_TEXT 'Please enter the location of your Crysis installation.'

!define MUI_FINISHPAGE_TITLE 'Installation complete.'
!define MUI_FINISHPAGE_TEXT 'You can launch the VR Mod by running LaunchVRMod.bat from your Crysis install directory.'


!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "" 
	SetOutPath $INSTDIR\Mods\VRMod
	File /r .\assembly\Mods\VRMod\*

	SetOutPath $INSTDIR\Bin64
	File .\assembly\Bin64\*

	SetOutPath $INSTDIR
	File .\assembly\LaunchVRMod.bat
SectionEnd

Function .onInit
	StrCpy $INSTDIR "C:\Program Files (x86)\Steam\steamapps\common\Crysis"
FunctionEnd
