# Concerto Script

Concerto Script is a musical programming language made in [Raylib](https://www.raylib.com/).

It is currently exclusive to Windows, see [windows_wrapper.h](./src/windows_wrapper.h) for dependencies.

## Running the program

Compile and run the program with [run.bat](run.bat).
Note that this requires an installation of [GCC](https://gcc.gnu.org/).

## Text editor

Running the program will open an empty music program in the integrated text editor.
The text editor is following many standard conventions such as `CTRL + C/V/X/G/F` but it has some unique features:

* `CRTL + O`: Open one of the built-in music programs.
* `CRTL + P`: Compile and run the current program.
* `CRTL + D`: Go to definition of identifier under the cursor.
* `CRTL + T`: Select another color theme.
* `CRTL + Q`: Quit application.

## Syntax

The syntax for creating music is as of now most easily understood by running one of the built-in music programs.
Proper documentation should exist at some point in the future.
