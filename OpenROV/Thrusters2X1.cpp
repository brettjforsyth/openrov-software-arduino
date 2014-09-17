#include "AConfig.h"
#if(HAS_STD_2X1_THRUSTERS)
#include "Device.h"
#include "Pin.h"
#include "Thrusters2X1.h"
#include "Settings.h"
#include "Motor.h"
#include "Timer.h"

//Motors motors(9, 10, 11);
//Motors motors(6, 7, 8);
Motor port_motor(PORT_PIN);
Motor vertical_motor(VERTICAL_PIN);
Motor starboard_motor(STARBOARD_PIN);

int new_p = MIDPOINT;
int new_s = MIDPOINT;
int new_v = MIDPOINT;
int p = MIDPOINT;
int v = MIDPOINT;
int s = MIDPOINT;

float trg_throttle,trg_yaw,trg_lift;
int trg_motor_power;

Timer controltime;
Timer thrusterOutput;
boolean bypasssmoothing;

#ifdef ESCPOWER_PIN
bool canPowerESCs = true;
Pin escpower("escpower", ESCPOWER_PIN, escpower.digital, escpower.out);
#else
boolean canPowerESCs = false;
#endif

int smoothAdjustedServoPosition(int target, int current){
  // if the MIDPOINT is betwen the change requested in velocity we want to go to MIDPOINT first, and right away.
  if (((current < MIDPOINT) && (MIDPOINT < target)) || ((target < MIDPOINT) && (MIDPOINT < current))){
    return MIDPOINT;
  }
  // if the change is moving us closer to MIDPOINT it is a reduction of power and we can move all the way to the target
  // in one command
  if (abs(MIDPOINT-target) < abs(MIDPOINT-current)){
    return target;
  }
  // else, we need to smooth out amp spikes by making a series of incrimental changes in the motors, so only move part of
  // the way to the target this time.
  double x = target - current;
  int sign = (x>0) - (x<0);
  int adjustedVal = current + sign * (min(abs(target - current), Settings::smoothingIncriment));
  // skip the deadzone
  if (sign<0) {
    return (min(adjustedVal,Settings::deadZone_min));
  } else if(sign>0){
    return (max(adjustedVal,Settings::deadZone_max));
  } else
    return (adjustedVal);
}

void Thrusters::device_setup(){
  port_motor.reset();
  vertical_motor.reset();
  starboard_motor.reset();
  thrusterOutput.reset();
  controltime.reset();
  bypasssmoothing = false;
  #ifdef ESCPOWER_PIN
    escpower.reset();
    escpower.write(1); //Turn on the ESCs
  #endif
}

void Thrusters::device_loop(Command command){
  if (command.cmp("mtrmod")) {
      port_motor.motor_positive_modifer = command.args[1]/100;
      vertical_motor.motor_positive_modifer = command.args[2]/100;
      starboard_motor.motor_positive_modifer = command.args[3]/100;
      port_motor.motor_negative_modifer = command.args[4]/100;
      vertical_motor.motor_negative_modifer = command.args[5]/100;
      starboard_motor.motor_negative_modifer = command.args[6]/100;
  }
  if (command.cmp("rmtrmod")) {
      Serial.print(F("mtrmod:"));
      Serial.print(port_motor.motor_positive_modifer);
      Serial.print (",");
      Serial.print(vertical_motor.motor_positive_modifer);
      Serial.print (",");
      Serial.print(starboard_motor.motor_positive_modifer);
      Serial.print (",");
      Serial.print(port_motor.motor_negative_modifer);
      Serial.print (",");
      Serial.print(vertical_motor.motor_negative_modifer);
      Serial.print (",");
      Serial.print(starboard_motor.motor_negative_modifer);
      Serial.println (";");
  }

  if (command.cmp("go")) {
      //ignore corrupt data
      if (command.args[1]>999 && command.args[2] >999 && command.args[3] > 999 && command.args[1]<2001 && command.args[2]<2001 && command.args[3] < 2001) {
        p = command.args[1];
        v = command.args[2];
        s = command.args[3];
        if (command.args[4] == 1) bypasssmoothing=true;
      }
    }

  if (command.cmp("port")) {
      //ignore corrupt data
      if (command.args[1]>999 && command.args[1]<2001) {
        p = command.args[1];
        if (command.args[2] == 1) bypasssmoothing=true;
      }
  }

  if (command.cmp("vertical")) {
      //ignore corrupt data
      if (command.args[1]>999 && command.args[1]<2001) {
        v = command.args[1];
        if (command.args[2] == 1) bypasssmoothing=true;
      }
  }

  if (command.cmp("starboard")) {
      //ignore corrupt data
      if (command.args[1]>999 && command.args[1]<2001) {
        s = command.args[1];
        if (command.args[2] == 1) bypasssmoothing=true;
      }
  }

  if (command.cmp("thro") || command.cmp("yaw")){
    if (command.cmp("thro")){
      if (command.args[1]>=-100 && command.args[1]<=100) {
        trg_throttle = command.args[1]/100.0;
      }
    }

    if (command.cmp("yaw")) {
        //ignore corrupt data
        if (command.args[1]>=-100 && command.args[1]<=100) { //percent of max turn
          trg_yaw = command.args[1]/100.0;
        }
    }

    // The code below spreads the throttle spectrum over the possible range
    // of the motor. Not sure this belongs here or should be placed with
    // deadzon calculation in the motor code.
    if (trg_throttle>=0){
      p = 1500 + (500/abs(port_motor.motor_positive_modifer))*trg_throttle;
      s = p;
    } else {
      p = 1500 + (500/abs(port_motor.motor_negative_modifer))*trg_throttle;
      s = p;
    }

    trg_motor_power = s;

    int turn = trg_yaw*250; //max range due to reverse range
    if (trg_throttle >=0){
      int offset = (abs(turn)+trg_motor_power)-2000;
      if (offset < 0) offset = 0;
      p = trg_motor_power+turn-offset;
      s = trg_motor_power-turn-offset;
    } else {
      int offset = 1000-(trg_motor_power-abs(turn));
      if (offset < 0) offset = 0;
      p = trg_motor_power+turn+offset;
      s = trg_motor_power-turn+offset;
    }

  }

  if (command.cmp("lift")){
    if (command.args[1]>=-100 && command.args[1]<=100) {
      trg_lift = command.args[1]/100.0;
      v = 1500 + 500*trg_lift;
    }
  }


  #ifdef ESCPOWER_PIN
    else if (command.cmp("escp")) {
      escpower.write(command.args[1]); //Turn on the ESCs
      Serial.print(F("log:escpower="));
      Serial.print(command.args[1]);
      Serial.println(';');
    }
  #endif
    else if (command.cmp("start")) {
      port_motor.reset();
      vertical_motor.reset();
      starboard_motor.reset();
    }
    else if (command.cmp("stop")) {
      p = MIDPOINT;
      v = MIDPOINT;
      s = MIDPOINT;
      // Not sure why the reset does not re-attach the servo.
      //port_motor.stop();
      //vertical_motor.stop();
      //starboard_motor.stop();
    }
    #ifdef ESCPOWER_PIN
    else if ((command.cmp("mcal")) && (canPowerESCs)){
      Serial.println(F("log:Motor Callibration Staring;"));
      //Experimental. Add calibration code here
      Serial.println(F("log:Motor Callibration Complete;"));
  }
    #endif
  //to reduce AMP spikes, smooth large power adjustments out. This incirmentally adjusts the motors and servo
  //to their new positions in increments.  The incriment should eventually be adjustable from the cockpit so that
  //the pilot could have more aggressive response profiles for the ROV.
  if (controltime.elapsed (50)) {
    if (p!=new_p || v!=new_v || s!=new_s) {
      new_p = smoothAdjustedServoPosition(p,new_p);
      new_v = smoothAdjustedServoPosition(v,new_v);
      new_s = smoothAdjustedServoPosition(s,new_s);
      if (bypasssmoothing)
      {
        new_p=p;
        new_v=v;
        new_s=s;
        bypasssmoothing = false;
      }
      Serial.print(F("motors:"));
      Serial.print(port_motor.goms(new_p));
      Serial.print(',');
      Serial.print(vertical_motor.goms(new_v));
      Serial.print(',');
      Serial.print(starboard_motor.goms(new_s));
      Serial.println(';');
    }

  }

  navdata::FTHR = map((new_p + new_s) / 2, 1000,2000,-100,100);

  //The output from the motors is unique to the thruster configuration
  if (thrusterOutput.elapsed(1000)){
    Serial.print(F("mtarg:"));
    Serial.print(p);
    Serial.print(',');
    Serial.print(v);
    Serial.print(',');
    Serial.print(s);
    Serial.println(';');
    thrusterdata::MATC = port_motor.attached() || port_motor.attached() || port_motor.attached();

  }
}
#endif
