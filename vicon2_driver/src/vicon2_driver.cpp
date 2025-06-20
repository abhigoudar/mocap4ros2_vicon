// Copyright 2019 Intelligent Robotics Lab
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: David Vargas Frutos <david.vargas@urjc.es>

#include <string>
#include <vector>
#include <memory>

#include "vicon2_driver/vicon2_driver.hpp"
#include "lifecycle_msgs/msg/state.hpp"

using std::min;
using std::max;
using std::string;
using std::map;
using std::stringstream;

// Transform the Vicon SDK enumerations to strings
string Enum2String(const ViconDataStreamSDK::CPP::Direction::Enum i_Direction)
{
  switch (i_Direction) {
    case ViconDataStreamSDK::CPP::Direction::Forward:
      return "Forward";
    case ViconDataStreamSDK::CPP::Direction::Backward:
      return "Backward";
    case ViconDataStreamSDK::CPP::Direction::Left:
      return "Left";
    case ViconDataStreamSDK::CPP::Direction::Right:
      return "Right";
    case ViconDataStreamSDK::CPP::Direction::Up:
      return "Up";
    case ViconDataStreamSDK::CPP::Direction::Down:
      return "Down";
    default:
      return "Unknown";
  }
}

// Transform the Vicon SDK enumerations to strings
string Enum2String(const ViconDataStreamSDK::CPP::Result::Enum i_result)
{
  switch (i_result) {
    case ViconDataStreamSDK::CPP::Result::ClientAlreadyConnected:
      return "ClientAlreadyConnected";
    case ViconDataStreamSDK::CPP::Result::ClientConnectionFailed:
      return "";
    case ViconDataStreamSDK::CPP::Result::CoLinearAxes:
      return "CoLinearAxes";
    case ViconDataStreamSDK::CPP::Result::InvalidDeviceName:
      return "InvalidDeviceName";
    case ViconDataStreamSDK::CPP::Result::InvalidDeviceOutputName:
      return "InvalidDeviceOutputName";
    case ViconDataStreamSDK::CPP::Result::InvalidHostName:
      return "InvalidHostName";
    case ViconDataStreamSDK::CPP::Result::InvalidIndex:
      return "InvalidIndex";
    case ViconDataStreamSDK::CPP::Result::InvalidLatencySampleName:
      return "InvalidLatencySampleName";
    case ViconDataStreamSDK::CPP::Result::InvalidMarkerName:
      return "InvalidMarkerName";
    case ViconDataStreamSDK::CPP::Result::InvalidMulticastIP:
      return "InvalidMulticastIP";
    case ViconDataStreamSDK::CPP::Result::InvalidSegmentName:
      return "InvalidSegmentName";
    case ViconDataStreamSDK::CPP::Result::InvalidSubjectName:
      return "InvalidSubjectName";
    case ViconDataStreamSDK::CPP::Result::LeftHandedAxes:
      return "LeftHandedAxes";
    case ViconDataStreamSDK::CPP::Result::NoFrame:
      return "NoFrame";
    case ViconDataStreamSDK::CPP::Result::NotConnected:
      return "NotConnected";
    case ViconDataStreamSDK::CPP::Result::NotImplemented:
      return "NotImplemented";
    case ViconDataStreamSDK::CPP::Result::ServerAlreadyTransmittingMulticast:
      return "ServerAlreadyTransmittingMulticast";
    case ViconDataStreamSDK::CPP::Result::ServerNotTransmittingMulticast:
      return "ServerNotTransmittingMulticast";
    case ViconDataStreamSDK::CPP::Result::Success:
      return "Success";
    case ViconDataStreamSDK::CPP::Result::Unknown:
      return "Unknown";
    default:
      return "unknown";
  }
}

// The vicon driver node has differents parameters to initialized with the vicon2_driver_params.yaml
ViconDriverNode::ViconDriverNode(const rclcpp::NodeOptions node_options)
: device_control::ControlledLifecycleNode(static_cast<string>("vicon2_driver_node"))
{
  declare_parameter<std::string>("stream_mode", "ClientPull");
  declare_parameter<std::string>("host_name", "192.168.10.1:801");
  declare_parameter<std::string>("tf_ref_frame_id", "vicon_world");
  declare_parameter<std::string>("tracked_frame_suffix", "vicon");
  declare_parameter<bool>("publish_markers", false);
  declare_parameter<bool>("publish_subjects", false);
  declare_parameter<bool>("marker_data_enabled", false);
  declare_parameter<bool>("broadcast_tf", false);
  declare_parameter<bool>("unlabeled_marker_data_enabled", false);
  declare_parameter<int>("lastFrameNumber", 0);
  declare_parameter<int>("frameCount", 0);
  declare_parameter<int>("droppedFrameCount", 0);
  declare_parameter<int>("n_markers", 0);
  declare_parameter<int>("n_unlabeled_markers", 0);
  declare_parameter<std::string>("qos_history_policy", "keep_all");
  declare_parameter<std::string>("qos_reliability_policy", "best_effort");
  declare_parameter<int>("qos_depth", 10);
}

// In charge of choose the different driver options related and provided by the Vicon SDK
void ViconDriverNode::set_settings_vicon()
{
  ViconDataStreamSDK::CPP::Result::Enum result(ViconDataStreamSDK::CPP::Result::Unknown);
  if (stream_mode_ == "ServerPush") {
    result = client.SetStreamMode(ViconDataStreamSDK::CPP::StreamMode::ServerPush).Result;
  } else if (stream_mode_ == "ClientPull") {
    result = client.SetStreamMode(ViconDataStreamSDK::CPP::StreamMode::ClientPull).Result;
  } else {
    RCLCPP_FATAL(get_logger(), "Unknown stream mode -- options are ServerPush, ClientPull");
    rclcpp::shutdown();
  }

  RCLCPP_INFO(
    get_logger(), "Setting Stream Mode to %s : %s",
    stream_mode_.c_str(), Enum2String(result).c_str());

  client.SetAxisMapping(
    ViconDataStreamSDK::CPP::Direction::Forward,
    ViconDataStreamSDK::CPP::Direction::Left, ViconDataStreamSDK::CPP::Direction::Up);
  ViconDataStreamSDK::CPP::Output_GetAxisMapping _Output_GetAxisMapping = client.GetAxisMapping();

  RCLCPP_INFO(
    get_logger(),
    "Axis Mapping: X-%s Y-%s Z-%s",
    Enum2String(_Output_GetAxisMapping.XAxis).c_str(),
    Enum2String(_Output_GetAxisMapping.YAxis).c_str(),
    Enum2String(_Output_GetAxisMapping.ZAxis).c_str());

  client.EnableSegmentData();

  RCLCPP_INFO(
    get_logger(), "IsSegmentDataEnabled? %s",
    client.IsSegmentDataEnabled().Enabled ? "true" : "false");

  ViconDataStreamSDK::CPP::Output_GetVersion _Output_GetVersion = client.GetVersion();

  RCLCPP_INFO(
    get_logger(), "Version: %d.%d.%d",
    _Output_GetVersion.Major,
    _Output_GetVersion.Minor,
    _Output_GetVersion.Point
  );

  /*
  if (publish_markers_) {
    marker_pub_ = create_publisher<mocap4ros_msgs::msg::Markers>(
      tracked_frame_suffix_ + "/markers", 100);
    RCLCPP_WARN(get_logger(), "publish_markers_ configured!!!");
  }
  */
}

// Start the vicon_driver_node if the Vicon system is OK.
void ViconDriverNode::start_vicon()
{
  set_settings_vicon();
  // rclcpp::WallRate d(1.0 / 240.0);
  auto period = std::chrono::milliseconds(100);
  rclcpp::Rate d(period);
  while (rclcpp::ok()) {
    while (client.GetFrame().Result != ViconDataStreamSDK::CPP::Result::Success && rclcpp::ok()) {
      RCLCPP_WARN(get_logger(), "getFrame returned false");
      d.sleep();
    }
    now_time = this->now();
    process_frame();
  }
}

// Stop the vicon_driver_node if the lifecycle node state is shutdown.
bool ViconDriverNode::stop_vicon()
{
  RCLCPP_INFO(get_logger(), "Disconnecting from Vicon DataStream SDK");
  client.Disconnect();
  RCLCPP_INFO(get_logger(), "... disconnected");
  return true;
}

// In charge of the transition of the lifecycle node
void ViconDriverNode::control_start() {
  trigger_transition(rclcpp_lifecycle::Transition(lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE));
}

// In charge of the transition of the lifecycle node
void ViconDriverNode::control_stop() {
  trigger_transition(rclcpp_lifecycle::Transition(lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE));
}

// In charge of get the Vicon information and convert it to vicon_msgs
void ViconDriverNode::process_frame()
{
  static rclcpp::Time lastTime;
  ViconDataStreamSDK::CPP::Output_GetFrameNumber OutputFrameNum = client.GetFrameNumber();
  ViconDataStreamSDK::CPP::Output_GetFrameRate OutputFrameRate = client.GetFrameRate();
  // RCLCPP_WARN(get_logger(), "Frame rate: %f", OutputFrameRate.FrameRateHz);

  int frameDiff = 0;
  if (lastFrameNumber_ != 0) {
    frameDiff = OutputFrameNum.FrameNumber - lastFrameNumber_;
    frameCount_ += frameDiff;
    if ((frameDiff) > 1) {
      droppedFrameCount_ += frameDiff;
      double droppedFramePct = static_cast<double>(droppedFrameCount_ / frameCount_ * 100);

      RCLCPP_DEBUG(
        get_logger(),
        "%d more (total %d / %d, %f %%) frame(s) dropped. Consider adjusting rates",
        frameDiff, droppedFrameCount_, frameCount_, droppedFramePct);
    }
  }
  lastFrameNumber_ = OutputFrameNum.FrameNumber;

  if (frameDiff != 0) {
    rclcpp::Duration vicon_latency(std::chrono::duration<double>(client.GetLatencyTotal().Total));
    if (publish_markers_) {
      process_markers(now_time - vicon_latency, lastFrameNumber_);
    }

    if (publish_subjects_) {
      process_subjects(now_time - vicon_latency);
    }
    lastTime = now_time;
  }
}
//
void ViconDriverNode::createSegmentThread(const std::string subject_name, const std::string segment_name)
{
  RCLCPP_INFO(this->get_logger(), "creating new object %s/%s ...", subject_name.c_str(), segment_name.c_str() );
  boost::mutex::scoped_lock lock(segments_mutex_);
  SegmentPublisher & spub = segment_publishers_[subject_name + "/" + segment_name];

  // we don't need the lock anymore, since rest is protected by is_ready
  lock.unlock();
  // auto qos = rclcpp::QoS(rclcpp::KeepLast(30));
  // qos.reliable();
  // qos.transient_local();
  auto qos = rclcpp::SensorDataQoS();
  spub.pub = create_publisher<geometry_msgs::msg::TransformStamped>
    (tracked_frame_suffix_ + "/" + subject_name + "/" + segment_name, qos);
  spub.odom_pub = create_publisher<nav_msgs::msg::Odometry>
    (tracked_frame_suffix_ + "/" + subject_name + "/" + segment_name + "_odom", qos);
  // In here only when client is active
  // try to get zero pose from parameter server
  // std::string param_suffix(subject_name + "/" + segment_name + "/zero_pose/");
  // double qw, qx, qy, qz, x, y, z;
  // bool have_params = true;
  // have_params = have_params && nh_priv.getParam(param_suffix + "orientation/w", qw);
  // have_params = have_params && nh_priv.getParam(param_suffix + "orientation/x", qx);
  // have_params = have_params && nh_priv.getParam(param_suffix + "orientation/y", qy);
  // have_params = have_params && nh_priv.getParam(param_suffix + "orientation/z", qz);
  // have_params = have_params && nh_priv.getParam(param_suffix + "position/x", x);
  // have_params = have_params && nh_priv.getParam(param_suffix + "position/y", y);
  // have_params = have_params && nh_priv.getParam(param_suffix + "position/z", z);

  // if (have_params)
  // {
  //   ROS_INFO("loaded zero pose for %s/%s", subject_name.c_str(), segment_name.c_str());
  //   spub.calibration_pose.setRotation(tf::Quaternion(qx, qy, qz, qw));
  //   spub.calibration_pose.setOrigin(tf::Vector3(x, y, z));
  //   spub.calibration_pose = spub.calibration_pose.inverse();
  // }
  // else
  // {
  //   ROS_WARN("unable to load zero pose for %s/%s", subject_name.c_str(), segment_name.c_str());
  spub.calibration_pose.setIdentity();
  // }
  spub.pub->on_activate();
  spub.odom_pub->on_activate();
  spub.is_ready = true;
  RCLCPP_INFO(this->get_logger(), "... done, advertised as \" %s/%s/%s\" ", 
    tracked_frame_suffix_.c_str(), subject_name.c_str(), segment_name.c_str());
}

void ViconDriverNode::createSegment(const std::string subject_name, const std::string segment_name)
{
  boost::thread(&ViconDriverNode::createSegmentThread, this, subject_name, segment_name);
}

//
void ViconDriverNode::process_subjects(const rclcpp::Time & frame_time)
{
  std::string tracked_frame, subject_name, segment_name;
  unsigned int n_subjects = client.GetSubjectCount().SubjectCount;
  SegmentMap::iterator pub_it;
  tf2::Transform transform;
  std::vector<geometry_msgs::msg::TransformStamped, std::allocator<geometry_msgs::msg::TransformStamped> > transforms;
  static unsigned int cnt = 0;

  for (unsigned int i_subjects = 0; i_subjects < n_subjects; i_subjects++)
  {

    subject_name = client.GetSubjectName(i_subjects).SubjectName;
    unsigned int n_segments = client.GetSegmentCount(subject_name).SegmentCount;

    for (unsigned int i_segments = 0; i_segments < n_segments; i_segments++)
    {
      segment_name = client.GetSegmentName(subject_name, i_segments).SegmentName;

      ViconDataStreamSDK::CPP::Output_GetSegmentGlobalTranslation trans = 
        client.GetSegmentGlobalTranslation(subject_name, segment_name);
      ViconDataStreamSDK::CPP::Output_GetSegmentGlobalRotationQuaternion quat = 
        client.GetSegmentGlobalRotationQuaternion(subject_name, segment_name);

      if (trans.Result == ViconDataStreamSDK::CPP::Result::Success && 
          quat.Result == ViconDataStreamSDK::CPP::Result::Success)
      {
        if (!trans.Occluded && !quat.Occluded)
        {
          transform.setOrigin(tf2::Vector3(trans.Translation[0] / 1000, 
            trans.Translation[1] / 1000, trans.Translation[2] / 1000));
          transform.setRotation(tf2::Quaternion(quat.Rotation[0], 
            quat.Rotation[1], quat.Rotation[2], quat.Rotation[3]));

          tracked_frame = tracked_frame_suffix_ + "/" + subject_name + "/" + segment_name;

          boost::mutex::scoped_try_lock lock(segments_mutex_);

          if (lock.owns_lock())
          {
            pub_it = segment_publishers_.find(subject_name + "/" + segment_name);
            if (pub_it != segment_publishers_.end())
            {
              SegmentPublisher& seg = pub_it->second;

              if (seg.is_ready)
              {
                transform = transform * seg.calibration_pose;
                //
                geometry_msgs::msg::TransformStamped tf_msg;
                tf_msg.header.stamp = frame_time;
                tf_msg.header.frame_id = tf_ref_frame_id_;
                tf_msg.child_frame_id = subject_name;
                tf_msg.transform.translation.x = transform.getOrigin().x();
                tf_msg.transform.translation.y = transform.getOrigin().y();
                tf_msg.transform.translation.z = transform.getOrigin().z();
                tf_msg.transform.rotation.x = transform.getRotation().x();
                tf_msg.transform.rotation.y = transform.getRotation().y();
                tf_msg.transform.rotation.z = transform.getRotation().z();
                tf_msg.transform.rotation.w = transform.getRotation().w();
                //
                nav_msgs::msg::Odometry odom_msg;
                odom_msg.header = tf_msg.header;
                odom_msg.child_frame_id = subject_name;
                odom_msg.pose.pose.position.x = transform.getOrigin().x();
                odom_msg.pose.pose.position.y = transform.getOrigin().y();
                odom_msg.pose.pose.position.z = transform.getOrigin().z();
                odom_msg.pose.pose.orientation.x = transform.getRotation().x();
                odom_msg.pose.pose.orientation.y = transform.getRotation().y();
                odom_msg.pose.pose.orientation.z = transform.getRotation().z();
                odom_msg.pose.pose.orientation.w = transform.getRotation().w();
                // TODO: Parameterize covariance
                for(int i=0; i < 36; i++){
                  if(i%7 == 0)
                    odom_msg.pose.covariance[i] = 0.0001;
                  else
                    odom_msg.pose.covariance[i] = 0.0;
                }
                //
                transforms.push_back(tf_msg);

                seg.pub->publish(tf_msg);
                seg.odom_pub->publish(odom_msg);
              }
            }
            else
            {
              lock.unlock();
              createSegment(subject_name, segment_name);
            }
          }
        }
        else
        {
          if (cnt % 100 == 0)
            RCLCPP_WARN(this->get_logger(), "[%s] occluded, not publishing... ", subject_name.c_str());
        }
      }
      else
      {
        RCLCPP_WARN(this->get_logger(), "GetSegmentGlobalTranslation/Rotation failed (result = %s, %s), not publishing...",
            Enum2String(trans.Result).c_str(), Enum2String(quat.Result).c_str());
      }
    }
  }

  if(broadcast_tf_)
    tf_broadcaster_->sendTransform(transforms);

  cnt++;
}

// Transform the information provided by the Vicon system into vicon_msgs and publish the information
void ViconDriverNode::process_markers(const rclcpp::Time & frame_time, unsigned int vicon_frame_num)
{
  int marker_cnt = 0;
  if (!marker_data_enabled_) {
    marker_data_enabled_ = true;
    client.EnableMarkerData();

    RCLCPP_INFO(
      get_logger(), "IsMarkerDataEnabled? %s",
      client.IsMarkerDataEnabled().Enabled ? "true" : "false");
  }
  if (!unlabeled_marker_data_enabled_) {
    unlabeled_marker_data_enabled_ = true;
    client.EnableUnlabeledMarkerData();

    RCLCPP_INFO(
      get_logger(), "IsUnlabeledMarkerDataEnabled? %s",
      client.IsUnlabeledMarkerDataEnabled().Enabled ? "true" : "false");
  }
  n_markers_ = 0;
  mocap_msgs::msg::Markers markers_msg;
  markers_msg.header.stamp = frame_time;
  markers_msg.frame_number = vicon_frame_num;

  // Count the number of subjects
  unsigned int SubjectCount = client.GetSubjectCount().SubjectCount;
  // Get labeled markers
  for (unsigned int SubjectIndex = 0; SubjectIndex < SubjectCount; ++SubjectIndex)
  {
    std::string this_subject_name = client.GetSubjectName(SubjectIndex).SubjectName;
    // Count the number of markers
    unsigned int num_subject_markers = client.GetMarkerCount(this_subject_name).MarkerCount;
    n_markers_ += num_subject_markers;
    //std::cout << "    Markers (" << MarkerCount << "):" << std::endl;
    for (unsigned int MarkerIndex = 0; MarkerIndex < num_subject_markers; ++MarkerIndex)
    {
      mocap_msgs::msg::Marker this_marker;
      this_marker.marker_name = client.GetMarkerName(this_subject_name, MarkerIndex).MarkerName;
      this_marker.subject_name = this_subject_name;
      this_marker.segment_name
          = client.GetMarkerParentName(this_subject_name, this_marker.marker_name).SegmentName;

      // Get the global marker translation
      ViconDataStreamSDK::CPP::Output_GetMarkerGlobalTranslation _Output_GetMarkerGlobalTranslation =
          client.GetMarkerGlobalTranslation(this_subject_name, this_marker.marker_name);

      this_marker.translation.x = _Output_GetMarkerGlobalTranslation.Translation[0];
      this_marker.translation.y = _Output_GetMarkerGlobalTranslation.Translation[1];
      this_marker.translation.z = _Output_GetMarkerGlobalTranslation.Translation[2];
      this_marker.occluded = _Output_GetMarkerGlobalTranslation.Occluded;

      markers_msg.markers.push_back(this_marker);
    }
  }

  unsigned int UnlabeledMarkerCount = client.GetUnlabeledMarkerCount().MarkerCount;

  // RCLCPP_INFO(
    // get_logger(),
    // "# unlabeled markers: %d", UnlabeledMarkerCount);
  n_markers_ += UnlabeledMarkerCount;
  n_unlabeled_markers_ = UnlabeledMarkerCount;
  for (unsigned int UnlabeledMarkerIndex = 0; UnlabeledMarkerIndex < UnlabeledMarkerCount; ++UnlabeledMarkerIndex)
  {
    // Get the global marker translationSegmentPublisher
    ViconDataStreamSDK::CPP::Output_GetUnlabeledMarkerGlobalTranslation
      _Output_GetUnlabeledMarkerGlobalTranslation =
      client.GetUnlabeledMarkerGlobalTranslation(UnlabeledMarkerIndex);

    if (_Output_GetUnlabeledMarkerGlobalTranslation.Result ==
      ViconDataStreamSDK::CPP::Result::Success)
    {
      mocap_msgs::msg::Marker this_marker;
      this_marker.translation.x = _Output_GetUnlabeledMarkerGlobalTranslation.Translation[0];
      this_marker.translation.y = _Output_GetUnlabeledMarkerGlobalTranslation.Translation[1];
      this_marker.translation.z = _Output_GetUnlabeledMarkerGlobalTranslation.Translation[2];
      // this_marker.occluded = false;
      markers_msg.markers.push_back(this_marker);

      if(broadcast_tf_)
        marker_to_tf(this_marker, marker_cnt, frame_time);
      marker_cnt++;
    } else {
      RCLCPP_WARN(
        get_logger(),
        "GetUnlabeledMarkerGlobalTranslation failed (result = %s)",
        Enum2String(_Output_GetUnlabeledMarkerGlobalTranslation.Result).c_str());
    }
  }
  if (!marker_pub_->is_activated()) {
    RCLCPP_WARN(
      get_logger(),
      "Lifecycle publisher is currently inactive. Messages are not published.");
  }
  marker_pub_->publish(markers_msg);
}

// Transform and publish the information previously procesed by the process_markers and converted in ROS-TFs.
void ViconDriverNode::marker_to_tf(
  mocap_msgs::msg::Marker marker,
  int marker_num, const rclcpp::Time & frame_time)
{
  tf2::Transform transform;
  std::vector<geometry_msgs::msg::TransformStamped> transforms;
  string tracked_frame;
  geometry_msgs::msg::TransformStamped tf_msg;

  transform.setOrigin(
    tf2::Vector3(
      marker.translation.x / 1000,
      marker.translation.y / 1000,
      marker.translation.z / 1000));

  transform.setRotation(tf2::Quaternion(0, 0, 0, 1));
  stringstream marker_num_str;
  marker_num_str << marker_num;
  tracked_frame = tracked_frame_suffix_ + "/marker_tf_" + marker_num_str.str();

  tf_msg.header.stamp = frame_time;
  tf_msg.header.frame_id = tf_ref_frame_id_;
  tf_msg.child_frame_id = tracked_frame;
  tf_msg.transform.translation.x = transform.getOrigin().x();
  tf_msg.transform.translation.y = transform.getOrigin().y();
  tf_msg.transform.translation.z = transform.getOrigin().z();
  tf_msg.transform.rotation.x = transform.getRotation().x();
  tf_msg.transform.rotation.y = transform.getRotation().y();
  tf_msg.transform.rotation.z = transform.getRotation().z();
  tf_msg.transform.rotation.w = transform.getRotation().w();

  transforms.push_back(tf_msg);
  tf_broadcaster_->sendTransform(transforms);
}

using CallbackReturnT =
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;


// The next Callbacks are used to manage behavior in the different states of the lifecycle node.
CallbackReturnT
ViconDriverNode::on_configure(const rclcpp_lifecycle::State &)
{
  initParameters();

  RCLCPP_INFO(get_logger(), "State id [%d]", get_current_state().id());
  RCLCPP_INFO(get_logger(), "State label [%s]", get_current_state().label().c_str());

  auto rmw_qos_history_policy = name_to_history_policy_map.find(qos_history_policy_);
  auto rmw_qos_reliability_policy = name_to_reliability_policy_map.find(qos_reliability_policy_);
  auto qos = rclcpp::QoS(
    rclcpp::QoSInitialization(
      // The history policy determines how messages are saved until taken by
      // the reader.
      // KEEP_ALL saves all messages until they are taken.
      // KEEP_LAST enforces a limit on the number of messages that are saved,
      // specified by the "depth" parameter.
      rmw_qos_history_policy->second,
      // Depth represents how many messages to store in history when the
      // history policy is KEEP_LAST.
      qos_depth_
  ));
  // The reliability policy can be reliable, meaning that the underlying transport layer will try
  // ensure that every message gets received in order, or best effort, meaning that the transport
  // makes no guarantees about the order or reliability of delivery.
  qos.reliability(rmw_qos_reliability_policy->second);

  tf_broadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);

  client_change_state_ = this->create_client<lifecycle_msgs::srv::ChangeState>(
    "/vicon2_driver/change_state");

  marker_pub_ = create_publisher<mocap_msgs::msg::Markers>(
    tracked_frame_suffix_ + "/markers", 100);

  update_pub_ = create_publisher<std_msgs::msg::Empty>(
    "/vicon2_driver/update_notify", qos);

  RCLCPP_INFO(get_logger(), "Configured!\n");

  return CallbackReturnT::SUCCESS;
}

CallbackReturnT
ViconDriverNode::on_activate(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "State id [%d]", get_current_state().id());
  RCLCPP_INFO(get_logger(), "State label [%s]", get_current_state().label().c_str());
  update_pub_->on_activate();
  marker_pub_->on_activate();
  for(auto& subject_pub : segment_publishers_)
  {
    subject_pub.second.pub->on_activate();
    subject_pub.second.odom_pub->on_activate();
    subject_pub.second.is_ready = true;
  }
  connect_vicon();
  RCLCPP_INFO(get_logger(), "Activated!\n");

  return CallbackReturnT::SUCCESS;
}

CallbackReturnT
ViconDriverNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "State id [%d]", get_current_state().id());
  RCLCPP_INFO(get_logger(), "State label [%s]", get_current_state().label().c_str());
  update_pub_->on_deactivate();
  marker_pub_->on_deactivate();
  for(auto& subject_pub : segment_publishers_)
  {
    subject_pub.second.pub->on_deactivate();
    subject_pub.second.odom_pub->on_deactivate();
    subject_pub.second.is_ready = false;
  }
  RCLCPP_INFO(get_logger(), "Deactivated!\n");

  return CallbackReturnT::SUCCESS;
}

CallbackReturnT
ViconDriverNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "State id [%d]", get_current_state().id());
  RCLCPP_INFO(get_logger(), "State label [%s]", get_current_state().label().c_str());
  /* Clean up stuff */
  RCLCPP_INFO(get_logger(), "Cleaned up!\n");

  return CallbackReturnT::SUCCESS;
}

CallbackReturnT
ViconDriverNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "State id [%d]", get_current_state().id());
  RCLCPP_INFO(get_logger(), "State label [%s]", get_current_state().label().c_str());
  /* Shut down stuff */
  RCLCPP_INFO(get_logger(), "Shutted down!\n");

  return CallbackReturnT::SUCCESS;
}

CallbackReturnT
ViconDriverNode::on_error(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(get_logger(), "State id [%d]", get_current_state().id());
  RCLCPP_INFO(get_logger(), "State label [%s]", get_current_state().label().c_str());

  return CallbackReturnT::SUCCESS;
}

// In charge of find and connect the driver with the Vicon SDK.
bool ViconDriverNode::connect_vicon()
{
  RCLCPP_WARN(
    get_logger(),
    "Trying to connect to Vicon DataStream SDK at %s ...", host_name_.c_str());

  if (client.Connect(host_name_).Result == ViconDataStreamSDK::CPP::Result::Success) {
    RCLCPP_INFO(get_logger(), "... connected!");
    start_vicon();
  } else {
    RCLCPP_INFO(get_logger(), "... not connected :( ");
  }

  return client.IsConnected().Connected;
}

// Init the necessary parameters to use the Vicon SDK.
void ViconDriverNode::initParameters()
{
  get_parameter<std::string>("stream_mode", stream_mode_);
  get_parameter<std::string>("host_name", host_name_);
  get_parameter<std::string>("tf_ref_frame_id", tf_ref_frame_id_);
  get_parameter<std::string>("tracked_frame_suffix", tracked_frame_suffix_);
  get_parameter<bool>("publish_markers", publish_markers_);
  get_parameter<bool>("publish_subjects", publish_subjects_);
  get_parameter<bool>("marker_data_enabled", marker_data_enabled_);
  get_parameter<bool>("unlabeled_marker_data_enabled", unlabeled_marker_data_enabled_);
  get_parameter<int>("lastFrameNumber", lastFrameNumber_);
  get_parameter<int>("frameCount", frameCount_);
  get_parameter<int>("droppedFrameCount", droppedFrameCount_);
  get_parameter<int>("n_markers", n_markers_);
  get_parameter<int>("n_unlabeled_markers", n_unlabeled_markers_);
  get_parameter<std::string>("qos_history_policy", qos_history_policy_);
  get_parameter<std::string>("qos_reliability_policy", qos_reliability_policy_);
  get_parameter<int>("qos_depth", qos_depth_);


  RCLCPP_INFO(
    get_logger(),
    "Param stream_mode: %s", stream_mode_.c_str());
  RCLCPP_INFO(
    get_logger(),
    "Param host_name: %s", host_name_.c_str());
  RCLCPP_INFO(
    get_logger(),
    "Param tf_ref_frame_id: %s", tf_ref_frame_id_.c_str());
  RCLCPP_INFO(
    get_logger(),
    "Param tracked_frame_suffix: %s", tracked_frame_suffix_.c_str());
  RCLCPP_INFO(
    get_logger(),
    "Param publish_markers: %s", publish_markers_ ? "true" : "false");
  RCLCPP_INFO(
    get_logger(),
    "Param publish_subjects: %s", publish_subjects_ ? "true" : "false");
  RCLCPP_INFO(
    get_logger(),
    "Param marker_data_enabled: %s", marker_data_enabled_ ? "true" : "false");
  RCLCPP_INFO(
    get_logger(),
    "Param unlabeled_marker_data_enabled: %s", unlabeled_marker_data_enabled_ ? "true" : "false");
  RCLCPP_INFO(
    get_logger(),
    "Param lastFrameNumber: %d", lastFrameNumber_);
  RCLCPP_INFO(
    get_logger(),
    "Param frameCount: %d", frameCount_);
  RCLCPP_INFO(
    get_logger(),
    "Param droppedFrameCount: %d", droppedFrameCount_);
  RCLCPP_INFO(
    get_logger(),
    "Param n_markers: %d", n_markers_);
  RCLCPP_INFO(
    get_logger(),
    "Param n_unlabeled_markers: %d", n_unlabeled_markers_);
  RCLCPP_INFO(
    get_logger(),
    "Param qos_history_policy: %s", qos_history_policy_.c_str());
  RCLCPP_INFO(
    get_logger(),
    "Param qos_reliability_policy: %s", qos_reliability_policy_.c_str());
  RCLCPP_INFO(
    get_logger(),
    "Param qos_depth: %d", qos_depth_);
}
