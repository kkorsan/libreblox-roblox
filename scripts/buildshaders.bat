@echo off
REM Batch equivalent of the bash script
REM No igncr workaround is needed in native Windows batch files

set "FOLDER=%~dp0"

REM Prefer python3 if available, otherwise fall back to python
where python3 >nul 2>nul
if %ERRORLEVEL%==0 (
  python3 "%FOLDER%buildshaders.py" "%FOLDER%..\engine\rendering\shaders"
) else (
  python "%FOLDER%buildshaders.py" "%FOLDER%..\engine\rendering\shaders"
)
