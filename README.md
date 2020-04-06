![License][license-image]

# evjs - Evdev Joystick Utilities

evjs is a set of utilities for testing, calibrating, and configuring evdev-based joysticks in Linux. Calibration values are stored in a persistent sqlite database and may be automatically configured on system boot and when a device is plugged in.

evjs also supports calibrating joysticks on the old joydev-based Linux interface and can calibrate both evdev and joydev simultaneously. Testing of force feedback effects is also supported if available in the joystick driver.

evjs includes the following utilities:

* evjstest - curses-based interface for testing and calibrating a joystick
* evjscal - command line tool for testing, calibration, and database maintenance

## Motivation

I am a flight simulator and retro-gaming enthusiast so I use a lot of old school flight sticks, driving wheels, and joysticks that need calibration across a number of games and emulators.  These programs are not consistent in their use of the two joystick APIs in Linux - evdev and joydev.  There are a number of joystick testing and calibration utilities out there such as evtest, jstest, evdev-joystick, fftest, etc, but I found myself having to put together a hodgepodge of scripts and udev rules to calibrate these devices.  I wanted a single, unified interface that made it easy to automatically apply calibrations from a database as joysticks are added or removed from the system regardless of the Linux joystick API used so evjs was born.

## Compiling

evjs requires the following build tools:

 * autotools
 * make
 * gcc

In addition, you must have the following libraries installed:

 * ncurses
 * sqlite3

You can build evjs by executing the following:

    $ ./bootstrap
    $ ./configure
    $ make

In addition to the standard configure features, you can also specify the following options:

  * --disable-joystick : Disable the joydev calibration support
  * --disable-effects  : Disable the force feedback effects support

For example, to compile evjs without joydev support execute:

    $ ./configure --disable-joystick

## Automatic Configuration

You can automatically configure the calibration values for joysticks attached to the system on boot or plugged in on the fly by creating a udev rule like the following:

    ACTION=="add", KERNEL=="event*", SUBSYSTEM=="input", RUN+="/usr/bin/evjscal -c '%E{DEVNAME}'"

This rule will cause udev to call evjscfg with the event device which will configure the joysticks from the system default calibration database /etc/evdev/cal.db. Any joystick added to the database by the evjs utilities is automatically calibrated without having to write additional udev rules.

## Usage

### evjstest

To test a specific joystick, specify its device path after evjstest:

    $ evjstest /dev/input/event11

If you run evjstest without specifying a device path, it will present a list of valid joysticks found in /dev/input:

    $ evjstest
    No device specified, scanning /dev/input/event*
    /dev/input/event11  : ShumaTech F-16 FLCS USB
    /dev/input/event14  : ShumaTech WCS mark II USB
    /dev/input/event15  : Sony PLAYSTATION(R)3 Controller
    Select the device number: 

Here is a snapshot of the evjstest screen:

![evjstest screenshot](https://filedn.com/lEnyCKkGcSaQKW9xHTReWxV/evjstest.png)

On start, evjstest will use the calibration values configured in the device and *NOT* the values saved in the database. To read and configure the values from the database, press the 'r' key.

To start the calibration process, press the 'c' key to show the calibration cursors. The cursors show the minimum and maximum values reached by an axis. Move all axes to their minimum and maximum positions and press the \<ENTER\> key to set the calibration values.  To cancel calibration, press the 'c' key again to turn off the cursors.  The new calibration values are not written to the database unless the 'w' key is pressed.  This allows one to test the new calibration values before committing them to the database.

To configure the fuzz and flat values, use the up and down arrow keys, or the 'j' and 'k' keys, to move the axis selection which is indicated by an underline on the axis name.  Press the 'f' key for fuzz or the 't' key for flat and enter the new value.

Pressing the '?' key will show the following help screen:

    Navigation:
      <up> or j    : move axis selection up
      <down> or k  : move axis selection down
      <page up>    : move axis selection up one screen
      <page dn>    : move axis selection down one screen
      <left> of h  : move effect selection left
      <right> or l : move effect selection right
    General:
      q            : Quit application
      c            : Toggle calibration cursors
      <ENTER>      : Set calibration from current cursors 
      f            : Set fuzz factor for selected axis
      t            : Set flatness for selected axis
      e            : Activate selected effect
    Database:
      r            : Read axis calibration
      w            : Write axis calibration
    
    To calibrate a device, first turn on the calibration cursors with 'c'. Then
    move all axes to their minimum and maximum values. Press <ENTER> to set the
    calibration values from the cursor positions or 'c' to cancel and turn off
    the cursors.
    
    Use the evjscfg command in combination with udev to automatically restore
    calibrations when the system is restarted or the device is plugged in.

### evjscal

evjscal is a command-line utility to manage joystick calibrations.  Below are some examples of its usage:

Calibrate a joystick, save the calibration to the database, and apply it to the evdev and joydev APIs:

    $ evjscal -C /dev/input/event15
    Move X axis to its extremes and press a button to continue.
    Axis X Value:102 Min:9 Max:227                 
    
    Move Y axis to its extremes and press a button to continue.
    Axis Y Value:134 Min:21 Max:252                
    
    Move HAT0X axis to its extremes and press a button to continue.
    Axis HAT0X Value:0 Min:-1 Max:1                
    
    Move HAT0Y axis to its extremes and press a button to continue.
    Axis HAT0Y Value:0 Min:-1 Max:1                
    
    Saving calibration

List all calibrations in the database by bus:vendor:product:

    $ evjscal -l
    0003:03eb:aaa0 = 0,1,235,2,12,1,2,252,5,15,16,-1,1,0,0,17,-1,1,0,0
    0003:03eb:aaa1 = 6,11,233,0,15,7,9,247,0,15

Delete a device's calibration from the database:

    $ evjscal -D /dev/input/event15

Write new calibration values to the database:

    $ evjscal -w 0,1,235,2,12,1,2,252,5,15,16  /dev/input/event15

Get the calibration values currently configured in the evdev API (note that these do not necessarily match the database):

    $ evjscal -g /dev/input/event12
    0,0,255,0,15,1,0,255,0,15,2,0,255,0,15,5,0,255,0,15,16,-1,1,0,0,17,-1,1,0,0

Apply any calibration values in the database to both the evdev and joydev APIs for a device:

    $ evjscal -c /dev/input/event15

Here is the help output:

    Usage: evjscal [OPTION]... [DEVICE]
    Manage calibration values for joystick event DEVICE.
    
    Options:
      -h, --help            Print this help
      -v, --verbose         Display verbose information
      -d, --database FILE   Use the specified database FILE
      -l, --list            List all calibration values in database
      -r, --read            Read and display calibration values from database
      -D, --delete          Delete calibration values from database
      -w, --write VALUES    Write calibration VALUES to database
      -c, --config          Read calibration values from database and configure
                            them in DEVICE
      -s  --set VALUES      Set new calibration VALUES in DEVICE
      -g  --get             Get the calibration VALUES configured in DEVICE
      -C, --calibrate       Execute calibration procedure
    
      VALUES is a comma separated list: [axis],[min],[max],[fuzz],[flat],...
    
    Examples:
      Read the database values with concise output:
        evjscal -r /dev/input/event11
      Read the database values with human output:
        evjscal -v -r /dev/input/event11
      Write new database values:
        evjscal -w 2,255,2,15 /dev/input/event11
      Delete database values:
        evjscal -D /dev/input/event11

