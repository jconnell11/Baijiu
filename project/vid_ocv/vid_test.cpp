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

#ifndef __linux__
  #include <windows.h>                 // needed for Sleep
  #include <stdio.h>
  #pragma comment(lib, "winmm.lib")    // for timeGetTime
#else
  #include <time.h>
  #include "jhc_str_s.h"

  static unsigned long timeGetTime ()
  {
    timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
  }
#endif

#include "jhc_conio.h"

#include "vid_ocv.h"


//= Speed test of spio_win.dll video capture and display.
// hardwired for ESP32Cam wifi streaming at VGA 
// about 24 fps @ 6', 18 fps @ 13' (no antenna -> EdiMax)
// argument is camera unit number (default = Baijiu streamer)

int main (int argc, char *argv[])
{
  char ipname[80] = "http://192.168.0.200:81/stream";
  const unsigned char *buf;
  unsigned long start;
  double secs;
  int rc, unit, cnt = 0, warp = 1, show = 1;

  // connect to camera
  if (argc > 1)
  {
    unit = atoi(argv[1]);
    printf("Opening camera %d ...\n", unit);
    rc = ocv_cam(unit, 1);
  }
  else
  {
    printf("Opening %s ...\n", ipname);
    rc = ocv_open(ipname, 1);
  }
  if (rc <= 0)
  {
    printf("  Failed to open video source!\n");
    return 0;
  }

  // optional geometric correct and display
  if (warp > 0)        
    ocv_warp(0.14, -0.13, 0.024, 219, 1, 1, 313, 242);    // wide lens
  if (show > 0)
    ocv_win(0, "Camera View", 1100, 0);     

  // continuously framegrab
  printf("Streaming video (hit any key to stop) ...\n");
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
  secs = 0.001 * (double)(timeGetTime() - start);
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
  _kbdone();                 // for Linux
  return 1;
}

