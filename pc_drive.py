# =========================================================================
#
# pc_drive.py : simple Python keyboard interface to Qtruck robot
#
# Written by Jonathan H. Connell, jconnell@alum.mit.edu
#
# =========================================================================
#
# Copyright 2024 Etaoin Systems
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 
# =========================================================================

# need to do: py -m pip install keyboard kaspersmicrobit

import time, keyboard

from kaspersmicrobit import KaspersMicrobit


# =========================================================================

# exchange speed statistics
i = 0
start = 0
last  = 0


# robot sensor variables
comp = 0
tilt = 0
roll = 0
dist = 0
line = 0xF
volt = 4.0


# robot control variables
lf   = 0
rt   = 0
base = 90
lift = 100
grip = 120
col  = 8
mth  = 0          


# =========================================================================

# get track motion commands (789 45 123)
def track_cmds():
  global lf, rt
  if keyboard.is_pressed('8'):         # fwd
    lf = 100
    rt = 100
  elif keyboard.is_pressed('2'):       # back
    lf = -100
    rt = -100
  elif keyboard.is_pressed('4'):       # left
    lf = -100
    rt = 100
  elif keyboard.is_pressed('6'):       # right
    lf = 100
    rt = -100
  elif keyboard.is_pressed('7'):       # left fwd
    lf = 0
    rt = 100
  elif keyboard.is_pressed('9'):       # right fwd
    lf = 100
    rt = 0
  elif keyboard.is_pressed('1'):       # left back
    lf = 0
    rt = -100
  elif keyboard.is_pressed('3'):       # right back
    lf = -100
    rt = 0
  else:                                # stopped
    lf = 0
    rt = 0


# get arm motion commands (ud lr oc)
def arm_cmds():  
  global base, lift, grip

  # servo changes
  if keyboard.is_pressed('u'):
    lift += 2
  elif keyboard.is_pressed('d'):
    lift -= 2
  if keyboard.is_pressed('l'):
    base += 2
  elif keyboard.is_pressed('r'):
    base -= 2
  if keyboard.is_pressed('o'):
    grip += 2
  elif keyboard.is_pressed('c'):
    grip -= 2

  # enforce arm joint limits
  if base > 180:
    base = 180
  elif base < 0:
    base = 0
  if lift > 120:
    lift = 120
  elif lift < 30:
    lift = 30
  if grip > 145:
    grip = 145
  elif grip < 80:
    grip = 80


# get breathing color command (`<TAB> qw as zx)
def color_cmd():
  global col
  if keyboard.is_pressed('`'):
    col = 0
  elif keyboard.is_pressed('tab'):
    col = 9
  elif keyboard.is_pressed('q'):
    col = 1
  elif keyboard.is_pressed('w'):
    col = 2
  elif keyboard.is_pressed('a'):
    col = 3
  elif keyboard.is_pressed('s'):
    col = 4
  elif keyboard.is_pressed('z'):
    col = 5
  elif keyboard.is_pressed('x'):
    col = 8


# get mouth flash command (<SPACE> <BACK>)
def mouth_cmd():
  global mth
  if keyboard.is_pressed('space'):
    mth = 2
  elif keyboard.is_pressed('backspace'):
    mth = 1
  else:
    mth = 0


# =========================================================================

# callback collects timing statistics and issues response
def update_issue(msg):
  global i, start, last, stop
  last = time.time()
  if (start <= 0):
    start = last
  i += 1
  try:
    microbit.uart.send_string(respond(msg) + '\n') 
  except:
    stop = 1


# takes in sensor string and returns command string
def respond(msg):
  global comp, tilt, roll, dist, line, volt

  # create command string: LL:RR:BBB:FF:GG:C:M
  cmd = ("%02d%02d%03d%02d%02d%d%d" % 
    (mot_val(lf), mot_val(rt), round(base), round(lift - 25), round(grip - 65), col, mth))

  # parse sensor string: CC:TT:RR:DD:L:V
  data = bytes.fromhex(msg)
  if len(data) >= 5:
    comp = ((data[1] & 0x80) << 1) | data[0]
    tilt = (data[1] & 0x7F) - 64
    roll = (data[2] & 0x7F) - 64
    dist = ((data[2] & 0x80) << 1) | data[3]
    line = data[4] >> 4
    volt = 3.25 + 0.05 * (data[4] & 0x0F)
    print("  %4d: comp %3d, tilt %3d, roll %3d, dist %3d, line %s, bat %4.2f, cmd \"%s\"" % 
      (i, comp, tilt, roll, dist, format(line, "04b"), volt, cmd), end = "\r")

  # send response to robot
  return cmd


# map bipolar motor speed with large central deadband
# -100 to -52 -> 0-48, deadband -> 49, 51 to 100 -> 50-99
def mot_val(sp):
  if sp >= 51:
    return round(sp - 1)
  if sp <= -52:
    return round(sp + 100)
  return 49


# =========================================================================

# START - link to robot via Bluetooth
print("Connecting to robot ...")
try:
  with KaspersMicrobit.find_one_microbit() as microbit:

    # bind receiver callback and prompt for first exchange
    microbit.uart.receive_string(update_issue)
    time.sleep(0.5)
    microbit.uart.send_string("0\n")
    last = time.time()

    # explain how to drive
    print("")
    print("--> Arm = ud lr oc, Color = `<TAB> qw as zx, Mouth = <SPACE> <BACK>")
    print("--> Drive using numberpad = 789 46 123 (<ESC> to quit) ...")
    print("")
  
    # watch keyboard (even if not in application)
    while True:

      # check for user exit request
      if keyboard.is_pressed('esc'):
        print("")
        print("")
        print("Shutdown requested ...")
        break

      # check if long time since packet received
      if (time.time() - last) > 2:
        print("")
        print("")
        print("Link lost ...")
        break

      # get new command values then wait a bit
      track_cmds()
      arm_cmds()
      color_cmd()
      mouth_cmd()
      time.sleep(0.033)

except:
  print("")
  print("Not connected to robot!")
  
# wait for last packets to arrive then show stats
time.sleep(0.5)
if i > 1:
  gaps = i - 1  
  secs = last - start
  print("")
  print("Average = %3.1f ms (%3.1f Hz)" % (1000 * secs / gaps, gaps / secs))

    