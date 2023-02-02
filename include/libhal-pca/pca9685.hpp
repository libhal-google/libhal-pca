#pragma once

#include <array>
#include <cmath>

#include <libhal-util/bit.hpp>
#include <libhal-util/enum.hpp>
#include <libhal-util/i2c.hpp>
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
  static constexpr hal::byte mode1 = 0x00;
  static constexpr hal::byte pwm_channel0_address = 0x06;
  static constexpr hal::byte byte_per_pwm_channel = 4;
  static constexpr hal::byte prescaler_address = 0xFE;
  static constexpr size_t max_channel_count = 16;
  static constexpr float max_pwm_ticks = 4095.0f;

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
    pwm_channel(pca9685* p_pca9685, hal::byte p_channel)
      : m_pca9685(p_pca9685)
      , m_channel(p_channel)
    {
    }

    hal::status driver_frequency(hal::hertz p_frequency) override
    {
      return m_pca9685->set_channel_frequency(p_frequency);
    }

    hal::status driver_duty_cycle(float p_duty_cycle) override
    {
      return m_pca9685->set_channel_duty_cycle(p_duty_cycle, m_channel);
    }

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
    std::optional<pca9685::settings> p_settings = std::nullopt)
  {
    using namespace hal::literals;
    pca9685 pca(p_i2c, p_address);
    HAL_CHECK(pca.configure(p_settings.value_or(settings{})));
    return pca;
  }

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
  pca9685(hal::i2c& p_i2c, hal::byte p_address)
    : m_i2c(&p_i2c)
    , m_address(p_address)
  {
  }

  static constexpr hal::byte pwm_channel_address(hal::byte p_channel)
  {
    return static_cast<hal::byte>(pwm_channel0_address +
                                  (p_channel * byte_per_pwm_channel));
  }

  hal::status set_channel_frequency(hal::hertz p_frequency);
  hal::status set_channel_duty_cycle(float p_duty_cycle, hal::byte p_channel);

  hal::i2c* m_i2c;
  hal::byte m_address;
  settings m_settings{};
};

inline hal::status pca9685::configure(pca9685::settings p_settings)
{
  static constexpr hal::byte mode_address = 0U;

  // Mode 0 flags
  static constexpr auto enable_external_oscillator = bit::mask::from<6>();
  static constexpr auto auto_increment_address = bit::mask::from<5>();
  static constexpr auto sleep = bit::mask::from<4>();
  static constexpr auto sub1_enable = bit::mask::from<3>();
  static constexpr auto sub2_enable = bit::mask::from<2>();
  static constexpr auto sub3_enable = bit::mask::from<1>();
  static constexpr auto all_call_enable = bit::mask::from<0>();

  // Mode 1 flags
  static constexpr auto invert_logic = bit::mask::from<4>();
  static constexpr auto update_on_acknowledge = bit::mask::from<3>();
  static constexpr auto output_drive = bit::mask::from<2>();
  static constexpr auto output_enable_pin_state = bit::mask::from<1, 0>();

  auto mode1_byte = bit::value<hal::byte>(0)
                      .insert<enable_external_oscillator>(0U)
                      .insert<auto_increment_address>(1U)
                      .insert<sleep>(hal::byte(p_settings.sleep))
                      .insert<sub1_enable>(0U)
                      .insert<sub2_enable>(0U)
                      .insert<sub3_enable>(0U)
                      .insert<all_call_enable>(0U);

  auto mode2_byte =
    bit::value<hal::byte>(0)
      .insert<invert_logic>(hal::byte(p_settings.invert_outputs))
      .insert<update_on_acknowledge>(
        hal::byte(p_settings.output_changes_on_i2c_acknowledge))
      .insert<output_drive>(hal::byte(p_settings.totem_pole_output))
      .insert<output_enable_pin_state>(
        hal::value(p_settings.pin_disabled_state));

  HAL_CHECK(
    hal::write(*m_i2c,
               m_address,
               std::array{ mode_address, mode1_byte.get(), mode2_byte.get() },
               hal::never_timeout()));

  m_settings = p_settings;

  return hal::success();
}

inline hal::status pca9685::set_channel_frequency(hal::hertz p_frequency)
{
  using namespace hal::literals;
  static constexpr auto internal_oscillator = 25.0_MHz;
  bool within_bounds = 24.0_Hz < p_frequency && p_frequency < 1526.0_Hz;
  if (!within_bounds) {
    return hal::new_error(std::errc::argument_out_of_domain);
  }

  auto prescale_value =
    std::round(internal_oscillator / (max_pwm_ticks * p_frequency));
  auto prescale_value_byte = static_cast<hal::byte>(prescale_value - 1.0f);

  // The device must be put to sleep before it can have its prescale value
  // updated.
  auto original_settings = m_settings;
  // Ensure that the settings put the device to sleep.
  m_settings.sleep = true;
  HAL_CHECK(configure(m_settings));

  HAL_CHECK(hal::write(*m_i2c,
                       m_address,
                       std::array{ prescaler_address, prescale_value_byte },
                       hal::never_timeout()));

  // Configure device back to what it was before which may or may not be asleep
  return configure(original_settings);
}

inline hal::status pca9685::set_channel_duty_cycle(float p_duty_cycle,
                                                   hal::byte p_channel)
{
  // The PCA9685 works by setting a HIGH point and a LOW point out of the 12-bit
  // timer cycle. This driver implements LEFT aligned PWM for all channels, and
  // thus, the point in which the pulse should go HIGH should be at the start at
  // position 0. The point in which the PWM duty cycle is pulled low is
  // calculated based on the passed in p_duty_cycle value.
  static constexpr hal::byte high_point_msb_lsb = 0U;

  auto low_point_float = std::round(max_pwm_ticks * p_duty_cycle);
  auto low_point = static_cast<std::uint16_t>(low_point_float);
  hal::byte low_point_lsb = low_point & 0xFF;
  hal::byte low_point_msb = low_point >> 8;

  return hal::write(*m_i2c,
                    m_address,
                    std::array{ pwm_channel_address(p_channel),
                                high_point_msb_lsb,
                                high_point_msb_lsb,
                                low_point_lsb,
                                low_point_msb },
                    hal::never_timeout());
}
}  // namespace hal::pca
