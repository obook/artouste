@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

rem build.bat -- Compilation du simulateur Artouste sous Windows.
rem
rem Equivalent de build.sh : configure CMake puis compile. libcurl est une
rem dependance OBLIGATOIRE (radio internet du cockpit) ; si CMake ne la trouve
rem pas, la configuration echoue et ce script s'arrete avec un message clair.
rem
rem Usage :
rem   build.bat              Configure et compile en Release.
rem   build.bat -t Debug     Choisit le type de build (defaut : Release).
rem   build.bat -c           Nettoie d'abord le repertoire de build.
rem   build.bat -r           Lance le simulateur apres compilation (via launch.bat).
rem   build.bat -h           Affiche cette aide.

rem Se placer a la racine du depot (le dossier de ce script).
cd /d "%~dp0"

set "BUILD_DIR=build"
set "BUILD_TYPE=Release"
set "DO_CLEAN=0"
set "DO_RUN=0"

:parse
if "%~1"=="" goto parse_done
if /i "%~1"=="-h"     goto show_help
if /i "%~1"=="--help" goto show_help
if    "%~1"=="/?"     goto show_help
if /i "%~1"=="-c"     ( set "DO_CLEAN=1" & shift & goto parse )
if /i "%~1"=="-r"     ( set "DO_RUN=1"   & shift & goto parse )
if /i "%~1"=="-t"     ( set "BUILD_TYPE=%~2" & shift & shift & goto parse )
echo Option inconnue : %~1 ^(voir build.bat -h^).
exit /b 1
:parse_done

echo ==============================================================
echo  Compilation du simulateur Artouste (Alouette II) - Windows
echo --------------------------------------------------------------
echo  Type de build       : %BUILD_TYPE%
echo  Repertoire de build : %BUILD_DIR%
echo  Les dependances tierces (GLFW, GLM, ImGui, Assimp, ...) sont
echo  recuperees automatiquement au premier lancement : sois patient.
echo ==============================================================

if "%DO_CLEAN%"=="1" (
    echo ^>^> Nettoyage de %BUILD_DIR%
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

echo ^>^> Configuration (%BUILD_TYPE%)
cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo.
    echo ERREUR : la configuration CMake a echoue.
    echo Cause probable : libcurl introuvable ^(dependance obligatoire pour la
    echo radio internet du cockpit^). Installe-la, par exemple avec vcpkg :
    echo     vcpkg install curl
    echo puis relance en fournissant la toolchain vcpkg a CMake, ou compile
    echo depuis Visual Studio avec l'integration vcpkg activee.
    exit /b 1
)

echo ^>^> Compilation
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% -j
if errorlevel 1 (
    echo ERREUR : la compilation a echoue.
    exit /b 1
)

echo ==============================================================
echo  Compilation terminee avec succes.
echo  Pour lancer le simulateur :  launch.bat
echo ==============================================================

if "%DO_RUN%"=="1" (
    echo ^>^> Lancement du simulateur
    call launch.bat
)
exit /b 0

:show_help
echo build.bat -- Compilation du simulateur Artouste sous Windows.
echo.
echo Usage :
echo   build.bat              Configure et compile en Release.
echo   build.bat -t Debug     Choisit le type de build (defaut : Release).
echo   build.bat -c           Nettoie d'abord le repertoire de build.
echo   build.bat -r           Lance le simulateur apres compilation (via launch.bat).
echo   build.bat -h           Affiche cette aide.
exit /b 0
