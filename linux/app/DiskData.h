// Bridge between upstream CAtaSmart (Windows-flavoured types) and the Qt UI.
// Only DiskData.cpp includes AtaSmart.h, so the shim never leaks into Qt code.
#pragma once

#include <QList>
#include <QSettings>
#include <QString>
#include <array>
#include <functional>
#include <memory>

class Lang;

enum class Health { Good, Caution, Bad, Unknown };
enum class AttrIcon { Good, Caution, Bad, Unknown };
enum class RawLayout { Ata6, Ata8, Reserved7, Nvme };

struct SmartAttr
{
	int id = 0;
	int current = 0, worst = 0, threshold = 0;
	bool thresholdValid = true;
	std::array<quint8, 6> raw{};
	quint8 reserved = 0;
	AttrIcon icon = AttrIcon::Unknown;
};

struct DiskView
{
	QString model, firmware, serial, interfaceName, transferMode, standard, features, driveMap;
	QString devnode, modelSerial, smartKey;
	quint32 totalDiskSizeMB = 0;
	Health health = Health::Unknown;
	int life = -1;
	int temperature = -1000;
	QList<int> nvmeSensors;          // Temperature Sensor 1..8 (°C), -1000 = absent
	int alarmTemperature = 50;
	bool isNvme = false, isSsd = false, smartEnabled = false, smartCorrect = false;
	int powerOnCount = 0;
	int powerOnHours = -1;
	bool powerOnHoursUncertain = false;
	int hostReads = -1, hostWrites = -1, nandWrites = -1, gbytesErased = -1;
	int bufferSize = 0;              // bytes
	quint64 nvCacheSize = 0;         // bytes
	int rotationRate = 0;            // 1 = SSD
	QString enclosure;

	// Columns: which of Current/Worst/Threshold are meaningful.
	bool showCurrent = true, showWorst = true, showThreshold = true;
	RawLayout rawLayout = RawLayout::Ata6;
	QList<SmartAttr> attributes;
};

// Per-disk health thresholds, stored like upstream under ModelSerial keys.
struct DiskThresholds
{
	int caution05 = 1, cautionC5 = 1, cautionC6 = 1, cautionFF = 10;
	int alarmTemperature = -1;   // -1 = default (HDD 50, SSD 60)
};

class DiskData
{
public:
	DiskData();
	~DiskData();

	// Runs CAtaSmart::Init on a fresh snapshot and evaluates health with the
	// per-disk thresholds looked up through `thresholds`.
	void refresh(const std::function<DiskThresholds(const QString& modelSerial)>& thresholds, bool greenMode);

	const QList<DiskView>& disks() const { return m_disks; }
	QStringList captureErrors() const { return m_errors; }

	static QString formatRaw(const SmartAttr& a, RawLayout layout, int mode);

private:
	struct Impl;
	std::unique_ptr<Impl> d;
	QList<DiskView> m_disks;
	QStringList m_errors;
};
