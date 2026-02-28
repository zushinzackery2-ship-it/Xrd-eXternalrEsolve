@echo off
call "D:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
cd /d "%~dp0"
if exist test_dump.obj del test_dump.obj
if exist test_dump.exe del test_dump.exe
cl /std:c++20 /EHsc /O2 /MT /I"include" /Fe:test_dump.exe test_dump.cpp /link /MACHINE:X64
