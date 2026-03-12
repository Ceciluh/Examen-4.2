# Examen 4.2

## Hardware necesario
- ESP32
- 2x L298N motor driver
- 2x motores DC
- Baterías 12V

## Cómo correrlo

### 1. Flashear el ESP32
Abre `micro_ros_publisher.ino` en Arduino IDE y flashealo al ESP32.

### 2. Clonar y buildear
```bash
mkdir -p ~/pb_ws/src
cd ~/pb_ws/src
git clone https://github.com/nezih-niegu/pb-j_control.git
cp -r pb-j_control/puzzlebot_control ~/pb_ws/src/
cd ~/pb_ws
colcon build --packages-select puzzlebot_control
source install/setup.bash
```

### 3. Lanzar Gazebo
```bash
ros2 launch puzzlebot_control gazebo.launch.py
```

### 4. Correr el agente de micro-ROS
```bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyUSB0 -b 115200
```

### 5. Mover el robot
```bash
ros2 run puzzlebot_control teleop_keyboard
```

Mueve el PuzzleBot con las flechas y los motores físicos se mueven igual.
