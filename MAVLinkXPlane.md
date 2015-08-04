# Introduction #

This page introduces the custom MAVLink plugin for X-Plane designed by LUCAS for both Software and Hardware in Loop (SIL/HIL) testing of autopilot systems.

This plugin was designed to allow the ArduPilot Mega autopilot system to be used in both SIL/HIL modes without requiring the use of the Mission Planner to provide the MAVLink interface. The reasoning behind the removal of the Mission Planner from the loop was to simplify multi-vehicle experiments, where vehicles would likely receive control signals from MATLAB/Simulink.

The plugin is available free for anyone to use (and the source code is available). Any bugs reports/suggestions/contributions are welcomed.

# Version 0.8 #
This is the current version of the plugin which has been tested in the following environment:

  * X-Plane v9.70 on a Windows 7 PC (x64)
  * ArduPlane running in Ubuntu (virtualised) in SIL mode
  * Using the HIL\_ATTITUDE mode, i.e. X-Plane outputs attitude directly, not 'raw' sensor data

The next development step is to enable HIL mode by implementing serial communication. This will be followed by support for ArduCopter as well as cross platform versions of X-Plane.

# Installation #
Download the plugin file from [here](http://code.google.com/p/lucas-research/downloads/list) (select win32 or win64 versions depending on your OS) and copy it to your X-Plane plugins folder (_<X-Plane root>/Resources/plugins_).

When you next start X-Plane the plugin will start automatically and will need configuring.

# Configuration #
To configure the plugin simply navigate to the _Plugins>LUCAS>MAVLink_ menu and select _Settings_. Select _TCP Connection_ (serial is not currently supported!) and enter the IP address and port number of your APM SIL execution.

# Testing #
If you do not have custom code to send commands to your APM you can still connect the Mission Planner (or any other GCS) to APM via the telemetry 'serial' port (TCP port 5763). The following video shows a simple set up.

<a href='http://www.youtube.com/watch?feature=player_embedded&v=6YWQzGO9Okk' target='_blank'><img src='http://img.youtube.com/vi/6YWQzGO9Okk/0.jpg' width='425' height=344 /></a>