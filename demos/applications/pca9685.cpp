#include <libhal-pca/pca9685.hpp>
#include <libhal-util/serial.hpp>
#include <libhal-util/steady_clock.hpp>

#include "../hardware_map.hpp"

hal::status application(hardware_map& p_map)
{
  using namespace std::chrono_literals;
  using namespace hal::literals;

  auto& clock = *p_map.clock;
  auto& console = *p_map.console;
  auto& i2c = *p_map.i2c;

  hal::print(console, "[pca9685] Application Starting...\n\n");
  auto pca9685 = HAL_CHECK(hal::pca::pca9685::create(i2c, 0b100'0000));
  auto pwm0 = pca9685.get_pwm_channel<0>();
  auto pwm1 = pca9685.get_pwm_channel<1>();
  auto pwm2 = pca9685.get_pwm_channel<2>();
  auto pwm3 = pca9685.get_pwm_channel<3>();
  auto pwm4 = pca9685.get_pwm_channel<4>();
  auto pwm5 = pca9685.get_pwm_channel<5>();
  auto pwm6 = pca9685.get_pwm_channel<6>();
  auto pwm7 = pca9685.get_pwm_channel<7>();
  auto pwm8 = pca9685.get_pwm_channel<8>();
  auto pwm9 = pca9685.get_pwm_channel<9>();
  auto pwm10 = pca9685.get_pwm_channel<10>();
  auto pwm11 = pca9685.get_pwm_channel<11>();
  auto pwm12 = pca9685.get_pwm_channel<12>();
  auto pwm13 = pca9685.get_pwm_channel<13>();
  auto pwm14 = pca9685.get_pwm_channel<14>();
  auto pwm15 = pca9685.get_pwm_channel<15>();

  // Setting the frequency of one channel will set the frequency of all channels
  HAL_CHECK(pwm0.frequency(1.0_kHz));

  while (true) {
    for (float duty_cycle = 0.0f; duty_cycle < 1.0f; duty_cycle += 0.05f) {
      (void)pwm0.duty_cycle(duty_cycle);
      (void)pwm1.duty_cycle(duty_cycle);
      (void)pwm2.duty_cycle(duty_cycle);
      (void)pwm3.duty_cycle(duty_cycle);
      (void)pwm4.duty_cycle(duty_cycle);
      (void)pwm5.duty_cycle(duty_cycle);
      (void)pwm6.duty_cycle(duty_cycle);
      (void)pwm7.duty_cycle(duty_cycle);
      (void)pwm8.duty_cycle(duty_cycle);
      (void)pwm9.duty_cycle(duty_cycle);
      (void)pwm10.duty_cycle(duty_cycle);
      (void)pwm11.duty_cycle(duty_cycle);
      (void)pwm12.duty_cycle(duty_cycle);
      (void)pwm13.duty_cycle(duty_cycle);
      (void)pwm14.duty_cycle(duty_cycle);
      (void)pwm15.duty_cycle(duty_cycle);
      HAL_CHECK(hal::delay(clock, 500ms));
    }
  }

  return hal::success();
}
