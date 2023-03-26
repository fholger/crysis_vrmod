@echo off
setlocal EnableDelayedExpansion

if "%CRYSIS_INSTALL_DIR%"=="" (goto err_missing_env)

set SOURCE_DIR=%~dp0
set VRMOD_INSTALL_DIR=%CRYSIS_INSTALL_DIR%\Mods\VRMod
if not exist %VRMOD_INSTALL_DIR% (mkdir %VRMOD_INSTALL_DIR%)

xcopy /i /y %SOURCE_DIR%\*.xml %VRMOD_INSTALL_DIR%
xcopy /i /y %SOURCE_DIR%\Bin64\*.dll %VRMOD_INSTALL_DIR%\Bin64

goto eof

:err_missing_env
echo CRYSIS_INSTALL_DIR was not set, aborting
exit 1

:eof
