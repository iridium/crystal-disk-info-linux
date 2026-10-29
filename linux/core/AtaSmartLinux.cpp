/*---------------------------------------------------------------------------*/
//  Linux back end for CAtaSmart.
//
//  Upstream AtaSmart.cpp is compiled almost verbatim (tools/extract_core.py
//  strips the functions that talk to Windows drivers). The replacements here
//  answer IDENTIFY / SMART / log requests from a snapshot of raw buffers that
//  rawio::Capture (in-process as root, or cdi-probe) collected.
/*---------------------------------------------------------------------------*/

#include "stdafx.h"
#include "AtaSmart.h"
#include "LinuxDisk.h"

#include <time.h>

// ---------------------------------------------------------------------------
// Process-wide disk table: PhysicalDriveId is an index into it.
// ---------------------------------------------------------------------------

static std::vector<LinuxDisk> g_disks;
static SnapshotSource g_source;

void SetSnapshotSource(SnapshotSource source) { g_source = std::move(source); }
const std::vector<LinuxDisk>& LinuxDisks() { return g_disks; }

static const LinuxDisk* DiskAt(INT physicalDriveId)
{
	if (physicalDriveId < 0 || physicalDriveId >= (INT)g_disks.size()) return nullptr;
	return &g_disks[(size_t)physicalDriveId];
}

static bool CopyBuffer(INT physicalDriveId, const std::string& key, void* dst, size_t len)
{
	const LinuxDisk* disk = DiskAt(physicalDriveId);
	const std::vector<uint8_t>* buf = disk ? disk->buffer(key) : nullptr;
	memset(dst, 0, len);
	if (!buf || buf->empty()) return false;
	memcpy(dst, buf->data(), std::min(len, buf->size()));
	return true;
}

void DebugPrint(CString cstr)
{
	static bool debug = getenv("CDI_DEBUG") != nullptr;
	if (debug) fprintf(stderr, "[cdi] %ls\n", cstr.GetString());
}

ULONGLONG GetTickCount64()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ULONGLONG)ts.tv_sec * 1000ULL + (ULONGLONG)ts.tv_nsec / 1000000ULL;
}

ULONGLONG GetTickCountFx() { return GetTickCount64(); }

std::string CString::toUtf8() const
{
	std::string out;
	for (wchar_t wc : str())
	{
		uint32_t c = (uint32_t)wc;
		if (c < 0x80) out += (char)c;
		else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 0x3F)); }
		else if (c < 0x10000) { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
		else { out += (char)(0xF0 | (c >> 18)); out += (char)(0x80 | ((c >> 12) & 0x3F)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
	}
	return out;
}

// ---------------------------------------------------------------------------
// CAtaSmart members replaced for Linux
// ---------------------------------------------------------------------------

CAtaSmart::CAtaSmart()
{
	m_bAtaPassThrough = TRUE;
	m_bAtaPassThroughSmart = TRUE;
	m_bNVMeStorageQuery = TRUE;
}

CAtaSmart::~CAtaSmart() {}

// The snapshot was taken with the drive awake; nothing to do.
VOID CAtaSmart::WakeUp(INT) {}

// Commands that change drive state (SMART enable, AAM/APM) are not offered.
BOOL CAtaSmart::SendAtaCommandPd(INT, BYTE, BYTE, BYTE, BYTE, PBYTE, DWORD) { return FALSE; }
BOOL CAtaSmart::ControlSmartStatusPd(INT, BYTE, BYTE) { return FALSE; }

BOOL CAtaSmart::DoIdentifyDevicePd(INT physicalDriveId, BYTE, IDENTIFY_DEVICE* data)
{
	if (!data || !CopyBuffer(physicalDriveId, "identify", data, sizeof(ATA_IDENTIFY_DEVICE))) return FALSE;
	CString model = data->A.Model;
	model.Trim();
	return !model.IsEmpty();
}

BOOL CAtaSmart::GetSmartAttributePd(INT physicalDriveId, BYTE, ATA_SMART_INFO* asi)
{
	if (!CopyBuffer(physicalDriveId, "smart", &(asi->SmartReadData), sizeof(asi->SmartReadData))) return FALSE;
	return FillSmartData(asi);
}

BOOL CAtaSmart::GetSmartThresholdPd(INT physicalDriveId, BYTE, ATA_SMART_INFO* asi)
{
	if (!CopyBuffer(physicalDriveId, "thresh", &(asi->SmartReadThreshold), sizeof(asi->SmartReadThreshold))) return FALSE;
	return FillSmartThreshold(asi);
}

BOOL CAtaSmart::ReadLogExtPd(INT physicalDriveId, BYTE, BYTE logAddress, BYTE logPage, PBYTE data, DWORD dataSize)
{
	if (!data || dataSize == 0) return FALSE;
	char key[16];
	snprintf(key, sizeof(key), "log.%02X.%02X", logAddress, logPage);
	return CopyBuffer(physicalDriveId, key, data, dataSize);
}

// USB-SATA bridges speak SAT, which is exactly what the capture used.
BOOL CAtaSmart::DoIdentifyDeviceSat(INT physicalDriveId, BYTE target, IDENTIFY_DEVICE* data, COMMAND_TYPE)
{
	return DoIdentifyDevicePd(physicalDriveId, target, data);
}
BOOL CAtaSmart::GetSmartAttributeSat(INT physicalDriveId, BYTE target, ATA_SMART_INFO* asi)
{
	return GetSmartAttributePd(physicalDriveId, target, asi);
}
BOOL CAtaSmart::GetSmartThresholdSat(INT physicalDriveId, BYTE target, ATA_SMART_INFO* asi)
{
	return GetSmartThresholdPd(physicalDriveId, target, asi);
}
BOOL CAtaSmart::ControlSmartStatusSat(INT, BYTE, BYTE, COMMAND_TYPE) { return FALSE; }
BOOL CAtaSmart::SendAtaCommandSat(INT, BYTE, BYTE, BYTE, BYTE, COMMAND_TYPE) { return FALSE; }
BOOL CAtaSmart::ReadLogExtSat(INT physicalDriveId, BYTE target, BYTE logAddress, BYTE logPage, PBYTE data, DWORD dataSize, COMMAND_TYPE)
{
	return ReadLogExtPd(physicalDriveId, target, logAddress, logPage, data, dataSize);
}

BOOL CAtaSmart::GetNvMeIdentifyControllerData(INT physicalDriveId, BYTE* outBuffer)
{
	return CopyBuffer(physicalDriveId, "nvme.ctrl", outBuffer, sizeof(NVME_IDENTIFY_DEVICE));
}

BOOL CAtaSmart::DoIdentifyDeviceNVMeStorageQuery(INT physicalDriveId, INT, INT, IDENTIFY_DEVICE* data, DWORD* diskSize)
{
	BYTE ns[4096];
	if (diskSize && CopyBuffer(physicalDriveId, "nvme.ns", ns, sizeof(ns)))
	{
		ULONG64 totalLBA = B8toB64le_ptr(&ns[0]);
		BYTE flbas = ns[26] & 0x0F;
		BYTE lbads = ns[128 + flbas * 4 + 2];
		ULONG64 sectorSize = lbads ? (1ULL << lbads) : 512;
		*diskSize = (DWORD)(totalLBA * sectorSize / 1000 / 1000);
	}
	return CopyBuffer(physicalDriveId, "nvme.ctrl", data, sizeof(NVME_IDENTIFY_DEVICE));
}

BOOL CAtaSmart::GetSmartAttributeNVMeStorageQuery(INT physicalDriveId, INT, INT, ATA_SMART_INFO* asi)
{
	if (!CopyBuffer(physicalDriveId, "nvme.smart", &(asi->SmartReadData), 512)) return FALSE;
	BYTE ctrl[4096];
	if (CopyBuffer(physicalDriveId, "nvme.ctrl", ctrl, sizeof(ctrl)))
	{
		asi->IsNvmeThresholdSupported = IsNVMeTemperatureThresholdDefined(ctrl);
		asi->IsNvmeThermalManagementSupported = IsNVMeThermalManagementTemperatureDefined(ctrl);
	}
	return TRUE;
}

// --- Windows-only controller paths: never selected on Linux ----------------

BOOL CAtaSmart::DoIdentifyDeviceScsi(INT, INT, IDENTIFY_DEVICE*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeScsi(INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartThresholdScsi(INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::ControlSmartStatusScsi(INT, INT, BYTE) { return FALSE; }
BOOL CAtaSmart::SendAtaCommandScsi(INT, INT, BYTE, BYTE, BYTE) { return FALSE; }
BOOL CAtaSmart::DoIdentifyDeviceSi(INT, INT, INT, DWORD, IDENTIFY_DEVICE*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeSi(INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartThresholdSi(INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeWmi(ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartThresholdWmi(ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::DoIdentifyDeviceCsmi(INT, PCSMI_SAS_PHY_ENTITY, IDENTIFY_DEVICE*) { return FALSE; }
BOOL CAtaSmart::DoIdentifyDeviceMegaRAID(INT, INT, IDENTIFY_DEVICE*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeCsmi(INT, PCSMI_SAS_PHY_ENTITY, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartThresholdCsmi(INT, PCSMI_SAS_PHY_ENTITY, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::ControlSmartStatusCsmi(INT, PCSMI_SAS_PHY_ENTITY, BYTE) { return FALSE; }
BOOL CAtaSmart::SendAtaCommandCsmi(INT, PCSMI_SAS_PHY_ENTITY, BYTE, BYTE, BYTE, PBYTE, DWORD) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeMegaRAID(INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartThresholdMegaRAID(INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::ControlSmartStatusMegaRAID(INT, INT, BYTE) { return FALSE; }
BOOL CAtaSmart::SendAtaCommandMegaRAID(INT, INT, BYTE, BYTE, BYTE) { return FALSE; }
BOOL CAtaSmart::RealtekSwitchMode(INT, INT, INT, INT, INT) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeNVMeIntel(INT, INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeNVMeIntelRst(INT, INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeNVMeIntelVroc(INT, INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeNVMeSamsung(INT, INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeNVMeSamsung951(INT, INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeNVMeJMicron(INT, INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeNVMeASMedia(INT, INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeNVMeRealtek(INT, INT, INT, ATA_SMART_INFO*) { return FALSE; }
BOOL CAtaSmart::GetSmartAttributeNVMeRealtek9220DP(INT, INT, INT, ATA_SMART_INFO*) { return FALSE; }

// ---------------------------------------------------------------------------
// PCIe link speed from sysfs (replaces SlotSpeedGetter.cpp)
// ---------------------------------------------------------------------------

static PCIeSpecification SpeedToSpec(const std::string& s)
{
	double gt = atof(s.c_str());
	if (gt >= 60) return (PCIeSpecification)6;
	if (gt >= 30) return PCIe5d0;
	if (gt >= 15) return PCIe4d0;
	if (gt >= 7.5) return PCIe3d0;
	if (gt >= 4.5) return PCIe2d0;
	if (gt > 0) return PCIe1d0;
	return (PCIeSpecification)0;
}

SlotMaxCurrSpeed GetPCIeSlotSpeed(const INT physicalDriveId, const BOOL)
{
	SlotMaxCurrSpeed r = {};
	const LinuxDisk* disk = DiskAt(physicalDriveId);
	if (!disk || disk->pciDevice.empty()) return r;
	const std::string& p = disk->pciDevice;
	r.Current.SpecVersion = SpeedToSpec(rawio::ReadSysfs(p + "/current_link_speed"));
	r.Current.LinkWidth = (PCIeDataWidth)atoi(rawio::ReadSysfs(p + "/current_link_width").c_str());
	r.Maximum.SpecVersion = SpeedToSpec(rawio::ReadSysfs(p + "/max_link_speed"));
	r.Maximum.LinkWidth = (PCIeDataWidth)atoi(rawio::ReadSysfs(p + "/max_link_width").c_str());
	return r;
}

CString SlotSpeedToString(SlotSpeed s)
{
	CString cstr;
	if (s.SpecVersion == 0 || s.LinkWidth == 0) return cstr;
	cstr.Format(L"PCIe %d.0 x%d", (int)s.SpecVersion, (int)s.LinkWidth);
	return cstr;
}

// ---------------------------------------------------------------------------
// Init: replaces the WMI / SetupAPI walk of upstream
// ---------------------------------------------------------------------------

VOID CAtaSmart::Init(BOOL, BOOL advancedDiskSearch, PBOOL flagChangeDisk, BOOL workaroundHD204UI, BOOL workaroundAdataSsd, BOOL flagHideNoSmartDisk, BOOL, BOOL flagHideRAIDVolume)
{
	IsAdvancedDiskSearch = advancedDiskSearch;
	IsEnabledWmi = FALSE;
	IsWorkaroundHD204UI = workaroundHD204UI;
	IsWorkaroundAdataSsd = workaroundAdataSsd;
	FlagNvidiaController = FALSE;
	FlagMarvellController = FALSE;

	std::vector<std::string> previous;
	for (INT_PTR i = 0; i < vars.GetCount(); i++) previous.push_back(vars[i].ModelSerial.toUtf8());

	vars.RemoveAll();
	externals.RemoveAll();

	if (g_source)
	{
		g_disks = g_source();
	}
	else
	{
		g_disks = rawio::Enumerate();
		for (auto& d : g_disks) rawio::Capture(d);
	}

	for (INT id = 0; id < (INT)g_disks.size(); id++)
	{
		const LinuxDisk& disk = g_disks[(size_t)id];
		if (!disk.error.empty())
		{
			DebugPrint(CString((disk.devnode + ": " + disk.error).c_str()));
			continue;
		}
		if (disk.kind == LinuxDisk::Kind::Nvme)
		{
			IDENTIFY_DEVICE identify = {};
			DWORD diskSize = 0;
			if (DoIdentifyDeviceNVMeStorageQuery(id, -1, -1, &identify, &diskSize))
			{
				AddDiskNVMe(id, -1, -1, -1, 0, CMD_TYPE_NVME_STORAGE_QUERY, &identify, &diskSize);
			}
		}
		else
		{
			IDENTIFY_DEVICE identify = {};
			if (DoIdentifyDevicePd(id, 0xA0, &identify))
			{
				COMMAND_TYPE type = disk.isUsb ? CMD_TYPE_SAT : CMD_TYPE_PHYSICAL_DRIVE;
				AddDisk(id, -1, -1, -1, 0xA0, type, &identify, 0, NULL, CString(disk.sysDevice.c_str()));
			}
		}
	}

	for (INT_PTR i = 0; i < vars.GetCount(); i++)
	{
		const LinuxDisk* disk = DiskAt(vars[i].PhysicalDriveId);
		if (!disk) continue;
		vars[i].DriveMap = CString(rawio::MountPoints(disk->sysname).c_str());
		if (disk->isUsb && !vars[i].IsNVMe && vars[i].Interface.Find(L"USB") < 0)
		{
			vars[i].Interface = L"USB (Serial ATA)";
		}
	}

	MeasuredGetTickCount = GetTickCountFx();

	if (flagHideNoSmartDisk)
	{
		for (INT_PTR i = vars.GetCount() - 1; i >= 0; i--)
			if (vars[i].AttributeCount == 0) vars.RemoveAt(i);
	}
	if (flagHideRAIDVolume)
	{
		for (INT_PTR i = vars.GetCount() - 1; i >= 0; i--)
		{
			CString model = vars[i].Model;
			model.MakeUpper();
			if (model.Find(L"RAID") >= 0) vars.RemoveAt(i);
		}
	}

	// Fake Samsung NVMe check from upstream Init.
	for (INT_PTR i = 0; i < vars.GetCount(); i++)
	{
		if (!vars[i].IsNVMe) continue;
		CString model = vars[i].Model;
		model.MakeUpper();
		bool fake = (model.Find(L"SAMSUNG SSD 990 PRO") >= 0 && vars[i].FirmwareRev.Find(L"8888888") == 0)
		         || (model.Find(L"SAMSUNG") >= 0 && vars[i].IdentifyDevice.N.PCIeVID != 0x144D);
		if (fake) vars[i].Model = L"[FAKE] " + vars[i].Model;
	}

	if (flagChangeDisk != NULL)
	{
		std::vector<std::string> now;
		for (INT_PTR i = 0; i < vars.GetCount(); i++) now.push_back(vars[i].ModelSerial.toUtf8());
		*flagChangeDisk = now != previous;
	}
}
