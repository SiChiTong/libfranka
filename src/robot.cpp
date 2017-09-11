#include <franka/robot.h>

#include <utility>

#include "control_loop.h"
#include "motion_generator_loop.h"
#include "network.h"
#include "robot_impl.h"

namespace franka {

Robot::Robot(const std::string& franka_address, RealtimeConfig realtime_config)
    : impl_{new Robot::Impl(
          std::make_unique<Network>(franka_address, research_interface::robot::kCommandPort),
          realtime_config)} {}

// Has to be declared here, as the Impl type is incomplete in the header
Robot::~Robot() noexcept = default;

Robot::Robot(Robot&& other) noexcept {
  std::lock_guard<std::mutex> _(other.control_mutex_);
  impl_ = std::move(other.impl_);
}

Robot& Robot::operator=(Robot&& other) noexcept {
  if (&other != this) {
    std::unique_lock<std::mutex> this_lock(control_mutex_, std::defer_lock);
    std::unique_lock<std::mutex> other_lock(other.control_mutex_, std::defer_lock);
    std::lock(this_lock, other_lock);
    impl_ = std::move(other.impl_);
  }
  return *this;
}

Robot::ServerVersion Robot::serverVersion() const noexcept {
  return impl_->serverVersion();
}

void Robot::control(ControllerMode controller_mode,
                    std::function<bool(const RobotState&)> read_callback) {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  research_interface::robot::SetControllerMode::ControllerMode mode;
  research_interface::robot::ControllerMode state_controller_mode;
  switch (controller_mode) {
    case ControllerMode::kJointImpedance:
      mode = decltype(mode)::kJointImpedance;
      state_controller_mode = decltype(state_controller_mode)::kJointImpedance;
      break;
    case ControllerMode::kCartesianImpedance:
      mode = decltype(mode)::kCartesianImpedance;
      state_controller_mode = decltype(state_controller_mode)::kCartesianImpedance;
      break;
    default:
      throw std::invalid_argument("Invalid controller mode given.");
  }
  impl_->executeCommand<research_interface::robot::SetControllerMode>(mode);

  while (true) {
    research_interface::robot::RobotState robot_state = impl_->updateWithoutConversion();
    if (robot_state.controller_mode != state_controller_mode) {
      throw ControlException("Controller mode changed.");
    }
    if (!read_callback(convertRobotState(robot_state))) {
      break;
    }
  }
}

void Robot::control(std::function<Torques(const RobotState&, franka::Duration)> control_callback) {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  ControlLoop loop(*impl_, std::move(control_callback));
  loop();
}

void Robot::control(
    std::function<Torques(const RobotState&, franka::Duration)> control_callback,
    std::function<JointPositions(const RobotState&, franka::Duration)> motion_generator_callback) {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  MotionGeneratorLoop<JointPositions> loop(*impl_, std::move(control_callback),
                                           std::move(motion_generator_callback));
  loop();
}

void Robot::control(
    std::function<Torques(const RobotState&, franka::Duration)> control_callback,
    std::function<JointVelocities(const RobotState&, franka::Duration)> motion_generator_callback) {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  MotionGeneratorLoop<JointVelocities> loop(*impl_, std::move(control_callback),
                                            std::move(motion_generator_callback));
  loop();
}

void Robot::control(
    std::function<Torques(const RobotState&, franka::Duration)> control_callback,
    std::function<CartesianPose(const RobotState&, franka::Duration)> motion_generator_callback) {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  MotionGeneratorLoop<CartesianPose> loop(*impl_, std::move(control_callback),
                                          std::move(motion_generator_callback));
  loop();
}

void Robot::control(std::function<Torques(const RobotState&, franka::Duration)> control_callback,
                    std::function<CartesianVelocities(const RobotState&, franka::Duration)>
                        motion_generator_callback) {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  MotionGeneratorLoop<CartesianVelocities> loop(*impl_, std::move(control_callback),
                                                std::move(motion_generator_callback));
  loop();
}

void Robot::control(
    std::function<JointPositions(const RobotState&, franka::Duration)> motion_generator_callback,
    ControllerMode controller_mode) {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  MotionGeneratorLoop<JointPositions> loop(*impl_, controller_mode,
                                           std::move(motion_generator_callback));
  loop();
}

void Robot::control(
    std::function<JointVelocities(const RobotState&, franka::Duration)> motion_generator_callback,
    ControllerMode controller_mode) {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  MotionGeneratorLoop<JointVelocities> loop(*impl_, controller_mode,
                                            std::move(motion_generator_callback));
  loop();
}

void Robot::control(
    std::function<CartesianPose(const RobotState&, franka::Duration)> motion_generator_callback,
    ControllerMode controller_mode) {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  MotionGeneratorLoop<CartesianPose> loop(*impl_, controller_mode,
                                          std::move(motion_generator_callback));
  loop();
}

void Robot::control(std::function<CartesianVelocities(const RobotState&, franka::Duration)>
                     motion_generator_callback,
                 ControllerMode controller_mode) {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  MotionGeneratorLoop<CartesianVelocities> loop(*impl_, controller_mode,
                                                std::move(motion_generator_callback));
  loop();
}

void Robot::read(std::function<bool(const RobotState&)> read_callback) {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  while (true) {
    RobotState robot_state = impl_->update();
    if (!read_callback(robot_state)) {
      break;
    }
  }
}

RobotState Robot::readOnce() {
  std::unique_lock<std::mutex> l(control_mutex_, std::try_to_lock);
  if (!l.owns_lock()) {
    throw InvalidOperationException(
        "libfranka robot: Cannot perform this operation while another control or read operation "
        "is running.");
  }

  return impl_->readOnce();
}

VirtualWallCuboid Robot::getVirtualWall(int32_t id) {
  VirtualWallCuboid virtual_wall;
  impl_->executeCommand<research_interface::robot::GetCartesianLimit>(id, &virtual_wall);
  return virtual_wall;
}

void Robot::setCollisionBehavior(const std::array<double, 7>& lower_torque_thresholds_acceleration,
                                 const std::array<double, 7>& upper_torque_thresholds_acceleration,
                                 const std::array<double, 7>& lower_torque_thresholds_nominal,
                                 const std::array<double, 7>& upper_torque_thresholds_nominal,
                                 const std::array<double, 6>& lower_force_thresholds_acceleration,
                                 const std::array<double, 6>& upper_force_thresholds_acceleration,
                                 const std::array<double, 6>& lower_force_thresholds_nominal,
                                 const std::array<double, 6>& upper_force_thresholds_nominal) {
  impl_->executeCommand<research_interface::robot::SetCollisionBehavior>(
      lower_torque_thresholds_acceleration, upper_torque_thresholds_acceleration,
      lower_torque_thresholds_nominal, upper_torque_thresholds_nominal,
      lower_force_thresholds_acceleration, upper_force_thresholds_acceleration,
      lower_force_thresholds_nominal, upper_force_thresholds_nominal);
}

void Robot::setCollisionBehavior(const std::array<double, 7>& lower_torque_thresholds,
                                 const std::array<double, 7>& upper_torque_thresholds,
                                 const std::array<double, 6>& lower_force_thresholds,
                                 const std::array<double, 6>& upper_force_thresholds) {
  impl_->executeCommand<research_interface::robot::SetCollisionBehavior>(
      lower_torque_thresholds, upper_torque_thresholds, lower_torque_thresholds,
      upper_torque_thresholds, lower_force_thresholds, upper_force_thresholds,
      lower_force_thresholds, upper_force_thresholds);
}

void Robot::setJointImpedance(
    const std::array<double, 7>& K_theta) {  // NOLINT (readability-named-parameter)
  impl_->executeCommand<research_interface::robot::SetJointImpedance>(K_theta);
}

void Robot::setCartesianImpedance(
    const std::array<double, 6>& K_x) {  // NOLINT (readability-named-parameter)
  impl_->executeCommand<research_interface::robot::SetCartesianImpedance>(K_x);
}

void Robot::setGuidingMode(const std::array<bool, 6>& guiding_mode, bool elbow) {
  impl_->executeCommand<research_interface::robot::SetGuidingMode>(guiding_mode, elbow);
}

void Robot::setK(const std::array<double, 16>& EE_T_K) {  // NOLINT (readability-named-parameter)
  impl_->executeCommand<research_interface::robot::SetEEToK>(EE_T_K);
}

void Robot::setEE(const std::array<double, 16>& F_T_EE) {  // NOLINT (readability-named-parameter)
  impl_->executeCommand<research_interface::robot::SetFToEE>(F_T_EE);
}

void Robot::setLoad(double load_mass,
                    const std::array<double, 3>& F_x_Cload,  // NOLINT (readability-named-parameter)
                    const std::array<double, 9>& load_inertia) {
  impl_->executeCommand<research_interface::robot::SetLoad>(load_mass, F_x_Cload, load_inertia);
}

void Robot::automaticErrorRecovery() {
  impl_->executeCommand<research_interface::robot::AutomaticErrorRecovery>();
}

Model Robot::loadModel() {
  return impl_->loadModel();
}

}  // namespace franka
