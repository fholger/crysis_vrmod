@echo off
setlocal EnableDelayedExpansion

if "%CRYSIS_INSTALL_DIR%"=="" (goto err_missing_env)

set SOURCE_DIR=%~dp0
set VRMOD_INSTALL_DIR=%CRYSIS_INSTALL_DIR%\Mods\VRMod
if not exist %VRMOD_INSTALL_DIR% (mkdir %VRMOD_INSTALL_DIR%)

xcopy /i /y %SOURCE_DIR%\*.xml %VRMOD_INSTALL_DIR%
xcopy /y %SOURCE_DIR%\Bin64\VRMod.dll %VRMOD_INSTALL_DIR%\Bin64
rem xcopy /y %SOURCE_DIR%\Bin64\dxgi.dll %CRYSIS_INSTALL_DIR%\Bin64
rem xcopy /y %SOURCE_DIR%\Bin64\d3d10.dll %CRYSIS_INSTALL_DIR%\Bin64
xcopy /y %SOURCE_DIR%\Bin64\openvr_api.dll %CRYSIS_INSTALL_DIR%\Bin64
xcopy /y %SOURCE_DIR%\LaunchVRMod.bat %VRMOD_INSTALL_DIR%

goto eof

:err_missing_env
echo CRYSIS_INSTALL_DIR was not set, aborting
exit 1

:eof
