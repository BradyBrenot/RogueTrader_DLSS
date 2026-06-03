#include <Services.hpp>
#include <Util.hpp>

#include "Interop.h"

DLL_EXPORT void DebugPrintFromManaged(const char* str)
{
#ifdef _DEBUG
    LOG_DEBUG("{}", str);
#endif
}
