@rem
@rem ----------------------------------------------------------------------------
@rem  Hvigor startup script for Windows, version 1.0.0
@rem
@rem  Required ENV vars:
@rem  ------------------
@rem    NODE_HOME - location of a Node home dir
@rem    or
@rem    Add %NODE_HOME%/bin to the PATH environment variable
@rem ----------------------------------------------------------------------------
@rem
@echo off

@rem Set local scope for the variables with windows NT shell
if "%OS%"=="Windows_NT" setlocal

set DIRNAME=%~dp0
if "%DIRNAME%" == "" set DIRNAME=.
set APP_BASE_NAME=%~n0
set APP_HOME=%DIRNAME%

set WRAPPER_MODULE_PATH=%APP_HOME%\hvigor\hvigor-wrapper.js
set NODE_EXE=node.exe
@rem set NODE_OPTS="--max-old-space-size=4096"

@rem Resolve any "." and ".." in APP_HOME to make it shorter.
for %%i in ("%APP_HOME%") do set APP_HOME=%%~fi

if not defined NODE_OPTS set NODE_OPTS="--"

@rem Find node.exe
if defined NODE_HOME (
  set NODE_HOME=%NODE_HOME:"=%
  set "PATH=%PATH%;%NODE_HOME%"
  set NODE_EXE_PATH=%NODE_HOME%/%NODE_EXE%
)

%NODE_EXE% --version >NUL 2>&1
if "%ERRORLEVEL%" == "0" (
  "%NODE_EXE%" "%NODE_OPTS%" "%WRAPPER_MODULE_PATH%" %*
) else if exist "%NODE_EXE_PATH%" (
  "%NODE_EXE_PATH%" "%NODE_OPTS%" "%WRAPPER_MODULE_PATH%" %*
) else (
  echo.
  echo ERROR: NODE_HOME is not set and no 'node' command could be found in your PATH.
  echo.
  echo Please set the NODE_HOME variable in your environment to match the
  echo location of your NodeJs installation.
)

if "%ERRORLEVEL%" == "0" (
  if "%OS%" == "Windows_NT" endlocal
) else (
  exit /b %ERRORLEVEL%
)