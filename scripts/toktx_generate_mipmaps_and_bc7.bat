@echo off
setlocal EnableDelayedExpansion

set "INPUT=%CD%\"
set "OUTPUT=%CD%\"

if not exist "%OUTPUT%" mkdir "%OUTPUT%"

for /R %%F in (*.png *.jpg *.jpeg *.tga *.bmp) do (

    set "FULL=%%~dpF"
    set "REL=!FULL:%INPUT%=!"

    if "!REL:~0,1!"=="\" set "REL=!REL:~1!"

    if not exist "%OUTPUT%\!REL!" mkdir "%OUTPUT%\!REL!"

    set "NAME=%%~nF"

    echo Converting %%~nxF
    echo Output folder: %OUTPUT%\!REL!

    if /I "!NAME:~-7!"=="_normal" (
        set "LINEAR=1"
    ) else if /I "!NAME:~-10!"=="_metalness" (
        set "LINEAR=1"
    ) else if /I "!NAME:~-9!"=="_metallic" (
        set "LINEAR=1"
    ) else if /I "!NAME:~-10!"=="_roughness" (
        set "LINEAR=1"
    ) else if /I "!NAME:~-3!"=="_ao" (
        set "LINEAR=1"
    ) else (
        set "LINEAR=0"
    )

    if "!LINEAR!"=="1" (
            toktx --t2 --genmipmap --encode uastc --uastc_quality 2 --uastc_rdo_l 1 --assign_oetf linear "%OUTPUT%\!REL!%%~nF.ktx2" "%%F"
    ) else (
            toktx --t2 --genmipmap --encode uastc --uastc_quality 2 --uastc_rdo_l 1 "%OUTPUT%\!REL!%%~nF.ktx2" "%%F"
    )
)

echo Done.
pause