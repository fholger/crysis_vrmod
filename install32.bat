@echo off
setlocal EnableDelayedExpansion

if "%CRYSIS_INSTALL_DIR%"=="" (goto err_missing_env)

set SOURCE_DIR=%~dp0
set VRMOD_INSTALL_DIR=%CRYSIS_INSTALL_DIR%\Mods\VRMod
if not exist %VRMOD_INSTALL_DIR%\Bin32 (mkdir %VRMOD_INSTALL_DIR%\Bin32)

xcopy /i /y %SOURCE_DIR%\*.xml %VRMOD_INSTALL_DIR%
xcopy /y %SOURCE_DIR%\Bin32\VRMod.dll %VRMOD_INSTALL_DIR%\Bin32
xcopy /y %SOURCE_DIR%\Bin32\CrysisVR.exe %CRYSIS_INSTALL_DIR%\Bin32
xcopy /y %SOURCE_DIR%\Code\ThirdParty\bhaptics\bin\win32\haptic_library.dll %CRYSIS_INSTALL_DIR%\Bin32
xcopy /i /s /y %SOURCE_DIR%\Game %VRMOD_INSTALL_DIR%\Game
xcopy /y %SOURCE_DIR%\README.md %VRMOD_INSTALL_DIR%
xcopy /y %SOURCE_DIR%\LICENSE.txt %VRMOD_INSTALL_DIR%

goto eof

:err_missing_env
echo CRYSIS_INSTALL_DIR was not set, aborting
exit 1

:eof
