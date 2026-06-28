#include "ballu_oneair_climate.h"

#include <cmath>
#include <cstring>
#include <strings.h>

namespace esphome {
namespace ballu_oneair_climate {

static const char *const TAG = "ballu_oneair_climate";

void BalluOneAirClimate::setup() {
  // Публикуются только требуемые пользовательские режимы вентилятора.
  // Low/Medium/High являются стандартными fan mode, Eco является стандартным preset.
  this->set_supported_custom_fan_modes({"Quiet", "Turbo"});

  this->mode = climate::CLIMATE_MODE_OFF;
  this->action = climate::CLIMATE_ACTION_OFF;
  this->swing_mode = climate::CLIMATE_SWING_OFF;
  this->target_temperature = 20.0f;
  this->set_fan_mode_(climate::CLIMATE_FAN_LOW);
  this->set_preset_(climate::CLIMATE_PRESET_NONE);

  if (this->sensor_ != nullptr) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      this->publish_state();
    });
    if (this->sensor_->has_state()) {
      this->current_temperature = this->sensor_->state;
    }
  }
  this->publish_state();
}

void BalluOneAirClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Ballu ONEAIR climate proxy:");
  LOG_SENSOR("  ", "Temperature sensor", this->sensor_);
}

climate::ClimateTraits BalluOneAirClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_supported_mode(climate::CLIMATE_MODE_OFF);
  traits.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);
  traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);

  // Стандартные режимы вентилятора для Home Assistant. Quiet и Turbo являются пользовательскими режимами.
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_LOW);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_MEDIUM);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_HIGH);

  // Eco используется как стандартный preset, не как пользовательский preset.
  traits.add_supported_preset(climate::CLIMATE_PRESET_ECO);

  // ESPHome/Home Assistant используют стандартные имена swing. Соответствие задано в YAML:
  // OFF=закрыто, HORIZONTAL=25%, VERTICAL=75%, BOTH=открыто.
  traits.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
  traits.add_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
  traits.add_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);
  traits.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);

  traits.set_visual_min_temperature(5.0f);
  traits.set_visual_max_temperature(25.0f);
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_target_temperature_step(1.0f);
  traits.set_visual_current_temperature_step(0.1f);
  return traits;
}

bool BalluOneAirClimate::is_eco_active_() const {
  return this->preset.has_value() && *this->preset == climate::CLIMATE_PRESET_ECO;
}

void BalluOneAirClimate::publish_action_for_mode_() {
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->action = climate::CLIMATE_ACTION_OFF;
  } else if (this->is_eco_active_()) {
    this->action = climate::CLIMATE_ACTION_IDLE;
  } else if (this->mode == climate::CLIMATE_MODE_HEAT) {
    this->action = climate::CLIMATE_ACTION_HEATING;
  } else {
    this->action = climate::CLIMATE_ACTION_FAN;
  }
}

void BalluOneAirClimate::control(const climate::ClimateCall &call) {
  const bool normal_change = call.get_mode().has_value() || call.get_fan_mode().has_value() ||
                             call.has_custom_fan_mode() || call.get_swing_mode().has_value();

  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
  }
  if (call.get_target_temperature().has_value()) {
    float t = *call.get_target_temperature();
    if (std::isnan(t)) t = 20.0f;
    if (t < 5.0f) t = 5.0f;
    if (t > 25.0f) t = 25.0f;
    this->target_temperature = t;
  }
  if (call.get_swing_mode().has_value()) {
    this->swing_mode = *call.get_swing_mode();
  }
  if (call.get_fan_mode().has_value()) {
    this->set_fan_mode_(*call.get_fan_mode());
  }
  if (call.has_custom_fan_mode()) {
    this->set_custom_fan_mode_(call.get_custom_fan_mode());
  }

  if (call.get_preset().has_value()) {
    this->set_preset_(*call.get_preset());
  }
  if (call.has_custom_preset()) {
    this->set_custom_preset_(call.get_custom_preset());
  } else if (normal_change && !call.get_preset().has_value()) {
    // Обычные команды fan/mode/swing отключают Eco. Явные команды preset сохраняют выбранный preset.
    if (this->preset.has_value() && *this->preset == climate::CLIMATE_PRESET_ECO) {
      this->set_preset_(climate::CLIMATE_PRESET_NONE);
    }
    if (this->has_custom_preset()) {
      this->clear_custom_preset_();
    }
  }

  this->publish_action_for_mode_();
  this->publish_state();
}

void BalluOneAirClimate::publish_off_state() {
  this->mode = climate::CLIMATE_MODE_OFF;
  this->action = climate::CLIMATE_ACTION_OFF;
  this->swing_mode = climate::CLIMATE_SWING_OFF;
  this->set_preset_(climate::CLIMATE_PRESET_NONE);
  this->clear_custom_preset_();
  this->publish_state();
}

void BalluOneAirClimate::publish_fan_only_state(const char *fan_mode) {
  this->mode = climate::CLIMATE_MODE_FAN_ONLY;
  this->action = climate::CLIMATE_ACTION_FAN;
  this->set_preset_(climate::CLIMATE_PRESET_NONE);
  this->clear_custom_preset_();
  if (fan_mode != nullptr) this->set_custom_fan_mode_(fan_mode);
  this->publish_state();
}

void BalluOneAirClimate::publish_heat_state(const char *fan_mode) {
  if (std::isnan(this->target_temperature)) this->target_temperature = 20.0f;
  this->mode = climate::CLIMATE_MODE_HEAT;
  this->action = climate::CLIMATE_ACTION_HEATING;
  this->set_preset_(climate::CLIMATE_PRESET_NONE);
  this->clear_custom_preset_();
  if (fan_mode != nullptr) this->set_custom_fan_mode_(fan_mode);
  this->publish_state();
}

void BalluOneAirClimate::publish_fan_only_state_standard(climate::ClimateFanMode fan_mode) {
  this->mode = climate::CLIMATE_MODE_FAN_ONLY;
  this->action = climate::CLIMATE_ACTION_FAN;
  this->set_preset_(climate::CLIMATE_PRESET_NONE);
  this->clear_custom_preset_();
  this->set_fan_mode_(fan_mode);
  this->publish_state();
}

void BalluOneAirClimate::publish_heat_state_standard(climate::ClimateFanMode fan_mode) {
  if (std::isnan(this->target_temperature)) this->target_temperature = 20.0f;
  this->mode = climate::CLIMATE_MODE_HEAT;
  this->action = climate::CLIMATE_ACTION_HEATING;
  this->set_preset_(climate::CLIMATE_PRESET_NONE);
  this->clear_custom_preset_();
  this->set_fan_mode_(fan_mode);
  this->publish_state();
}

void BalluOneAirClimate::publish_eco_state() {
  this->mode = climate::CLIMATE_MODE_FAN_ONLY;
  this->action = climate::CLIMATE_ACTION_IDLE;
  this->swing_mode = climate::CLIMATE_SWING_BOTH;
  this->set_preset_(climate::CLIMATE_PRESET_ECO);
  this->publish_state();
}

void BalluOneAirClimate::publish_swing_segment(int segment) {
  if (segment <= 0) {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  } else if (segment == 1) {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else if (segment == 2) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  }
  this->publish_state();
}

}  // namespace ballu_oneair_climate
}  // namespace esphome
