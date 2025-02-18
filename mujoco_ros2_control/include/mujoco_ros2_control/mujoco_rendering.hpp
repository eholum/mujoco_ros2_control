#ifndef MUJOCO_ROS2_CONTROL__MUJOCO_RENDERING_HPP_
#define MUJOCO_ROS2_CONTROL__MUJOCO_RENDERING_HPP_

#include <string>
#include <vector>

#include "mujoco/mujoco.h"
#include "GLFW/glfw3.h"

#include "rclcpp/node.hpp"
#include "rclcpp/publisher.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/msg/image.hpp"

namespace mujoco_ros2_control
{

struct CameraData
{
  mjvCamera mjv_cam;
  mjrRect viewport;

  std::string name;
  std::string frame_name;
  uint32_t width;
  uint32_t height;

  sensor_msgs::msg::Image image;
  sensor_msgs::msg::CameraInfo camera_info;

  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub;
  rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr camera_info_pub;
};

class MujocoRendering
{
public:
  MujocoRendering(const MujocoRendering& obj) = delete;
  void operator=(const MujocoRendering &) = delete;

  static MujocoRendering* get_instance();
  void init(rclcpp::Node::SharedPtr & node, mjModel* mujoco_model, mjData* mujoco_data);
  bool is_close_flag_raised();
  void update();
  void update_cameras();
  void close();

private:

  MujocoRendering();

  static void keyboard_callback(GLFWwindow* window, int key, int scancode, int act, int mods);
  static void mouse_button_callback(GLFWwindow* window, int button, int act, int mods);
  static void mouse_move_callback(GLFWwindow* window, double xpos, double ypos);
  static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

  void keyboard_callback_impl(GLFWwindow* window, int key, int scancode, int act, int mods);
  void mouse_button_callback_impl(GLFWwindow* window, int button, int act, int mods);
  void mouse_move_callback_impl(GLFWwindow* window, double xpos, double ypos);
  void scroll_callback_impl(GLFWwindow* window, double xoffset, double yoffset);

  void register_cameras();

  static MujocoRendering* instance_;
  rclcpp::Node::SharedPtr node_;

  mjModel* mj_model_;
  mjData* mj_data_;

  // Window and primary camera for the simulation's viewer
  GLFWwindow* window_;
  mjvCamera mjv_cam_;

  // Options for the rendering context and scene, all of these are hard coded to defaults.
  mjvOption mjv_opt_;
  mjvScene mjv_scn_;
  mjrContext mjr_con_;

  // Additional user specified cameras
  std::vector<CameraData> cameras_;

  bool button_left_;
  bool button_middle_;
  bool button_right_;
  double lastx_;
  double lasty_;
};
}  // namespace mujoco_ros2_control

#endif  // MUJOCO_ROS2_CONTROL__MUJOCO_RENDERING_HPP_
