@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
REM WinINet backend (default):
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64\cl.exe" /c /FA /GS- http_porxy.c
REM WinHTTP backend: add /DUSE_WINHTTP to the command above