// jhcQtCamCal.cpp : interactive extrinsic camera calibration for Qtruck robot
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

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <conio.h>

#include "vid_ocv.h"

#include "jhcQtCamCal.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default destructor does necessary cleanup.

jhcQtCamCal::~jhcQtCamCal ()
{
  delete [] img;
}


//= Default constructor initializes certain values.

jhcQtCamCal::jhcQtCamCal ()
{
  img = new unsigned char [3 * 640 * 480];
  step = 0;
  np = 0;
  trial = 0;
  wait = 0;
}


//= Initializes system before attempting Bluetooth robot connection.
// returns positive if successful, 0 or negative for failure

int jhcQtCamCal::Setup ()
{
  // connect to ESP32Cam 
  if (ocv_open("http://192.168.5.1:81/stream") <= 0)
  {
    printf("  No video stream -- is wifi set to HW_ESP32Cam?\n");
    return 0;
  }
  ocv_warp(0.7, -11.0);

  // make a window to display video
  ocv_win(0, "Marked Points", 20, 50);
  return 1;
}


///////////////////////////////////////////////////////////////////////////
//                       Primary Loop Overrides                          //
///////////////////////////////////////////////////////////////////////////

//= Override to initialize external system before loop.
// returns positive if okay, 0 or negative for problem

int jhcQtCamCal::Launch ()
{
  printf("\nCamera extrinsic calibration (click R to abort)\n");
  return 1;
}


//= Override to run core step of primary external loop.
// generate one output command based on current sensor values
// returns positive if okay, 0 or negative for system quit

int jhcQtCamCal::Respond ()
{
  // run loop at about 30 Hz and keep refreshing image
  if (step >= 2)
    Pace();
  else if (ocv_get(img, 1) <= 0)
    return -1;
  ocv_queue(0, img);
  ocv_show(); 

  // progress through point designation and analysis
  switch (step)
  {
    case 0:
      return lower_hand();
    case 1:
      return close_hand();
    case 2:
      return mark_points();
    case 3:
      return compute_calib();
    case 4:
      return await_key();
    default:
      return save_values();
  }
}     


//= Move hand near floor so camera gazing downward.
// reads robot sensors and generates commands

int jhcQtCamCal::lower_hand ()
{
  double p, t, t0 = -45.0;   

  // get presumed tilt and tell hand to move down
  Update();
  CamDir(&p, &t);
  Gaze(0.0, t0);
  Issue();

  // see if good enough or stuck 
  if ((fabs(p) < 1.0) && (fabs(t - t0) < 1.0))
  {
    step++;
    trial = 0;
    return 1;
  }
  if (++trial >= 50)
  {                          
    printf("Cannot lower hand!\n");
    return -1;
  }
  return 1;
}


//= Make fingers touch each other for single grasp point.
// reads robot sensors and generates commands

int jhcQtCamCal::close_hand ()
{
  double w;

  // get presumed width and tell fingers to close
  Update();
  w = Width();
  Grip(0.0);
  Issue();

  // see if good enough or stuck 
  if (w < 0.1)
    if (++wait >= 10)        // for image stabilization
    {
      step++;
      printf("  Click on fingertip grasp point ...\n");
      return 1;
    }
  if (++trial >= 50)
  {                          
    printf("Cannot close hand!\n");
    return -1;
  }
  return 1;
}


//= Mark fingertips and servo screws.
// ignores physical robot

int jhcQtCamCal::mark_points ()
{
  int but, mx, my;

  // regularly poll for new mouse click in window
  ocv_queue(0, img);
  ocv_show(); 
  if ((but = ocv_click(0, mx, my)) <= 0)
    return 1;
  if (but == 3)
    return 0;                // user abort

  // save point and show on image 
  ptx[np] = mx;
  pty[np] = my;
  draw_cross(mx, my);
  np++;

  // prompt for next point
  if (np == 1)
    printf("  Click on left rear servo screw ...\n");
  else if (np == 2)
    printf("  Click on right rear servo screw ...\n");
  else
    step++;
  return 1;
}


//= Draw small cross on stored image in red
// given y is TOP DOWN

void jhcQtCamCal::draw_cross (int x, int y)
{
  int i, x0, x1, y0, y1, by = 479 - y, arm = 8;
  unsigned char *d;

  // clip to image boundaries
  x0 = __max(0, x - arm);
  x1 = __min(x + arm, 639);
  y0 = __max(0, by - arm);
  y1 = __min(by + arm, 479);
  
  // horizontal line 
  d = img + 1920 * by;
  x0 *= 3;
  x1 *= 3;
  for (i = x0; i < x1; i += 3)
  {
    d[i]     = 0;
    d[i + 1] = 0;
    d[i + 2] = 255;
  }

  // vertical line
  d = img + 3 * x;
  y0 *= 1920;
  y1 *= 1920;
  for (i = y0; i <= y1; i += 1920)
  {
    d[i]     = 0;
    d[i + 1] = 0;
    d[i + 2] = 255;
  }
}


//= Use marked points and known robot geometry to solve for offsets.

int jhcQtCamCal::compute_calib ()
{
  double R2D = 180.0 / M_PI, xmid = 319.5, ymid = 239.5, flen = 204.4;
  double hx, hy, hz, cx, cy, cz, th, tc, ti;

double p = cp0, t = ct0, r = cr0;

  // find expected tilt of finger midpoint based on geometry
  HandLoc(hx, hy, hz);
  CamLoc(cx, cy, cz);
  th = R2D * atan2(hz - cz, hy - cy);

  // compare to image tilt of finger midpoint (should be same)
  ti = R2D * atan2(ymid - pty[0], flen);
  CamDir(NULL, &tc);                   // includes original ct0
  ct0 += th - (tc + ti);               

  // find pan of fingertips and roll of screws (both should be zero)
  cp0 = R2D * atan2(xmid - ptx[0], flen);
  cr0 = R2D * atan2(pty[1] - pty[2], ptx[2] - ptx[1]);
  
  // announce estimates and assess magnitudes
  printf("\nEstimated offsets: pan %3.1f, tilt %3.1f, roll %3.1f\n", cp0, ct0, cr0);
  if ((fabs(cp0) > 5.0) || (fabs(ct0) > 10.0) || (fabs(cr0) > 5.0))
    printf("  >>> Corrections seem too large!\n");
  printf("\nHit any key to save values (ESC to abort) ...\n");
  step++;
  return 1;
}


//= Let user ponder points and calculation.

int jhcQtCamCal::await_key ()
{
  if (!_kbhit())
    return 1;
  if (_getch() == 0x1B)
    return -1;
  step++;
  return 1;
}


//= Modify robot calibration file associated to contain new values.

int jhcQtCamCal::save_values ()
{
  char fname[80];
  FILE *out;

  // open calibration file
  sprintf_s(fname, "config/%s_calib.cfg", mb);
  if (fopen_s(&out, fname, "w") != 0)
  {
    printf("  Could not write to file: %s !\n", fname);
    return -1;
  }

  // write previous name and servo offsets plus new camera values 
  fprintf(out, "%s\n", name);
  fprintf(out, "%1.0f %1.0f %1.0f\n", boff, soff, goff);
  fprintf(out, "%3.1f %3.1f %3.1f\n", cp0, ct0, cr0);
  fclose(out);
  printf("  SUCCESS - wrote values to: %s\n", fname);
  return 0;
}


//= Override to perform shutdown operations on external system.

void jhcQtCamCal::Cleanup ()
{
  printf("\n");
}