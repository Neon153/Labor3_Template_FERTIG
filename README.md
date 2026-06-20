# Laboratory 3 for Computer Architecture
This directory contains the template for Lab3 in Computer Architecture.
You must have followed closely the README of the main
computerarchitektur-GIT / ZIP and have a working installation, i.e.
VisualStudio Code, the PICO-SDK, the cross-compiler tools.

## Most important parts
The file ``config.cmake`` is the main configuration file:
it needs the tools in the ``external`` directory, so create a *soft-link*
to this directory, first:
   ln -s $HOME/WHERE_MY_DIRECTORY_IS_CHECKED_OUT/external external

Same for the link to the libraries (such as lcd):
   ln -s $HOME/WHERE_MY_DIRECTORY_IS_CHECKED_OUT/lib lib

This makes sure, You're feeding the PICO-SDK with the right C-Preprocessor defines,
function definitions and the right cross-compiler tools.

For debugging in VScode, the directory ``.vscode`` and it's JSON files
are essential.

First, let's try to compile from the command-line using CMake with the "Ninja" generator
into a new build directory of the same name:
```
   cmake -GNinja  -Bbuild
   cmake --build build
```

You *must* then open the lab3a Directory in it's *own*, *new* VScode window!

## Where to start
Open the first Laboratory Template with VScode and try to compile it.
Then try to attach the Debugger.

Only then, search for the parts, where You need to code.
