#include "DiskData.h"

#include <QString>
#include <QStringList>

#include "stdafx.h"
#include "AtaSmart.h"
#include "LinuxDisk.h"

static QString Q(const CString& s) { return QString::fromWCharArray(s.GetString(), s.GetLength()); }

struct DiskData::Impl
{
	CAtaSmart ata;
};

DiskData::DiskData() : d(new Impl) {}
DiskData::~DiskData() = default;

// Port of the icon rules in upstream CDiskInfoDlg::UpdateListCtrl.
static AttrIcon IconFor(CAtaSmart& ata, const CAtaSmart::ATA_SMART_INFO& v, DWORD j)
{
	using A = CAtaSmart;
	const auto& at = v.Attribute[j];
	const auto& th = v.Threshold[j];

	if (v.IsNVMe)
	{
		if (v.Model.Find(L"Parallels") == 0 || v.Model.Find(L"VMware") == 0 || v.Model.Find(L"QEMU") == 0) return AttrIcon::Unknown;
		const int spare = v.Attribute[2].RawValue[0], spareThreshold = v.Attribute[3].RawValue[0];
		if (at.Id == 0x01 && at.RawValue[0]) return AttrIcon::Bad;
		if (at.Id == 0x02)
		{
			int t = MAKEWORD(v.Attribute[1].RawValue[0], v.Attribute[1].RawValue[1]) - 273;
			if (t >= v.AlarmTemperature) return AttrIcon::Bad;
			return AttrIcon::Good;
		}
		if (at.Id == 0x03 && spareThreshold > 100) return AttrIcon::Good;
		if (at.Id == 0x03 && spare == 0 && spareThreshold == 0) return AttrIcon::Good;
		if (at.Id == 0x03 && spare < spareThreshold) return AttrIcon::Bad;
		if (at.Id == 0x03 && spare == spareThreshold && spareThreshold != 100) return AttrIcon::Caution;
		if (at.Id == 0x05 && (100 - at.RawValue[0]) <= v.ThresholdFF) return AttrIcon::Caution;
		return AttrIcon::Good;
	}

	if (!(v.IsSmartCorrect && v.IsThresholdCorrect && !v.IsThresholdBug)) return AttrIcon::Unknown;

	if (!v.IsSsd && (at.Id == 0x05 || at.Id == 0xC5 || at.Id == 0xC6))
	{
		WORD raw = MAKEWORD(at.RawValue[0], at.RawValue[1]);
		WORD threshold = at.Id == 0x05 ? v.Threshold05 : at.Id == 0xC5 ? v.ThresholdC5 : v.ThresholdC6;
		if (th.ThresholdValue != 0 && at.CurrentValue < th.ThresholdValue) return AttrIcon::Bad;
		if (threshold > 0 && raw >= threshold) return AttrIcon::Caution;
		return AttrIcon::Good;
	}
	if (at.Id == 0xE8 && v.FlagLifeSanDiskUsbMemory) return AttrIcon::Good;
	if (at.Id == 0xC2 || at.Id == 0xB8) return AttrIcon::Good;

	const auto vendor = v.DiskVendorId;
	if (at.Id == 0xE6 && (vendor == A::SSD_VENDOR_WDC || vendor == A::SSD_VENDOR_SANDISK))
	{
		int life;
		if (v.FlagLifeSanDiskUsbMemory) return AttrIcon::Good;
		else if (v.FlagLifeSanDisk0_1) life = 100 - (at.RawValue[1] * 256 + at.RawValue[0]) / 100;
		else if (v.FlagLifeSanDiskLenovo) life = at.CurrentValue;
		else life = 100 - at.RawValue[1];
		if (life <= 0) return AttrIcon::Bad;
		if (life <= v.ThresholdFF) return AttrIcon::Caution;
		return AttrIcon::Good;
	}

	auto in = [&](std::initializer_list<int> vs) { for (int x : vs) if ((int)vendor == x) return true; return false; };
	const bool lifeAttr =
		   (at.Id == 0xA9 && (vendor == A::SSD_VENDOR_REALTEK || (vendor == A::SSD_VENDOR_KINGSTON && v.HostReadsWritesUnit == A::HOST_READS_WRITES_32MB) || vendor == A::SSD_VENDOR_SILICONMOTION))
		|| (at.Id == 0xAD && in({ A::SSD_VENDOR_TOSHIBA, A::SSD_VENDOR_KIOXIA }))
		|| (at.Id == 0xB1 && vendor == A::SSD_VENDOR_SAMSUNG)
		|| (at.Id == 0xBB && vendor == A::SSD_VENDOR_MTRON)
		|| (at.Id == 0xCA && in({ A::SSD_VENDOR_MICRON, A::SSD_VENDOR_MICRON_MU03, A::SSD_VENDOR_INTEL_DC, A::SSD_VENDOR_SILICONMOTION_CVC }))
		|| (at.Id == 0xD1 && vendor == A::SSD_VENDOR_INDILINX)
		|| (at.Id == 0xE7 && in({ A::SSD_VENDOR_SANDFORCE, A::SSD_VENDOR_CORSAIR, A::SSD_VENDOR_KINGSTON, A::SSD_VENDOR_SKHYNIX, A::SSD_VENDOR_REALTEK,
		                          A::SSD_VENDOR_SANDISK, A::SSD_VENDOR_SSSTC, A::SSD_VENDOR_APACER, A::SSD_VENDOR_JMICRON, A::SSD_VENDOR_SEAGATE,
		                          A::SSD_VENDOR_MAXIOTEK, A::SSD_VENDOR_YMTC, A::SSD_VENDOR_SCY, A::SSD_VENDOR_RECADATA, A::SSD_VENDOR_ADATA_INDUSTRIAL }))
		|| (at.Id == 0xE8 && vendor == A::SSD_VENDOR_PLEXTOR)
		|| (at.Id == 0xE9 && in({ A::SSD_VENDOR_INTEL, A::SSD_VENDOR_OCZ, A::SSD_VENDOR_OCZ_VECTOR, A::SSD_VENDOR_SKHYNIX, A::SSD_VENDOR_SANDISK_LENOVO_HELEN_VENUS }))
		|| (at.Id == 0xE6 && in({ A::SSD_VENDOR_SANDISK_LENOVO, A::SSD_VENDOR_SANDISK_DELL }))
		|| (at.Id == 0xC9 && in({ A::SSD_VENDOR_SANDISK_HP, A::SSD_VENDOR_SANDISK_HP_VENUS }));
	if (lifeAttr)
	{
		if (v.FlagLifeRawValue || v.FlagLifeRawValueIncrement) return AttrIcon::Good;
		if (at.CurrentValue == 0 || at.CurrentValue < th.ThresholdValue) return AttrIcon::Bad;
		if (at.CurrentValue <= v.ThresholdFF) return AttrIcon::Caution;
		return AttrIcon::Good;
	}
	if (at.Id == 0x01 && vendor == A::SSD_VENDOR_SANDFORCE)
	{
		if (at.CurrentValue == 0 && at.RawValue[0] == 0 && at.RawValue[1] == 0) return AttrIcon::Good;
		if (th.ThresholdValue != 0 && at.CurrentValue < th.ThresholdValue) return AttrIcon::Bad;
		return AttrIcon::Good;
	}
	const int id = at.Id;
	if ((v.IsSsd && !v.IsRawValues8)
	 || (0x01 <= id && id <= 0x0D) || id == 0x16 || (0xBB <= id && id <= 0xC1) || (0xC3 <= id && id <= 0xD1)
	 || (0xD3 <= id && id <= 0xD4) || (0xDC <= id && id <= 0xE4) || (0xE6 <= id && id <= 0xE7) || id == 0xF0 || id == 0xFA || id == 0xFE)
	{
		if (th.ThresholdValue != 0 && at.CurrentValue < th.ThresholdValue) return AttrIcon::Bad;
	}
	(void)ata;
	return AttrIcon::Good;
}

void DiskData::refresh(const std::function<DiskThresholds(const QString&)>& thresholds, bool)
{
	CAtaSmart& ata = d->ata;
	ata.Init(FALSE, FALSE, NULL, FALSE, FALSE, FALSE, FALSE, FALSE);

	m_errors.clear();
	for (const auto& disk : LinuxDisks())
		if (!disk.error.empty()) m_errors << QString::fromStdString(disk.devnode + ": " + disk.error);

	m_disks.clear();
	for (INT_PTR i = 0; i < ata.vars.GetCount(); i++)
	{
		auto& v = ata.vars[i];
		const DiskThresholds t = thresholds(Q(v.ModelSerial));
		v.Threshold05 = (WORD)t.caution05;
		v.ThresholdC5 = (WORD)t.cautionC5;
		v.ThresholdC6 = (WORD)t.cautionC6;
		v.ThresholdFF = (WORD)t.cautionFF;
		v.AlarmTemperature = t.alarmTemperature >= 0 ? t.alarmTemperature : (v.IsSsd ? 60 : 50);
		v.DiskStatus = ata.CheckDiskStatus((DWORD)i);

		DiskView dv;
		dv.model = Q(v.Model);
		dv.firmware = Q(v.FirmwareRev);
		dv.serial = Q(v.SerialNumber);
		dv.interfaceName = Q(v.Interface);
		dv.modelSerial = Q(v.ModelSerial);
		dv.smartKey = Q(v.SmartKeyName);
		dv.driveMap = Q(v.DriveMap);
		dv.enclosure = Q(v.Enclosure);
		if (v.PhysicalDriveId >= 0 && v.PhysicalDriveId < (INT)LinuxDisks().size())
			dv.devnode = QString::fromStdString(LinuxDisks()[(size_t)v.PhysicalDriveId].devnode);

		if (!v.CurrentTransferMode.IsEmpty() || !v.MaxTransferMode.IsEmpty())
			dv.transferMode = Q(v.CurrentTransferMode) + " | " + Q(v.MaxTransferMode);
		dv.standard = v.MinorVersion.IsEmpty() ? Q(v.MajorVersion) : Q(v.MajorVersion) + " | " + Q(v.MinorVersion);

		QStringList f;
		if (v.IsSmartSupported) f << "S.M.A.R.T.";
		if (v.IsApmSupported) f << "APM";
		if (v.IsAamSupported) f << "AAM";
		if (v.IsNcqSupported) f << "NCQ";
		if (v.IsTrimSupported) f << "TRIM";
		if (v.IsDeviceSleepSupported) f << "DevSleep";
		if (v.IsStreamingSupported) f << "Streaming";
		if (v.IsGplSupported) f << "GPL";
		if (v.IsVolatileWriteCachePresent) f << "VolatileWriteCache";
		dv.features = f.join(", ");

		dv.totalDiskSizeMB = v.TotalDiskSize;
		switch (v.DiskStatus)
		{
		case CAtaSmart::DISK_STATUS_GOOD: dv.health = Health::Good; break;
		case CAtaSmart::DISK_STATUS_CAUTION: dv.health = Health::Caution; break;
		case CAtaSmart::DISK_STATUS_BAD: dv.health = Health::Bad; break;
		default: dv.health = Health::Unknown; break;
		}
		dv.life = v.Life;
		dv.temperature = v.Temperature;
		for (int s = 0; s < 8; s++)
		{
			int temp = -1000;
			if (v.IsNVMe)
			{
				temp = v.SmartReadData[200 + s * 2 + 1] * 256 + v.SmartReadData[200 + s * 2] - 273;
				if (temp == -273 || temp > 200) temp = -1000;
			}
			dv.nvmeSensors << temp;
		}
		dv.alarmTemperature = v.AlarmTemperature;
		dv.isNvme = v.IsNVMe;
		dv.isSsd = v.IsSsd;
		dv.smartEnabled = v.IsSmartEnabled;
		dv.smartCorrect = v.IsSmartCorrect;
		dv.powerOnCount = (int)v.PowerOnCount;
		const bool measured = v.MeasuredPowerOnHours > 0;
		dv.powerOnHours = measured ? v.MeasuredPowerOnHours : v.DetectedPowerOnHours;
		const DWORD unit = measured ? v.MeasuredTimeUnitType : v.DetectedTimeUnitType;
		dv.powerOnHoursUncertain = unit == CAtaSmart::POWER_ON_MINUTES && v.IsMaxtorMinute;
		dv.hostReads = v.HostReads;
		dv.hostWrites = v.HostWrites;
		dv.nandWrites = v.NandWrites;
		dv.gbytesErased = v.GBytesErased;
		dv.bufferSize = (int)v.BufferSize;
		dv.nvCacheSize = v.NvCacheSize;
		dv.rotationRate = (int)v.NominalMediaRotationRate;

		if (v.DiskVendorId == CAtaSmart::SSD_VENDOR_NVME)
		{
			dv.showCurrent = dv.showWorst = dv.showThreshold = false;
			dv.rawLayout = RawLayout::Nvme;
		}
		else if (v.DiskVendorId == CAtaSmart::SSD_VENDOR_INDILINX)
		{
			dv.showCurrent = dv.showWorst = dv.showThreshold = false;
			dv.rawLayout = RawLayout::Ata8;
		}
		else if (v.DiskVendorId == CAtaSmart::SSD_VENDOR_JMICRON && v.IsRawValues8)
		{
			dv.showWorst = dv.showThreshold = false;
			dv.rawLayout = RawLayout::Ata8;
		}
		else if (v.IsRawValues8)
			dv.rawLayout = RawLayout::Ata8;
		else if (v.DiskVendorId == CAtaSmart::SSD_VENDOR_SANDFORCE || v.IsRawValues7)
			dv.rawLayout = RawLayout::Reserved7;

		for (DWORD j = 0; j < v.AttributeCount; j++)
		{
			const auto& at = v.Attribute[j];
			if (at.Id == 0x00 || at.Id == 0xFF) continue;
			SmartAttr a;
			a.id = at.Id;
			a.current = at.CurrentValue;
			a.worst = at.WorstValue;
			a.threshold = v.Threshold[j].ThresholdValue;
			a.thresholdValid = v.IsThresholdCorrect;
			for (int b = 0; b < 6; b++) a.raw[(size_t)b] = at.RawValue[b];
			a.reserved = at.Reserved;
			a.icon = IconFor(ata, v, j);
			dv.attributes << a;
		}
		m_disks << dv;
	}
}

// Raw Values display modes from upstream (Function > Advanced Feature > Raw Values).
QString DiskData::formatRaw(const SmartAttr& a, RawLayout layout, int mode)
{
	const auto& r = a.raw;
	auto w = [](quint8 lo, quint8 hi) { return (int)(lo | (hi << 8)); };
	switch (layout)
	{
	case RawLayout::Ata8:
		switch (mode)
		{
		case 3: return QString::asprintf("%d %d %d %d %d %d %d %d", r[5], r[4], r[3], r[2], r[1], r[0], a.worst, a.current);
		case 2: return QString::asprintf("%d %d %d %d", w(r[4], r[5]), w(r[2], r[3]), w(r[0], r[1]), w(a.current, a.worst));
		case 1:
			return QString::number(((quint64)r[5] << 56) | ((quint64)r[4] << 48) | ((quint64)r[3] << 40) | ((quint64)r[2] << 32)
			                     | ((quint64)r[1] << 24) | ((quint64)r[0] << 16) | ((quint64)a.worst << 8) | (quint64)a.current);
		default: return QString::asprintf("%02X%02X%02X%02X%02X%02X%02X%02X", r[5], r[4], r[3], r[2], r[1], r[0], a.worst, a.current);
		}
	case RawLayout::Reserved7:
	case RawLayout::Nvme:
		switch (mode)
		{
		case 3: return QString::asprintf("%d %d %d %d %d %d %d", a.reserved, r[5], r[4], r[3], r[2], r[1], r[0]);
		case 2: return QString::asprintf("%d %d %d %d", a.reserved, w(r[4], r[5]), w(r[2], r[3]), w(r[0], r[1]));
		case 1:
			return QString::number(((quint64)a.reserved << 48) | ((quint64)r[5] << 40) | ((quint64)r[4] << 32) | ((quint64)r[3] << 24)
			                     | ((quint64)r[2] << 16) | ((quint64)r[1] << 8) | (quint64)r[0]);
		default: return QString::asprintf("%02X%02X%02X%02X%02X%02X%02X", a.reserved, r[5], r[4], r[3], r[2], r[1], r[0]);
		}
	case RawLayout::Ata6:
	default:
		switch (mode)
		{
		case 3: return QString::asprintf("%d %d %d %d %d %d", r[5], r[4], r[3], r[2], r[1], r[0]);
		case 2: return QString::asprintf("%d %d %d", w(r[4], r[5]), w(r[2], r[3]), w(r[0], r[1]));
		case 1:
			return QString::number(((quint64)r[5] << 40) | ((quint64)r[4] << 32) | ((quint64)r[3] << 24) | ((quint64)r[2] << 16)
			                     | ((quint64)r[1] << 8) | (quint64)r[0]);
		default: return QString::asprintf("%02X%02X%02X%02X%02X%02X", r[5], r[4], r[3], r[2], r[1], r[0]);
		}
	}
}
