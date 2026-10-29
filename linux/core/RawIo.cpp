// Raw device capture for the Linux port. Deliberately independent of the
// Windows shim and CAtaSmart so cdi-probe (the only privileged piece) stays small.

#include "LinuxDisk.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <limits.h>
#include <linux/nvme_ioctl.h>
#include <scsi/sg.h>
#include <set>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace rawio
{

std::string ReadSysfs(const std::string& path)
{
	std::ifstream f(path);
	std::string s;
	if (f) std::getline(f, s);
	while (!s.empty() && (s.back() == '\n' || s.back() == ' ' || s.back() == '\r')) s.pop_back();
	return s;
}

static std::string RealPath(const std::string& path)
{
	char buf[PATH_MAX];
	if (realpath(path.c_str(), buf)) return buf;
	return std::string();
}

static bool SkipBlockDevice(const std::string& n)
{
	static const char* prefixes[] = { "loop", "ram", "zram", "dm-", "md", "sr", "fd", "nbd", "mmcblk",
	                                  "zd", "rbd", "drbd", "pmem", "vd", "xvd", "bcache" };
	for (const char* p : prefixes)
		if (n.rfind(p, 0) == 0) return true;
	return false;
}

static std::string FindPciDevice(const std::string& sysDevice)
{
	std::string p = sysDevice;
	while (p.size() > 1)
	{
		struct stat st;
		if (stat((p + "/current_link_speed").c_str(), &st) == 0) return p;
		size_t slash = p.rfind('/');
		if (slash == std::string::npos || slash == 0) break;
		p = p.substr(0, slash);
	}
	return std::string();
}

std::vector<LinuxDisk> Enumerate()
{
	std::vector<std::string> names;
	if (DIR* d = opendir("/sys/block"))
	{
		while (dirent* e = readdir(d))
		{
			std::string n = e->d_name;
			if (n[0] == '.' || SkipBlockDevice(n)) continue;
			// nvmeXcYnZ are hidden multipath paths of nvmeXnZ
			if (n.rfind("nvme", 0) == 0 && n.find('c') != std::string::npos) continue;
			names.push_back(n);
		}
		closedir(d);
	}
	// nvme0n1 < nvme1n1 < nvme10n1, sda < sdb < sdaa
	std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
		bool an = a.rfind("nvme", 0) == 0, bn = b.rfind("nvme", 0) == 0;
		if (an != bn) return an;
		if (a.size() != b.size()) return a.size() < b.size();
		return a < b;
	});

	std::vector<LinuxDisk> disks;
	for (const auto& name : names)
	{
		LinuxDisk disk;
		disk.sysname = name;
		disk.devnode = "/dev/" + name;
		disk.sysDevice = RealPath("/sys/block/" + name + "/device");
		disk.isUsb = disk.sysDevice.find("/usb") != std::string::npos;
		if (name.rfind("nvme", 0) == 0)
		{
			disk.kind = LinuxDisk::Kind::Nvme;
			disk.nsid = (uint32_t)atoi(name.c_str() + name.rfind('n') + 1);
			disk.pciDevice = FindPciDevice(disk.sysDevice);
		}
		disks.push_back(disk);
	}
	return disks;
}

// ---------------------------------------------------------------------------
// ATA via SCSI ATA PASS-THROUGH (16)
// ---------------------------------------------------------------------------

// regs: features, count, lba low, lba mid, lba high, device, command
static bool AtaPioIn(int fd, const uint8_t regs[7], bool ext, uint8_t* data, uint32_t len, std::string& err)
{
	uint8_t cdb[16] = {};
	uint8_t sense[64] = {};
	cdb[0] = 0x85;
	cdb[1] = (4 << 1) | (ext ? 1 : 0);   // protocol: PIO data-in
	cdb[2] = 0x0E;                        // T_DIR=in, BYT_BLOK=1, T_LENGTH=count
	cdb[4] = regs[0];
	cdb[6] = regs[1];
	cdb[8] = regs[2];
	cdb[10] = regs[3];
	cdb[12] = regs[4];
	cdb[13] = regs[5];
	cdb[14] = regs[6];

	memset(data, 0, len);
	sg_io_hdr_t io = {};
	io.interface_id = 'S';
	io.cmdp = cdb;
	io.cmd_len = sizeof(cdb);
	io.sbp = sense;
	io.mx_sb_len = sizeof(sense);
	io.timeout = 10000;
	io.dxfer_direction = SG_DXFER_FROM_DEV;
	io.dxferp = data;
	io.dxfer_len = len;

	if (ioctl(fd, SG_IO, &io) < 0)
	{
		err = strerror(errno);
		return false;
	}
	if (io.host_status != 0 || (io.driver_status & 0x0F & ~0x08) != 0)
	{
		err = "transport error";
		return false;
	}
	if (io.status != 0)
	{
		// Descriptor sense 0x72 with ATA Status Return (0x09) is informational
		// on some bridges; anything else is a failed command.
		bool ataReturn = io.sb_len_wr >= 8 && (sense[0] & 0x7F) == 0x72 && sense[8] == 0x09;
		if (!ataReturn || (sense[8 + 13] & 0x01))
		{
			err = "command rejected";
			return false;
		}
	}
	return true;
}

static bool NvmeAdmin(int fd, uint8_t opcode, uint32_t nsid, uint32_t cdw10, void* data, uint32_t len, std::string& err)
{
	struct nvme_admin_cmd cmd = {};
	cmd.opcode = opcode;
	cmd.nsid = nsid;
	cmd.addr = (uint64_t)(uintptr_t)data;
	cmd.data_len = len;
	cmd.cdw10 = cdw10;
	cmd.timeout_ms = 10000;
	int rc = ioctl(fd, NVME_IOCTL_ADMIN_CMD, &cmd);
	if (rc < 0) { err = strerror(errno); return false; }
	if (rc > 0) { err = "NVMe status " + std::to_string(rc); return false; }
	return true;
}

void Capture(LinuxDisk& disk)
{
	int fd = open(disk.devnode.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
	{
		disk.error = strerror(errno);
		return;
	}

	std::string err;
	if (disk.kind == LinuxDisk::Kind::Nvme)
	{
		std::vector<uint8_t> ctrl(4096), ns(4096), smart(512);
		if (!NvmeAdmin(fd, 0x06, 0, 1, ctrl.data(), 4096, err))
		{
			disk.error = err;
			close(fd);
			return;
		}
		disk.buffers["nvme.ctrl"] = ctrl;
		if (NvmeAdmin(fd, 0x06, disk.nsid, 0, ns.data(), 4096, err)) disk.buffers["nvme.ns"] = ns;
		const uint32_t numd = 512 / 4 - 1;
		if (NvmeAdmin(fd, 0x02, 0xFFFFFFFF, (numd << 16) | 0x02, smart.data(), 512, err)
		 || NvmeAdmin(fd, 0x02, disk.nsid, (numd << 16) | 0x02, smart.data(), 512, err))
		{
			disk.buffers["nvme.smart"] = smart;
		}
		close(fd);
		return;
	}

	std::vector<uint8_t> buf(512);
	const uint8_t identify[7] = { 0x00, 0x01, 0x00, 0x00, 0x00, 0xA0, 0xEC };
	if (!AtaPioIn(fd, identify, false, buf.data(), 512, err))
	{
		// A sleeping or slow USB bridge sometimes needs a second try.
		usleep(200 * 1000);
		if (!AtaPioIn(fd, identify, false, buf.data(), 512, err))
		{
			disk.error = err;
			close(fd);
			return;
		}
	}
	disk.buffers["identify"] = buf;

	const uint8_t smartRead[7] = { 0xD0, 0x01, 0x01, 0x4F, 0xC2, 0xA0, 0xB0 };
	if (AtaPioIn(fd, smartRead, false, buf.data(), 512, err)) disk.buffers["smart"] = buf;
	const uint8_t smartThresh[7] = { 0xD1, 0x01, 0x01, 0x4F, 0xC2, 0xA0, 0xB0 };
	if (AtaPioIn(fd, smartThresh, false, buf.data(), 512, err)) disk.buffers["thresh"] = buf;

	// General Purpose Logs consulted by upstream (Device Statistics, IDENTIFY log).
	const uint16_t* id = reinterpret_cast<const uint16_t*>(disk.buffers["identify"].data());
	bool gpl = (id[84] & (1 << 5)) && (id[87] & (1 << 5));
	if (gpl)
	{
		auto readLog = [&](uint8_t addr, uint8_t page) {
			const uint8_t regs[7] = { 0x00, 0x01, addr, page, 0x00, 0xA0, 0x2F };
			char key[16];
			snprintf(key, sizeof(key), "log.%02X.%02X", addr, page);
			if (AtaPioIn(fd, regs, true, buf.data(), 512, err)) disk.buffers[key] = buf;
		};
		readLog(0x00, 0);
		for (uint8_t p = 0; p <= 7; ++p) readLog(0x04, p);
		readLog(0x30, 2);
	}
	close(fd);
}

// ---------------------------------------------------------------------------
// Snapshot text format
// ---------------------------------------------------------------------------

static std::string Hex(const std::vector<uint8_t>& v)
{
	static const char* digits = "0123456789abcdef";
	std::string s;
	s.reserve(v.size() * 2);
	for (uint8_t b : v) { s += digits[b >> 4]; s += digits[b & 15]; }
	return s;
}

static std::vector<uint8_t> Unhex(const std::string& s)
{
	std::vector<uint8_t> v;
	v.reserve(s.size() / 2);
	for (size_t i = 0; i + 1 < s.size(); i += 2)
		v.push_back((uint8_t)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
	return v;
}

std::string Serialize(const std::vector<LinuxDisk>& disks)
{
	std::ostringstream o;
	o << "CDI-SNAPSHOT 1\n";
	for (const auto& d : disks)
	{
		o << "disk " << d.sysname << "\n";
		o << "kind " << (d.kind == LinuxDisk::Kind::Nvme ? "nvme" : "ata") << "\n";
		o << "nsid " << d.nsid << "\n";
		o << "usb " << (d.isUsb ? 1 : 0) << "\n";
		if (!d.error.empty()) o << "error " << d.error << "\n";
		for (const auto& kv : d.buffers) o << "buf " << kv.first << " " << Hex(kv.second) << "\n";
		o << "end\n";
	}
	return o.str();
}

std::vector<LinuxDisk> Parse(const std::string& text)
{
	// Paths are recomputed locally; only sysnames and buffers travel.
	std::vector<LinuxDisk> known = Enumerate();
	std::vector<LinuxDisk> out;
	std::istringstream in(text);
	std::string line;
	LinuxDisk cur;
	bool inDisk = false;
	while (std::getline(in, line))
	{
		size_t sp = line.find(' ');
		std::string key = line.substr(0, sp);
		std::string val = sp == std::string::npos ? "" : line.substr(sp + 1);
		if (key == "disk")
		{
			cur = LinuxDisk();
			for (const auto& k : known)
				if (k.sysname == val) cur = k;
			cur.sysname = val;
			cur.devnode = "/dev/" + val;
			inDisk = true;
		}
		else if (!inDisk) continue;
		else if (key == "kind") cur.kind = val == "nvme" ? LinuxDisk::Kind::Nvme : LinuxDisk::Kind::Ata;
		else if (key == "nsid") cur.nsid = (uint32_t)atoi(val.c_str());
		else if (key == "usb") cur.isUsb = val == "1";
		else if (key == "error") cur.error = val;
		else if (key == "buf")
		{
			size_t sp2 = val.find(' ');
			if (sp2 != std::string::npos) cur.buffers[val.substr(0, sp2)] = Unhex(val.substr(sp2 + 1));
		}
		else if (key == "end")
		{
			out.push_back(cur);
			inDisk = false;
		}
	}
	return out;
}

// ---------------------------------------------------------------------------
// Mount points
// ---------------------------------------------------------------------------

std::string MountPoints(const std::string& sysname)
{
	std::set<std::string> names = { sysname };
	if (DIR* d = opendir(("/sys/block/" + sysname).c_str()))
	{
		while (dirent* e = readdir(d))
		{
			std::string n = e->d_name;
			if (n.rfind(sysname, 0) == 0 && n != sysname) names.insert(n);
		}
		closedir(d);
	}

	// Follow holders (LUKS, LVM, md) so encrypted roots still show "/".
	std::vector<std::string> queue(names.begin(), names.end());
	for (size_t qi = 0; qi < queue.size() && qi < 256; ++qi)
	{
		const std::string& n = queue[qi];
		std::string base = n == sysname || n.rfind(sysname, 0) != 0 ? "/sys/class/block/" + n : "/sys/block/" + sysname + "/" + n;
		if (DIR* h = opendir((base + "/holders").c_str()))
		{
			while (dirent* e = readdir(h))
			{
				if (e->d_name[0] == '.') continue;
				if (names.insert(e->d_name).second) queue.push_back(e->d_name);
			}
			closedir(h);
		}
	}

	std::set<std::string> devs;
	for (const auto& n : names)
	{
		devs.insert("/dev/" + n);
		std::string dm = ReadSysfs("/sys/class/block/" + n + "/dm/name");
		if (!dm.empty()) devs.insert("/dev/mapper/" + dm);
	}

	std::vector<std::string> mounts;
	std::ifstream m("/proc/self/mounts");
	std::string line;
	while (std::getline(m, line))
	{
		std::istringstream ss(line);
		std::string src, dst;
		ss >> src >> dst;
		std::string real = RealPath(src);
		if (!devs.count(src) && (real.empty() || !devs.count(real))) continue;
		size_t p;
		while ((p = dst.find("\\040")) != std::string::npos) dst.replace(p, 4, " ");
		if (std::find(mounts.begin(), mounts.end(), dst) == mounts.end()) mounts.push_back(dst);
	}
	std::sort(mounts.begin(), mounts.end());
	std::string out;
	for (const auto& s : mounts)
	{
		if (!out.empty()) out += " ";
		out += s;
	}
	return out;
}

} // namespace rawio
