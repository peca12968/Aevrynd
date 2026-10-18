#include <stdexcept>
namespace RecenterTrial {
extern unsigned checks;
void Need(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
}
