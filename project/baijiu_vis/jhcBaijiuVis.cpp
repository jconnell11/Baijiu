// jhcBaijiuVis.cpp : coordinate Qtruck with ALIA variables and camera
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2024-2026 Etaoin Systems
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
#include "vid_ocv.h"
#include "rng_flr.h"
#include "alia_vis.h"

#include "jhcBaijiuVis.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default destructor does necessary cleanup.

jhcBaijiuVis::~jhcBaijiuVis ()
{
  delete [] view;
  delete [] map;
}


//= Default constructor initializes certain values.

jhcBaijiuVis::jhcBaijiuVis ()
{
  snap = NULL;
  view = NULL;
  map  = NULL;
  vok  = 0;
  got  = 0;
  show = 0;
  p0 = 6;                    // x1.25 = younger voice
  r0 = 0;                    // normal speed talking
}


///////////////////////////////////////////////////////////////////////////
//                             Primary Loop                              //
///////////////////////////////////////////////////////////////////////////

//= Override to initialize external system before loop.
// "dbg" argument specifies which images to show (if any)
// returns positive if okay, 0 or negative for problem

int jhcBaijiuVis::Launch (int dbg)
{
  // connect to ESP32-Cam 
  printf("Starting video stream ...\n");
  vok = 0;
  if (ocv_open(ipcam, 1) <= 0)                   // bottom-up
  {
    printf("\x1b[1;33m  >>> No active camera found!\x1b[0m\n");
    return 0;
  }
  ocv_warp(0.14, -0.13, 0.024, 219, 1, cr0, 313, 242);       
  vok = 1;

  // start TTS and web speech recognition
  if (spio_start() < 0)
  {
    printf("\x1b[1;33m  >>> Problem with speech recognition!\x1b[0m\n");
    PlaySFX("toot");                             // only warn
  }
  else
    PlaySFX("R2D2_faint");                       // whistle
  Sleep(1000);

  // start reasoning engine (builds name list for speech reco)
  if (alia_reset(NULL, "Waldo Baijiu", "baijiu_vis", dbg) <= 0)
  {
    printf("\x1b[1;33m  >>> Problem with ALIA!\x1b[0m\n");
    return 0;                                    // fail
  }
  reco_spell();                                  // names
  sf = 1.0;

  // configure optional debugging output images
  show = 0;
  if (dbg != 0)
  {
    show = 1;
    view = new unsigned char [3 * 640 * 480];
    map  = new unsigned char [3 * alia_wmap() * alia_hmap()];
    alia_view = view;
    alia_map  = map;
    alia_vfmt = 1;
    alia_mfmt = 1;
    ocv_win(0, "Camera View", 340, 0);        
    ocv_win(1, alia_tmap(), 1000, 0);
  }

  // set up to initialize pose buffer
  fill = -1;

  // reset cumulative odometry
  mapx = 0.0;
  mapy = 0.0;
  trav = 0.0;
  wind = 0.0;

  // reset interim adjustments
  imapx = 0.0;
  imapy = 0.0;
  itrav = 0.0;
  iwind = 0.0;

  // reset depth-from-motion processor
  rng_init(219, 640, 480);
  return 1;
}


//= Override to run core step of primary external loop.
// generates an output command based on current sensor values
// returns positive if okay, 0 or negative for system quit

int jhcBaijiuVis::Respond ()
{
  int rc;

  // prepare ALIA inputs
  img_update();                        // read new frame (if any - blocks)
  Update();                            // receive data from robot base
  assemble_sensors();                  // digest recent sensor information

  // run behavioral routines
  rc = alia_think();

  // digest ALIA outputs
  expand_commands();                   // flesh out commands for this cycle
  Issue();                             // transmit actions to robot base
  img_issue();                         // possibly show debugging images

  // wait for constant 30 Hz rate
  Pace();                     
  return __min(rc, vok);
}


//= Override to perform shutdown operations on external system.

void jhcBaijiuVis::Cleanup ()
{
  alia_done(0);
  jhcQtruck::Cleanup();
  ocv_close();
  if (vok <= 0)
    PlaySFX("squawk");
}


//= Analyze data from sensors and reconfigure for ALIA reasoner.

void jhcBaijiuVis::assemble_sensors ()
{
  reco_update();
  body_update();
  neck_update();
  arm_update();
  base_update();
}


//= Translate commands from ALIA reasoner to detailed actuator values.

void jhcBaijiuVis::expand_commands ()
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
  int prev = alia_hear;

  if ((alia_hear = reco_status()) == 2)
    alia_spin(reco_heard(), reco_delay());
  else if ((prev >= 0) && (alia_hear < 0))
  {
    printf("\x1b[1;33m  >>> Speech recognition disconnected!\x1b[0m\n");
    PlaySFX("toot");                             // only warn
  }
}


//= Possibly speak output text and set status flag. 

void jhcBaijiuVis::tts_issue ()
{
  const char *output;
  int dp, dr; 

  output = alia_spout();
  if (*output != '\0')
  {
    prosody(dp, dr, alia_mood);
    tts_say(output, p0 + dp, r0 + dr);
  }
  alia_talk = ((tts_status() > 0) ? 1 : 0);
}


//= Set appropriate voice pitch and rate for current mood (bits).
// [ surprised angry scared happy : unhappy bored lonely tired ]
// upper 8 bits mirror lower 8 but are intensifiers ("very")

void jhcBaijiuVis::prosody (int& dp, int& dr, int bits) const
{
  // prioritize bits in mood vector
  if ((bits & 0x80) != 0)             // excited (surprised)
  {
    dp = 2;
    dr = 2;
  }
  else if ((bits & 0x40) != 0)        // angry or annoyed
  {
    dp = (((bits & 0x4000) != 0) ? -2 : -1);
    dr = 1;
  }
  else if ((bits & 0x20) != 0)         // scared or wary
  {
    dp = (((bits & 0x2000) != 0) ? 4 : 2);
    dr = 1;
  }
  else if ((bits & 0x10) != 0)         // happy or pleased
  {
    dp = (((bits & 0x1000) != 0) ? 3 : 2);
    dr = 0;
  }
  else if ((bits & 0x04) != 0)         // bored or zoned out (tired)
  {
    dp = (((bits & 0x0400) != 0) ? -2 : -1);
    dr = -1;
  }
  else if ((bits & 0x02) != 0)         // sad (lonely) or discontent
  { 
    dp = (((bits & 0x0200) != 0) ? -2 : -1);
    dr = dp;
  }
  else if ((bits & 0x01) != 0)         // tired
  {
    dp = -1;
    dr = -1;
  }
  else                                 // default neutral
  {
    dp = 0;
    dr = 0;
  }
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
// delayed for capture + compress + transmit + receive + decompress time

void jhcBaijiuVis::neck_update ()
{
  double x, y, z, p, t, r;
  int i, lag = 4;                      // cycles at ALIA update rate (30Hz)

  // get current pose
  CamLoc(x, y, z);
  CamDir(p, t, r);

  // possibly preload pose history buffer
  if (fill < 0)
    for (i = 0; i < 10; i++)
    {
      pose[i][0] = x;
      pose[i][1] = y;
      pose[i][2] = z;
      pose[i][3] = p;
      pose[i][4] = t;
      pose[i][5] = r;
    }

  // add current pose to history
  fill = (fill + 1) % 10;
  pose[fill][0] = x;
  pose[fill][1] = y;
  pose[fill][2] = z;
  pose[fill][3] = p;
  pose[fill][4] = t;
  pose[fill][5] = r;

  // report delayed pose to ALIA
  i = (fill + 10 - lag) % 10;          // modulo neg is neg!
  alia_cx = (float) pose[i][0];        // color image
  alia_cy = (float) pose[i][1];
  alia_cz = (float) pose[i][2];
  alia_cp = (float) pose[i][3];
  alia_ct = (float) pose[i][4];
  alia_cr = (float) pose[i][5];
  alia_rx = alia_cx;                   // range = color
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
  double p0, t0, r0, p, t, ndps = 90.0, gips = 12.0;       
  int rbid = __max(alia_rpi, alia_rti), gbid = alia_rgi;
  int cbid = __max(alia_cpi, alia_cti);

  // determine neck command mode
  if (cbid > __max(rbid, gbid))        // color camera angles 
  {
    CamDir(p0, t0, r0);
    p = ((alia_cpv > 0.0) ? alia_cpt : p0);
    t = ((alia_ctv > 0.0) ? alia_ctt : t0);
    Gaze(p, t, sf * __max(alia_cpv, alia_ctv) * ndps);
  }
  else if (rbid >= gbid)               // range-finder angles (range = color)
  {
    CamDir(p0, t0, r0);
    p = ((alia_rpv > 0.0) ? alia_rpt : p0);
    t = ((alia_rtv > 0.0) ? alia_rtt : t0);
    Gaze(p, t, sf * __max(alia_rpv, alia_rtv) * ndps);
  }
  else if (gbid > 0)                   // range-finder view location (xyz)
    LookAt(alia_rxt, alia_ryt, alia_rzt, sf * alia_rgv * ndps); 

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
  double x, y, z, p, t, r, w, hold = 5.0;        // half max force (oz)

  // tell angular offset from home position
  alia_aj = (float) Astray();

  // record current hand position and orientation (no delay)
  HandLoc(x, y, z);
  HandDir(p, t, r);
  alia_ax = (float) x;
  alia_ay = (float) y;
  alia_az = (float) z;
  alia_ap = (float) p;
  alia_at = (float) t;
  alia_ar = (float) r;

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
  double rads = D2R * (wind + iwind + 0.5 * dr);   // avg heading

  // accumulate incremental motion into on-going adjustments
  imapx += dm * cos(rads);
  imapy += dm * sin(rads);
  itrav += dm;
  iwind += dr;

  // pass cumulative best guesses to ALIA (main vals updated by rng_d16)
  alia_bx = (float)(mapx + imapx);
  alia_by = (float)(mapy + imapy);
  alia_bt = (float)(trav + itrav);
  alia_bw = (float)(wind + iwind);     
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


///////////////////////////////////////////////////////////////////////////
//                                Images                                 //
///////////////////////////////////////////////////////////////////////////

//= See if next frame in camera stream is available.
// sets got = 1 if new frame, vok = 0 if wifi drops out

void jhcBaijiuVis::img_update ()
{
  double cx, cy, cz, cp, ct, cr;

  // check if new frame in camera stream is available.
  if ((got = ocv_get(&snap, 1)) < 0)
  {
    printf("\n\x1b[1;33m  >>> Video stream lost!\x1b[0m\n");
    vok = 0;
    return;
  }

  // record best-guess odometry at time of image acquisition
  mapx += imapx;
  mapy += imapy;
  trav += itrav;
  wind += iwind;

  // restart interim accumulation of offsets
  imapx = 0.0;
  imapy = 0.0;
  itrav = 0.0;
  iwind = 0.0;

  // launch background range inference routine with best-guess pose
  CamLoc(cx, cy, cz);
  CamDir(cp, ct, cr);
  rng_est(snap, cz, ct);

  // wait for completion then rewrite main cumulative odometry
  if (rng_rdy(200) > 0)
  {
    rng_d16(&alia_rng, &alia_col);
    alia_cfmt = 1;           // mark both images as ready
    alia_rfmt = 1;
  }
}


//= Display new marked-up camera view and overhead map (if desired).

void jhcBaijiuVis::img_issue ()
{
  HWND term;

  // show both the color and map (or debug) images in separate windows
  if (show <= 0)
    return;
  ocv_queue(0, view, 640, 480);
  ocv_queue(1, map, alia_wmap(), alia_hmap());
  ocv_show();                                    // blocks for 1ms 

  // put the terminal window back on top so it will accept keyboard input
  if (show > 1)
    return;
  term = GetConsoleWindow();
  SetForegroundWindow(term);     
  SetActiveWindow(term);
  SetFocus(term);           
  show = 2;
}

