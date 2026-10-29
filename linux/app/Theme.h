// Themes (CdiResource/themes/<name>/theme.ini + *-NNN.png) and editions.
//
// Upstream builds a separate executable per edition (Standard, Shizuku,
// Kurei Kei, Aoi, ...). Here an edition is just a layout that follows the
// selected theme: a theme that ships e.g. ShizukuBackground-300.png switches
// the window to the Shizuku character layout.
#pragma once

#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QString>
#include <QStringList>

struct Edition
{
	QString id;              // "standard", "shizuku", ...
	QString name;            // "Shizuku Edition"
	QString background;      // background image base name
	QString defaultTheme;    // fallback theme for missing images
	QString recommendTheme;  // first-run theme
	QString copyright;       // image shown under the character
	QString voicePrefix;     // voice clip directory under CdiResource/voice
	bool character = false;  // 1000px layout with a character on one side

	static const QList<Edition>& all();
	static const Edition& byId(const QString& id);
};

class Theme
{
public:
	void setRoot(const QString& themesDir) { m_root = themesDir; }
	QString root() const { return m_root; }

	// Loads theme.ini of `name` and decides which edition layout applies.
	void load(const QString& name, const Edition& preferred);

	QString name() const { return m_name; }
	const Edition& edition() const { return *m_edition; }

	// IP(): image by base name, best resolution, searched in
	// current -> ParentTheme1 -> ParentTheme2 -> edition default -> Default.
	QPixmap image(const QString& base) const;
	bool hasImage(const QString& base) const { return !image(base).isNull(); }

	QStringList list() const;

	QColor labelText, buttonText, listText1, listText2, listTextSelected;
	QColor listBk1, listBk2, listBkSelected, listLine1, listLine2;
	QColor glass, frame, background;
	int glassAlpha = 128;
	int characterPosition = 0;   // 0 = character on the left, 1 = right

private:
	QStringList searchChain() const;

	QString m_root;
	QString m_name;
	QString m_parent1, m_parent2;
	const Edition* m_edition = nullptr;
	mutable QHash<QString, QPixmap> m_cache;
};
