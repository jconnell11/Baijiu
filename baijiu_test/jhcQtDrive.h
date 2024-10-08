// jhcQtDrive.h : use keyboard to drive Hiwonder Qtruck robot
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


//= Use keyboard to drive Hiwonder Qtruck robot.

class jhcQtDrive : public jhcQtruck
{
// PRIVATE MEMBER VARIABLES
private:
  unsigned char *buf;


// PUBLIC MEMBER FUNCTIONS
public:
  // creation and initialization
  ~jhcQtDrive ();
  jhcQtDrive ();
  int Setup ();

 
// PROTECTED MEMBER FUNCTIONS
protected:
  // primary loop overrides
  int Launch ();
  int Respond ();


// PRIVATE MEMBER FUNCTIONS
private:
  // keyboard interpretation 
  void get_track ();
  void get_arm ();
  void get_color ();
  void get_mouth ();
  bool key (int code) const;

};
