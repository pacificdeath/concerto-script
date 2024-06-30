@echo off
setlocal enabledelayedexpansion

set D="-g -DDEBUG"
set PROGRAM=""
set DEBUG=""
set GDB=0
set TEST_THREAD=""
set LIST_TOKENS=""
set LIST_FREQUENCIES=""

for %%x in (%*) do (
    if "%%x"=="-help" (
        goto :help
    ) else if "%%x"=="-debug" (
        set DEBUG=%D%
    ) else if "%%x"=="-gdb" (
        set GDB=1
        set DEBUG=%D%
    ) else if "%%x"=="-test-thread" (
        set DEBUG=%D%
        set TEST_THREAD="-DTEST_THREAD"
    ) else if "%%x"=="-list-token" (
        set DEBUG=%D%
        set LIST_TOKENS="-DLIST_TOKENS"
    ) else if "%%x"=="-list-freq" (
        set DEBUG=%D%
        set LIST_FREQUENCIES="-DLIST_FREQUENCIES"
    ) else (
        set PROGRAM="%%x"
    )
)

set DEBUG=%DEBUG:"=%
set TEST_THREAD=%TEST_THREAD:"=%
set LIST_TOKENS=%LIST_TOKENS:"=%
set LIST_FREQUENCIES=%LIST_FREQUENCIES:"=%

gcc -c ^
    !DEBUG! ^
    ./src/bird_windows.c ^
    -o bird_windows.o

if not %errorlevel% equ 0 (
    echo compilation of bird_windows.c failed
    goto :end
)

gcc -c ^
    !DEBUG! ^
    !TEST_THREAD! ^
    !LIST_TOKENS! ^
    !LIST_FREQUENCIES! ^
    ./src/main.c ^
    -o main.o ^
    -I./raylib-5.0_win64_mingw-w64/include/

if not %errorlevel% equ 0 (
    echo compilation of main.c failed
    goto :end
)

gcc ^
    main.o ^
    bird_windows.o ^
    -o bird.exe ^
    -O0 ^
    -Wall ^
    -std=c99 ^
    -L./raylib-5.0_win64_mingw-w64/lib/ ^
    -lraylib ^
    -lopengl32 ^
    -lgdi32 ^
    -lwinmm

if not %errorlevel% equ 0 (
    echo compilation of bird.exe failed
    goto :end
)

if %errorlevel% equ 0 (
    if !GDB!==1 (
        gdb bird.exe
    ) else (
        bird.exe !PROGRAM!
    )
)
goto :end

:help
    echo %0 ^[Options^] ^<Program File^>
    echo Options:
    echo    -help           this thing
    echo    -debug          enable debug
    echo    -gdb            run gdb after compiliation
    echo    -test-thread    test multithreading
    echo    -list-tok       print internal compiler tokens to console
    echo    -list-freq      print internal compiler frequencies to console
goto :end

:end