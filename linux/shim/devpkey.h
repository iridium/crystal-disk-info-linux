// Device property keys are a Windows-only concept.
#pragma once
#define DEFINE_DEVPROPKEY(name, ...) static_assert(true, "")
