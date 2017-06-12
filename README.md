# Telemetry Visualizer

A data-driven telemetry visualization plug-in for Stingray. Developed by [Marcus Lilja](https://github.com/Liljan) and [Martin Engelbrektsson](https://github.com/Jingelman) as master thesis at Autodesk.

The Visual Studio project files were made with Visual Studio 2015 and compiled with the 2015 C++ compiler. The plug-in uses the MongoDB C driver to communicate with a MongoDB database.

Prerequisites:
* Autodesk Stingray v. 1.9.0
* MongoDB database

Building Project files:
* Follow the [Stingray plug-in](https://github.com/AutodeskGames/stingray-plugin) guide

Installation:
* Place *bson-1.0.dll* and *mongoc-1.0.dll* in Stingray editor folder next to the *.exe*
* Import the plug-in via Stingray's plug-in manager

Usage:
* Connect to an address and enter database name
* (Optional: Load a Stingray level)
* Select from which collection to fetch data from
* Select what data fields from the database collections to include in visualization
* Select which documents to include in visualization
* Select visualization type and match visualization properties with a suitable data field

Notes for usage:
* This plug-in supports several position parsers. Currently positions can be parsed from strings as "Vector3(x,y,z)" or "Position(x,y,z)". This can be extended by following instructions in the source code.
* If the position attribute is not a valid field the visualization is not shown
* The color scale uses three colors. *Min*: red, *Desired*: black, *Max*: green
* If the scalar attrubute is not set to a valid scalar type (such as number) the visualization color is set to purple

