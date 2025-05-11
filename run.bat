@echo off
setlocal enabledelayedexpansion

set raylib=.\raylib-5.0_win64_mingw-w64\
set raylib_include=%raylib%include\
set raylib_lib=%raylib%lib\

set src=.\src\
set windows_wrapper_src=%src%windows_wrapper.c
set compiler_src=%src%compiler\compiler.c
set editor_src=%src%editor\editor.c
set main_src=%src%main.c

set build=.\build\
set exe=%build%concerto-script.exe

set d="-g -DDEBUG"
set compile_only=0
set debug=""
set gdb=0
set test_thread=""

if not exist %build% (
    mkdir %build%
)

for %%x in (%*) do (
    if "%%x"=="help" (
        goto :help
    ) else if "%%x"=="c" (
        set COMPILE_ONLY=1
    ) else if "%%x"=="d" (
        set DEBUG=%D%
    ) else if "%%x"=="g" (
        set GDB=1
        set DEBUG=%D%
    ) else if "%%x"=="test-thread" (
        set DEBUG=%D%
        set TEST_THREAD="-DTEST_THREAD"
    )
)

set debug=%debug:"=%
set test_thread=%test_thread:"=%

gcc ^
    !debug! ^
    %windows_wrapper_src% %compiler_src% %editor_src% %main_src% ^
    -o%exe% ^
    -O0 ^
    -Wall -Wextra -Wconversion ^
    -std=c99 ^
    -I%raylib_include% ^
    -L%raylib_lib% ^
    -lraylib -lopengl32 -lgdi32 -lwinmm

if %errorlevel% equ 0 (
    if !compile_only!==1 (
        exit 0
    ) else if !gdb!==1 (
        gdb --args %exe%
    ) else (
        %exe%
    )
)

exit %errorlevel%

:help
    echo %0 ^[Options^]
    echo Options:
    echo    help           this thing
    echo    c              compile only
    echo    d              enable debug
    echo    g              run gdb after compiliation
    echo    test-thread    test multithreading
exit 0

