#include "app.h"
#include "util.h"

const int bumper_sensor = EV3_PORT_1;
const int linemon_sensor = EV3_PORT_3;
const int left_motor = EV3_PORT_A;
const int right_motor = EV3_PORT_C;
const int carrier_sensor = EV3_PORT_2;
const int walldetector_sensor = EV3_PORT_4;

#define WD_DISTANCE 10
int wd_distance = WD_DISTANCE;

#define LM_THRESHOLD 20
int lm_threshold = LM_THRESHOLD;

#define DR_POWER 20
int dr_power = DR_POWER;

#include "time.h"
static unsigned long last_confirm_time = 0;

int carrier_cargo_is_loaded(void) {
  return ev3_touch_sensor_is_pressed(carrier_sensor);
}

int wall_detector_is_detected(void) {
  return ev3_ultrasonic_sensor_get_distance(walldetector_sensor) < wd_distance;
}

int bumper_is_pushed(void) {
  return ev3_touch_sensor_is_pressed(bumper_sensor);
}

int linemon_is_online(void) {
  return ev3_color_sensor_get_reflect(linemon_sensor) < lm_threshold;
}

void driver_turn_left(void) {
  ev3_motor_set_power(left_motor, 0);
  ev3_motor_set_power(right_motor, dr_power);
}

void driver_turn_right(void) {
  ev3_motor_set_power(left_motor, dr_power);
  ev3_motor_set_power(right_motor, 0);
}

void driver_stop(void) {
  ev3_motor_stop(left_motor, false);
  ev3_motor_stop(right_motor, false);
}

void tracer_run(void) {
  if (linemon_is_online()) {
    driver_turn_left();
  } else {
    driver_turn_right();
  }
}

void tracer_stop(void) {
  driver_stop();
}

typedef enum {
  P_WAIT_FOR_LOADING, P_TRANSPORTING,
  P_WAIT_FOR_UNLOADING, P_RETURNING, P_ARRIVED
} porter_state;

porter_state p_state = P_WAIT_FOR_LOADING;
int p_entry = true;

void porter_transport(void) {
  num_f(p_state, 2);
  switch (p_state) {
  case P_WAIT_FOR_LOADING:
    if (p_entry) {
      p_entry = false;
    }
    if (carrier_cargo_is_loaded()) {
      p_state = P_TRANSPORTING;
      p_entry = true;
    }
    break;

  case P_TRANSPORTING:
    if (p_entry) {
      p_entry = false;
    }
    tracer_run();
    if (wall_detector_is_detected()) {
      tracer_stop();
      horn_play_arrival(); // 到着音を鳴らす
      p_state = P_WAIT_FOR_UNLOADING;
      p_entry = true;
      last_confirm_time = clock(); // 確認音タイマー初期化
    }
    break;

  case P_WAIT_FOR_UNLOADING:
    if (p_entry) {
      p_entry = false;
    }
    if (!carrier_cargo_is_loaded()) {
      p_state = P_RETURNING;
      p_entry = true;
    } else {
      // 乗客が乗っているのにバンパーが押されていない → 確認音を5秒おきに鳴らす
      if (!bumper_is_pushed()) {
        unsigned long now = clock();
        if ((now - last_confirm_time) > 5000) {
          horn_play_confirm(); // 確認音
          last_confirm_time = now;
        }
      }
    }
    break;

  case P_RETURNING:
    if (p_entry) {
      p_entry = false;
    }
    tracer_run();
    if (bumper_is_pushed()) {
      tracer_stop();
      p_state = P_ARRIVED;
      p_entry = true;
    }
    break;

  case P_ARRIVED:
    break;

  default:
    break;
  }
}

void main_task(intptr_t unused) {
  static int is_initialized = false;
  if (!is_initialized) {
    is_initialized = true;
    init_f("sample04");
    ev3_motor_config(left_motor, LARGE_MOTOR);
    ev3_motor_config(right_motor, LARGE_MOTOR);
    ev3_sensor_config(walldetector_sensor, ULTRASONIC_SENSOR);
    ev3_sensor_config(linemon_sensor, COLOR_SENSOR);
    ev3_sensor_config(bumper_sensor, TOUCH_SENSOR);
    ev3_sensor_config(carrier_sensor, TOUCH_SENSOR);
  }

  porter_transport();
  ext_tsk();
}