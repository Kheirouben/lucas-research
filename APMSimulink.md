# Introduction #

This page gives some details on the Ardupilot Mega (APM) to MATLAB/Simulink interface developed by LUCAS. Specifically this is a Simulink block designed to encode/decode MAVLink messages on a particular serial port to which the APM is connected (either via Zigbee or USB).

This wiki refers to the particular example of sending APM roll, pitch and thrust commands from Simulink whilst in flight. To achieve this a small modification to the APM code is required to decode and utilise the correct MAVLink packet.

I am assuming that you are familiar with downloading custom code to APM, if not check out the [APM Wiki](http://code.google.com/p/ardupilot-mega/wiki/Code) on the subject.

Note that all APM files used by LUCAS have been updated to Arduino v1.0 (resulting in small changes to a number of includes and a change of file extension to ".ino").

At present this interface has undergone very little testing but you are welcome to use it at your own risk!

# Prerequisites #

In order to make use of the APM to Simulink interface you'll need...

  * An Ardupilot Mega 2560 (1280 may work)
  * A Zigbee telemetry kit
  * Arduino 1.0
  * MATLAB for Windows
  * A C++ build environment for MATLAB (see below)

## Setting up MATLAB to build C++ MEX functions ##

You will need to compile the Simulink interface on the machine you wish to use and to do this you must set up MATLAB to support C++ compiling. This is explained in depth in the MATLAB documentation but here is a breif version...

  1. Download and install [Microsoft Visual C++ 2010 Express Edition](http://www.microsoft.com/visualstudio/en-us/products/2010-editions/visual-cpp-express)
  1. Download and install the [Microsoft Windows SDK](http://www.microsoft.com/download/en/details.aspx?id=8279)
  1. Start MATLAB
  1. Type the following at the MATLAB prompt ```matlab
mex -setup```
  1. Select 'y' when prompted to locate installed compilers
  1. Enter the number which corresponds to 'Microsoft Visual C++ 2010 Express'
  1. Select 'y' to confirm

You'll likely see a couple of warnings about run-time libraries and the MATLAB API but you can ignore these. Assuming there are no errors reported you have now successfully set up MATLAB as a C++ build environment.

# Setting up the Simulink block #

A zip file containing the required files is available [here](http://lucas-research.googlecode.com/files/MAVLink%20Simulink%20block.zip). Unzip this in to your MATLAB working directory where you want to create your Simulink model. The zip contains the following files...

  * example.mdl
    * An example model
  * LUCAS.mdl
    * A library containing all LUCAS Simulink blocks (currently just the 1!)
  * MAVLink\_Interface.cpp
    * The source code for the block
  * MAVLink\_Interface.mexw64
    * The binary file for the block (for x64 systems)

If you are running a 32-bit version of MATLAB you will need to build your own binary which is done with the following command (at the MATLAB prompt)
```matlab
mex MAVLink_Interface.cpp```
This should produce a file named 'MAVLink\_Interface.mexw32'.

# Using the block #

To use the block you must create a new Simulink model by choosing File > New > Model from the MATLAB menu. Once you have a new model open, double click on 'LUCAS.mdl' in the Current Directory pane. This library contains a single block which you can drag in to your model. Once you have a copy of the block in your model you can close the LUCAS library.

Double click the block to enter the configuration parameters. Note that the COM port must be specified in single quotes, e.g. 'COM5'. Various input and output messages are selectable, each with various states of maturity. The default messages are an input of ATTITUDE\_COMMAND and outputs of ATTITUDE, GLOBAL\_POSITION and VFR\_HUD. This configuration has been flight tested briefly and used to implement a Simulink based heading hold controller.

Each message is represented as a multiplexed signal whose dimension is determined by the message itself, at present you are recommended to refer to the source code for details as these details are not documented anywhere else!

# Custom APM Code #

In order to get the APM to respond to the ATTITUDE\_COMMAND message (more correctly, the MAVLINK\_MSG\_ID\_SET\_ROLL\_PITCH\_YAW\_THRUST message), a modification to the code is required. The modifications are listed below and the complete modified APM code (Arduplane v2.27 alpha) is available [here](http://lucas-research.googlecode.com/files/ArduPlane%20v2.27%20alpha%20%2B%20Simulink%20Control.zip). I don't recommend that anyone simply downloads the code and uses it without atleast a basic understanding of the modifications made as we currently do not have much experience with APM and have had little testing time to date so may be doing something completely wrong!

## ArduPlane.ino ##

This file is modified to include the logic that if an attitude command is being received it should be used in preference to APMs own navigation commands (in AUTO mode)

Below the default APM global declarations the following are added
```cxx

// Roll, pitch, yaw and thrust commands
// ------------------------------------
#if ATTITUDE_COMMAND == ENABLED
static bool in_command = false;
static uint32_t command_fs_timer;
static long roll_command;
static long pitch_command;
static long yaw_command;
static int thrust_command;
#endif
```

A helper function is added to check if we're still receiving a command
```cxx

#if ATTITUDE_COMMAND == ENABLED
static void check_command() {
if (millis() - command_fs_timer > FAILSAFE_SHORT_TIME) {
in_command = false;
}
}
#endif
```

Finally the 'update\_current\_flight\_mode' function is modified to include the override of APMs default navigation commands in the default AUTO mode
```cxx

default:
hold_course = -1;
calc_nav_roll();
calc_nav_pitch();
calc_throttle();
#if ATTITUDE_COMMAND == ENABLED
check_command();
if (in_command) {
nav_pitch = pitch_command;
nav_roll = roll_command;
g.channel_throttle.servo_out = thrust_command;
}
#endif
```

## GCS\_MAVLink.ino ##

The 'GCS\_MAVLINK::handleMessage' function is modified to deal with the new MAVLink message
```cxx

#if ATTITUDE_COMMAND == ENABLED
case  MAVLINK_MSG_ID_SET_ROLL_PITCH_YAW_THRUST:
{
// Allows a ground control station to command attitude/thrust set points
if(msg->sysid != g.sysid_my_gcs) break;		// Only accept control from our gcs
mavlink_set_roll_pitch_yaw_thrust_t packet;
mavlink_msg_set_roll_pitch_yaw_thrust_decode(msg, &packet);
if (mavlink_check_target(packet.target_system,packet.target_component)) break;

roll_command = (long)(mavlink_msg_set_roll_pitch_yaw_thrust_get_roll(msg)*100.0*180.0/3.14159);
pitch_command = (long)(mavlink_msg_set_roll_pitch_yaw_thrust_get_pitch(msg)*100.0*180.0/3.14159);
yaw_command = (long)(mavlink_msg_set_roll_pitch_yaw_thrust_get_yaw(msg)*100.0*180.0/3.14159);
thrust_command = (int)(mavlink_msg_set_roll_pitch_yaw_thrust_get_thrust(msg)*100.0);
in_command = true;
command_fs_timer = millis();
break;
}
#endif
```

## config.h ##

Finally we must enable the functionality by default. This is done by adding the following to the bottom of the configuration header.

```cxx

#ifndef ATTITUDE_COMMAND
# define ATTITUDE_COMMAND ENABLED
#endif
```