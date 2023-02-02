#include <libhal-pca/pca9685.hpp>

#include <boost/ut.hpp>

namespace hal::pca {
void pca9685_test()
{
  using namespace boost::ut;
  using namespace std::literals;

  "pca9685::create()"_test = []() {
    // Setup
    // Exercise
    // Verify
  };
};
}  // namespace hal::pca
