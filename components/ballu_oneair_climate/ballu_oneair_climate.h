#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ballu_oneair_climate {

class BalluOneAirClimate : public climate::Climate, public Component {
 public:
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

  void setup() override;
  void dump_config() override;
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  // Публичные методы для синхронизации физических действий с состоянием climate из YAML-lambda.
  void publish_off_state();
  void publish_fan_only_state(const char *fan_mode = nullptr);
  void publish_heat_state(const char *fan_mode = nullptr);
  void publish_fan_only_state_standard(climate::ClimateFanMode fan_mode);
  void publish_heat_state_standard(climate::ClimateFanMode fan_mode);
  void publish_eco_state();
  void publish_swing_segment(int segment);

 protected:
  sensor::Sensor *sensor_{nullptr};
  bool is_eco_active_() const;
  void publish_action_for_mode_();
};

}  // namespace ballu_oneair_climate
}  // namespace esphome
