# Telemetry Visualizer

A data-driven telemetry visualization plug-in for Stingray. Developed by [Marcus Lilja](https://github.com/Liljan) and [Martin Engelbrektsson](https://github.com/Jingelman) as master thesis at Autodesk.

The Visual Studio project files were made with Visual Studio 2015 and compiled with the 2015 C++ compiler. The plug-in uses the MongoDB C driver to communicate with a MongoDB database.

Building Project files:

* Follow the [Stingray plug-in](https://github.com/AutodeskGames/stingray-plugin) guide
* Generate plug-in project files from *make.rb* using Ruby. If this does not work at first, place *FindMongo.cmake* in the CMake folder

Installation:

* Install Autodesk Stingray v. 1.9.0 from source code
* Set-up a MongoDB database
* Place *bson-1.0.dll* and *mongoc-1.0.dll* in Stingray editor folder next to the *.exe*
* Import the plug-in via Stingray's plug-in manager

Usage:

* Connect to an address and enter database name
* (Optional: Load a Stingray level)
* Select from which collection to fetch data from
* Select what data fields to include in visualization
* Select which documents to include in visualization
* Select visualization type and match visualization properties with a suitable data field
