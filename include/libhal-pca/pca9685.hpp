// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <libhal/i2c.hpp>
#include <libhal/pwm.hpp>
#include <libhal/units.hpp>

namespace hal::pca {
/**
 * @brief pca9685 driver: 16 channel 12-bit PWM generator over I2C
 *
 * Some details about the device are:
 *
 * - 16 PWM channels
 * - Single frequency for every channel
 * - 12-bits resolution
 * - Min frequency is 24 Hz, max is 1526 Hz
 * - Supports i2c clock frequency up to 1 MHz
 *
 * USAGE:
 *
 *    static constexpr hal::byte address = 0b100'0000;
 *    auto pca9685 = HAL_CHECK(hal::pca::pca9685::create(i2c, address));
 *    auto pwm0 = pca9685.get_pwm_channel<0>();
 *    HAL_CHECK(pwm0.frequency(1_kHz));
 *    HAL_CHECK(pwm0.duty_cycle(0.25));
 *
 * After creating the `pca9685` driver, it can be configured or have its
 * frequency updated. It defaults to ~200 Hz. In order to control individual PWM
 * channels, `get_pwm_channel<N>()` must be called with the channel number in
 * the template argument. A compile-time error will be issued if a port beyond
 * the 16 channels is requested. The returned pwm object internally references
 * the pca9685 object it came from. All pwm channel objects are invalidated if
 * their parent pca9685 driver is destroyed. It is important to ensure that
 * the lifetime of the pca9685 exceeds the lifetime of the pwm channel objects,
 * otherwise, using such object will result in undefined behavior.
 */
class pca9685
{
public:
  static constexpr size_t max_channel_count = 16;
  /**
   * @brief Representation & implementation of a pca9685 pwm channel/pin
   *
   * Setting the duty cycle or frequency of a pwm_channel object will update the
   * pwm channel pin on the pca9685 chip.
   *
   * Maximum frequency is 1526 Hz
   * Minimum frequency is 24 Hz
   *
   * NOTE: that setting the frequency of one pwm channel will set the frequency
   * of all pwm channels.
   */
  class pwm_channel : public hal::pwm
  {
  private:
    pwm_channel(pca9685* p_pca9685, hal::byte p_channel);
    result<frequency_t> driver_frequency(hal::hertz p_frequency) override;
    result<duty_cycle_t> driver_duty_cycle(float p_duty_cycle) override;

    pca9685* m_pca9685;
    hal::byte m_channel;

    friend class pca9685;
  };

  /**
   * @brief Enumeration describing the pin state choices if the OE pin is active
   *
   */
  enum class disabled_pin_state : hal::byte
  {
    /// Set all pins to LOW voltage when output enable is asserted.
    set_low = 0b00,
    /// Set all the pins to HIGH voltage when output enable is asserted.
    /// When the pins are set to be open_collector, meaning the device was
    /// configured for `totem_pole_output = false`, then the output is high-Z.
    set_high = 0b10,
    /// Set all pins to HIGH-Z output
    set_high_z = 0b11,
  };

  /**
   * @brief Collection of settings that can be configured
   *
   */
  struct settings
  {
    /// Invert the voltage for all pins.
    bool invert_outputs = false;
    /// Update PWM channels on acknowledge
    bool output_changes_on_i2c_acknowledge = false;
    /// If true: output channels are configured as totem pole.
    /// If false, the pin will be open collector
    bool totem_pole_output = true;
    /// Put the device to sleep and turn off the oscillator
    /// This will disable all outputs if set to `true`.
    /// Set to false to re-enable the device.
    bool sleep = false;
    /// Control what the state of the pins is when the output enable is
    /// asserted.
    disabled_pin_state pin_disabled_state = disabled_pin_state::set_low;
  };

  /**
   * @brief Create a pca9685 driver object
   *
   * @param p_i2c - an i2c bus driver to communicate with the chip
   * @param p_address - the address of the device which can be anywhere from
   * 0b100'0000 to 0x111'1111 depending on if the address pins are pulled to GND
   * (0) or to VCC (1). The address is 0b100'0000 if all of the address pins are
   * pulled to GND.
   * @param p_settings - optional starting settings for the device. If this is
   * not entered or is set to `std::nullopt`, then it
   * @return hal::result<pca9685> - pca9685 driver object or errors from invalid
   * configuration.
   */
  static hal::result<pca9685> create(
    hal::i2c& p_i2c,
    hal::byte p_address,
    std::optional<pca9685::settings> p_settings = std::nullopt);

  /**
   * @brief Get a pwm channel object
   *
   * NOTE: If two pwm channel objects are created with the same channel number,
   * both objects will be able to control the individual pin.
   *
   * @tparam Channel - Which channel pin to get. Can be from 0 to 15.
   * @return pwm_channel - pwm channel object
   */
  template<hal::byte Channel>
  pwm_channel get_pwm_channel()
  {
    static_assert(Channel <= max_channel_count,
                  "The PCA9685 only has 16 channels!");

    return pwm_channel(this, Channel);
  }

  /**
   * @brief Configure the device
   *
   * The settings will be cached by the driver so that it can be restored when
   * updating the PWM frequency.
   *
   * @param p_settings - settings to configure the device to
   * @return hal::status - success or i2c communication error
   */
  hal::status configure(settings p_settings);

private:
  pca9685(hal::i2c& p_i2c, hal::byte p_address);

  hal::status set_channel_frequency(hal::hertz p_frequency);
  hal::status set_channel_duty_cycle(float p_duty_cycle, hal::byte p_channel);

  hal::i2c* m_i2c;
  hal::byte m_address;
  settings m_settings{};
};
}  // namespace hal::pca
