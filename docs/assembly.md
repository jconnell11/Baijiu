# Assembly and Software

This will likely take several hours, largely devoted to assembling the physical robot which arrives in kit form.

### Hiwonder Qtruck

Start by assembling the Qtruck robot, following the instructions for the default "transfer" model. For long-term stability, add a __lockwasher__ to the central screw of the arm swivel servo.

### Software Environment

Start by __copying__ the whole GitHub [project](../project) directory somewhere on your machine (e.g. "Baijiu"). You also need to download and install [Python](https://www.python.org/downloads/windows/) if you do not already have it. Finally, install the additional needed infrastructure by opening a command line and typing:

    py -m pip install kaspersmicrobit keyboard

The Qtruck robot also needs to have software installed that establishes a Bluetooth link with the laptop. To do this, connect the robot to your computer using the mini-USB at the rear of the small Microbit board on top. You should see a drive window pop up with just a few files in it. Drag and drop __qt_blulink.hex__ onto this window to start the programming process. When it is finished, the drive window should re-appear (but there will be no trace of the hex file). 

On the first run, the robot will calibrate its onboard __magnetic compass__. As suggested by the scrolling message, rotate the robot through all 3 axes until the 25 lights on the back are all on. After this you should see a big red "X" which means the robot is waiting for a connection. You only have to do the calibration once but, if you want to force it to run again, hold down the left "B" button while powering-on to enter this mode.

    py pc_drive.py

You can test out the basic Bluetooth functionality by invoking the command above. This program uses simple keystrokes to drive the robot's actuators and LEDs, and shows you the state of the simple sensors available. It also reports the robot's __ID__ upon connection.

### Configuration File

The main programs will work somewhat better if the robot has a proper configuration file. Otherwise you may notice the complaint: "Could not read file: config/xxxxx_calib.cfg !" Each Microbit controller has a unique 5 character ID which is reflected in the xxxxx. Once you know the ID of your board (e.g. from running pc_drive, above), rename the file [robot_calib.cfg](../project/config/robot_calib.cfg) to match (e.g. "tagig_calib.cfg"). The first line inside this file is the __robot's name__. You can change it to whatever you want. The second line is the streaming camera's URL (set later). The third line has the zero degree offsets for the 3 arm servos. The fourth line lists the pan, tilt, and roll offsets for the camera (also set later). 

    py pc_blulink.py

To get proper values for the __servo offsets__, start up the [baijiu_test](../project/baijiu_test/baijiu_test.cpp) sample program via the command above. Using the left and right arrow keys (while holding down "Alt" for finer positioning), align the arm with the robot's direction of travel. Copy the first value in the status line "... servo[ -2 0 12] ..." to the first value of line 3 in the calibration file. Next, use the up and down arrow keys (with Alt) to move the grasp point between the fingertips exactly 43 mm off the floor. Copy the second value in "servo[...]" to the second value in the calibration file. Finally, use Alt with PgUp and PgDn to adjust the spacing between the fingers until they just touch. Copy the resulting third servo value into the configuration file then save it.

---

June 2026 - Jonathan Connell - jconnell@alum.mit.edu