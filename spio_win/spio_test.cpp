// spio_test.cpp : simple test of spio_win.dll recognition and TTS
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

#include <windows.h>                   // needed for Sleep
#include <stdio.h>
#include <conio.h>

#include "spio_win.h"


//= Simple test of spio_win.dll recognition and TTS.

int main (int arg, char *argv[])
{
  int rc;

  // connect to web   
  if ((rc = spio_start("..", 1)) <= 0)
  {
    printf("Failed to start -> %d\n", rc);
    return 0;
  }

  // announce entry
  printf("Recognizing speech (hit any key to exit) ...\n");
  reco_mute(1);
  tts_say("Ready to go.");
  while (tts_status() > 0)
    Sleep(33);

  // continuously listen
  reco_mute(0);
  while (!_kbhit())
  {
    if (reco_status() == 2)
      printf("Heard: %s\n", reco_heard());
    Sleep(33);
  }

  // announce exit (with overriding interruption)
  reco_mute(1);
  tts_say("See you later.");
  Sleep(800);
  tts_say("Hey, why did you turn me off?");
  printf("visemes: ");
  while ((rc = tts_status()) > 0)
  {
    printf("%d ", rc - 1);
    Sleep(33);
  }

  // cleanup
  spio_done();
  printf("\nDone\n");
}

