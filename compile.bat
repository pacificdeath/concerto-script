@echo off
setlocal enabledelayedexpansion

set raylib=.\raylib-5.0_win64_mingw-w64\
set raylib_include=%raylib%include\
set raylib_lib=%raylib%lib\

set src=.\src\
set windows_wrapper_src=%src%windows_wrapper.c
set main_src=%src%main.c

set build=.\build\
set exe=%build%concerto-script.exe
set gdb_init=.\gdb-init.txt

set run=0
set release=0
set gdb=0
set test=0
set verbose=0

if not exist %build% (
    mkdir %build%
)

for %%x in (%*) do (
    if "%%x"=="help" (
        goto :help
    ) else if "%%x"=="run" (
        set run=1
    ) else if "%%x"=="release" (
        set release=1
    ) else if "%%x"=="test" (
        set test=1
    ) else if "%%x"=="gdb" (
        set gdb=1
    ) else if "%%x"=="verbose" (
        set verbose=1
    )
)

set "gcc_flags="
if !release!==1 (set gcc_flags=!gcc_flags! -DNDEBUG) else (set gcc_flags=!gcc_flags! -g -DDEBUG)
if !test!==1 (set gcc_flags=!gcc_flags! -DTEST)
if !verbose!==1 (set gcc_flags=!gcc_flags! -DVERBOSE)

gcc ^
    !gcc_flags! ^
    %windows_wrapper_src% ^
    %main_src% ^
    -o%exe% ^
    -O0 ^
    -Wall -Wextra -Wpedantic ^
    -std=c99 ^
    -I%raylib_include% ^
    -L%raylib_lib% ^
    -lraylib -lopengl32 -lgdi32 -lwinmm

if !errorlevel! equ 0 (
    if !gdb!==1 (
        gdb --command %gdb_init% --args %exe%
    ) else if !run!==1 (
        %exe%
    )
)

exit !errorlevel!

:help
    echo %0 ^[Options^]
    echo Options:
    echo    help        this thing
    echo    run         run executable after compilation
    echo    release     add asserts and debug symbols gcc
    echo    gdb         run gdb after compilation
    echo    test        executable will be set up to run tests
exit 0

