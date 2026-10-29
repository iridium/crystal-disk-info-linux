// Raw disk access for the Linux port.
//
// Capturing (needs CAP_SYS_RAWIO/CAP_SYS_ADMIN or root) is kept separate from
// interpreting: cdi-probe only captures the raw IDENTIFY / SMART / NVMe log
// buffers, and the unprivileged GUI feeds them to the upstream CAtaSmart logic.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

struct LinuxDisk
{
	enum class Kind { Ata, Nvme };

	std::string sysname;    // sda, nvme0n1
	std::string devnode;    // /dev/sda
	std::string sysDevice;  // resolved /sys/block/<name>/device
	std::string pciDevice;  // sysfs path of the PCIe function (NVMe only)
	Kind kind = Kind::Ata;
	uint32_t nsid = 0;      // NVMe namespace
	bool isUsb = false;
	std::string error;      // capture error, e.g. "Permission denied"

	// Captured buffers: identify, smart, thresh, log.AA.PP,
	// nvme.ctrl, nvme.ns, nvme.smart
	std::map<std::string, std::vector<uint8_t>> buffers;

	const std::vector<uint8_t>* buffer(const std::string& key) const
	{
		auto it = buffers.find(key);
		return it == buffers.end() ? nullptr : &it->second;
	}
};

namespace rawio
{
	// Block devices that may carry SMART data (no buffers captured yet).
	std::vector<LinuxDisk> Enumerate();

	// Fill disk.buffers by talking to the device. Sets disk.error on failure.
	void Capture(LinuxDisk& disk);

	// Text snapshot format exchanged between cdi-probe and the GUI.
	std::string Serialize(const std::vector<LinuxDisk>& disks);
	std::vector<LinuxDisk> Parse(const std::string& text);

	// Mount points ("/ /home") of the disk, its partitions and dm/md holders.
	std::string MountPoints(const std::string& sysname);

	std::string ReadSysfs(const std::string& path);
}

// Where CAtaSmart::Init gets its disks from. Default: enumerate and capture
// in-process (works as root). The GUI installs a source that runs cdi-probe.
using SnapshotSource = std::function<std::vector<LinuxDisk>()>;
void SetSnapshotSource(SnapshotSource source);
const std::vector<LinuxDisk>& LinuxDisks();
