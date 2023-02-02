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

class pca9685
{
public:
  static constexpr hal::byte mode1 = 0x00;
  static constexpr hal::byte pwm_channel0_address = 0x06;
  static constexpr hal::byte byte_per_pwm_channel = 4;
  static constexpr hal::byte prescaler_address = 0xFE;
  static constexpr size_t max_channel_count = 16;
  static constexpr float max_pwm_ticks = 4096.0f;

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

  struct settings
  {
    /// Invert the voltage for all pins.
    bool invert_outputs = false;
    /// Update PWM channels on acknowledge
    bool output_changes_on_i2c_acknowledge = false;
    /// If true: output channels are configured as totem pole.
    /// If false, the pin will be open collector
    bool totem_pole_output = true;
    /// Control what the state of the pins is when the output enable is
    /// asserted.
    disabled_pin_state pin_disabled_state = disabled_pin_state::set_low;
    /// Set this to `std::nullopt` to use the internal 25MHz oscillator
    /// To use an external oscillator, set this to the external oscillator's
    /// frequency.
    std::optional<hal::hertz> external_oscillator_hz = std::nullopt;
  };

  static constexpr hal::hertz internal_oscillator()
  {
    using namespace hal::literals;
    return 25.0_MHz;
  }

  static constexpr hal::byte pwm_channel_address(hal::byte p_channel)
  {
    return static_cast<hal::byte>(pwm_channel0_address +
                                  (p_channel * byte_per_pwm_channel));
  }

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

  template<hal::byte Channel>
  pwm_channel get_pwm_channel()
  {
    static_assert(Channel <= max_channel_count,
                  "The PCA9685 only has 16 channels!");

    return pwm_channel(this, Channel);
  }

  hal::status configure(settings p_settings);

private:
  pca9685(hal::i2c& p_i2c, hal::byte p_address)
    : m_i2c(&p_i2c)
    , m_address(p_address)
  {
  }

  hal::status set_channel_frequency(hal::hertz p_frequency);
  hal::status set_channel_duty_cycle(float p_duty_cycle, hal::byte p_channel);

  hal::i2c* m_i2c;
  hal::byte m_address;
  hal::hertz m_frequency = internal_oscillator();
};

inline hal::status pca9685::configure(pca9685::settings p_settings)
{
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

  auto enable_oscillator = p_settings.external_oscillator_hz.has_value();

  auto mode1_byte =
    bit::value<hal::byte>(0)
      .insert<enable_external_oscillator>(hal::byte(enable_oscillator))
      .insert<auto_increment_address>(1U)
      .insert<sleep>(0U)
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

  HAL_CHECK(hal::write(*m_i2c,
                       m_address,
                       std::array{ mode1_byte.get(), mode2_byte.get() },
                       hal::never_timeout()));

  // Only set the frequency if the write operation was successful.
  m_frequency =
    p_settings.external_oscillator_hz.value_or(internal_oscillator());

  return hal::success();
}

inline hal::status pca9685::set_channel_frequency(hal::hertz p_frequency)
{
  using namespace hal::literals;
  if (24.0_Hz < p_frequency && p_frequency < 1526.0_Hz) {
    return hal::new_error(std::errc::argument_out_of_domain);
  }

  auto prescale_value = std::round(m_frequency / (4096.0f * p_frequency));
  auto prescale_value_byte = static_cast<hal::byte>(prescale_value);
  return hal::write(*m_i2c,
                    m_address,
                    std::array{ prescaler_address, prescale_value_byte },
                    hal::never_timeout());
}

inline hal::status pca9685::set_channel_duty_cycle(float p_duty_cycle,
                                                   hal::byte p_channel)
{
  auto duty_cycle = std::clamp(p_duty_cycle, -1.0f, 1.0f);
  auto high_ticks_float = max_pwm_ticks * duty_cycle;
  auto low_ticks_float = max_pwm_ticks - high_ticks_float;

  auto high_ticks = static_cast<std::uint16_t>(high_ticks_float);
  auto low_ticks = static_cast<std::uint16_t>(low_ticks_float);

  hal::byte high_ticks_lsb = high_ticks & 0xFF;
  hal::byte high_ticks_msb = high_ticks >> 8;
  hal::byte low_ticks_lsb = low_ticks & 0xFF;
  hal::byte low_ticks_msb = low_ticks >> 8;

  return hal::write(*m_i2c,
                    m_address,
                    std::array{ pwm_channel_address(p_channel),
                                low_ticks_lsb,
                                low_ticks_msb,
                                high_ticks_lsb,
                                high_ticks_msb },
                    hal::never_timeout());
}
}  // namespace hal::pca
