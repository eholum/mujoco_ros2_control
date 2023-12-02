#ifndef MUJOCO_ROS2_CONTROL__MUJOCO_SYSTEM_HPP_
#define MUJOCO_ROS2_CONTROL__MUJOCO_SYSTEM_HPP_

#include <Eigen/Dense>
#include "mujoco_ros2_control/mujoco_system_interface.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "joint_limits/joint_limits.hpp"

namespace mujoco_ros2_control
{
class MujocoSystem : public MujocoSystemInterface
{
public:
  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;
  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  hardware_interface::return_type read(const rclcpp::Time & time, const rclcpp::Duration & period) override;
  hardware_interface::return_type write(const rclcpp::Time & time, const rclcpp::Duration & period) override;

  bool init_sim(rclcpp::Node::SharedPtr & node, mjModel* mujoco_model, mjData *mujoco_data,
    const urdf::Model& urdf_model, const hardware_interface::HardwareInfo & hardware_info) override;

  struct JointState
  {
    std::string name;
    double position;
    double velocity;
    double effort;
    double position_command;
    double velocity_command;
    double effort_command;
    double min_position_command;
    double max_position_command;
    double min_velocity_command;
    double max_velocity_command;
    double min_effort_command;
    double max_effort_command;
    joint_limits::JointLimits joint_limits;
    bool is_position_control_enabled = false;
    bool is_velocity_control_enabled = false;
    bool is_effort_control_enabled = false;
    int mj_joint_type;
    int mj_pos_adr;
    int mj_vel_adr;
  };

  template <typename T>
  struct SensorData
  {
    std::string name;
    T data;
    int mj_sensor_index;
  };

  struct FTSensorData
  {
    std::string name;
    SensorData<Eigen::Vector3d> force;
    SensorData<Eigen::Vector3d> torque;
  };

  struct IMUSensorData
  {
    std::string name;
    SensorData<Eigen::Quaternion<double>> orientation;
    SensorData<Eigen::Vector3d> angular_velocity;
    SensorData<Eigen::Vector3d> linear_velocity;
  };

private:
  void register_joints(const urdf::Model& urdf_model, const hardware_interface::HardwareInfo & hardware_info);
  void register_sensors(const urdf::Model& urdf_model, const hardware_interface::HardwareInfo & hardware_info);
  void get_joint_limits(urdf::JointConstSharedPtr urdf_joint, joint_limits::JointLimits& joint_limits);
  double clamp(double v, double lo, double hi)
  {
    return (v < lo) ? lo : (hi < v) ? hi : v;
  }

  std::vector<hardware_interface::StateInterface> state_interfaces_;
  std::vector<hardware_interface::CommandInterface> command_interfaces_;

  std::vector<JointState> joint_states_;
  std::vector<FTSensorData> ft_sensor_data_;
  std::vector<IMUSensorData> imu_sensor_data_;

  mjModel* mj_model_;
  mjData* mj_data_;
};
}  // namespace mujoco_ros2_control

#endif  // MUJOCO_ROS2_CONTROL__MUJOCO_SYSTEM_HPP_