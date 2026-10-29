// Linux replacement for the MFC precompiled header.
#pragma once
#include "windows.h"
#include "winioctl.h"
#include "Priscilla/UtilityFx.h"
// NVME_PORT_20/40 and NVME_ID are referenced by AddDiskNVMe's signature.
#include "NVMeInterpreter.h"
#include "JMicronUsbRaidDef.h"
