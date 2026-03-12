#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <geometry_msgs/msg/twist.h>
#include <stdio.h>

#define PIN_IN1 18
#define PIN_IN2 19
#define PIN_PWM 5
#define PIN_IN3 26
#define PIN_IN4 27
#define PIN_PWM2 25

#define PWM_CHANNEL  0
#define PWM_CHANNEL2 1
#define PWM_FREQ     980
#define PWM_RES      8
#define MAX_VEL      2.0f

rcl_node_t node;
rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_subscription_t subscriber;
geometry_msgs__msg__Twist msg;

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){return false;}}
#define EXECUTE_EVERY_N_MS(MS, X) do { \
  static volatile int64_t init = -1; \
  if (init == -1) { init = uxr_millis();} \
  if (uxr_millis() - init > MS) { X; init = uxr_millis();} \
} while (0)

enum states { WAITING_AGENT, AGENT_AVAILABLE, AGENT_CONNECTED, AGENT_DISCONNECTED } state;

void set_motor(int in1, int in2, int ch, float vel) {
  float cmd = vel / MAX_VEL;
  if (cmd >  1.0f) cmd =  1.0f;
  if (cmd < -1.0f) cmd = -1.0f;
  
  int duty = (int)(fabs(cmd) * 255.0f);
  
  if (duty > 0 && duty < 45) duty = 45;

  if (cmd > 0.01f)       { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); }
  else if (cmd < -0.01f) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); }
  else                   { digitalWrite(in1, LOW);  digitalWrite(in2, LOW); duty = 0; }
  
  ledcWrite(ch, duty);
}

void cmd_vel_callback(const void * msgin) {
  const geometry_msgs__msg__Twist * m = (const geometry_msgs__msg__Twist *)msgin;
  
  float linear  = m->linear.x;
  float angular = m->angular.z;

  float left_speed  = linear - angular;
  float right_speed = linear + angular;

  set_motor(PIN_IN1, PIN_IN2, PWM_CHANNEL,  left_speed);
  set_motor(PIN_IN3, PIN_IN4, PWM_CHANNEL2, right_speed);
}

bool create_entities() {
  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "motor_controller", "", &support));
  RCCHECK(rclc_subscription_init_default(&subscriber, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist), "/cmd_vel"));

  executor = rclc_executor_get_zero_initialized_executor();
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &msg, &cmd_vel_callback, ON_NEW_DATA));
  return true;
}

void destroy_entities() {
  rmw_context_t * rmw_context = rcl_context_get_rmw_context(&support.context);
  (void) rmw_uros_set_context_entity_destroy_session_timeout(rmw_context, 0);
  rcl_subscription_fini(&subscriber, &node);
  rclc_executor_fini(&executor);
  rcl_node_fini(&node);
  rclc_support_fini(&support);
}

void setup() {
  set_microros_transports();
  pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT); pinMode(PIN_IN4, OUTPUT);
  
  ledcSetup(PWM_CHANNEL,  PWM_FREQ, PWM_RES); ledcAttachPin(PIN_PWM,  PWM_CHANNEL);
  ledcSetup(PWM_CHANNEL2, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_PWM2, PWM_CHANNEL2);
  
  state = WAITING_AGENT;
}

void loop() {
  switch (state) {
    case WAITING_AGENT:
      EXECUTE_EVERY_N_MS(500, state = (RMW_RET_OK == rmw_uros_ping_agent(100, 1)) ? AGENT_AVAILABLE : WAITING_AGENT;);
      break;
    case AGENT_AVAILABLE:
      state = (true == create_entities()) ? AGENT_CONNECTED : WAITING_AGENT;
      if (state == WAITING_AGENT) destroy_entities();
      break;
    case AGENT_CONNECTED:
      EXECUTE_EVERY_N_MS(200, state = (RMW_RET_OK == rmw_uros_ping_agent(100, 1)) ? AGENT_CONNECTED : AGENT_DISCONNECTED;);
      if (state == AGENT_CONNECTED) rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
      break;
    case AGENT_DISCONNECTED:
      destroy_entities();
      state = WAITING_AGENT;
      break;
    default: break;
  }
}