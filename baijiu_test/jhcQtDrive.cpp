// jhcQtDrive.cpp : use keyboard to drive Hiwonder Qtruck robot
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2024 Etaoin Systems
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
///////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "vid_ocv.h"

#include "jhcQtDrive.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default destructor does necessary cleanup.

jhcQtDrive::~jhcQtDrive ()
{
  delete [] buf;
}


//= Default constructor initializes certain values.

jhcQtDrive::jhcQtDrive ()
{
  buf = new unsigned char [3 * 640 * 480];
}


//= Initializes system before attempting Bluetooth robot connection.
// returns positive if successful, 0 or negative for failure

int jhcQtDrive::Setup ()
{
  // connect to ESP32Cam 
  if (ocv_open("http://192.168.5.1:81/stream") <= 0)
  {
    printf("  No video stream -- is wifi set to HW_ESP32Cam?\n");
    return 0;
  }
  ocv_warp(0.7, -11.0);

  // make a window to display video
  ocv_win(0, "Esp32 camera", 20, 50);
  return 1;
}


///////////////////////////////////////////////////////////////////////////
//                       Primary Loop Overrides                          //
///////////////////////////////////////////////////////////////////////////

//= Override to initialize external system before loop.
// returns positive if okay, 0 or negative for problem

int jhcQtDrive::Launch ()
{
  R2D2();
  printf("\n--> Arm = arrows PgUp PgDn <ALT>, Color = 0-9, Mouth = <SPACE> <BACK>\n");
  printf("--> Drive using NumPad/NumLock = 789 46 123 (<ESC> to quit) ...\n\n");
  return 1;
}


//= Override to run core step of primary external loop.
// generate one output command based on current sensor values
// returns positive if okay, 0 or negative for system quit

int jhcQtDrive::Respond ()
{
  // wait for next video frame then display it
  if (ocv_get(buf, 1) <= 0)
    return -1;
  ocv_queue(0, buf);
  ocv_show(); 

  // print sensors (or Cartesian positions)
  Update();
  Show();
//  XYZ();

  // check for user requested exit then display sensor values
  if (key(0x1B))
  {
    printf("\n");         
    return 0;
  }

  // gather new commands to be sent to robot then wait a bit
  get_track();
  get_arm();
  get_color();
  get_mouth();
  Issue();
  return 1;
}


///////////////////////////////////////////////////////////////////////////
//                       Keyboard Interpretation                         //
///////////////////////////////////////////////////////////////////////////

//= Get track motion commands (numpad: 789 45 123).

void jhcQtDrive::get_track ()
{
  double mcmd = 100.0;       // about 5.5 in/sec, 90 deg/sec

  // left track direction
  if (key(VK_NUMPAD6) || key(VK_NUMPAD8) || key(VK_NUMPAD9))
    lf = mcmd;
  else if (key(VK_NUMPAD2) || key(VK_NUMPAD3) || key(VK_NUMPAD4))
    lf = -mcmd;
  else
    lf = 0.0;

  // right track direction
  if (key(VK_NUMPAD4) || key(VK_NUMPAD7) || key(VK_NUMPAD8))
    rt = mcmd;
  else if (key(VK_NUMPAD1) || key(VK_NUMPAD2) || key(VK_NUMPAD6))
    rt = -mcmd;
  else
    rt = 0.0;
}


//= Get arm motion commands (arrows PgUp PgDn Alt).
// constrained to valid bounds by pack_commands()

void jhcQtDrive::get_arm ()
{
  double da = 3.0;           // 90 deg/sec

  // use alt key for fine positioning
  if (key(VK_MENU))
    da = 1.0;                // 30 deg/sec

  // swivel
  if (key(VK_LEFT))
    base += da;
  else if (key(VK_RIGHT))
    base -= da;

  // shoulder
  if (key(VK_UP))
    lift += da;
  else if (key(VK_DOWN))
    lift -= da;

  // hand
  if (key(VK_PRIOR))
    grip += da;
  else if (key(VK_NEXT))
    grip -= da;
}


//= Get breathing color command (0-9).

void jhcQtDrive::get_color ()
{
  int i;

  for (i = 0; i < 10; i++)
    if (key('0' + i))
    {
      col = i;
      break;
    }
}


//= Get mouth flash command (<SPACE> <BACK>).

void jhcQtDrive::get_mouth ()
{
  if (key(' '))
    mth = 2;
  else if (key(VK_BACK))
    mth = 1;
  else
    mth = 0;
}


//= Check if some particular key is currently pressed.

bool jhcQtDrive::key (int code) const
{
  return((GetAsyncKeyState(code) >> 15) != 0);
}