// vid_test.cpp : speed test of spio_win.dll video capture and display
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

#pragma comment(lib, "winmm.lib")      // for timeGetTime

#include <windows.h>                   // needed for Sleep
#include <stdio.h>
#include <conio.h>

#include "vid_ocv.h"


//= Speed test of spio_win.dll video capture and display.
// hardwired for ESP32Cam wifi streaming at VGA 
// about 24 fps @ 6', 18 fps @ 13' (no antenna -> EdiMax)
// takes final IP number as argument, e.g. 240 or 155

int main (int argc, char *argv[])
{
  char ipnam[40] = "http://192.168.0.155:81/stream";
  const unsigned char *buf;
  unsigned long start;
  double secs;
  int rc, cnt = 0, warp = 1, show = 1;

//show = 0;
//warp = 0;

  // build camera URL using argument to exec
  if (argc > 1)
    sprintf_s(ipnam, "http://192.168.0.%s:81/stream", argv[1]);

  // connect to camera
  printf("Opening %s ...\n", ipnam);
  if ((rc = ocv_open(ipnam, 1)) <= 0)
  {
    printf("Failed to open video source -> %d\n", rc);
    return 0;
  }

  // optional geometric correct and display
  if (warp > 0)        
    ocv_warp(0.1405, -0.1331, 0.0249, 219, 1, 313.1, 242.3);     
  if (show > 0)
    ocv_win(0, "Camera View", 1100, 0);     

  // continuously framegrab
  printf("Streaming video (hit any key to exit) ...\n");
  start = timeGetTime();
  while (!_kbhit())
  {
    if (ocv_get(&buf, 1) <= 0)         // blocks
    {
      printf("Video connection lost!\n");
      break;
    }
    if (show > 0)
    {
      ocv_queue(0, buf, 640, 480);
      ocv_show(); 
    }
    cnt++;
    printf("\r  %d ", cnt);
    fflush(stdout);
  }

  // report speed and cleanup
  secs = 0.001 * (timeGetTime() - start);
  if (cnt > 0)
    printf("frames in %3.1f secs = %3.1f fps\n", secs, cnt / secs);
  else
    printf("  0 frames in 0.0 secs = 0.0 fps\n");
  ocv_close();

  // keep terminal window visible
  while (_kbhit())
    _getch();
  printf("Hit any key to exit ...\n");
  _getch();
  return 1;
}

