// cdi-probe: the only privileged piece of the Linux port.
//
// It captures raw IDENTIFY / SMART / NVMe log buffers from block devices and
// prints them as a text snapshot. All interpretation happens in the
// unprivileged GUI. Grant it access with:
//   setcap cap_sys_rawio,cap_sys_admin,cap_dac_read_search+ep cdi-probe
// (or run it through pkexec / sudo).

#include "LinuxDisk.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

int main(int argc, char** argv)
{
	// Never trust the environment of a capability-elevated binary.
	clearenv();

	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
		{
			puts("usage: cdi-probe\nPrints a raw SMART snapshot of every disk for CrystalDiskInfo.");
			return 0;
		}
	}

	std::vector<LinuxDisk> disks = rawio::Enumerate();
	for (auto& d : disks) rawio::Capture(d);
	std::string out = rawio::Serialize(disks);
	fwrite(out.data(), 1, out.size(), stdout);
	return 0;
}
