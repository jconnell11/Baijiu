// jhcQtruck.h : handles text messages to/from Hiwonder Qtruck robot
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

#pragma once

#include <windows.h>

#include "jhc_pthread.h"


//= Handles text messages to/from Hiwonder Qtruck robot.
// The easiest way to interface to Bluetooth LE is through the Python "Bleak" 
// library, but it wants to be "boss". This class turns the situation inside-out 
// by spawning a background thread with a new "primary" loop.

class jhcQtruck
{
// PRIVATE MEMBER VARIABLES
private:
  // Bluetooth information exchange
  char data[15], cmd[15], actuators[15];
  int fresh;

  // primary loop control
  pthread_t ctrl;
  pthread_mutex_t xchg;
  int run, active;

  // timing, compass filter, and speed servo
  unsigned long tick, todo;
  double hvar, ips0, dps0, msum, rsum;
  int mdir, rdir;

  // arm servo commands, angles, targets, and speeds
  double bc, sc, bnow, snow, bt, st, asp;

  // hand servo command, angle, target, and speed
  double gc, gnow, gt, gsp;


// PROTECTED MEMBER PARAMETERS
protected:
  // Microbit ID and robot name 
  char mb[10], name[40];

  // arm geometry
  double by, bs, sz, sw, boff, soff, goff;

  // hand geometry
  double fout, fdn, fsep, jaw, fext, g0;

  // camera geometry
  double cdot, crt, cp0, ct0, cr0;

  // tank tracks
  double tsep, scrub, kips, moff;


// PROTECTED MEMBER VARIABLES
protected:
  // raw robot sensor values
  double comp, tilt, roll, dist, volt;
  int line;

  // raw robot actuator settings
  double lf, rt, base, lift, grip;
  int col, mth;

  // derived odometric values
  double head, dt, dm, dr;


// PUBLIC MEMBER FUNCTIONS
public:
  // creation and initialization
  jhcQtruck ();
  virtual int Setup ();

  // Bluetooth connection
  int BluStart (const char *id);
  const char *BluSwap (const char *sensors);
  void BluDone ();


// PROTECTED MEMBER FUNCTIONS
protected:
  // primary loop overrides
  virtual int Launch ();
  virtual int Respond ();
  virtual void Cleanup ();

  // message exchange
  int Update ();
  void Issue ();

  // neck interface
  void Gaze (double p, double t, double dps =90.0);
  void CamLoc (double& x, double& y, double& z) const;
  void CamDir (double *p, double *t, double *r =NULL) const;

  // arm interface
  void Home (double dps =90.0);
  double Astray () const;
  void Reach (double x, double y, double z, double ips =6.0);
  void HandLoc (double& x, double& y, double& z) const;
  void HandDir (double *p, double *t =NULL, double *r =NULL) const;
  double HandErr (double x, double y, double z) const;

  // hand interface
  void Grip (double w, double ips =5.0);
  double Width () const;

  // base interface
  void Drive (double ips, double dps);
  double Battery () const;

  // utilities
  void Pace (int ms =33);
  void R2D2 () const;
  void Show () const;
  void XYZ () const;
  void Stop ();


// PRIVATE MEMBER FUNCTIONS
private:
  // creation and initialization
  void cfg_params ();
  void init_state ();
  void def_vals ();

  // Bluetooth connection
  static pthread_ret churn_away (void *qt);
  void calib_vals (const char *id);

  // message exchange
  void decode_info (const char *msg);
  void compute_odom ();
  void ramp_arm ();
  void ramp_hand ();
  void encode_cmds (char *msg, int ssz);
  void calc_speeds ();

  // loop helpers
  void servo_correct (double& mv, double& rot, double ips, double dps);

};
