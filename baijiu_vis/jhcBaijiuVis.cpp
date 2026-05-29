// jhcBaijiuVis.cpp : coordinate Qtruck with ALIA variables and camera
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2026 Etaoin Systems
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

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>

#include "spio_win.h"
#include "alia_vis.h"

#include "jhcBaijiuVis.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Initializes system before attempting Bluetooth robot connection.
// returns positive if successful, 0 or negative for failure

int jhcBaijiuVis::Setup ()
{
  mapx = 0.0;
  mapy = 0.0;
  trav = 0.0;
  wind = 0.0;
  sf   = 1.0;
  return 1;
}


///////////////////////////////////////////////////////////////////////////
//                             Primary Loop                              //
///////////////////////////////////////////////////////////////////////////

//= Override to initialize external system before loop.
// returns positive if okay, 0 or negative for problem

int jhcBaijiuVis::Launch ()
{
  // whistle to user after Bluetooth connected
  R2D2();

  // start reasoning engine (builds name list)
  if (alia_reset(NULL, "Waldo Baijiu", "baijiu_vis") <= 0)
  {
    printf("  Problem with ALIA !\n");
    return 0;
  }

  // start TTS and speech reco (uses name list)
  if (spio_start() <= 0)
  {
    printf("  Problem with speech !\n");
    return 0;
  }
  return 1;
}


//= Override to run core step of primary external loop.
// generates an output command based on current sensor values
// returns positive if okay, 0 or negative for system quit

int jhcBaijiuVis::Respond ()
{
  int rc;

  Update();
  get_sensors();              // digest recent sensor information
  rc = alia_think();          // determine new strategic direction
  set_commands();             // flesh out commands for this cycle
  Issue();
  Pace();                     // wait for constant 30 Hz rate
  return rc;
}


//= Override to perform shutdown operations on external system.

void jhcBaijiuVis::Cleanup ()
{
  alia_done(0);
  jhcQtruck::Cleanup();
}


//= Analyze data from sensors and reconfigure for ALIA reasoner.

void jhcBaijiuVis::get_sensors ()
{
  reco_update();
  body_update();
  neck_update();
  arm_update();
  base_update();
}


//= Translate commands from ALIA reasoner to detailed actuator values.

void jhcBaijiuVis::set_commands ()
{
  tts_issue();
  body_issue();
  if (arm_mode() > 0)
    neck_issue();
  else
    arm_issue();               
  base_issue();
}


//= Determine if arm is in regular (0) or pseudo-neck mode (1).

int jhcBaijiuVis::arm_mode ()
{
  int arm = __max(__max(alia_api, alia_adi), alia_aji);
  int rng = __max(__max(alia_rpi, alia_rti), alia_rgi);
  int col = __max(alia_cpi, alia_cti); 

  if (__max(rng, col) > arm)
    return 1;
  return 0;
}


///////////////////////////////////////////////////////////////////////////
//                                Speech                                 //
///////////////////////////////////////////////////////////////////////////

//= Get any speech recognition results and set status flag.

void jhcBaijiuVis::reco_update ()
{
  if ((alia_hear = reco_status()) == 2)
    alia_spin(reco_heard(), reco_delay());
}


//= Possibly speak output text and set status flag. 

void jhcBaijiuVis::tts_issue ()
{
  const char *output;

  output = alia_spout();
  if (*output != '\0')
    tts_say(output);
  alia_talk = ((tts_status() > 0) ? 1 : 0);
}


///////////////////////////////////////////////////////////////////////////
//                                 Body                                  //
///////////////////////////////////////////////////////////////////////////

//= Get battery level from main robot.

void jhcBaijiuVis::body_update ()
{
  alia_batt = (float) Battery();
  alia_tilt = (float) tilt;
  alia_roll = (float) roll;
}


//= Adjust LEDs on main robot body and handle muting.

void jhcBaijiuVis::body_issue ()
{
  int shape;

  // get talking status and possibly mute microphone input
  shape = tts_status() - 1;
  alia_talk = ((shape >= 0) ? 1 : 0);
  reco_mute(shape + 1);

  // bright diamond if vowel (visemes 1-11, w -> 7) else dim
  if ((shape >= 1) && (shape <= 11) && (shape != 7))        
    mth = 2;
  else if (shape >= 0)
    mth = 1;
  else
    mth = 0;

  // corner LEDs solid green if listening, otherwise color reflects mood
  // bits: [ surprised angry scared happy : unhappy bored lonely tired ]
  if (alia_attn > 0)
    col = 4;
  else if ((alia_mood & 0x80) != 0)    // surprised -> white
    col = 9;
  else if ((alia_mood & 0x40) != 0)    // angry -> red
    col = 1;
  else if ((alia_mood & 0x20) != 0)    // scared -> yellow
    col = 3;
  else if ((alia_mood & 0x10) != 0)    // happy -> magenta
    col = 8;
  else if ((alia_mood & 0x08) != 0)    // unhappy -> blue
    col = 5;
  else                                 // neutral -> lavender
    col = 7;

  // modulate action speeds based on emotion
  sf = 1.0;
  if ((alia_mood & 0x21) != 0)         // scared or tired
    sf = 0.8;
  if ((alia_mood & 0x0140) != 0)       // very happy or angry
    sf *= 1.2;
}


///////////////////////////////////////////////////////////////////////////
//                                 Neck                                  //
///////////////////////////////////////////////////////////////////////////

//= Get current camera location and viewing direction.

void jhcBaijiuVis::neck_update ()
{
  double x, y, z, p, t, r;

  CamLoc(x, y, z);
  CamDir(&p, &t, &r);
  alia_cx = (float) x;       // color image
  alia_cy = (float) y;
  alia_cz = (float) z;
  alia_cp = (float) p;
  alia_ct = (float) t;
  alia_cr = (float) r;
  alia_rx = alia_cx;         // range = color
  alia_ry = alia_cy;
  alia_rz = alia_cz;
  alia_rp = alia_cp;
  alia_rt = alia_ct;
  alia_rr = alia_cr;
}


//= Set arm joint angles based on some camera aiming specification.
// arbitration with hand pose commands happens in main set_commands()

void jhcBaijiuVis::neck_issue ()
{
  double p0, t0, p, t, ndps = 180.0, gips = 12.0;
  int rbid = __max(alia_rpi, alia_rti), gbid = alia_rgi;
  int cbid = __max(alia_cpi, alia_cti);

  // determine neck command mode
  if (cbid > __max(rbid, gbid))        // color camera angles 
  {
    CamDir(&p0, &t0);
    p = ((alia_cpv > 0.0) ? alia_cpt : p0);
    t = ((alia_ctv > 0.0) ? alia_ctt : t0);
    Gaze(p, t, sf * __max(alia_cpv, alia_ctv) * ndps);
  }
  else if (rbid >= gbid)               // range-finder angles (range = color)
  {
    CamDir(&p0, &t0);
    p = ((alia_rpv > 0.0) ? alia_rpt : p0);
    t = ((alia_rtv > 0.0) ? alia_rtt : t0);
    Gaze(p, t, sf * __max(alia_rpv, alia_rtv) * ndps);
  }
  else                                 // range-finder view location (xyz)
    LookAt(alia_rxt, alia_ryt, alia_rzt, sf * alia_rgv); 

  // ALWAYS adjust hand (apply force -> close fully)
  if (alia_awt < 0.0)
    Grip(0.0, sf * alia_awv * gips);
  else
    Grip(alia_awt, sf * alia_awv * gips);
}


///////////////////////////////////////////////////////////////////////////
//                                  Arm                                  //
///////////////////////////////////////////////////////////////////////////

//= Get current hand pose, deviation from home, and gripper status.

void jhcBaijiuVis::arm_update ()
{
  double x, y, z, p, w, hold = 5.0;              // half max force (oz)

  // tell angular offset from home position
  alia_aj = (float) Astray();

  // record current hand position and orientation
  HandLoc(x, y, z);
  HandDir(&p);
  alia_ax = (float) x;
  alia_ay = (float) y;
  alia_az = (float) z;
  alia_ap = (float) p;
  alia_at = 0.0;
  alia_ar = 0.0;

  // record current gripper width and force
  w = Width();
  alia_aw = (float) w;
  if ((alia_af <= 0.0) && (w < 0.2))             // surely closed
    alia_af = (float) hold;
  else if ((alia_af > 0.0) && (w > 3.0))         // surely open
    alia_af = 0.0;
}


//= Set arm joint angles based on some desired hand pose.
// arbitration with neck commands happens in main set_commands()

void jhcBaijiuVis::arm_issue ()
{
  double aips = 6.0, gips = 12.0;
  int mbid = __max(alia_api, alia_adi);

  // determine arm control mode
  if (alia_aji > mbid)                 // goto standard pose                                  
    Home(sf * alia_ajv * aips);         
  else if (mbid > 0)                   // Cartesian positioning        
    Reach(alia_axt, alia_ayt, alia_azt, sf * alia_apv * aips);

  // ALWAYS adjust hand (apply force -> close fully)
  if (alia_awt < 0.0)
    Grip(0.0, sf * alia_awv * gips);
  else
    Grip(alia_awt, sf * alia_awv * gips);
}


///////////////////////////////////////////////////////////////////////////
//                                 Base                                  //
///////////////////////////////////////////////////////////////////////////

//= Convert odometry into progress along axis and perpendicular to it.

void jhcBaijiuVis::base_update ()
{
  double rads = (wind + 0.5 * dr) * M_PI / 180.0;   // avg heading

  // accumulate incremental motion into global frame
  mapx += dm * cos(rads);
  mapy += dm * sin(rads);
  trav += dm;
  wind += dr;

  // pass to ALIA
  alia_bx = (float) mapx;
  alia_by = (float) mapy;
  alia_bt = (float) trav;
  alia_bw = (float) wind;
}


//= Set wheel velocities based on rate and sign of incremental amount.

void jhcBaijiuVis::base_issue ()
{
  double ips, dps, msp = 5.0, tsp = 90.0;                

  ips = msp * alia_bmv * sf;
  if (alia_bmt < trav)
    ips = -ips;
  dps = tsp * alia_brv * sf;
  if (alia_brt < wind)
    dps = -dps;
  Drive(ips, dps);
}
