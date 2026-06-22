@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

rem launch.bat -- Lanceur interactif du simulateur Artouste (Windows).
rem
rem Affiche un menu qui demande la carte à charger, puis si la turbine et le rotor
rem doivent être déjà démarrés au lancement. Les choix sont transmis par les
rem variables d'environnement ARTOUSTE_TERRAIN et ARTOUSTE_TURBINE_DEMARREE, qui
rem ont priorité sur assets\config.txt : le fichier de configuration n'est pas
rem modifié.
rem
rem Usage :
rem   launch.bat        Menu interactif puis lancement du simulateur.
rem   launch.bat -n     Essai à blanc : affiche les choix sans lancer la fenêtre.
rem   launch.bat -h     Affiche cette aide.

rem Se placer à la racine du dépôt (le dossier de ce script).
cd /d "%~dp0"

set "TERRAIN_DIR=assets\terrain"
set "DRY_RUN=0"

rem Analyse de l'argument éventuel.
if "%~1"=="" goto args_done
if /i "%~1"=="-n"        ( set "DRY_RUN=1" & goto args_done )
if /i "%~1"=="--dry-run" ( set "DRY_RUN=1" & goto args_done )
if /i "%~1"=="-h"        goto show_help
if /i "%~1"=="--help"    goto show_help
if    "%~1"=="/?"        goto show_help
echo Option inconnue : %~1 ^(voir launch.bat -h^).
exit /b 1
:args_done

rem Trouver le binaire compilé (l'emplacement varie selon le générateur CMake).
set "BIN="
for %%P in (
    "build\bin\Release\artouste.exe"
    "build\bin\RelWithDebInfo\artouste.exe"
    "build\bin\Debug\artouste.exe"
    "build\bin\artouste.exe"
    "build\Release\artouste.exe"
    "build\artouste.exe"
) do if not defined BIN if exist "%%~P" set "BIN=%%~P"

if not defined BIN (
    echo Le simulateur n'est pas compilé ^(artouste.exe introuvable^).
    echo Compile-le d'abord, par exemple :
    echo     cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    echo     cmake --build build --config Release -j
    exit /b 1
)

rem Découvrir les cartes : tout sous-dossier de assets\terrain qui contient un
rem terrain.txt. Le titre lisible est la première ligne de ce fichier, sans son
rem préfixe de commentaire.
set "n=0"
for /d %%D in ("%TERRAIN_DIR%\*") do (
    if exist "%%D\terrain.txt" (
        set /a n+=1
        set "map[!n!]=%%~nxD"
        set "title[!n!]=%%~nxD"
        set "firstline="
        set /p "firstline=" < "%%D\terrain.txt"
        if defined firstline set "title[!n!]=!firstline:# Terrain Artouste - =!"
    )
)

if "%n%"=="0" (
    echo Aucune carte trouvée dans %TERRAIN_DIR%\.
    exit /b 1
)

echo ==============================================================
echo  Lanceur du simulateur Artouste (Alouette II)
echo ==============================================================
echo.
echo Cartes disponibles :
for /l %%i in (1,1,%n%) do echo   %%i^) !map[%%i]!  -  !title[%%i]!
echo.

:ask_map
set "choice="
set /p "choice=Numéro de la carte [1-%n%] (défaut 1) : "
if not defined choice set "choice=1"
echo !choice!| findstr /r "^[1-9][0-9]*$" >nul
if errorlevel 1 ( echo Choix invalide, recommence. & goto ask_map )
if !choice! gtr %n% ( echo Choix invalide, recommence. & goto ask_map )

set "terrain=!map[%choice%]!"
set "seltitle=!title[%choice%]!"

echo.
set "rep="
set /p "rep=Turbine et rotor déjà démarrés au lancement ? [o/N] : "
set "turbine=0"
if /i "!rep!"=="o"   set "turbine=1"
if /i "!rep!"=="oui" set "turbine=1"
if /i "!rep!"=="y"   set "turbine=1"
if /i "!rep!"=="yes" set "turbine=1"

echo.
echo --------------------------------------------------------------
echo  Carte            : !terrain! ^(!seltitle!^)
if "!turbine!"=="1" (
    echo  Turbine et rotor : démarrés ^(décollage immédiat^)
) else (
    echo  Turbine et rotor : à froid ^(séquence de démarrage^)
)
echo --------------------------------------------------------------

set "ARTOUSTE_TERRAIN=!terrain!"
set "ARTOUSTE_TURBINE_DEMARREE=!turbine!"

if "%DRY_RUN%"=="1" (
    echo  ^(essai à blanc^) Commande qui serait lancée :
    echo      set ARTOUSTE_TERRAIN=!terrain! ^& set ARTOUSTE_TURBINE_DEMARREE=!turbine! ^& "!BIN!"
    exit /b 0
)

echo  Lancement...
"!BIN!"
exit /b %errorlevel%

:show_help
echo launch.bat -- Lanceur interactif du simulateur Artouste (Windows).
echo.
echo Affiche un menu qui demande la carte à charger, puis si la turbine et le
echo rotor doivent être déjà démarrés au lancement. Les choix sont transmis par
echo les variables ARTOUSTE_TERRAIN et ARTOUSTE_TURBINE_DEMARREE, qui ont
echo priorité sur assets\config.txt : le fichier de configuration n'est pas modifié.
echo.
echo Usage :
echo   launch.bat        Menu interactif puis lancement du simulateur.
echo   launch.bat -n     Essai à blanc : affiche les choix sans lancer la fenêtre.
echo   launch.bat -h     Affiche cette aide.
exit /b 0
