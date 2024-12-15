@echo off
setlocal enabledelayedexpansion

set D="-g -DDEBUG"
set COMPILE_ONLY=0
set DEBUG=""
set GDB=0
set TEST_THREAD=""

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

set DEBUG=%DEBUG:"=%
set TEST_THREAD=%TEST_THREAD:"=%

gcc -c ^
    !DEBUG! ^
    ./src/windows_wrapper.c ^
    -o windows_wrapper.o

if not %errorlevel% equ 0 (
    echo compilation of windows_wrapper.c failed
    goto :end
)

gcc -c ^
    !DEBUG! ^
    ./src/compiler/compiler.c ^
    -o compiler.o ^
    -I./raylib-5.0_win64_mingw-w64/include/

if not %errorlevel% equ 0 (
    echo compilation of compiler.c failed
    goto :end
)

gcc -c ^
    !DEBUG! ^
    !TEST_THREAD! ^
    ./src/main.c ^
    -o main.o ^
    -I./raylib-5.0_win64_mingw-w64/include/

if not %errorlevel% equ 0 (
    echo compilation of main.c failed
    goto :end
)

gcc ^
    windows_wrapper.o ^
    compiler.o ^
    main.o ^
    -o main.exe ^
    -O0 ^
    -Wall ^
    -Wextra ^
    -Wconversion ^
    -std=c99 ^
    -L./raylib-5.0_win64_mingw-w64/lib/ ^
    -lraylib ^
    -lopengl32 ^
    -lgdi32 ^
    -lwinmm

if not %errorlevel% equ 0 (
    echo compilation of main.exe failed
    goto :end
)

if %errorlevel% equ 0 (
    if !COMPILE_ONLY!==1 (
        goto :end
    ) else if !GDB!==1 (
        gdb --args main.exe
    ) else (
        main.exe
    )
)
goto :end

:help
    echo %0 ^[Options^]
    echo Options:
    echo    help           this thing
    echo    c              compile only
    echo    d              enable debug
    echo    g              run gdb after compiliation
    echo    test-thread    test multithreading
goto :end

:end