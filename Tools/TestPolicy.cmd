@echo off
call "C:\Workshops\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 "Plugins\HorrorSystems\Tests\PolicyTests.cpp" /Fo"Saved\Verification\PolicyTests.obj" /Fe"Saved\Verification\PolicyTests.exe"
if errorlevel 1 exit /b 1
"Saved\Verification\PolicyTests.exe"
exit /b %errorlevel%
