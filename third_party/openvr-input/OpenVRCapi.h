#ifndef MCOSU_OPENVR_INPUT_CAPI_H
#define MCOSU_OPENVR_INPUT_CAPI_H

// The generated C header typedefs bool under MinGW's legacy __WIN32 macro.
#pragma push_macro("__WIN32")
#undef __WIN32
#include "openvr_capi.h"
#pragma pop_macro("__WIN32")

S_API intptr_t VR_GetGenericInterface(const char *version, EVRInitError *error);

#endif
