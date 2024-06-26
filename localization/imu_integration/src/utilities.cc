/* Copyright (c) 2017, United States Government, as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 *
 * All rights reserved.
 *
 * The Astrobee platform is licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <imu_integration/utilities.h>
#include <localization_common/logger.h>
#include <localization_common/utilities.h>

namespace imu_integration {
namespace lc = localization_common;
namespace lm = localization_measurements;

boost::optional<lm::ImuMeasurement> Interpolate(const lm::ImuMeasurement& imu_measurement_a,
                                                const lm::ImuMeasurement& imu_measurement_b, const lc::Time timestamp) {
  if (timestamp < imu_measurement_a.timestamp || timestamp > imu_measurement_b.timestamp) {
    LogError(
      "Interpolate: Interpolation timestamp out of range of imu "
      "measurements.");
    return boost::none;
  }

  const double alpha =
    (timestamp - imu_measurement_a.timestamp) / (imu_measurement_b.timestamp - imu_measurement_a.timestamp);
  const Eigen::Vector3d interpolated_acceleration =
    (1.0 - alpha) * imu_measurement_a.acceleration + alpha * imu_measurement_b.acceleration;
  const Eigen::Vector3d interpolated_angular_velocity =
    (1.0 - alpha) * imu_measurement_a.angular_velocity + alpha * imu_measurement_b.angular_velocity;

  return lm::ImuMeasurement(interpolated_acceleration, interpolated_angular_velocity, timestamp);
}

gtsam::PreintegratedCombinedMeasurements Pim(
  const gtsam::imuBias::ConstantBias& bias,
  const boost::shared_ptr<gtsam::PreintegratedCombinedMeasurements::Params>& params) {
  gtsam::PreintegratedCombinedMeasurements pim(params);
  pim.resetIntegrationAndSetBias(bias);
  return pim;
}

void AddMeasurement(const lm::ImuMeasurement& imu_measurement, lc::Time& last_added_imu_measurement_time,
                    gtsam::PreintegratedCombinedMeasurements& pim) {
  const double dt = imu_measurement.timestamp - last_added_imu_measurement_time;
  // TODO(rsoussan): check if dt too large? Pass threshold param?
  if (dt == 0) {
    LogError("AddMeasurement: Timestamp difference 0, failed to add measurement.");
    return;
  }
  pim.integrateMeasurement(imu_measurement.acceleration, imu_measurement.angular_velocity, dt);
  last_added_imu_measurement_time = imu_measurement.timestamp;
}

lc::CombinedNavState PimPredict(const lc::CombinedNavState& combined_nav_state,
                                const gtsam::PreintegratedCombinedMeasurements& pim) {
  const gtsam::NavState predicted_nav_state = pim.predict(combined_nav_state.nav_state(), pim.biasHat());
  const lc::Time timestamp = combined_nav_state.timestamp() + pim.deltaTij();
  return lc::CombinedNavState(predicted_nav_state, pim.biasHat(), timestamp);
}
}  // namespace imu_integration
