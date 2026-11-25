@echo off
rem Default game server deploy for the editor's Vedg tab (a project-root deploy.bat overrides it).
rem Builds the project's Dockerfile, pushes it to the registry Vedg returned, and prints VEDG_IMAGE=<image>.
rem Args: <repository> <registry> <region>. Run from the project root.
set REPOSITORY=%~1
set REGISTRY=%~2
set REGION=%~3
if "%REPOSITORY%"=="" (
	echo Missing image repository: configure it in Vedg Admin ^> Builds
	exit /b 1
)
if not exist Dockerfile (
	echo No Dockerfile at the project root: add one that packages the dedicated server, or provide a deploy.bat
	exit /b 1
)

set VERSION=0
for /f "tokens=2 delims==" %%v in ('findstr /b "ProjectVersion=" Config\DefaultGame.ini') do set VERSION=%%~v
set SHA=nogit
for /f %%s in ('git rev-parse --short HEAD 2^>nul') do set SHA=%%s
for /f %%t in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMddHHmmss"') do set STAMP=%%t
set IMAGE=%REPOSITORY%:%VERSION%-%SHA%-%STAMP%

if not "%REGION%"=="" (
	aws sts get-caller-identity >nul 2>&1 || aws login
	rem Docker Desktop's bundled ecr-login helper cannot read an "aws login" session, only env credentials
	for /f "delims=" %%l in ('aws configure export-credentials --format windows-cmd') do %%l
	findstr /c:"\"%REGISTRY%\": \"ecr-login\"" "%USERPROFILE%\.docker\config.json" >nul 2>&1
	if errorlevel 1 (
		echo Logging in to %REGISTRY%
		aws ecr get-login-password --region %REGION% | docker login --username AWS --password-stdin %REGISTRY%
	)
	if errorlevel 1 exit /b 1
)

echo Building %IMAGE%
docker build --provenance=false -t %IMAGE% .
if errorlevel 1 exit /b 1

echo Pushing %IMAGE%
docker push --quiet %IMAGE%
if errorlevel 1 exit /b 1

echo VEDG_IMAGE=%IMAGE%
