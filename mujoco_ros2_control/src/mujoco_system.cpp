#include "mujoco_ros2_control/mujoco_system.hpp"

namespace mujoco_ros2_control
{
MujocoSystem::MujocoSystem() : logger_(rclcpp::get_logger(""))
{
}

std::vector<hardware_interface::StateInterface> MujocoSystem::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> new_state_interfaces;

  for (auto& joint : joint_states_)
  {
    // Add state interfaces for joint hardware.
    if (auto it = joint_hw_info_.find(joint.name); it != joint_hw_info_.end())
    {
      for (const auto& state_if : it->second.state_interfaces)
      {
        if (state_if.name == hardware_interface::HW_IF_POSITION)
        {
          new_state_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_POSITION, &joint.position);
        }
        else if (state_if.name == hardware_interface::HW_IF_VELOCITY)
        {
          new_state_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_VELOCITY, &joint.velocity);
        }
        else if (state_if.name == hardware_interface::HW_IF_EFFORT)
        {
          new_state_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_EFFORT, &joint.effort);
        }
      }
    }
  }

  // Add state interfaces for sensors
  for (auto& sensor : ft_sensor_data_)
  {
    if (auto it = sensors_hw_info_.find(sensor.name); it != sensors_hw_info_.end())
    {
      for (const auto& state_if : it->second.state_interfaces)
      {
        if (state_if.name == "force.x")
        {
          new_state_interfaces.emplace_back(sensor.name, state_if.name, &sensor.force.data.x());
        }
        else if (state_if.name == "force.y")
        {
          new_state_interfaces.emplace_back(sensor.name, state_if.name, &sensor.force.data.y());
        }
        else if (state_if.name == "force.z")
        {
          new_state_interfaces.emplace_back(sensor.name, state_if.name, &sensor.force.data.z());
        }
        else if (state_if.name == "torque.x")
        {
          new_state_interfaces.emplace_back(sensor.name, state_if.name, &sensor.torque.data.x());
        }
        else if (state_if.name == "torque.y")
        {
          new_state_interfaces.emplace_back(sensor.name, state_if.name, &sensor.torque.data.y());
        }
        else if (state_if.name == "torque.z")
        {
          new_state_interfaces.emplace_back(sensor.name, state_if.name, &sensor.torque.data.z());
        }
      }
    }
  }

  return new_state_interfaces;
}

std::vector<hardware_interface::CommandInterface> MujocoSystem::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> new_command_interfaces;

  // Joint command interfaces
  for (auto& joint : joint_states_)
  {
    // Add command interfaces for joint hardware.
    if (auto it = joint_hw_info_.find(joint.name); it != joint_hw_info_.end())
    {
      for (const auto& command_if : it->second.command_interfaces)
      {
        if (command_if.name.find(hardware_interface::HW_IF_POSITION) != std::string::npos)
        {
          new_command_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_POSITION, &joint.position_command);
        }
        else if (command_if.name.find(hardware_interface::HW_IF_VELOCITY) != std::string::npos)
        {
          new_command_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_VELOCITY, &joint.velocity_command);
        }
        else if (command_if.name == hardware_interface::HW_IF_EFFORT)
        {
          new_command_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_EFFORT, &joint.effort_command);
        }
      }
    }
  }

  return new_command_interfaces;
}

hardware_interface::return_type MujocoSystem::read(const rclcpp::Time & /* time */, const rclcpp::Duration & /* period */)
{
  // Joint states
  for (auto& joint_state : joint_states_)
  {
    joint_state.position = mj_data_->qpos[joint_state.mj_pos_adr];
    joint_state.velocity = mj_data_->qvel[joint_state.mj_vel_adr];
    joint_state.effort = mj_data_->qfrc_applied[joint_state.mj_vel_adr];
  }

  // IMU Sensor data
  // TODO: For now all sensors are assumed to be FTS
  // for (auto& data : imu_sensor_data_)
  // {
  // }

  // FT Sensor data
  for (auto& data : ft_sensor_data_)
  {
    data.force.data.x() = -mj_data_->sensordata[data.force.mj_sensor_index];
    data.force.data.y() = -mj_data_->sensordata[data.force.mj_sensor_index + 1];
    data.force.data.z() = -mj_data_->sensordata[data.force.mj_sensor_index + 2];

    data.torque.data.x() = -mj_data_->sensordata[data.torque.mj_sensor_index];
    data.torque.data.y() = -mj_data_->sensordata[data.torque.mj_sensor_index + 1];
    data.torque.data.z() = -mj_data_->sensordata[data.torque.mj_sensor_index + 2];
  }

  return hardware_interface::return_type::OK;
}

hardware_interface::return_type MujocoSystem::write(const rclcpp::Time & /* time */, const rclcpp::Duration & period)
{
  // update mimic joint
  for (auto& joint_state : joint_states_)
  {
    if (joint_state.is_mimic)
    {
      joint_state.position_command = joint_state.mimic_multiplier*joint_states_.at(joint_state.mimicked_joint_index).position_command;
      joint_state.velocity_command = joint_state.mimic_multiplier*joint_states_.at(joint_state.mimicked_joint_index).velocity_command;
      joint_state.effort_command = joint_state.mimic_multiplier*joint_states_.at(joint_state.mimicked_joint_index).effort_command;
    }
  }
  // Joint states
  for (auto& joint_state : joint_states_)
  {
    if (joint_state.is_position_control_enabled)
    {
      if (joint_state.is_pid_enabled)
      {
        double error = joint_state.position_command - mj_data_->qpos[joint_state.mj_pos_adr];
        mj_data_->qfrc_applied[joint_state.mj_vel_adr] = joint_state.position_pid.computeCommand(error, period.nanoseconds());
      }
      else
      {
        mj_data_->qpos[joint_state.mj_pos_adr] = joint_state.position_command;
      }
    }

    if (joint_state.is_velocity_control_enabled)
    {
      if (joint_state.is_pid_enabled)
      {
        double error = joint_state.velocity_command - mj_data_->qvel[joint_state.mj_vel_adr];
        mj_data_->qfrc_applied[joint_state.mj_vel_adr] = joint_state.velocity_pid.computeCommand(error, period.nanoseconds());;
      }
      else
      {
        mj_data_->qvel[joint_state.mj_vel_adr] = joint_state.velocity_command;
      }
    }

    if (joint_state.is_effort_control_enabled)
    {
      double min_eff, max_eff;
      min_eff = joint_state.joint_limits.has_effort_limits ? -1*joint_state.joint_limits.max_effort : std::numeric_limits<double>::lowest();
      min_eff = std::max(min_eff, joint_state.min_effort_command);

      max_eff = joint_state.joint_limits.has_effort_limits ? joint_state.joint_limits.max_effort : std::numeric_limits<double>::max();
      max_eff = std::min(max_eff, joint_state.max_effort_command);

      mj_data_->qfrc_applied[joint_state.mj_vel_adr] = clamp(joint_state.effort_command, min_eff, max_eff);
    }
  }

  return hardware_interface::return_type::OK;
}

bool MujocoSystem::init_sim(rclcpp::Node::SharedPtr& node, mjModel* mujoco_model, mjData *mujoco_data,
  const urdf::Model& urdf_model, const hardware_interface::HardwareInfo & hardware_info)
{
  node_ = node;
  mj_model_ = mujoco_model;
  mj_data_ = mujoco_data;

  logger_ = rclcpp::get_logger(node_->get_name() + std::string("mujoco_system"));

  register_joints(urdf_model, hardware_info);
  register_sensors(urdf_model, hardware_info);

  set_initial_pose();
  return true;
}

void MujocoSystem::register_joints(const urdf::Model& urdf_model, const hardware_interface::HardwareInfo & hardware_info)
{
  joint_states_.resize(hardware_info.joints.size());

  for (size_t joint_index = 0; joint_index < hardware_info.joints.size(); joint_index++)
  {
    auto joint = hardware_info.joints.at(joint_index);
    int mujoco_joint_id = mj_name2id(mj_model_, mjtObj::mjOBJ_JOINT, joint.name.c_str());
    if (mujoco_joint_id == -1)
    {
      RCLCPP_ERROR_STREAM(logger_, "Failed to find joint in mujoco model, joint name: " << joint.name);
      continue;
    }

    // Add to the joint hw information map
    joint_hw_info_.insert(std::make_pair(joint.name, joint));

    // save information in joint_states_ variable
    JointState joint_state;
    joint_state.name = joint.name;
    joint_state.mj_joint_type = mj_model_->jnt_type[mujoco_joint_id];
    joint_state.mj_pos_adr = mj_model_->jnt_qposadr[mujoco_joint_id];
    joint_state.mj_vel_adr = mj_model_->jnt_dofadr[mujoco_joint_id];

    joint_states_.at(joint_index) = joint_state;
    JointState& last_joint_state = joint_states_.at(joint_index);

    // get joint limit from urdf
    get_joint_limits(urdf_model.getJoint(last_joint_state.name), last_joint_state.joint_limits);

    // check if mimicked
    if (joint.parameters.find("mimic") != joint.parameters.end()) {
      const auto mimicked_joint = joint.parameters.at("mimic");
      const auto mimicked_joint_it = std::find_if(
        hardware_info.joints.begin(), hardware_info.joints.end(),
        [&mimicked_joint](const hardware_interface::ComponentInfo & info) {
          return info.name == mimicked_joint;
        });
      if (mimicked_joint_it == hardware_info.joints.end()) {
        throw std::runtime_error(
                std::string("Mimicked joint '") + mimicked_joint + "' not found");
      }
      last_joint_state.is_mimic = true;
      last_joint_state.mimicked_joint_index = std::distance(
        hardware_info.joints.begin(), mimicked_joint_it);

      auto param_it = joint.parameters.find("multiplier");
      if (param_it != joint.parameters.end()) {
        last_joint_state.mimic_multiplier = std::stod(joint.parameters.at("multiplier"));
      }
      else
      {
        last_joint_state.mimic_multiplier = 1.0;
      }
    }

    auto get_initial_value = [this](const hardware_interface::InterfaceInfo & interface_info)
    {
      if (!interface_info.initial_value.empty())
      {
        double value = std::stod(interface_info.initial_value);
        return value;
      }
      else
      {
        return 0.0;
      }
    };

    // Set initial values
    for (const auto& state_if : joint.state_interfaces)
    {
      if (state_if.name == hardware_interface::HW_IF_POSITION)
      {
        last_joint_state.position = get_initial_value(state_if);
      }
      else if (state_if.name == hardware_interface::HW_IF_VELOCITY)
      {
        last_joint_state.velocity = get_initial_value(state_if);
      }
      else if (state_if.name == hardware_interface::HW_IF_EFFORT)
      {
        last_joint_state.effort = get_initial_value(state_if);
      }
    }

    auto get_min_value = [this](const hardware_interface::InterfaceInfo & interface_info)
    {
      if (!interface_info.min.empty())
      {
        double value = std::stod(interface_info.min);
        return value;
      }
      else
      {
        return -1*std::numeric_limits<double>::max();
      }
    };

    auto get_max_value = [this](const hardware_interface::InterfaceInfo & interface_info)
    {
      if (!interface_info.max.empty())
      {
        double value = std::stod(interface_info.max);
        return value;
      }
      else
      {
        return std::numeric_limits<double>::max();
      }
    };

    // command interfaces
    // overwrite joint limit with min/max value
    for (const auto& command_if : joint.command_interfaces)
    {
      if (command_if.name.find(hardware_interface::HW_IF_POSITION) != std::string::npos)
      {
        last_joint_state.is_position_control_enabled = true;
        last_joint_state.position_command = last_joint_state.position;
        // TODO: These are not used at all. Potentially can be removed.
        last_joint_state.min_position_command = get_min_value(command_if);
        last_joint_state.max_position_command = get_max_value(command_if);
      }
      else if (command_if.name.find(hardware_interface::HW_IF_VELOCITY) != std::string::npos)
      {
        last_joint_state.is_velocity_control_enabled = true;
        last_joint_state.velocity_command = last_joint_state.velocity;
        // TODO: These are not used at all. Potentially can be removed.
        last_joint_state.min_velocity_command = get_min_value(command_if);
        last_joint_state.max_velocity_command = get_max_value(command_if);
      }
      else if (command_if.name == hardware_interface::HW_IF_EFFORT)
      {
        last_joint_state.is_effort_control_enabled = true;
        last_joint_state.effort_command = last_joint_state.effort;
        last_joint_state.min_effort_command = get_min_value(command_if);
        last_joint_state.max_effort_command = get_max_value(command_if);
      }

      if (command_if.name.find("_pid") != std::string::npos)
      {
        last_joint_state.is_pid_enabled = true;
      }
    }

    // Get PID gains, if needed
    if (last_joint_state.is_pid_enabled)
    {
      last_joint_state.position_pid = get_pid_gains(joint, hardware_interface::HW_IF_POSITION);
      last_joint_state.velocity_pid = get_pid_gains(joint, hardware_interface::HW_IF_VELOCITY);
    }
  }
}

void MujocoSystem::register_sensors(const urdf::Model& /* urdf_model */, const hardware_interface::HardwareInfo & hardware_info)
{
  // TODO: for now, assuming all sensors are ft_sensor
  ft_sensor_data_.resize(hardware_info.sensors.size());

  for (size_t sensor_index = 0; sensor_index < hardware_info.sensors.size(); sensor_index++)
  {
    auto sensor = hardware_info.sensors.at(sensor_index);

    // Add to the sensor hw information map
    sensors_hw_info_.insert(std::make_pair(sensor.name, sensor));

    FTSensorData sensor_data;
    sensor_data.name = sensor.name;
    sensor_data.force.name = sensor.name + "_force";
    sensor_data.torque.name = sensor.name + "_torque";

    int force_sensor_id = mj_name2id(mj_model_, mjtObj::mjOBJ_SENSOR, sensor_data.force.name.c_str());
    int torque_sensor_id = mj_name2id(mj_model_, mjtObj::mjOBJ_SENSOR, sensor_data.torque.name.c_str());

    if (force_sensor_id == -1 || torque_sensor_id == -1)
    {
      RCLCPP_ERROR_STREAM(logger_, "Failed to find sensor in mujoco model, sensor name: " << sensor.name);
      continue;
    }

    sensor_data.force.mj_sensor_index = mj_model_->sensor_adr[force_sensor_id];
    sensor_data.torque.mj_sensor_index = mj_model_->sensor_adr[torque_sensor_id];

    ft_sensor_data_.at(sensor_index) = sensor_data;
  }
}

void MujocoSystem::set_initial_pose()
{
  for (auto& joint_state : joint_states_)
  {
    mj_data_->qpos[joint_state.mj_pos_adr] = joint_state.position;
  }
}

void MujocoSystem::get_joint_limits(urdf::JointConstSharedPtr urdf_joint, joint_limits::JointLimits& joint_limits)
{
  if (urdf_joint->limits)
  {
    joint_limits.min_position = urdf_joint->limits->lower;
    joint_limits.max_position = urdf_joint->limits->upper;
    joint_limits.max_velocity = urdf_joint->limits->velocity;
    joint_limits.max_effort = urdf_joint->limits->effort;
  }
}

control_toolbox::Pid MujocoSystem::get_pid_gains(const hardware_interface::ComponentInfo& joint_info, std::string command_interface)
{
  double kp, ki, kd, i_max, i_min;
  std::string key;
  key = command_interface + std::string(PARAM_KP);
  if (joint_info.parameters.find(key) != joint_info.parameters.end())
  {
    kp = std::stod(joint_info.parameters.at(key));
  }
  else
  {
    kp = 0.0;
  }

  key = command_interface + std::string(PARAM_KI);
  if (joint_info.parameters.find(key) != joint_info.parameters.end())
  {
    ki = std::stod(joint_info.parameters.at(key));
  }
  else
  {
    ki = 0.0;
  }

  key = command_interface + std::string(PARAM_KD);
  if (joint_info.parameters.find(key) != joint_info.parameters.end())
  {
    kd = std::stod(joint_info.parameters.at(key));
  }
  else
  {
    kd = 0.0;
  }

  bool enable_anti_windup = false;
  key = command_interface + std::string(PARAM_I_MAX);
  if (joint_info.parameters.find(key) != joint_info.parameters.end())
  {
    i_max = std::stod(joint_info.parameters.at(key));
    enable_anti_windup = true;
  }
  else
  {
    i_max = std::numeric_limits<double>::max();
  }

  key = command_interface + std::string(PARAM_I_MIN);
  if (joint_info.parameters.find(key) != joint_info.parameters.end())
  {
    i_min = std::stod(joint_info.parameters.at(key));
    enable_anti_windup = true;
  }
  else
  {
    i_min = std::numeric_limits<double>::lowest();
  }

  return control_toolbox::Pid(kp, ki, kd, i_max, i_min, enable_anti_windup);
}
} // namespace mujoco_ros2_control

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(mujoco_ros2_control::MujocoSystem, mujoco_ros2_control::MujocoSystemInterface)
