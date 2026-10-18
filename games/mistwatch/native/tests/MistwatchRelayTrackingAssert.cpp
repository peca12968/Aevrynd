#include <stdexcept>

namespace TrackingTest {
extern unsigned checks;
void Need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
}
