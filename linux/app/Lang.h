// CrystalDiskInfo .lang files (UTF-16 INI) with English fallback.
#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

class Lang
{
public:
	bool load(const QString& dir, const QString& name);

	// Upstream i18n(section, key): current language, then English.
	QString tr(const QString& section, const QString& key) const;
	// Menu text with Windows '&' accelerators turned into Qt ones (same syntax).
	QString menu(const QString& key) const { return tr("Menu", key); }

	QString name() const { return m_name; }
	static QStringList available(const QString& dir);

private:
	using Table = QHash<QString, QString>;   // "Section\nKEY" -> value
	static Table parse(const QString& path);

	Table m_current;
	Table m_english;
	QString m_name;
};
