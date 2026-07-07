# Samples and Coding

The goal of this project was to port [ALIA](https://github.com/jconnell11/ALIA) to a very simple robot. However, you do not need to use this reasoning layer but can instead use Waldo as __peripheral__ controlled by your own Python or C++ code. 

### Python Coding

Windows does not allow easy access to Bluetooth LE serial devices. For this reason [KaspersMicrobit](https://kaspersmicrobit.readthedocs.io) is used, which in turn uses the Python [Bleak](https://github.com/hbldh/bleak) library. This means the simplest way to implement the data exchange program is to let Python be the "boss". The Windows PC stub [__pc_blulink__](../project/pc_blulink.py) initiates an exchange by sending down a small decimal-coded (0-9) command packet. The micro:bit processor on the robot then [replies](../project/qt_blulink.py) with its own small hexadecimal-coded (0-F) sensor packet. The bulk of the processing is handled via callbacks: on_uart_data_received() for the micro:bit, and update_issue() for Windows.

    py pc_drive.py

If you want to code in Python directly, look at the [__pc_drive__](../project/pc_drive.py) sample. This is a modified version of pc_blulink.py with a main loop that calls the respond() function to examine the keyboard. This updates a collection of global control variables such as "lf" and "grip" that get automatically packaged up and sent down to the robot during the Bluetooth callback update_issue(). The robot's sensors are accessible in the main loop through a set of global variables, like "comp" and "dist".

The sensors are:
* comp - robot heading from 0 to 359 degrees __counter-clockwise__
* tilt - forward/backward attitude from -63 to +64 (front up) degrees (saturates)
* roll - left/right attitude from -63 to +64 (right side up) degrees (saturates)
* dist - obstacle distance from front returned by sonar in inches (200" means none)
* volt - main battery voltage from 3.25 to 4.00 volts (saturates)
* line - 4 line/floor detector bits (1 = reflection) with MSB being far left 

The actuators are:
* lf - left track speed from -100 to +100 (forward), but -50 < lf < 50 --> stop
* rt - right track speed from -100 to +100 (forward), but -50 < rt < 50 --> stop
* base - arm swivel servo in degrees from 0 (far right) to 180 (far left)
* lift - arm shoulder servo in degrees from 30 to 120 (full up), 20 = on floor
* grip - hand finger servo in degrees from 80 to 145 (full open), 85 = closed
* col - breathing color: 0 to 9 = none, red, orn, yel, grn, blu, vio, lav, mag, white
* mth - red diamond symbol on back: 0 = off, 1 = dim, 2 = bright

### C++ Coding

To program in C++ the equivalent is the class [__jhcQtruck__](../project/shared/jhcQtruck.h) but the servo __angles are different__. "Base" is the deviation from straight ahead (-90 to 90), "lift" is the deviation from horizontal (-30 to 40), and "grip" is the deviation from fingers straight out (-15 to 55). 

This class interfaces through a DLL to pc_blulink.py to execute a small amount of additional code during Bluetooth callbacks. In particular, inside the update_issue() function in pc_blulink.py the DLL function ext_swap() calls jhcQtruck::BluSwap(). To maximize the Bluetooth exchange rate, the jhcQtruck class just stores the sensor string and returns a cached command string. Actual work gets performed in a background thread using the overridable member function __Respond()__. Within this function the derived class should call Update() to unpack all the low-level robot sensor info into member variables (like "volt"). And, after the main work is done, it should call Issue() to assemble the low-level actuator member variables (like "lift") into a suitable robot command packet.

    py pc_blulink.py baijiu_test

The base jhcQtruck class just uses Respond() to print the sensor variables, but you can derive your own class to do fancier things. For instance, the [__jhcQtDrive__](../project/baijiu_test/jhcQtDrive.cpp) class in [baijiu_test](../project/baijiu_test) uses this override to display the camera image and to scan which keys are pressed in order to modify the actuator variables (see get_track()). This example uses the free [Visual Studio IDE](https://visualstudio.microsoft.com/vs/community/) (double click on baijiu_test.sln) to produce a DLL which can be fed as an argument to the Python Bluetooth code. You can then run the example by typing "py pc_blulink.py baijiu_test" (or simply "py pc_blulink.py" since pc_blulink defaults to this DLL). If you re-compile the DLL, make sure to copy it to the "DLL" directory to switch to the new version.

### Video, Speech, and Reasoning

Images from the robot's camera are obtained using the [__vid_ocv__](../project/shared/vid_ocv.h) DLL. This code hides much of the mess of OpenCV in Windows and is used to flip the ESP32 image vertically and remove the lens distortion. The pixel buffer returned by function ocv_get() is bottom-up, left-to-right, BGR order with a focal length of 219 pixels (111 degrees horizontal). In addition to capturing an image buffer, the DLL also has the function ocv_queue() to display an image buffer on a named window. You can try this out by double clicking the [DLL/vid_test.exe](../project/vid_ocv/vid_test.cpp) program.

The [__rng_flr__](../project/shared/rng_flr.h) DLL attempts to synthesize a frontal depth map for the current scene by assuming a non-textured floor and objects. It ingests an image and partial camera pose with rng_est(), then processes this in a background thread. The call rng_d16() supplies the registered depth image (when ready). You can try this out by double clicking the [DLL/flr_test.exe](../project/rng_flr/flr_test.cpp) program. Note that if you want to recompile this DLL you will have to bring in a number of files from ALIA's source tree, like [jhcPlainFloor](https://github.com/jconnell11/ALIA/tree/master/robot/common/Environ/jhcPlainFloor.cpp).

Speech interactions are mediated via the [__spio_win__](../project/shared/spio_win.h) DLL. This hides some of the complexity of the online Azure speech recognition system and local Text-to-Speech generation. The primary calls are reco_status(), reco_heard(), and tts_say(). You can try it out by double clicking the [DLL/spio_test.exe](../project/spio_win/spio_test.cpp) program once you have proper credentials.

For details on the integration with the [ALIA](https://github.com/jconnell11/ALIA) cognitive architecture, see the [baijiu_vis](../project/baijiu_vis) example. The actual interface to the reasoner is primarily mediated by a bunch of shared variables in the [__alia_vis__](../project/baijiu_vis/alia_vis.h) DLL. For instance, the current heading of the robot is communicated through the variable "alia_bh", and the speed of the robot is commanded through "alia_bmv" (relative to a canonical speed). Note that there are many variables in alia_vis that are not used by Qtruck since the DLL was designed to be used with a variety of different (and more sophisticated) robots. 

---

July 2026 - Jonathan Connell - jconnell@alum.mit.edu