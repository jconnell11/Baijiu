# add single char to "data" (ascii '0' = 48, 'A' = 65)
def add_hex(num: number):
    global code, data
    code = Math.constrain(num, 0, 15) + 48
    if num >= 10:
        code += 7
    data = "" + data + String.from_char_code(code)
# add 2x2 chars: 1 MSB sonar + 7 bit roll, 8 LSB sonar
def get_roll_dist():
    global roll, dist
    roll = Math.constrain(input.rotation(Rotation.ROLL), -64, 63) + 64
    dist = Math.constrain(StartbitV2.startbit_ultrasonic(), 0, 511)
    if dist >= 256:
        dist = dist % 256
        roll += 128
    add_byte(roll)
    add_byte(dist)
# add single char: 4 bit line follower + 4 bit battery
def get_linebat():
    global val
    val = Math.constrain(20 * (volt - 3.25), 0, 15)
    if StartbitV2.startbit_line_followers(StartbitV2.startbit_LineFollowerSensors.S1,
        StartbitV2.startbit_LineColor.WHITE):
        val += 128
    if StartbitV2.startbit_line_followers(StartbitV2.startbit_LineFollowerSensors.S2,
        StartbitV2.startbit_LineColor.WHITE):
        val += 64
    if StartbitV2.startbit_line_followers(StartbitV2.startbit_LineFollowerSensors.S3,
        StartbitV2.startbit_LineColor.WHITE):
        val += 32
    if StartbitV2.startbit_line_followers(StartbitV2.startbit_LineFollowerSensors.S4,
        StartbitV2.startbit_LineColor.WHITE):
        val += 16
    add_byte(val)
# sample voltage every 1-2 sec (33 * 30-60ms)
def sm_volt():
    global vcnt, v, volt, hys
    if vcnt < 33:
        vcnt += 1
    else:
        vcnt = 0
        # start beeping when battery is low (only when connected!)
        v = StartbitV2.startbit_getBatVoltage() / 3000
        if v > 2 and v < 4.5:
            volt += 0.2 * (v - volt)
        if v > 3.6:
            hys = 0
        elif hys > 0 or volt < 3.4:
            hys = 1
            music.play(music.tone_playable(988, music.beat(BeatFraction.WHOLE)),
                music.PlaybackMode.IN_BACKGROUND)
# decode 2x1 decimal chars: color, mouth
def set_leds():
    global hue, mth, mth0
    hue = parse_float(cmd.char_at(11))
    mth = parse_float(cmd.char_at(12))
    if mth != mth0:
        if mth <= 0:
            led.set_brightness(0)
        elif mth == 1:
            led.set_brightness(5)
        else:
            led.set_brightness(255)
        mth0 = mth

def on_bluetooth_connected():
    global hue0, bt
    # PC link established
    led.set_brightness(0)
    basic.show_icon(IconNames.TARGET)
    hue0 = 4
    bt = 1
bluetooth.on_bluetooth_connected(on_bluetooth_connected)

# add 2 chars to "data" for 8 bit value
def add_byte(num2: number):
    global clip
    clip = Math.round(num2)
    add_hex(clip / 16)
    add_hex(clip % 16)

def on_bluetooth_disconnected():
    global hue, mth0, bt
    # PC link dropped
    StartbitV2.startbit_setMotorSpeed(0, 0)
    hue = 0
    StartbitV2.startbit_setPixelRGBArgs(StartbitLights.LIGHT1, 0)
    StartbitV2.startbit_setPixelRGBArgs(StartbitLights.LIGHT2, 0)
    StartbitV2.startbit_showLight()
    mth0 = 0
    basic.show_icon(IconNames.NO)
    led.set_brightness(255)
    bt = 0
bluetooth.on_bluetooth_disconnected(on_bluetooth_disconnected)

# ramp brightness up and down for breathing (cycle = 4 sec)
# bot = ramp / (sqrt(hi/lo) - 1), top = bot + ramp, norm = top^2 / hi
# lo = 2, hi = 80, ramp = 33 -> bot = 5, top = 38, norm = 18.05
def breathe():
    global brite, inc, hue0
    # green does not pulse ("attention")
    if hue == 4:
        brite = 37
        inc = 0
    elif inc == 0:
        brite = 5
    # normal brightening and dimming
    brite += inc
    if brite >= 38:
        brite = 38
        inc = -1
    elif brite <= 5:
        brite = 5
        inc = 1
        hue0 = hue
    StartbitV2.startbit_setBrightness(Math.round(brite * brite / 18.05))
    # off, white, and green are immediate
    if hue0 <= 0 or hue <= 0 or hue == 4 or hue >= 9:
        hue0 = hue
    StartbitV2.startbit_setPixelRGBArgs(StartbitLights.LIGHT1, hue0)
    StartbitV2.startbit_setPixelRGBArgs(StartbitLights.LIGHT2, hue0)
    StartbitV2.startbit_showLight()
# add 2x2 hex chars: 8 LSB compass, 1 MSB compass + 7 bit tilt
def get_comp_tilt():
    global tilt, comp
    # tilt > 0 means front lifted
    tilt = 64 - Math.constrain(input.rotation(Rotation.PITCH), -63, 64)
    # measure counterclockwise
    comp = 360 - input.compass_heading()
    if comp == 360:
        comp = 0
    if comp >= 256:
        comp = comp % 256
        tilt += 128
    add_byte(comp)
    add_byte(tilt)

def on_uart_data_received():
    global cmd, data
    # MAIN LOOP - exchange sensors and commands with PC
    # Bluetooth command receipt takes 30-60 ms (governs overall pacing)
    cmd = bluetooth.uart_read_until(serial.delimiters(Delimiters.NEW_LINE))
    if len(cmd) >= 13:
        set_motors()
        set_arm()
        set_leds()
    breathe()
    sm_volt()
    # gather fresh sensor info and transmit
    data = ""
    get_comp_tilt()
    get_roll_dist()
    get_linebat()
    bluetooth.uart_write_string(data)
bluetooth.on_uart_data_received(serial.delimiters(Delimiters.NEW_LINE),
    on_uart_data_received)

# decode 2x2 decimal chars: left, right
# map: below -> -100 to -52, 49 -> stop, above -> 51 to 100
def set_motors():
    global lf, rt
    lf = parse_float(cmd.substr(0, 2)) + 1
    if lf == 50:
        lf = 0
    elif lf < 50:
        lf = lf - 101
    rt = parse_float(cmd.substr(2, 2)) + 1
    if rt == 50:
        rt = 0
    elif rt < 50:
        rt = rt - 101
    StartbitV2.startbit_setMotorSpeed(rt, lf)
# decode 1x3 + 2x2 decimal chars: base, lift, grip
def set_arm():
    global base, berr, lift, lerr, grip, gerr, base0, lift0, grip0
    # parse commands and find differences from last values
    base = parse_float(cmd.substr(4, 3))
    berr = abs(base - base0)
    lift = parse_float(cmd.substr(7, 2)) + 25
    lerr = abs(lift - lift0)
    grip = parse_float(cmd.substr(9, 2)) + 65
    gerr = abs(grip - grip0)
    # update the 2 servos with the biggest errors (updating all crashes Bluetooth!)
    if berr > 0:
        StartbitV2.set_pwm_servo(StartbitV2.startbit_servorange.RANGE1, 1, base, 100)
        base0 = base
    if lerr > 0 and (berr <= 0 or lerr >= gerr):
        StartbitV2.set_pwm_servo(StartbitV2.startbit_servorange.RANGE1, 2, lift, 100)
        lift0 = lift
    if gerr > 0 and (berr <= 0 or gerr > lerr):
        StartbitV2.set_pwm_servo(StartbitV2.startbit_servorange.RANGE1, 3, grip, 100)
        grip0 = grip
gerr = 0
grip = 0
lerr = 0
lift = 0
berr = 0
base = 0
rt = 0
lf = 0
comp = 0
tilt = 0
brite = 0
inc = 0
clip = 0
bt = 0
hue0 = 0
mth0 = 0
mth = 0
cmd = ""
hue = 0
hys = 0
v = 0
vcnt = 0
val = 0
dist = 0
roll = 0
data = ""
code = 0
volt = 0
grip0 = 0
lift0 = 0
base0 = 0
# qt_blulink.py : remote Bluetooth interface to Qtruck robot
# 
# Written by Jonathan H. Connell, jconnell@alum.mit.edu
# 
# ===============================================
# 
# Copyright 2024 Etaoin Systems
# 
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
StartbitV2.startbit_Init()
if input.compass_heading() < 0 or input.button_is_pressed(Button.B):
    input.calibrate_compass()
StartbitV2.startbit_setMotorSpeed(0, 0)
StartbitV2.ultrasonic_init(StartbitV2.startbit_ultrasonicPort.PORT2)
base0 = 90
lift0 = 100
grip0 = 120
StartbitV2.set_pwm_servo(StartbitV2.startbit_servorange.RANGE1, 1, base0, 500)
StartbitV2.set_pwm_servo(StartbitV2.startbit_servorange.RANGE1, 2, lift0, 500)
StartbitV2.set_pwm_servo(StartbitV2.startbit_servorange.RANGE1, 3, grip0, 500)
# needs Project Settings = No Pairing Required, Disable Bluetooth Event Service
bluetooth.start_uart_service()
basic.show_icon(IconNames.NO)
volt = 4