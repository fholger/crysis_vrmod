set CURRENT_DIR=%~dp0
set PACKAGE_DIR=%CURRENT_DIR%\assembly
if exist %PACKAGE_DIR% (rmdir /S /Q %PACKAGE_DIR%)

mkdir %PACKAGE_DIR%\Bin64
mkdir %PACKAGE_DIR%\Bin32

set CRYSIS_INSTALL_DIR=%PACKAGE_DIR%
call %CURRENT_DIR%\..\install.bat
call %CURRENT_DIR%\..\install32.bat
call "C:\Program Files (x86)\NSIS\makensis.exe" crysis.nsi
