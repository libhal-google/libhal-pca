#include <cstdlib>
#include <exception>

#include <libhal-pca/pca9685.hpp>

int main()
{
}

namespace boost {
void throw_exception(std::exception const& e)
{
  std::abort();
}
}  // namespace boost
