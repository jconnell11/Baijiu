// jhcQtruck.cpp : handles text messages to/from Hiwonder Qtruck robot
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

#pragma comment(lib, "winmm.lib")       // for PlaySound and timeGetTime

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <conio.h>

#include "jhcQtruck.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default constructor initializes certain values.

jhcQtruck::jhcQtruck ()
{
  // unknown robot
  *mb = '\0';
  strcpy_s(name, "Waldo");

  // initialize exchange
  *data = '\0';
  fresh = 0;

  // clear background thread
  run = 0;
  active = 0;

  // set control variables
  cfg_params();
  def_vals();
  init_state();
  Issue();     
} 


//= Set up hardware geometry and control constants.

void jhcQtruck::cfg_params ()
{
  // arm geometry
  by = 1.14;                 // rotate in front of center (29mm)
  bs = 0.35;                 // rotate to shoulder radial (9mm)
  sz = 3.15;                 // upper sh joint above floor (80mm) 
  sw = 2.36;                 // sw 4 bar linkage length (60mm)
  boff = 0.0;                // base servo offset for center (deg)
  soff = 0.0;                // lift servo offset for level (deg)
  goff = 0.0;                // grip servo offset for closed (deg)

  // hand geometry
  fout = 1.30;               // finger servo out from wrist (33mm)
  fdn  = 1.46;               // finger down from upper pivot (37mm)
  fsep = 0.87;               // finger axes separation (22mm)
  jaw  = 1.93;               // finger axis to inner grip (49mm)
  fext = 1.88;               // extension = sqrt(jaw^2 - (fsep/2)^2)
  g0   = 13.0;               // fully closed = asin((sep/2) / jaw)

  // camera geometry
  cdot = 2.36;               // offset along sw link (60mm)
  crt  = 1.69;               // offset ortho to sw link (43mm)
  cp0  = 0.0;                // true pan wrt calculated (deg)
  ct0  = 0.0;                // true tilt wrt calculated (deg)
  cr0  = 0.0;                // image constant roll (deg)

  // tank tracks
  kips  = 12.0;              // convert from ips to motor cmd
  moff  = 35.0;              // min motor cmd value (linear fit)
  tsep  = 4.80;              // track center separation (122mm)
  scrub = 0.80;              // turn inefficiency (voltage dependent)
}


//= Set up defaults for low-level sensors and actuators.

void jhcQtruck::def_vals ()
{
  // sensor data
  comp = 0.0;                // jittery magnetic compass (0 to 359 degs CCW)
  tilt = 0.0;                // upwards body angle (-64 to 63 degs)
  roll = 0.0;                // right side lifted up (-64 to 63 degs)
  dist = 0.0;                // 15 deg sonar from front (1.2 to 200 in)
  line = 0x0F;               // 4 bit underneath reflectance (MSB = leftmost)
  volt = 4.0;                // measured battery voltage

  // actuator commands
  lf   = 0.0;                // left motor command (-100 to +100)
  rt   = 0.0;                // left motor command (-100 to +100)
  base = 0.0;                // arm rotation physical angle (-90 to 90 degs)
  lift = 25.0;               // arm lift physical angle (-30 to 40 degs)
  grip = 20.0;               // gripper physical angle (-15 to 55 degs)
  col  = 7;                  // breathing color (0 to 9 = KROYGBPVMW)
  mth  = 0;                  // red LED diamond (0 to 2 = off, dim, bright)
}


//= Initialize all runtime control variables.
// assumes def_vals() already called

void jhcQtruck::init_state ()
{
  // servo targets and speeds
  bt   = base;
  bnow = base;
  st   = lift;
  snow = lift;
  gt   = grip;
  gnow = grip;
  asp  = 0.0;
  gsp  = 0.0;

  // timing and servo state
  tick = 0;                  // time of last Pace() call
  todo = 0;                  // time of last odometry update
  head = -1.0;               // compass filter not initialized
  ips0 = 0.0;                // expected translation speed
  dps0 = 0.0;                // expected rotation speed
  mdir = 0;                  // previous translation direction
  rdir = 0;                  // previous rotation direction
  msum = 0.0;                // sum of translation speed errors
  rsum = 0.0;                // sum of rotation speed errors
}


//= Initializes system before attempting Bluetooth robot connection.
// returns positive if successful, 0 or negative for failure

int jhcQtruck::Setup ()
{
  printf("  No early initializations\n");
  return 1;
}


///////////////////////////////////////////////////////////////////////////
//                          Bluetooth Connection                         //
///////////////////////////////////////////////////////////////////////////

//= Initialize external system from Python and start background thread.
// returns positive if okay, 0 or negative for problem

int jhcQtruck::BluStart (const char *id)
{
  // initialize main system
  calib_vals(id);
  def_vals();
  if (Launch() <= 0)                   // override                   
    return 0;

  // start background thread (uses Respond override)
  run = 1;
  active = 1;
  pthread_create(&ctrl, NULL, churn_away, this);
  return 1;
}


//= Take a sensor data string and return an actuator command string.

const char *jhcQtruck::BluSwap (const char *sensors)
{
  pthread_mutex_lock(xchg)
  *actuators = '\0';
  if (run > 0)
    strcpy_s(actuators, cmd);
  strcpy_s(data, sensors);
  fresh = 1;
  pthread_mutex_unlock(xchg)
  return actuators;
}


//= Cleanly terminate external system from Python.

void jhcQtruck::BluDone ()
{
  if (active <= 0)
    return;
  run = 0;
  pthread_join(ctrl, NULL);
}


//= Load per-robot parameters from calibration file based on Microbit ID.
// simple file format:
//   line 1 = name                name of robot (e.g. Waldo)
//   line 2 = boff soff goff      arm servo offsets for zero angle
//   line 3 = cp0 ct0 cr0         camera angle adjustments wrt nominal

void jhcQtruck::calib_vals (const char *id)
{
  char fname[80], line[80];
  FILE *in;

  // look for calibration file associated with this robot
  strcpy_s(mb, id);
  sprintf_s(fname, "config/%s_calib.cfg", id);
  if (fopen_s(&in, fname, "r") != 0)
  {
    printf("Could not read file: %s !\n", fname);
    return;
  }

  // read servo zero offsets and camera misalignments
  if (fgets(name, 40, in) != NULL)
  {
    name[strlen(name) - 1] = '\0';
    if (fgets(line, 90, in) != NULL)
    {
      sscanf_s(line, "%lf %lf %lf", &boff, &soff, &goff); 
      if (fgets(line, 90, in) != NULL)
        sscanf_s(line, "%lf %lf %lf", &cp0, &ct0, &cr0);
    }
  }
  fclose(in);
}


//= Background thread does all reasoning for robot independent of Bluetooth.

pthread_ret jhcQtruck::churn_away (void *qt)
{
  jhcQtruck *me = (jhcQtruck *) qt;

  while (me->run > 0)
    if (me->Respond() <= 0)            // override    
      break;
  me->run = 0;
  me->Cleanup();                       // override
  return 0;
}


///////////////////////////////////////////////////////////////////////////
//                       Primary Loop Overrides                          //
///////////////////////////////////////////////////////////////////////////

//= Override to initialize external system before loop.
// returns positive if okay, 0 or negative for problem

int jhcQtruck::Launch ()
{
  R2D2();
  printf("  Hit any key to exit ...\n");
  return 1;
}


//= Override to run core step of primary external loop.
// generate one output command based on current sensor values
// returns positive if okay, 0 or negative for system quit

int jhcQtruck::Respond ()
{
  Pace();
  if (_kbhit())
  {
    printf("\n\n");
    return 0;
  }
  Show();
  return 1;
}


//= Override to perform shutdown operations on external system.

void jhcQtruck::Cleanup ()
{
  printf("\n--> Remember to power-down speaker!\n\n");
  Stop();
}


///////////////////////////////////////////////////////////////////////////
//                          Message Translation                          //
///////////////////////////////////////////////////////////////////////////

//= Update all local sensor variables.
// return 1 if new data from robot, 0 for just odometry
// NOTE: generates odometry info needed by SetSpeeds()

int jhcQtruck::Update()
{
  char msg[15];
  int fr;

  // make copy of most recent Bluetooth input
  //   sensors[] -> data[] -> msg[]
  pthread_mutex_lock(xchg)
  strcpy_s(msg, data);                
  fr = fresh;
  fresh = 0;
  pthread_mutex_unlock(xchg)

  // extract raw and derived sensor values
  if (fr > 0)
    decode_info(msg);
  compute_odom();
  return((fr > 0) ? 1 : 0);
}


//= Parse input sensor data into various fields.
// sensor info string is hex coded  = CC:TT:RR:DD:L:V   

void jhcQtruck::decode_info (const char *msg)
{
  char hex[5];
  int i, val, cm;

  // make sure data is new and has correct format
  if (strlen(msg) < 10)
    return;
  for (i = 0; i < 10; i++)
    if (!isxdigit(msg[i]))
      return;

  // byte0 = compass LSB (8)
  strncpy_s(hex, msg, 2);
  sscanf_s(hex, "%x", &val);

  // byte1 = compass MSB (1) + tilt (7)
  strncpy_s(hex, msg + 2, 2);
  sscanf_s(hex, "%x", &i);
  comp = (double)(((i & 0x80) << 1) | val);               // degs ccw
  comp += 180.0;                                          // robot front
  if (comp > 360.0)
    comp -= 360.0;
  tilt = (double)((i & 0x7F) - 64);
    
  // byte2 = distance MSB (1) + roll (7)
  strncpy_s(hex, msg + 4, 2);
  sscanf_s(hex, "%x", &i);
  roll = (double)((i & 0x7F) - 64);

  // byte3 = distance LSB (8)
  strncpy_s(hex, msg + 6, 2);
  sscanf_s(hex, "%x", &val);
  cm = ((i & 0x80) << 1) | val;      
  dist = (((cm > 400) || (cm <= 0)) ? 200.0 : cm / 2.54);  // inches

  // byte4 = line sensors (4) + battery voltage (4)
  strncpy_s(hex, msg + 8, 2);
  sscanf_s(hex, "%x", &val);
  line = (val & 0xF0) >> 4;
  volt = 0.05 * (val & 0x0F) + 3.25;
}


// Get smoothed heading and find characteristics of motion during last cycle.
// sets sensor variables "head", "dt", "dm", and "dr" (also "todo" and "hvar")
// updates expected servo angles "bnow", "snow", and "gnow"
// ignores details of transfer times and rates -- assumes perfect control
// NOTE: smoothed direction "head" is still noisy and not very accurate

void jhcQtruck::compute_odom ()
{
  double diff, vm, kal, h0 = head, f = 0.1, nv = 81.0;     // compass +/- 9 degs
  unsigned long last = todo;

  // get duration "dt" of last cycle (time since last call)
  todo = timeGetTime();
  dt = 0.0;
  if (last != 0)
    dt = 0.001 * (double)(todo - last);

  // estimate translation "dm" and rotation "dr" based on speeds 
  dm = ips0 * dt;
  dr = dps0 * dt;

  // assume all servo targets reached (account for clipping)
  bnow = (bc - 90.0) - boff;
  snow = (sc - 75.0) - soff;
  gnow = ((gc - 90.0) - g0) - goff;

  // possibly initialize compass filter (to cut jitter in half)
  if (h0 < 0.0)
  {
    head = comp;
    hvar = nv;
    dr = 0.0;                 // no rotation "dr"
    return;
  }

  // project from last step and estimate variance
  diff = comp - head;
  if (diff > 180.0)
    diff -= 360.0;
  else if (diff <= -180.0)
    diff += 360.0;
  vm = (1.0 - f) * hvar + f * diff * diff;

  // add in new measurement using Kalman gain
  kal = vm / (vm + nv);
  head += kal * diff;
  if (head >= 360.0)
    head -= 360.0;
  else if (head < 0.0)
    head += 360.0;
  hvar = (1.0 - kal) * vm; 
}


//= Build transfer command string from individual command variables. 
// NOTE: call this at end to generate valid robot command string

void jhcQtruck::Issue ()
{
  char msg[15];

  // slowly change servo setpoints then assemble command string
  ramp_arm();
  ramp_hand();
  encode_cmds(msg, 15);
 
  // copy to persistent Bluetooth output buffer
  //   msg[] -> cmd[] -> actuators[]
  pthread_mutex_lock(xchg)
  strcpy_s(cmd, msg);                   
  pthread_mutex_unlock(xchg)

  // save expected base speeds (compass is lousy for turns!)
  calc_speeds();
}


//= Change arm servo command angles based on assigned targets and rates.
// expects next cycle will take same amount of time as this cycle

void jhcQtruck::ramp_arm ()
{
  double bdev, sdev, binc, sinc, clr = 0.0;

  // skip if not using high-level Reach()
  if (asp <= 0.0)
    return;

  // make sure arm lifts up near corners of base
  if (fabs(bnow) >= 36.0)
  {
    if (snow < clr)
      bt = bnow;                    
    st = __max(clr, st);
  } 

  // enforce minimum speed to prevent gear binding (> 90 dps)
  binc = __max(90.0, asp) * dt;
  sinc = binc;

  // coordinate speeds so servos finish simultaneously
  bdev = fabs(bnow - bt);
  sdev = fabs(snow - st);
  if (bdev > sdev)
    sinc *= sdev / bdev;
  else if (sdev > 0.0)
    binc *= bdev / sdev;
   
  // adjust base servo angle (avoid limit stall)
  binc = __min(bdev, binc);
  if (bt >= bnow)
    base = bnow + binc;
  else
    base = bnow - binc;
  base = __max(-90.0, __min(base, 90.0));

  // adjust shoulder servo angle (avoid limit stall)
  sinc = __min(sdev, sinc);
  if (st >= snow)
    lift = snow + sinc;
  else
    lift = snow - sinc;
  lift = __max(-30.0, __min(lift, 40.0));
}


//= Change gripper servo command angle based on assigned target and rate.
// expects next cycle will take same amount of time as this cycle

void jhcQtruck::ramp_hand ()
{  
  double gdev, ginc;

  // skip if not using high-level Grip()
  if (gsp <= 0.0)
    return;

  // determine amount to change (> 90 dps to prevent gear binding)
  ginc = __max(90.0, gsp) * dt;
  gdev = fabs(gnow - gt);
  ginc = __min(gdev, ginc);

  // alter desired angle and clip to valid range
  if (gt >= gnow)
    grip = gnow + ginc;
  else
    grip = gnow - ginc;
  grip = __max(-15.0, __min(grip, 55.0));
}


//= Assemble various actuator values into command string.
// motor command string is decimal coded = LL:RR:BBB:FF:GG:C:M 

void jhcQtruck::encode_cmds (char *msg, int ssz)
{
  double lgap, rgap;

  // convert physical angles to servo angles
  bc = (base + 90.0) + boff;
  sc = (lift + 75.0) + soff;
  gc = ((g0 + grip) + 90.0) + goff;

  // enforce servo command ranges 
  lf  = __max(-100.0, __min(lf, 100.0));
  rt  = __max(-100.0, __min(rt, 100.0));
  bc  = __max( 0.0, __min(bc, 180.0));
  sc  = __max(30.0, __min(sc, 120.0));
  gc  = __max(80.0, __min(gc, 145.0));
  col = __max(0, __min(col, 9));
  mth = __max(0, __min(mth, 2));

  // remap motors: -100 to -52 -> 0-48, deadband -> 49, 51 to 100 -> 50-99
  lgap = ((lf >= 51.0) ? lf - 1.0 : ((lf <= -52.0) ? lf + 100.0 : 49.0));
  rgap = ((rt >= 51.0) ? rt - 1.0 : ((rt <= -52.0) ? rt + 100.0 : 49.0));

  // lift servo offset = 25 deg, grip servo offset = 65 deg
  sprintf_s(msg, ssz, "%02d%02d%03d%02d%02d%d%d", 
            (int)(lgap + 0.5), (int)(rgap + 0.5), 
            (int)(bc + 0.5), (int)(sc - 24.5), (int)(gc - 64.5), 
            col, mth);
}


//= Get expected rotation and translation speeds by back-computing from motor settings.
// assumes variables "lf" and "rt" have current command values (incl. limits)

void jhcQtruck::calc_speeds ()
{
  double lsp, rsp, lag = 0.94;         // timing fudge factor 

  // convert left motor command to ips
  lsp = (fabs(lf) - moff) / kips;         
  lsp = __max(0.0, lsp);
  if (lf < 0.0)
    lsp = -lsp;

  // convert left motor command to ips
  rsp = (fabs(rt) - moff) / kips;          
  rsp = __max(0.0, rsp);
  if (rt < 0.0)
    rsp = -rsp;

  // average is translation, scaled difference is rotation
  ips0 = lag * 0.5 * (lsp + rsp);
  dps0 = lag * scrub * 180.0 * (rsp - lsp) / (tsep * M_PI);
}


///////////////////////////////////////////////////////////////////////////
//                            Neck Interface                             //
///////////////////////////////////////////////////////////////////////////

//= Aim camera centerline in a particular direction.
// generally needs to change position of camera to achieve this
// pan is CCW from forward, tilt is up from horizontal, angles in degrees

void jhcQtruck::Gaze (double p, double t, double dps)
{
  bt = p - cp0;
  st = (t + 15.0) - ct0;
  asp = dps;
}


//= Tell current location of camera wrt center of robot body.
// y points forward, x is to right, z up from floor, coordinates in inches

void jhcQtruck::CamLoc (double& x, double& y, double& z) const
{
  double b = bnow * M_PI / 180.0, s = snow * M_PI / 180.0;
  double r = bs + cdot * cos(s) - crt * sin(s);

  x = r * sin(b);
  y = r * cos(b) + by;
  z = sz + cdot * sin(s) + crt * cos(s);
}                    


//= Tell current viewing direction of camera wrt its own position.
// pan is CCW from forward, tilt is up from horizontal, angles in degrees

void jhcQtruck::CamDir (double *p, double *t, double *r) const
{
  if (p != NULL)
    *p = bnow + cp0;
  if (t != NULL)
    *t = (snow - 15.0) + ct0;
  if (r != NULL)
    *r = cr0;
}


///////////////////////////////////////////////////////////////////////////
//                            Arm Interface                              //
///////////////////////////////////////////////////////////////////////////

//= Move hand to default high forward facing location.

void jhcQtruck::Home (double dps)
{
  bt =  90.0;
  st = 100.0;
  asp = dps;
}
 

//= Tell how many degrees worst joint is away from Home() position.

double jhcQtruck::Astray () const
{
  return __max(fabs(bnow - 90.0), fabs(snow - 100.0));
}


//= Move fingertips close to given location wrt center of body.
// y points forward, x is to right, z up from floor, coordinates in inches
// can only really move hand in thin arc (shell) in front of robot

void jhcQtruck::Reach (double x, double y, double z, double ips)
{
  double dy, err;

  // aim so grasp point on line from shoulder to target
  dy = y - by;
  bt = atan2(x, dy) * -180.0 / M_PI;
  st = asin((z + fdn - sz) / sw) * 180.0 / M_PI;

  // compute angular speed based on Cartesian distance
  err = __max(fabs(bt - bnow), fabs(st - snow));
  asp = ips * err / HandErr(x, y, z);
}                 
 

//= Tell current location of fingertips wrt center of robot body.
// y points forward, x is to right, z up from floor, coordinates in inches

void jhcQtruck::HandLoc (double& x, double& y, double& z) const
{
  double b = bnow * M_PI / 180.0, s = snow * M_PI / 180.0;
  double r = bs + sw * cos(s) + fout + fext;

  x = -r * sin(b);
  y =  r * cos(b) + by;
  z = sz + sw * sin(s) - fdn;
}


//= Tell current orientation of finger jaws.
// pan is CCW from forward, tilt is up from horizontal, angles in degrees

void jhcQtruck::HandDir (double *p, double *t, double *r) const
{
  if (p != NULL)
    *p = bnow;
  if (t != NULL)
    *t = 0.0;
  if (r != NULL)
    *r = 0.0;
}


//= Tell how far current hand location is from given target (inches).

double jhcQtruck::HandErr (double x, double y, double z) const
{
  double dx, dy, dz;

  HandLoc(dx, dy, dz);
  dx -= x;
  dy -= y;
  dz -= z;
  return sqrt(dx * dx + dy * dy + dz * dz);
}


///////////////////////////////////////////////////////////////////////////
//                            Hand Interface                             //
///////////////////////////////////////////////////////////////////////////

//= Change fingertip separation to the given width in inches.
// hand servo may stall out before goal if gripping an object

void jhcQtruck::Grip (double w, double ips)
{
  double s = (w - fsep) / (2.0 * jaw);
  
  gt = 180.0 * asin(s) / M_PI;         // g = 0 is straight out
  gsp = 90.0 * ips / (M_PI * jaw);     // approximate
}


//= Tell the nominal fingertip separation in inches.
// hand servo may have stalled before this if gripping an object
 
double jhcQtruck::Width () const
{
  double rads = gnow * M_PI / 180.0;

  return(fsep + 2.0 * jaw * sin(rads));
}


///////////////////////////////////////////////////////////////////////////
//                            Base Interface                             //
///////////////////////////////////////////////////////////////////////////

//= Specify desired travel (in/sec) and rotational (deg/sec) speeds.
// sets raw base motor variables "lf" and "rt"
// generally on carpet max: ips = 5.5"/sec, dps = 90 deg/sec 

void jhcQtruck::Drive (double ips, double dps)
{  
  double mv, rot, diff, lsp, rsp, over;

  // alter commands to catch-up if too slow
  servo_correct(mv, rot, ips, dps);
 
  // mix translation and rotation then convert to motor commands
  diff = rot * (tsep * M_PI) / (360.0 * scrub);
  lsp = mv - diff;
  rsp = mv + diff;
  lf = kips * fabs(lsp) + moff;
  rt = kips * fabs(rsp) + moff;

  // turning is unreliable unless one track is at full power (100)
  if ((lf < 100.0) && (rt < 100.0))
  {
    lf = kips * fabs(mv) + moff;
    rt = lf;
  }

  // normalize to scale rotation and translation the same
  over = __max(lf, rt) / 100.0;
  if (over > 1.0)
  {
    lf /= over;
    rt /= over;
  }
  
  // squelch deadband (motors don't move for small cmds) then restore sign 
  if (lf < 50.0)
    lf = 0.0;
  if (rt < 50.0)
    rt = 0.0;
  if (lsp < 0.0)
    lf = -lf;
  if (rsp < 0.0)
    rt = -rt;
}


//= Boost or diminish commanded translation and rotation rates based on speed errors.
// updates variables "mdir", "rdir", "msum", and "rsum"

void jhcQtruck::servo_correct (double& mv, double& rot, double ips, double dps)
{
  int md0 = mdir, rd0 = rdir;

  // save requested directions (+1, 0, -1)
  mdir = ((ips > 0.0) ? 1 : ((ips == 0.0) ? 0 : -1));
  rdir = ((dps > 0.0) ? 1 : ((dps == 0.0) ? 0 : -1));

  // reset accumulator if type of motion changes
  if ((mdir != md0) || (rdir != rd0))
  {
    msum = 0.0;
    rsum = 0.0;
  }
  else 
  {
    // accumulate errors but limit integrals (windup)
    msum += (ips - ips0);              
    msum = __max(-6.0, __min(msum, 6.0));   
    rsum += (dps - dps0);              
    rsum = __max(-100.0, __min(rsum, 100.0)); 
  }

  // boost speeds by accumulated slowness
  mv  = ips + msum;            
  rot = dps + rsum;           
}


//= Tells battery charge state in percent (0-100).
// uses linear approximation based on measured voltage

double jhcQtruck::Battery () const
{
  double pct, v100 = 4.0, v20 = 3.5, v10 = 3.35, v0 = 3.25;  

  if (volt >= v100)
    pct = 100.0;
  else if (volt >= v20)
    pct = 80.0 * (volt - v20) / (v100 - v20) + 20.0;
  else if (volt >= v10)
    pct = 10.0 * (volt - v10) / (v20 - v10) + 10.0;
  else if (volt >= v0)
    pct = 10.0 * (volt - v0) / (v10 - v0);
  else
    pct = 0.0;
  return pct;
}


///////////////////////////////////////////////////////////////////////////
//                               Utilities                               //
///////////////////////////////////////////////////////////////////////////

//= Wait until next cycle tick governed by "ms" variable.

void jhcQtruck::Pace (int ms) 
{
  unsigned long now = timeGetTime();
  int wait;

  if (tick == 0)
    tick = now;
  wait = tick - now;
  tick += ms;               // no cumulative error
  wait = __max(0, wait);
  Sleep(wait);              // Sleep(0) yields briefly
}


//= Play the R2D2 whistle audio clip (blocks until finished).

void jhcQtruck::R2D2 () const
{
  PlaySound(L"R2D2_half.wav", NULL, SND_SYNC);
}


//= Keep overwriting same line with current sensors info.
// generally need to printf("\n") after this to preserve last line

void jhcQtruck::Show () const
{
  printf("    head %5.1f, tilt %3.0f, roll %3.0f, dist %5.1f, line %d%d%d%d, bat %4.2f, servo[%3.0f %3.0f %3.0f] -> cmd \"%s\"\r",
         head, tilt, roll, dist, line >> 3, (line >> 2) & 1, (line >> 1) & 1, line & 1, volt, bnow, snow, gnow + g0, cmd);
}


//= Keep overwriting same line with Cartesian positions (in mm).
// generally need to printf("\n") after this to preserve last line

void jhcQtruck::XYZ () const
{
  double hx, hy, hz, dir, w, cx, cy, cz, ct;

  // gather data
  HandLoc(hx, hy, hz);
  HandDir(&dir);
  w = Width();
  CamLoc(cx, cy, cz);
  CamDir(NULL, &ct);

  // convert from inches to mm
  hx *= 25.4;
  hy *= 25.4;
  hz *= 25.4;
  w  *= 25.4;
  cx *= 25.4;
  cy *= 25.4;
  cz *= 25.4;

  // show user
  printf("    [%3.0f %3.0f %3.0f] --(%4.0f)--> hand [%3.0f %3.0f %3.0f] x %3.0f, cam [%3.0f %3.0f %3.0f] up %3.0f\r", 
         bnow, snow, gnow, dir, hx, hy, hz, w, cx, cy, cz, ct);
}


//= Set both track speeds to zero and wait for acceptance.

void jhcQtruck::Stop ()
{
  int i;

  // set speed command
  lf = 0.0;
  rt = 0.0;
  Issue();

  // wait a while for new transfer to occur
  Update();
  for (i = 0; i < 20; i++)
  {
    if (Update() > 0)
      break;
    Sleep(50);
  }
}

 