// jhcQtCamCal.h : interactive extrinsic camera calibration for Qtruck robot
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

#include "jhcQtruck.h"


//= Interactive extrinsic camera calibration for Qtruck robot.

class jhcQtCamCal : public jhcQtruck
{
// PRIVATE MEMBER VARIABLES
private:
  // static camera image
  unsigned char *img;

  // marked image points 
  int ptx[3], pty[3];
  int step, np, trial, wait;


// PUBLIC MEMBER FUNCTIONS
public:
  // creation and initialization
  ~jhcQtCamCal ();
  jhcQtCamCal ();
  int Setup ();
 

// PROTECTED MEMBER FUNCTIONS
protected:
  // primary loop overrides
  int Launch ();
  int Respond ();
  void Cleanup ();


// PRIVATE MEMBER FUNCTIONS
private:
  // primary loop
  int lower_hand ();
  int close_hand ();
  int mark_points ();
  void draw_cross (int x, int y);
  int compute_calib ();
  int await_key ();
  int save_values ();


};
