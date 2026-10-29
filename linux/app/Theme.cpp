#include "Theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>

// Mirrors the #ifdef blocks in upstream DiskInfoDlg.cpp / DiskInfo.h.
const QList<Edition>& Edition::all()
{
	static const QList<Edition> editions = {
		{ "standard", "Standard Edition",  "Background",         "Default",  "Default",                                  "",                  "",        false },
		{ "shizuku",  "Shizuku Edition",   "ShizukuBackground",  "Shizuku",  "ShizukuLightAnimalEars~TenmuShinryuusai",  "ShizukuCopyright",  "Shizuku", true },
		{ "kureikei", "Kurei Kei Edition", "KureiKeiBackground", "KureiKei", "KureiKeiRecoding",                         "KureiKeiCopyright", "KureiKei", true },
		{ "aoi",      "Aoi Edition",       "AoiBackground",      "Aoi",      "AoiLightAnimalEars~TenmuShinryuusai",      "AoiCopyright",      "Aoi",     true },
	};
	return editions;
}

const Edition& Edition::byId(const QString& id)
{
	for (const Edition& e : all())
		if (e.id == id) return e;
	return all().first();
}

// theme.ini is not a strict INI: values look like "0x1A2B3C;" and the
// section header may carry a comment ("[Color];RGB").
static QHash<QString, QString> ReadThemeIni(const QString& path)
{
	QHash<QString, QString> out;
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) return out;
	QByteArray raw = f.readAll();
	QString text = raw.startsWith("\xFF\xFE") ? QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData() + 2), (raw.size() - 2) / 2)
	                                          : QString::fromUtf8(raw);
	QString section;
	for (QString line : text.split('\n'))
	{
		line = line.trimmed();
		if (line.startsWith('['))
		{
			section = line.mid(1, line.indexOf(']') - 1);
			continue;
		}
		int eq = line.indexOf('=');
		if (eq <= 0 || line.startsWith(';')) continue;
		QString value = line.mid(eq + 1);
		int semi = value.indexOf(';');
		if (semi >= 0) value.truncate(semi);
		out.insert(section + '/' + line.left(eq).trimmed(), value.trimmed());
	}
	return out;
}

static QColor IniColor(const QHash<QString, QString>& ini, const QString& key, int gray)
{
	bool ok = false;
	QString v = ini.value("Color/" + key);
	uint rgb = v.startsWith("0x", Qt::CaseInsensitive) ? v.mid(2).toUInt(&ok, 16) : v.toUInt(&ok);
	if (!ok) return QColor(gray, gray, gray);
	return QColor((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

static int IniInt(const QHash<QString, QString>& ini, const QString& key, int def)
{
	bool ok = false;
	int v = ini.value(key).toInt(&ok);
	return ok ? v : def;
}

void Theme::load(const QString& name, const Edition& preferred)
{
	m_cache.clear();
	m_name = name;
	auto ini = ReadThemeIni(QDir(m_root).filePath(name + "/theme.ini"));

	labelText = IniColor(ini, "LabelText", 0);
	buttonText = IniColor(ini, "ButtonText", 0);
	listText1 = IniColor(ini, "ListText1", 0);
	listText2 = IniColor(ini, "ListText2", 0);
	listTextSelected = IniColor(ini, "ListTextSelected", 0);
	listBk1 = IniColor(ini, "ListBk1", 255);
	listBk2 = IniColor(ini, "ListBk2", 255);
	listBkSelected = IniColor(ini, "ListBkSelected", 0);
	listLine1 = IniColor(ini, "ListLine1", 0);
	listLine2 = IniColor(ini, "ListLine2", 0);
	glass = IniColor(ini, "Glass", 255);
	frame = IniColor(ini, "Frame", 128);
	background = ini.contains("Color/Background") ? IniColor(ini, "Background", 255) : QColor();
	glassAlpha = IniInt(ini, "Alpha/GlassAlpha", 128);
	characterPosition = IniInt(ini, "Character/Position", 0);
	m_parent1 = ini.value("Info/ParentTheme1");
	m_parent2 = ini.value("Info/ParentTheme2");

	// Pick the edition whose background this theme (or its parents) provides.
	m_edition = &preferred;
	const QStringList chain = { name, m_parent1, m_parent2 };
	for (const QString& dir : chain)
	{
		if (dir.isEmpty()) continue;
		for (const Edition& e : Edition::all())
		{
			if (!e.character) continue;
			if (QFileInfo::exists(QDir(m_root).filePath(dir + "/" + e.background + "-300.png"))
			 || QFileInfo::exists(QDir(m_root).filePath(dir + "/" + e.background + "-100.png")))
			{
				m_edition = &e;
				return;
			}
		}
	}
	if (m_edition->character) m_edition = &Edition::byId("standard");
}

QStringList Theme::searchChain() const
{
	QStringList chain;
	for (const QString& t : { m_name, m_parent1, m_parent2, m_edition ? m_edition->defaultTheme : QString(), QStringLiteral("Default") })
		if (!t.isEmpty() && !chain.contains(t)) chain << t;
	return chain;
}

QPixmap Theme::image(const QString& base) const
{
	auto it = m_cache.constFind(base);
	if (it != m_cache.constEnd()) return *it;

	QPixmap pm;
	static const char* zooms[] = { "300", "250", "200", "150", "125", "100" };
	for (const QString& t : searchChain())
	{
		for (const char* z : zooms)
		{
			QString path = QDir(m_root).filePath(QString("%1/%2-%3.png").arg(t, base, z));
			if (QFileInfo::exists(path) && pm.load(path))
			{
				// Remember the source scale so callers can map pixels to layout units.
				pm.setDevicePixelRatio(QString(z).toDouble() / 100.0);
				m_cache.insert(base, pm);
				return pm;
			}
		}
	}
	m_cache.insert(base, pm);
	return pm;
}

QStringList Theme::list() const
{
	QStringList out;
	for (const QString& d : QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase))
		if (QFileInfo::exists(QDir(m_root).filePath(d + "/theme.ini"))) out << d;
	return out;
}
