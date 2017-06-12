# Telemetry Visualizer

A data-driven telemetry visualization plug-in for Stingray. Developed by [Marcus Lilja](https://github.com/Liljan) and [Martin Engelbrektsson](https://github.com/Jingelman) as master thesis at Autodesk.

The Visual Studio project files were made with Visual Studio 2015 and compiled with the 2017 C++ compiler.

Installation:

* Install Autodesk Stingray v. 1.9 from source code.
* Set-up a MongoDB database.
* Generate plug-in project files from *make.rb* using Ruby. If this does not work at first, place *FindMongo.cmake* in the CMake folder.
* Generate native DLL extension by compiling the *editor project* in the build directory.
* Place *bson-1.0.dll* and *mongoc-1.0.dll* in Stingray editor folder next to the *.exe*.
* Import the plug-in via Stingray's plug-in manager.

Usage:

* First
* Second
* Third
