// baijiu_vis.cpp : interface to ALIA with camera for Qtruck using pc_blulink.py
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

#ifndef DEXP
 #define DEXP __declspec(dllexport)
#endif

#include <windows.h>
#include <stdio.h>

#include "jhcBaijiuVis.h"


///////////////////////////////////////////////////////////////////////////
//                          Global Variables                             //
///////////////////////////////////////////////////////////////////////////

//= An instance of the main computational class.

static jhcBaijiuVis vis;


///////////////////////////////////////////////////////////////////////////
//                      Initialization and Locking                       //
///////////////////////////////////////////////////////////////////////////

//= Defines the entry point for the DLL application.

BOOL APIENTRY DllMain (HANDLE hModule,
                       DWORD ul_reason_for_call, 
                       LPVOID lpReserved)
{
  HANDLE out;
  DWORD mode;

  // enable virtual terminal processing for color text
  if ((out = GetStdHandle(STD_OUTPUT_HANDLE)) != INVALID_HANDLE_VALUE)
    if (GetConsoleMode(out, &mode))
      SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

  return TRUE;
}


///////////////////////////////////////////////////////////////////////////
//                           Main Functions                              //
///////////////////////////////////////////////////////////////////////////

//= Initializes external system before attempting robot connection.
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ext_init ()
{
  return vis.Setup();
}


//= Resets processing state at the start of a run given robot ID.
// at this point robot should be connected and responsive
// returns positive if successful, 0 or negative for failure

extern "C" DEXP int ext_start (const char *id)
{
  return vis.BluStart(id);
}


//= Takes a Qtruck sensor string and returns a command string.
// sensor data is hex coded = CC:TT:RR:DD:L:V      (10 chars)
// command is decimal coded = LL:RR:BBB:FF:GG:C:M  (13 chars)
// Note: call rate varies from 16-32 Hz, return NULL or "" to exit

extern "C" DEXP const char *ext_swap (const char *data)
{
  return vis.BluSwap(data);
}


//= Releases any allocated resources (call at end of run).

extern "C" DEXP void ext_done ()
{
  return vis.BluDone();
}

