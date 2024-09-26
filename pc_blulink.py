# =========================================================================
#
# pc_blulink.py : Python Bluetooth message pump for Qtruck robot
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

# run program with base DLL name as argument: py pc_blulink.py baijiu_vis
# DLL has: int ext_start(), void ext_done(), char *ext_respond(char *)
# uses KaspersMicrobit/Bleak to interface with Bluetooth LE GATT services
# so must first do: py -m pip install kaspersmicrobit

import time, sys

from kaspersmicrobit import KaspersMicrobit

from ctypes import CDLL, c_char_p

if len(sys.argv) > 1:
  lib = CDLL("./" + sys.argv[1] + ".dll")
else:
  lib = CDLL("./baijiu_test.dll")
lib.ext_swap.restype = c_char_p


# -------------------------------------------------------------------------

# whether to keep exchanging packets with Qtruck
stop = -1


# packet speed statistics
i = 0
start = 0
last  = 0


# callback exchanges sensors string for command string
def update_issue(msg):
  global i, start, last, stop

  # collect packet statistics (16-32 Hz)
  last = time.time()
  if (start <= 0):
    start = last
  i += 1

  # send sensors to external system and get commands back 
  # sensor "msg" hex coded    = CC:TT:RR:DD:L:V      (10 chars)
  # motor "cmd" decimal coded = LL:RR:BBB:FF:GG:C:M  (13 chars)
  # cmd = NULL or "" is a request from external system to exit
  ptr = lib.ext_swap(c_char_p(msg.encode()))
  if not ptr:
    stop = 1
  else:
    cmd = ptr.decode()
    if not cmd:
      stop = 1
    else:
      microbit.uart.send_string(cmd + '\n') 


# -------------------------------------------------------------------------

# PROGRAM START - link to robot via Bluetooth and initiate exchange
if lib.ext_init() <= 0:
  print("Link: Main init failed ...")
else:
  print("Link: Connecting to robot ...")
  try:
    with KaspersMicrobit.find_one_microbit() as microbit:
      name = microbit.generic_access.read_device_name()
      if lib.ext_start(c_char_p(name[15:20].encode())) <= 0:
        print("Link: Main start failed ...")
      else:
        # bind receiver callback and prompt for first exchange
        microbit.uart.receive_string(update_issue)
        stop = 0
        last = time.time()
        microbit.uart.send_string("0\n")

        # check periodically (10 Hz) for termination conditions
        while True:
          if stop > 0:
            print("Link: Stop requested ...")
            break
          if (time.time() - last) > 2:
            print("")
            print("Link: Connection lost ...")
            break
          time.sleep(0.1)                
  except:
    print("")
    print("Link: Not connected!")

# wait for last packets to arrive then cleanly terminate
time.sleep(0.5)
if stop >= 0:
  lib.ext_done()                       # only call if startup()   

# show packet statistics
if i > 1:
  gaps = i - 1  
  secs = last - start
  print("Link: Exchange avg %3.1f ms (%3.1f Hz)" % (1000 * secs / gaps, gaps / secs))                        
