#include "MainWindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QAudioOutput>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMediaPlayer>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextStream>
#include <QToolTip>
#include <QUrl>
#include <QWheelEvent>
#include <QPushButton>
#include <QLocale>
#include <functional>

#include <unistd.h>

#include "LinuxDisk.h"

static const char* kVersion = "9.9.3";

// ---------------------------------------------------------------------------
// Canvas: everything above the S.M.A.R.T. list, drawn like upstream's owner-
// drawn controls (background image, glass boxes, image buttons with text).
// Coordinates are upstream's 100% layout units; the painter scales by zoom.
// ---------------------------------------------------------------------------

class Canvas : public QWidget
{
public:
	explicit Canvas(MainWindow* w) : QWidget(w), m(w)
	{
		setMouseTracking(true);
		setAttribute(Qt::WA_OpaquePaintEvent);
	}

	struct Hit
	{
		QRectF rect;
		std::function<void()> click;
		QString tip;
	};

protected:
	void paintEvent(QPaintEvent*) override;
	void mouseMoveEvent(QMouseEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override;
	void leaveEvent(QEvent*) override { m_hover = -1; update(); }
	void wheelEvent(QWheelEvent* e) override;
	bool event(QEvent* e) override;

private:
	QPointF toLayout(const QPointF& p) const { return p / m->zoom(); }
	int hitAt(const QPointF& p) const;
	void drawImage(QPainter& p, const QRectF& r, const QString& name, int state = 0) const;
	void drawText(QPainter& p, const QRectF& r, const QString& text, int px, Qt::Alignment align, const QColor& color, bool bold = false) const;
	void drawGlass(QPainter& p, const QRectF& r, const QString& text, Qt::Alignment align) const;
	QFont font(int px, bool bold = false) const;

	MainWindow* m;
	QList<Hit> m_hits;
	int m_hover = -1;
};

QFont Canvas::font(int px, bool bold) const
{
	QFont f = QApplication::font();
	QString face = m->m_settings.value("Setting/FontFace").toString();
	if (!face.isEmpty()) f.setFamily(face);
	f.setPixelSize(px);
	f.setBold(bold);
	return f;
}

void Canvas::drawImage(QPainter& p, const QRectF& r, const QString& name, int state) const
{
	QPixmap pm = m->theme().image(name);
	if (pm.isNull()) return;
	// Sprites stack their states vertically: normal, hover, focus, selected.
	const qreal dpr = pm.devicePixelRatio();
	const int frameH = qRound(r.height() * dpr);
	const int frames = frameH > 0 ? std::max(1, pm.height() / frameH) : 1;
	if (state >= frames) state = state == 3 && frames > 1 ? 1 : 0;
	QRectF src(0, state * frameH, pm.width(), frames > 1 ? frameH : pm.height());
	p.drawPixmap(r, pm, src);
}

void Canvas::drawText(QPainter& p, const QRectF& r, const QString& text, int px, Qt::Alignment align, const QColor& color, bool bold) const
{
	p.setFont(font(px, bold));
	p.setPen(color);
	p.drawText(r, align | Qt::TextWordWrap, text);
}

void Canvas::drawGlass(QPainter& p, const QRectF& r, const QString& text, Qt::Alignment align) const
{
	const Theme& t = m->theme();
	QColor g = t.glass;
	g.setAlpha(t.glassAlpha);
	p.fillRect(r, g);
	QColor f = t.frame;
	f.setAlpha(std::max(64, t.glassAlpha));
	p.setPen(QPen(f, 1.0 / m->zoom()));
	p.drawRect(r.adjusted(0, 0, -1.0 / m->zoom(), -1.0 / m->zoom()));
	QFontMetricsF fm(font(12));
	p.setFont(font(12));
	p.setPen(t.labelText);
	QRectF tr = r.adjusted(4, 0, -4, 0);
	p.drawText(tr, align | Qt::AlignVCenter, fm.elidedText(text, Qt::ElideRight, tr.width()));
}

void Canvas::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	p.setRenderHint(QPainter::Antialiasing);
	p.setRenderHint(QPainter::TextAntialiasing);
	const Theme& t = m->theme();
	const Edition& ed = t.edition();

	p.fillRect(rect(), t.background.isValid() ? t.background : t.listBk1);
	p.scale(m->zoom(), m->zoom());

	// Background (the character editions bake the character into it).
	QPixmap bg = t.image(ed.background);
	if (!bg.isNull())
	{
		QSizeF s = bg.deviceIndependentSize();
		p.drawPixmap(QRectF(QPointF(0, 0), s), bg, bg.rect());
	}

	m_hits.clear();
	const int ox = m->offsetX();
	const QList<DiskView>& disks = m->m_data.disks();
	const DiskView* d = disks.isEmpty() ? nullptr : &disks[m->m_selected];

	// --- drive bar -----------------------------------------------------------
	const bool half = disks.size() > 8;
	const int slotCount = half ? 16 : 8;
	const int slotW = half ? 42 : 84;
	const QString mini = half ? "Mini" : "";
	for (int s = 0; s < slotCount; s++)
	{
		int i = m->m_drivePage * slotCount + s;
		QRectF r(ox + s * slotW, 0, slotW, 48);
		if (i >= disks.size())
		{
			drawImage(p, r, "noDisk" + mini);
			continue;
		}
		const DiskView& dv = disks[i];
		int hitIndex = m_hits.size();
		int state = i == m->m_selected ? 3 : (m_hover == hitIndex ? 1 : 0);
		drawImage(p, r, m->diskStatusClass(dv.health).replace("diskStatus", "disk") + mini, state);

		QString letter = dv.driveMap.isEmpty() ? QFileInfo(dv.devnode).fileName() : dv.driveMap;
		if (letter.size() > 15) letter = letter.left(11) + " ...";
		QString temp = dv.temperature > -300 ? m->temperatureText(dv.temperature) : (m->m_fahrenheit ? "-- °F" : "-- °C");
		// Compact slots: upstream shows the bare drive letter; use the mount's
		// last path component ("/", "home", "ssd1") or the device name.
		QString shortName = QFileInfo(dv.devnode).fileName();
		if (!dv.driveMap.isEmpty())
		{
			const QString first = dv.driveMap.section(' ', 0, 0);
			shortName = first == "/" ? first : QFileInfo(first).fileName();
		}
		shortName = QFontMetricsF(font(10)).elidedText(shortName, Qt::ElideRight, slotW - 4);
		QString text = half ? QString(" \n%1\n%2").arg(temp, shortName)
		                    : QString("%1\n%2\n%3").arg(m->diskStatusText(dv.health), temp, letter);
		drawText(p, r.adjusted(3, 0, -3, 0), text, half ? 10 : 12, Qt::AlignCenter, t.labelText);

		QString tip = QString("(%1) %2\n%3").arg(i + 1).arg(dv.model, dv.devnode);
		if (!dv.driveMap.isEmpty()) tip += "\n" + dv.driveMap;
		m_hits << Hit{ r, [this, i] { m->selectDisk(i); }, tip };
	}

	// --- model line ------------------------------------------------------------
	QString model;
	if (d)
	{
		QString cap = d->totalDiskSizeMB >= 1000 ? QString::asprintf("%.1f GB", d->totalDiskSizeMB / 1000.0)
		            : d->totalDiskSizeMB > 0 ? QString("%1 MB").arg(d->totalDiskSizeMB) : m->m_lang.tr("Dialog", "UNKNOWN");
		model = d->model + " " + cap;
	}
	else
	{
		model = m->m_lang.tr("Message", "DISK_NOT_FOUND");
	}
	QFontMetricsF fm(font(ed.character ? 20 : 24));
	QRectF modelRect(32 + ox, 52, 608, 32);
	drawText(p, modelRect, fm.elidedText(model, Qt::ElideRight, modelRect.width()), ed.character ? 20 : 24, Qt::AlignCenter, t.labelText);
	if (d) m_hits << Hit{ modelRect, nullptr, model + (d->enclosure.isEmpty() ? QString() : "\n" + d->enclosure) };

	{
		QRectF pre(8 + ox, 56, 24, 24), next(640 + ox, 56, 24, 24);
		int hi = m_hits.size();
		drawImage(p, pre, "preDisk", m_hover == hi ? 1 : 0);
		m_hits << Hit{ pre, [this] { if (!m->m_data.disks().isEmpty()) m->selectDisk((m->m_selected + m->m_data.disks().size() - 1) % m->m_data.disks().size()); }, QString() };
		drawImage(p, next, "nextDisk", m_hover == hi + 1 ? 1 : 0);
		m_hits << Hit{ next, [this] { if (!m->m_data.disks().isEmpty()) m->selectDisk((m->m_selected + 1) % m->m_data.disks().size()); }, QString() };
	}

	// --- health / temperature --------------------------------------------------
	const Lang& L = m->m_lang;
	QString statusText, statusClass = "diskStatusUnknown", tempText = m->m_fahrenheit ? "-- °F" : "-- °C", tempClass = "temperatureUnknown";
	QString tempTip;
	if (d)
	{
		statusClass = m->diskStatusClass(d->health);
		statusText = m->diskStatusText(d->health);
		if (d->life >= 0) statusText += ed.character ? QString(" (%1 %)").arg(d->life) : QString("\n%1 %").arg(d->life);
		if (d->temperature > -300) tempText = m->temperatureText(d->temperature);
		tempClass = m->temperatureClass(*d);
		for (int s = 0; s < d->nvmeSensors.size(); s++)
			if (d->nvmeSensors[s] > -270)
				tempTip += L.tr("SmartNVMe", QString::number(s + 0x12, 16).toUpper()) + ": " + m->temperatureText(d->nvmeSensors[s]) + "\n";
	}
	auto openHealth = [this] { m->editHealthSettings(); };
	if (ed.character)
	{
		QString sd = statusClass;
		sd.replace("Green", "");
		if (d && d->health == Health::Good && d->life == 100) sd += "100";
		QRectF lifeRect(ox, 88, 128, 192);
		drawImage(p, lifeRect, "SD" + sd);
		m_hits << Hit{ lifeRect, [this] { m->playVoice(); }, QString() };

		drawText(p, QRectF(128 + ox, 260, 124, 20), L.tr("Dialog", "HEALTH_STATUS"), 12, Qt::AlignRight | Qt::AlignVCenter, t.labelText);
		QRectF sr(256 + ox, 256, 180, 28);
		drawImage(p, sr, statusClass);
		drawText(p, sr, statusText, 14, Qt::AlignCenter, t.buttonText, true);
		m_hits << Hit{ sr, openHealth, QString() };

		drawText(p, QRectF(436 + ox, 260, 124, 20), L.tr("Dialog", "TEMPERATURE"), 12, Qt::AlignRight | Qt::AlignVCenter, t.labelText);
		QRectF tr(564 + ox, 256, 100, 28);
		drawImage(p, tr, tempClass);
		drawText(p, tr, tempText, 14, Qt::AlignCenter, t.buttonText, true);
		m_hits << Hit{ tr, openHealth, tempTip.trimmed() };
	}
	else
	{
		drawText(p, QRectF(8 + ox, 88, 100, 20), L.tr("Dialog", "HEALTH_STATUS"), 12, Qt::AlignCenter, t.labelText);
		QRectF sr(8 + ox, 112, 100, 60);
		drawImage(p, sr, statusClass);
		drawText(p, sr.adjusted(4, 0, -4, 0), statusText, 16, Qt::AlignCenter, t.buttonText, true);
		m_hits << Hit{ sr, openHealth, QString() };

		drawText(p, QRectF(8 + ox, 184, 100, 20), L.tr("Dialog", "TEMPERATURE"), 12, Qt::AlignCenter, t.labelText);
		QRectF tr(8 + ox, 208, 100, 40);
		drawImage(p, tr, tempClass);
		drawText(p, tr, tempText, 16, Qt::AlignCenter, t.buttonText, true);
		m_hits << Hit{ tr, openHealth, tempTip.trimmed() };
	}

	// --- information grid -----------------------------------------------------
	struct Row { QString label; QString value; QString tip; };
	QString serial;
	if (d) serial = m->m_hideSerial ? QString(d->serial.size(), '*') : d->serial;
	QList<Row> left = {
		{ L.tr("Dialog", "FIRMWARE"), d ? d->firmware : "", "" },
		{ L.tr("Dialog", "SERIAL_NUMBER"), serial, "" },
		{ L.tr("Dialog", "INTERFACE"), d ? d->interfaceName : "", "" },
		{ L.tr("Dialog", "TRANSFER_MODE"), d ? d->transferMode : "", L.tr("Dialog", "CURRENT_MODE_SUPPORTED_MODE") },
		{ L.tr("Dialog", "DRIVE_LETTER"), d ? d->driveMap : "", d ? d->driveMap : "" },
		{ L.tr("Dialog", "STANDARD"), d ? d->standard : "", L.tr("Dialog", "MAJOR_VERSION_MINOR_VERSION") },
		{ L.tr("Dialog", "FEATURE"), d ? d->features : "", "" },
	};
	const int labelW = 124;
	for (int r = 0; r < left.size(); r++)
	{
		int y = 88 + r * 24;
		drawText(p, QRectF(128 + ox, y, labelW, 20), left[r].label, 12, Qt::AlignRight | Qt::AlignVCenter, t.labelText);
		QRectF vr(256 + ox, y, r >= 5 ? 408 : 180, 20);
		drawGlass(p, vr, left[r].value, Qt::AlignLeft);
		QString tip = left[r].tip;
		if (tip.isEmpty() && QFontMetricsF(font(12)).horizontalAdvance(left[r].value) > vr.width() - 8) tip = left[r].value;
		if (!tip.isEmpty()) m_hits << Hit{ vr, nullptr, tip };
	}

	auto big = [](int gb) {
		if (gb > 1024 * 1024) return QString::asprintf("%.3f PB", gb / 1024.0 / 1024.0);
		if (gb > 1024) return QString::asprintf("%.3f TB", gb / 1024.0);
		return gb ? QString("%1 GB").arg(gb) : QString();
	};
	QList<Row> right;
	if (d)
	{
		if (d->hostReads >= 0) right << Row{ L.tr("Dialog", "TOTAL_HOST_READS"), QString("%1 GB").arg(d->hostReads), big(d->hostReads) };
		else if (d->bufferSize == 0xFFFF * 512) right << Row{ L.tr("Dialog", "BUFFER_SIZE"), ">= 32 MB", "" };
		else if (d->bufferSize > 0) right << Row{ L.tr("Dialog", "BUFFER_SIZE"), QString("%1 KB").arg(d->bufferSize / 1024), "" };
		else right << Row{ "----", "----", "" };

		if (d->hostWrites >= 0) right << Row{ L.tr("Dialog", "TOTAL_HOST_WRITES"), QString("%1 GB").arg(d->hostWrites), big(d->hostWrites) };
		else if (d->nvCacheSize > 0) right << Row{ L.tr("Dialog", "NV_CACHE_SIZE"), QString("%1 MB").arg(d->nvCacheSize / 1024 / 1024), "" };
		else right << Row{ "----", "----", "" };

		if (d->nandWrites >= 0) right << Row{ L.tr("Dialog", "TOTAL_NAND_WRITES"), QString("%1 GB").arg(d->nandWrites), big(d->nandWrites) };
		else if (d->gbytesErased >= 0) right << Row{ L.tr("SmartSandForce", "64"), QString("%1 GB").arg(d->gbytesErased), "" };
		else if (d->rotationRate == 1) right << Row{ L.tr("Dialog", "ROTATION_RATE"), "---- (SSD)", "" };
		else if (d->rotationRate > 0) right << Row{ L.tr("Dialog", "ROTATION_RATE"), QString("%1 RPM").arg(d->rotationRate), "" };
		else right << Row{ "----", "----", "" };

		right << Row{ L.tr("Dialog", "POWER_ON_COUNT"),
		              d->powerOnCount > 0 ? QString("%1 %2").arg(d->powerOnCount).arg(L.tr("Dialog", "POWER_ON_COUNT_UNIT")) : L.tr("Dialog", "UNKNOWN"), "" };
		QString hours = L.tr("Dialog", "UNKNOWN"), hoursTip;
		if (d->powerOnHours >= 0)
		{
			hours = QString("%1%2%3").arg(d->powerOnHours).arg(d->powerOnHoursUncertain ? "?" : " ").arg(L.tr("Dialog", "POWER_ON_HOURS_UNIT"));
			int years = d->powerOnHours / (365 * 24), days = (d->powerOnHours - 365 * 24 * years) / 24, h = d->powerOnHours % 24;
			if (years > 0) hoursTip = QString("%1 %2 ").arg(years).arg(L.tr("Dialog", "POWER_ON_YEARS_UNIT"));
			hoursTip += QString("%1 %2 %3 %4").arg(days).arg(L.tr("Dialog", "POWER_ON_DAYS_UNIT")).arg(h).arg(L.tr("Dialog", "POWER_ON_HOURS_UNIT"));
		}
		right << Row{ L.tr("Dialog", "POWER_ON_HOURS"), hours, hoursTip };
	}
	else
	{
		for (const char* k : { "BUFFER_SIZE", "NV_CACHE_SIZE", "ROTATION_RATE", "POWER_ON_COUNT", "POWER_ON_HOURS" })
			right << Row{ L.tr("Dialog", k), "", "" };
	}
	for (int r = 0; r < right.size(); r++)
	{
		int y = 88 + r * 24;
		QRectF lr(436 + ox, y, labelW, 20);
		drawText(p, lr, QFontMetricsF(font(12)).elidedText(right[r].label, Qt::ElideRight, labelW), 12, Qt::AlignRight | Qt::AlignVCenter, t.labelText);
		QRectF vr(564 + ox, y, 100, 20);
		drawGlass(p, vr, right[r].value, Qt::AlignRight);
		if (!right[r].tip.isEmpty()) m_hits << Hit{ vr, nullptr, right[r].tip };
		if (QFontMetricsF(font(12)).horizontalAdvance(right[r].label) > labelW) m_hits << Hit{ lr, nullptr, right[r].label };
	}

	// --- character editions: voice area and copyright ----------------------------
	if (ed.character)
	{
		const int cx = m->characterX();
		const qreal h = height() / m->zoom();
		m_hits << Hit{ QRectF(cx, 48, 328, h - 24 - 48), [this] { m->playVoice(); }, QString() };
		if (!ed.copyright.isEmpty())
		{
			QRectF cr(cx, h - 24, 328, 24);
			drawImage(p, cr, ed.copyright);
		}
	}
}

int Canvas::hitAt(const QPointF& pos) const
{
	QPointF lp = toLayout(pos);
	for (int i = m_hits.size() - 1; i >= 0; i--)
		if (m_hits[i].rect.contains(lp)) return i;
	return -1;
}

void Canvas::mouseMoveEvent(QMouseEvent* e)
{
	int h = hitAt(e->position());
	bool clickable = h >= 0 && m_hits[h].click;
	setCursor(clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
	if (h != m_hover)
	{
		m_hover = h;
		update();
	}
}

bool Canvas::event(QEvent* e)
{
	if (e->type() == QEvent::ToolTip)
	{
		auto* he = static_cast<QHelpEvent*>(e);
		int h = hitAt(he->pos());
		if (h >= 0 && !m_hits[h].tip.isEmpty())
			QToolTip::showText(he->globalPos(), m_hits[h].tip, this);
		else
			QToolTip::hideText();
		return true;
	}
	return QWidget::event(e);
}

void Canvas::mousePressEvent(QMouseEvent* e)
{
	if (e->button() != Qt::LeftButton) return;
	int h = hitAt(e->position());
	if (h >= 0 && m_hits[h].click)
	{
		auto click = m_hits[h].click;
		click();
	}
}

void Canvas::wheelEvent(QWheelEvent* e)
{
	// Scroll through drive pages when there are more than 16 disks.
	int pages = (m->m_data.disks().size() + 15) / 16;
	if (pages <= 1 || e->position().y() / m->zoom() > 48) return;
	m->m_drivePage = (m->m_drivePage + (e->angleDelta().y() < 0 ? 1 : pages - 1)) % pages;
	update();
}

// ---------------------------------------------------------------------------
// MainWindow
// ---------------------------------------------------------------------------

MainWindow::MainWindow(const AppPaths& paths, const QString& editionHint, QWidget* parent)
	: QMainWindow(parent),
	  m_paths(paths),
	  m_settings(QSettings::IniFormat, QSettings::UserScope, "CrystalDiskInfo", "DiskInfo")
{
	m_preferredEdition = &Edition::byId(editionHint.isEmpty() ? m_settings.value("Setting/Edition", "standard").toString() : editionHint);
	if (!editionHint.isEmpty()) m_settings.setValue("Setting/Edition", editionHint);

	m_rawValues = m_settings.value("Setting/RawValues", 0).toInt();
	m_fahrenheit = m_settings.value("Setting/Fahrenheit", false).toBool();
	m_hideSerial = m_settings.value("Setting/HideSerialNumber", false).toBool();
	m_greenMode = m_settings.value("Setting/GreenMode", false).toBool();

	m_theme.setRoot(QDir(m_paths.resources).filePath("themes"));

	m_canvas = new Canvas(this);
	setCentralWidget(m_canvas);

	m_table = new QTableWidget(m_canvas);
	m_table->setColumnCount(7);
	m_table->verticalHeader()->hide();
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setShowGrid(false);
	m_table->setWordWrap(false);
	m_table->setFocusPolicy(Qt::NoFocus);
	m_table->horizontalHeader()->setHighlightSections(false);
	m_table->horizontalHeader()->setSectionsClickable(false);

	buildMenus();

	QString lang = m_settings.value("Setting/Language").toString();
	if (lang.isEmpty())
	{
		// Pick the system language when a matching .lang file exists.
		QString sys = QLocale::languageToString(QLocale::system().language());
		lang = Lang::available(QDir(m_paths.resources).filePath("language")).contains(sys) ? sys : "English";
	}
	applyLanguage(lang);

	QString themeName = m_settings.value("Setting/Theme").toString();
	if (themeName.isEmpty() || !m_theme.list().contains(themeName))
	{
		const QStringList themes = m_theme.list();
		themeName = themes.contains(m_preferredEdition->recommendTheme) ? m_preferredEdition->recommendTheme
		          : themes.contains(m_preferredEdition->defaultTheme) ? m_preferredEdition->defaultTheme : "Default";
	}
	applyTheme(themeName);

	connect(&m_autoRefresh, &QTimer::timeout, this, [this] { refresh(false); });
	int minutes = m_settings.value("Setting/AutoRefresh", 10).toInt();
	if (minutes > 0) m_autoRefresh.start(minutes * 60 * 1000);

	m_player = new QMediaPlayer(this);
	m_audio = new QAudioOutput(this);
	m_player->setAudioOutput(m_audio);

	refresh(true);
}

void MainWindow::buildMenus()
{
	QMenuBar* mb = menuBar();
	m_menuFile = mb->addMenu(QString());
	m_actSave = m_menuFile->addAction(QString(), QKeySequence::Save, this, &MainWindow::saveText);
	m_menuFile->addSeparator();
	m_actExit = m_menuFile->addAction(QString(), QKeySequence::Quit, this, &QWidget::close);

	m_menuEdit = mb->addMenu(QString());
	m_actCopy = m_menuEdit->addAction(QString(), QKeySequence::Copy, this, &MainWindow::copyText);

	m_menuFunction = mb->addMenu(QString());
	m_actRefresh = m_menuFunction->addAction(QString(), QKeySequence(Qt::Key_F5), this, [this] { refresh(false); });
	m_actRescan = m_menuFunction->addAction(QString(), QKeySequence(Qt::Key_F6), this, [this] { refresh(true); });
	m_menuAutoRefresh = m_menuFunction->addMenu(QString());
	auto* autoGroup = new QActionGroup(this);
	int current = m_settings.value("Setting/AutoRefresh", 10).toInt();
	for (int minutes : { 0, 1, 3, 5, 10, 30, 60, 120, 180, 360, 720, 1440 })
	{
		QAction* a = m_menuAutoRefresh->addAction(QString());
		a->setData(minutes);
		a->setCheckable(true);
		a->setChecked(minutes == current);
		autoGroup->addAction(a);
		connect(a, &QAction::triggered, this, [this, minutes] {
			m_settings.setValue("Setting/AutoRefresh", minutes);
			if (minutes > 0) m_autoRefresh.start(minutes * 60 * 1000);
			else m_autoRefresh.stop();
		});
	}
	m_menuFunction->addSeparator();
	m_actHealth = m_menuFunction->addAction(QString(), this, &MainWindow::editHealthSettings);
	m_menuAdvanced = m_menuFunction->addMenu(QString());
	m_menuRaw = m_menuAdvanced->addMenu(QString());
	auto* rawGroup = new QActionGroup(this);
	const char* rawNames[] = { "16 [HEX]", "10 [DEC]", "2byte [DEC]", "1byte [DEC]" };
	for (int i = 0; i < 4; i++)
	{
		QAction* a = m_menuRaw->addAction(rawNames[i]);
		a->setCheckable(true);
		a->setChecked(i == m_rawValues);
		rawGroup->addAction(a);
		connect(a, &QAction::triggered, this, [this, i] {
			m_rawValues = i;
			m_settings.setValue("Setting/RawValues", i);
			updateTable();
		});
	}
	m_actHideSerial = m_menuFunction->addAction(QString());
	m_actHideSerial->setCheckable(true);
	m_actHideSerial->setChecked(m_hideSerial);
	connect(m_actHideSerial, &QAction::toggled, this, [this](bool on) {
		m_hideSerial = on;
		m_settings.setValue("Setting/HideSerialNumber", on);
		m_canvas->update();
	});
	m_actFahrenheit = m_menuAdvanced->addAction(QString());
	m_actFahrenheit->setCheckable(true);
	m_actFahrenheit->setChecked(m_fahrenheit);
	connect(m_actFahrenheit, &QAction::toggled, this, [this](bool on) {
		m_fahrenheit = on;
		m_settings.setValue("Setting/Fahrenheit", on);
		m_canvas->update();
	});
	m_actElevate = m_menuAdvanced->addAction(QString(), this, [this] { m_elevate = true; refresh(true); });

	m_menuTheme = mb->addMenu(QString());
	m_menuDisk = mb->addMenu(QString());

	m_menuHelp = mb->addMenu(QString());
	m_actHelpWeb = m_menuHelp->addAction(QString(), this, [] { QDesktopServices::openUrl(QUrl("https://crystalmark.info/en/software/crystaldiskinfo/")); });
	m_actAboutSmart = m_menuHelp->addAction(QString(), this, [this] {
		QString url = m_lang.tr("Url", "WIKIPEDIA_SMART");
		QDesktopServices::openUrl(QUrl(url.isEmpty() ? "https://en.wikipedia.org/wiki/S.M.A.R.T." : url));
	});
	m_menuHelp->addSeparator();
	m_actAbout = m_menuHelp->addAction(QString(), this, &MainWindow::about);

	m_menuLanguage = mb->addMenu("&Language");
	auto* langGroup = new QActionGroup(this);
	for (const QString& l : Lang::available(QDir(m_paths.resources).filePath("language")))
	{
		QAction* a = m_menuLanguage->addAction(l);
		a->setCheckable(true);
		a->setData(l);
		langGroup->addAction(a);
		connect(a, &QAction::triggered, this, [this, l] { applyLanguage(l); });
	}
}

void MainWindow::retranslate()
{
	const Lang& L = m_lang;
	m_menuFile->setTitle(L.menu("FILE"));
	m_actSave->setText(L.menu("SAVE_TEXT").isEmpty() ? "Save (&Text)" : L.menu("SAVE_TEXT"));
	m_actExit->setText(L.menu("EXIT"));
	m_menuEdit->setTitle(L.menu("EDIT"));
	m_actCopy->setText(L.menu("COPY"));
	m_menuFunction->setTitle(L.menu("FUNCTION"));
	m_actRefresh->setText(L.menu("REFRESH"));
	m_actRescan->setText(L.menu("RESCAN"));
	m_menuAutoRefresh->setTitle(L.menu("AUTO_REFRESH"));
	for (QAction* a : m_menuAutoRefresh->actions())
	{
		int minutes = a->data().toInt();
		a->setText(minutes == 0 ? L.menu("DISABLE") : QString("%1 %2").arg(minutes).arg(L.menu("MINUTE")));
	}
	m_actHealth->setText(L.tr("WindowTitle", "HEALTH_STATUS_SETTING"));
	m_menuAdvanced->setTitle(L.menu("ADVANCED_FEATURE"));
	m_menuRaw->setTitle(L.tr("Dialog", "LIST_RAW_VALUES"));
	m_actHideSerial->setText(L.menu("HIDE_SERIAL_NUMBER"));
	m_actFahrenheit->setText(L.menu("FAHRENHEIT").isEmpty() ? "Fahrenheit" : L.menu("FAHRENHEIT"));
	m_actElevate->setText("Rescan as Administrator (pkexec)");
	m_menuTheme->setTitle(L.menu("THEME"));
	m_menuDisk->setTitle(L.menu("DISK"));
	m_menuHelp->setTitle(L.menu("HELP"));
	m_actAbout->setText(L.menu("HELP_ABOUT"));
	m_actHelpWeb->setText(L.menu("HELP_WEB"));
	m_actAboutSmart->setText(L.menu("HELP_ABOUT_SMART"));
	m_menuLanguage->setTitle(L.menu("LANGUAGE").isEmpty() ? "&Language" : L.menu("LANGUAGE"));

	m_table->setHorizontalHeaderLabels({ "", L.tr("Dialog", "LIST_ID"), L.tr("Dialog", "LIST_ATTRIBUTE_NAME"), L.tr("Dialog", "LIST_CURRENT"),
	                                     L.tr("Dialog", "LIST_WORST"), L.tr("Dialog", "LIST_THRESHOLD"), L.tr("Dialog", "LIST_RAW_VALUES") });
}

void MainWindow::applyLanguage(const QString& name)
{
	m_lang.load(QDir(m_paths.resources).filePath("language"), name);
	m_settings.setValue("Setting/Language", m_lang.name());
	for (QAction* a : m_menuLanguage->actions()) a->setChecked(a->data().toString() == m_lang.name());
	retranslate();
	if (!m_data.disks().isEmpty())
	{
		selectDisk(m_selected);
	}
	m_canvas->update();
}

void MainWindow::applyTheme(const QString& name)
{
	m_theme.load(name, *m_preferredEdition);
	m_settings.setValue("Setting/Theme", name);

	// Theme menu: grouped by edition so the character themes are easy to find.
	m_menuTheme->clear();
	auto* group = new QActionGroup(m_menuTheme);
	QAction* random = m_menuTheme->addAction(m_lang.menu("RANDOM").isEmpty() ? "Random" : m_lang.menu("RANDOM"));
	connect(random, &QAction::triggered, this, [this] {
		QStringList themes = m_theme.list();
		if (!themes.isEmpty()) applyTheme(themes[QRandomGenerator::global()->bounded(themes.size())]);
	});
	m_menuTheme->addSeparator();
	for (const QString& t : m_theme.list())
	{
		QAction* a = m_menuTheme->addAction(t);
		a->setCheckable(true);
		a->setChecked(t == name);
		group->addAction(a);
		connect(a, &QAction::triggered, this, [this, t] { applyTheme(t); });
	}
	if (!m_theme.edition().character)
	{
		m_menuTheme->addSeparator();
		m_actGreen = m_menuTheme->addAction("Green Mode");
		m_actGreen->setCheckable(true);
		m_actGreen->setChecked(m_greenMode || name == "Simplicity");
		connect(m_actGreen, &QAction::toggled, this, [this](bool on) {
			m_greenMode = on;
			m_settings.setValue("Setting/GreenMode", on);
			updateTable();
			m_canvas->update();
		});
	}

	QString iconDir = m_theme.edition().id == "shizuku" ? "resS" : m_theme.edition().id == "kureikei" ? "resK" : m_theme.edition().id == "aoi" ? "resA" : "resN";
	QIcon icon(QDir(m_paths.icons).filePath(iconDir + "/DiskInfo.ico"));
	if (!icon.isNull()) setWindowIcon(icon);

	setWindowTitle(QString("CrystalDiskInfo %1 %2").arg(kVersion, m_theme.edition().character ? m_theme.edition().name : QString()).trimmed());
	applyZoom();
	updateTable();
	m_canvas->update();
}

void MainWindow::applyZoom()
{
	int type = m_settings.value("Setting/ZoomType", 0).toInt();
	m_zoom = type > 0 ? type / 100.0 : 1.0;
	int height = m_settings.value("Setting/Height", 640).toInt();
	height = std::clamp(height, minY(), 1000);
	const int w = qRound(sizeX() * m_zoom);
	setMinimumSize(0, 0);
	setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
	const int menuH = menuBar()->sizeHint().height();
	resize(w, qRound(height * m_zoom) + menuH);
	setMinimumSize(w, qRound(minY() * m_zoom) + menuH);
	setMaximumSize(w, qRound(1000 * m_zoom) + menuH);

	QFont f = QApplication::font();
	f.setPixelSize(qRound(12 * m_zoom));
	m_table->setFont(f);
	m_table->horizontalHeader()->setFont(f);
	m_table->verticalHeader()->setDefaultSectionSize(qRound(18 * m_zoom));

	const Theme& t = m_theme;
	auto rgba = [](QColor c, int a) { return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a); };
	m_table->setStyleSheet(QString(
		"QTableWidget { background: %1; alternate-background-color: %2; color: %3; border: 1px solid %4; }"
		"QTableWidget::item { border-bottom: 1px solid %4; padding: 0 2px; }"
		"QTableWidget::item:selected { background: %5; color: %6; }"
		"QHeaderView::section { background: %7; color: %3; border: none; border-right: 1px solid %4; border-bottom: 1px solid %4; padding: 2px; }"
		"QTableCornerButton::section { background: %7; border: none; }"
		"QScrollBar:vertical { background: %7; width: 12px; margin: 0; }"
		"QScrollBar::handle:vertical { background: %4; min-height: 24px; border-radius: 4px; margin: 2px; }"
		"QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
		.arg(rgba(t.listBk1, 192), rgba(t.listBk2, 192), t.listText1.name(), rgba(t.listLine1, 255),
		     rgba(t.listBkSelected.isValid() && t.listBkSelected != Qt::black ? t.listBkSelected : t.listLine1, 220),
		     t.listTextSelected.name(), rgba(t.glass, std::max(160, t.glassAlpha))));
	m_table->setAlternatingRowColors(true);
}

void MainWindow::resizeEvent(QResizeEvent* e)
{
	QMainWindow::resizeEvent(e);
	const int ox = offsetX();
	const int h = m_canvas->height();
	m_table->setGeometry(qRound((8 + ox) * m_zoom), qRound(headerY() * m_zoom), qRound(656 * m_zoom), std::max(0, h - qRound((headerY() + 8) * m_zoom)));
	if (isVisible()) m_settings.setValue("Setting/Height", qRound(h / m_zoom));
	m_canvas->update();
}

void MainWindow::closeEvent(QCloseEvent* e)
{
	m_settings.sync();
	QMainWindow::closeEvent(e);
}

// --- data -------------------------------------------------------------------

void MainWindow::refresh(bool)
{
	if (!m_paths.snapshotFile.isEmpty())
	{
		QFile f(m_paths.snapshotFile);
		if (f.open(QIODevice::ReadOnly))
		{
			const std::string text = f.readAll().toStdString();
			SetSnapshotSource([text] { return rawio::Parse(text); });
		}
		m_data.refresh([this](const QString& ms) { return thresholdsFor(ms); }, m_greenMode);
		restoreSelection();
		return;
	}
	if (geteuid() == 0)
	{
		SetSnapshotSource(nullptr);   // capture in-process
		m_data.refresh([this](const QString& ms) { return thresholdsFor(ms); }, m_greenMode);
		restoreSelection();
		return;
	}
	runProbe(m_elevate);
}

void MainWindow::runProbe(bool elevated)
{
	if (m_probe) return;
	auto* proc = new QProcess(this);
	m_probe = proc;
	connect(proc, &QProcess::finished, this, &MainWindow::onProbeFinished);
	connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError err) {
		if (err == QProcess::FailedToStart)
		{
			QMessageBox::warning(this, "CrystalDiskInfo", "Could not start the disk helper:\n" + proc->program() + "\n" + proc->errorString());
			proc->deleteLater();
		}
	});
	if (elevated)
		proc->start("pkexec", { m_paths.probe });
	else
		proc->start(m_paths.probe, {});
}

void MainWindow::onProbeFinished(int exitCode, QProcess::ExitStatus status)
{
	QProcess* proc = m_probe;
	if (!proc) return;
	QString out = QString::fromUtf8(proc->readAllStandardOutput());
	proc->deleteLater();
	if (status != QProcess::NormalExit || exitCode != 0 || !out.startsWith("CDI-SNAPSHOT"))
	{
		// pkexec cancelled: keep the previous snapshot.
		if (m_elevate) m_elevate = false;
		if (m_snapshot.isEmpty()) out = "CDI-SNAPSHOT 1\n";
		else out = m_snapshot;
	}
	m_snapshot = out;
	const std::string text = out.toStdString();
	SetSnapshotSource([text] { return rawio::Parse(text); });
	m_data.refresh([this](const QString& ms) { return thresholdsFor(ms); }, m_greenMode);

	// Disk menu
	m_menuDisk->clear();
	auto* group = new QActionGroup(m_menuDisk);
	for (int i = 0; i < m_data.disks().size(); i++)
	{
		const DiskView& d = m_data.disks()[i];
		QAction* a = m_menuDisk->addAction(QString("(%1) %2 [%3]").arg(i + 1).arg(d.model, d.driveMap.isEmpty() ? d.devnode : d.driveMap));
		a->setCheckable(true);
		group->addAction(a);
		connect(a, &QAction::triggered, this, [this, i] { selectDisk(i); });
	}
	restoreSelection();
	reportCaptureProblems();
}

void MainWindow::restoreSelection()
{
	const QString want = m_settings.value("Setting/SelectDisk").toString();
	for (int i = 0; i < m_data.disks().size(); i++)
	{
		if (m_data.disks()[i].modelSerial == want)
		{
			selectDisk(i);
			return;
		}
	}
	selectDisk(std::min(m_selected, std::max(0, (int)m_data.disks().size() - 1)));
}

void MainWindow::reportCaptureProblems()
{
	QStringList errs = m_data.captureErrors();
	bool denied = false;
	for (const QString& e : errs)
		if (e.contains("Permission denied") || e.contains("Operation not permitted")) denied = true;
	if (!denied || m_warnedAccess) return;
	m_warnedAccess = true;

	QMessageBox box(this);
	box.setIcon(QMessageBox::Information);
	box.setWindowTitle("CrystalDiskInfo");
	box.setText("Reading S.M.A.R.T. data needs raw disk access.");
	box.setInformativeText(
		"Grant it once to the small helper (recommended):\n\n"
		"  sudo setcap cap_sys_rawio,cap_sys_admin,cap_dac_read_search+ep \"" + m_paths.probe + "\"\n\n"
		"or scan now with an administrator password via pkexec.");
	box.setDetailedText(errs.join("\n"));
	QPushButton* pk = box.addButton("Scan with pkexec", QMessageBox::AcceptRole);
	box.addButton(QMessageBox::Close);
	box.exec();
	if (box.clickedButton() == pk)
	{
		m_elevate = true;
		refresh(true);
	}
}

DiskThresholds MainWindow::thresholdsFor(const QString& ms) const
{
	DiskThresholds t;
	QString k = QString(ms).replace('/', '_');
	t.caution05 = m_settings.value("ThreasholdOfCaution05/" + k, 1).toInt();
	t.cautionC5 = m_settings.value("ThreasholdOfCautionC5/" + k, 1).toInt();
	t.cautionC6 = m_settings.value("ThreasholdOfCautionC6/" + k, 1).toInt();
	t.cautionFF = m_settings.value("ThreasholdOfCautionFF/" + k, 10).toInt();
	t.alarmTemperature = m_settings.value("AlarmTemperature/" + k, -1).toInt();
	return t;
}

void MainWindow::selectDisk(int index)
{
	const int n = m_data.disks().size();
	m_selected = n ? std::clamp(index, 0, n - 1) : 0;
	m_drivePage = n > 16 ? m_selected / 16 : 0;
	const auto acts = m_menuDisk->actions();
	for (int i = 0; i < acts.size(); i++) acts[i]->setChecked(i == m_selected);
	if (n)
	{
		const DiskView& d = m_data.disks()[m_selected];
		m_settings.setValue("Setting/SelectDisk", d.modelSerial);
		setWindowTitle(QString("CrystalDiskInfo %1 %2 - %3").arg(kVersion, m_theme.edition().character ? m_theme.edition().name : QString(), d.model).simplified());
	}
	updateTable();
	m_canvas->update();
}

void MainWindow::updateTable()
{
	m_table->setRowCount(0);
	if (m_data.disks().isEmpty()) return;
	const DiskView& d = m_data.disks()[m_selected];

	auto iconFile = [this](AttrIcon i) {
		switch (i)
		{
		case AttrIcon::Good: return QString(m_greenMode ? "goodGreen.ico" : "good.ico");
		case AttrIcon::Caution: return QStringLiteral("caution.ico");
		case AttrIcon::Bad: return QStringLiteral("bad.ico");
		default: return QStringLiteral("unknown.ico");
		}
	};
	m_table->setRowCount(d.attributes.size());
	for (int r = 0; r < d.attributes.size(); r++)
	{
		const SmartAttr& a = d.attributes[r];
		auto* icon = new QTableWidgetItem(QIcon(QDir(m_paths.icons).filePath(iconFile(a.icon))), QString());
		m_table->setItem(r, 0, icon);
		auto* id = new QTableWidgetItem(QString::asprintf("%02X", a.id));
		id->setTextAlignment(Qt::AlignCenter);
		m_table->setItem(r, 1, id);
		QString name = m_lang.tr(d.smartKey, QString::asprintf("%02X", a.id));
		if (name.isEmpty()) name = m_lang.tr("Smart", "VENDOR_SPECIFIC");
		m_table->setItem(r, 2, new QTableWidgetItem(name));
		auto num = [](int v) { auto* it = new QTableWidgetItem(QString::number(v)); it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter); return it; };
		m_table->setItem(r, 3, num(a.current));
		m_table->setItem(r, 4, num(a.worst));
		auto* th = a.thresholdValid ? num(a.threshold) : new QTableWidgetItem("---");
		th->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
		m_table->setItem(r, 5, th);
		auto* raw = new QTableWidgetItem(DiskData::formatRaw(a, d.rawLayout, m_rawValues));
		raw->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
		QFont mono = m_table->font();
		mono.setFamily("monospace");
		mono.setStyleHint(QFont::Monospace);
		raw->setFont(mono);
		m_table->setItem(r, 6, raw);
	}

	const double z = m_zoom;
	auto* hh = m_table->horizontalHeader();
	hh->setSectionResizeMode(QHeaderView::Fixed);
	m_table->setColumnWidth(0, qRound(22 * z));
	m_table->setColumnWidth(1, qRound(32 * z));
	m_table->setColumnHidden(3, !d.showCurrent);
	m_table->setColumnHidden(4, !d.showWorst);
	m_table->setColumnHidden(5, !d.showThreshold);
	for (int c : { 3, 4, 5 }) m_table->setColumnWidth(c, qRound(72 * z));
	m_table->setColumnWidth(6, qRound((d.rawLayout == RawLayout::Ata6 ? 108 : 140) * z));
	hh->setSectionResizeMode(2, QHeaderView::Stretch);
	m_table->setIconSize(QSize(qRound(16 * z), qRound(16 * z)));
}

// --- text helpers --------------------------------------------------------------

QString MainWindow::diskStatusText(Health h) const
{
	switch (h)
	{
	case Health::Good: return m_lang.tr("DiskStatus", "GOOD");
	case Health::Caution: return m_lang.tr("DiskStatus", "CAUTION");
	case Health::Bad: return m_lang.tr("DiskStatus", "BAD");
	default: return m_lang.tr("DiskStatus", "UNKNOWN");
	}
}

QString MainWindow::diskStatusClass(Health h) const
{
	switch (h)
	{
	case Health::Good: return m_greenMode && !m_theme.edition().character ? "diskStatusGoodGreen" : "diskStatusGood";
	case Health::Caution: return "diskStatusCaution";
	case Health::Bad: return "diskStatusBad";
	default: return "diskStatusUnknown";
	}
}

QString MainWindow::temperatureText(int c) const
{
	return m_fahrenheit ? QString("%1 °F").arg(c * 9 / 5 + 32) : QString("%1 °C").arg(c);
}

QString MainWindow::temperatureClass(const DiskView& d) const
{
	if (!(d.smartCorrect || d.isNvme)) return "temperatureUnknown";
	if (d.temperature >= d.alarmTemperature) return "temperatureBad";
	if (d.temperature == -1000) return "temperatureUnknown";
	return m_greenMode && !m_theme.edition().character ? "temperatureGoodGreen" : "temperatureGood";
}

QString MainWindow::textReport() const
{
	QString out;
	QTextStream s(&out);
	s << "----------------------------------------------------------------------------\n";
	s << "CrystalDiskInfo " << kVersion << " (Linux) " << m_theme.edition().name << "\n";
	s << "----------------------------------------------------------------------------\n\n";
	s << "    Date : " << QDateTime::currentDateTime().toString("yyyy/MM/dd HH:mm:ss") << "\n\n";
	const Lang& L = m_lang;
	for (int i = 0; i < m_data.disks().size(); i++)
	{
		const DiskView& d = m_data.disks()[i];
		s << "----------------------------------------------------------------------------\n";
		s << QString(" (%1) %2\n").arg(i + 1, 2).arg(d.model);
		s << "----------------------------------------------------------------------------\n";
		auto line = [&](const QString& k, const QString& v) { s << QString("%1 : %2\n").arg(k, 18).arg(v); };
		line("Model", d.model);
		line("Firmware", d.firmware);
		line("Serial Number", m_hideSerial ? QString(d.serial.size(), '*') : d.serial);
		line("Disk Size", QString::asprintf("%.1f GB", d.totalDiskSizeMB / 1000.0));
		line("Interface", d.interfaceName);
		line("Standard", d.standard);
		line("Transfer Mode", d.transferMode);
		if (d.powerOnHours >= 0) line("Power On Hours", QString("%1 hours").arg(d.powerOnHours));
		if (d.powerOnCount > 0) line("Power On Count", QString("%1 count").arg(d.powerOnCount));
		if (d.hostReads >= 0) line("Host Reads", QString("%1 GB").arg(d.hostReads));
		if (d.hostWrites >= 0) line("Host Writes", QString("%1 GB").arg(d.hostWrites));
		if (d.nandWrites >= 0) line("NAND Writes", QString("%1 GB").arg(d.nandWrites));
		if (d.temperature > -300) line("Temperature", temperatureText(d.temperature));
		line("Health Status", diskStatusText(d.health) + (d.life >= 0 ? QString(" (%1 %)").arg(d.life) : QString()));
		line("Features", d.features);
		line("Drive Letter", d.driveMap);
		line("Device", d.devnode);
		s << "\n-- S.M.A.R.T. --------------------------------------------------------------\n";
		if (d.isNvme) s << "ID RawValues(6) Attribute Name\n";
		else s << "ID Cur Wor Thr RawValues(6) Attribute Name\n";
		for (const SmartAttr& a : d.attributes)
		{
			QString name = L.tr(d.smartKey, QString::asprintf("%02X", a.id));
			if (name.isEmpty()) name = L.tr("Smart", "VENDOR_SPECIFIC");
			if (d.isNvme)
				s << QString::asprintf("%02X ", a.id) << DiskData::formatRaw(a, RawLayout::Ata6, 0) << " " << name << "\n";
			else
				s << QString::asprintf("%02X %3d %3d %3d ", a.id, a.current, a.worst, a.threshold) << DiskData::formatRaw(a, RawLayout::Ata6, 0) << " " << name << "\n";
		}
		s << "\n";
	}
	return out;
}

void MainWindow::copyText()
{
	QApplication::clipboard()->setText(textReport());
}

void MainWindow::saveText()
{
	QString def = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
	                  .filePath("CrystalDiskInfo_" + QDateTime::currentDateTime().toString("yyyyMMddHHmmss") + ".txt");
	QString path = QFileDialog::getSaveFileName(this, QString(), def, "TEXT (*.txt)");
	if (path.isEmpty()) return;
	QFile f(path);
	if (f.open(QIODevice::WriteOnly | QIODevice::Text)) f.write(textReport().toUtf8());
}

void MainWindow::playVoice()
{
	// Upstream plays CDI_VOICE_007 or 008 when the character is clicked. The
	// clips are extracted from the edition's voice DLL at build time.
	const Edition& ed = m_theme.edition();
	if (ed.voicePrefix.isEmpty()) return;
	QDir root(QDir(m_paths.resources).filePath("voice"));
	QString dirName = ed.voicePrefix;
	if (!root.exists(dirName))
		dirName = ed.voicePrefix + (m_lang.name().startsWith("Japanese") ? "-ja" : "-en");
	if (!root.exists(dirName)) dirName = ed.voicePrefix + "-ja";
	QDir dir(root.filePath(dirName));
	QString clip = dir.filePath(QString("CDI_VOICE_%1.opus").arg(QRandomGenerator::global()->bounded(2) ? "007" : "008"));
	if (!QFileInfo::exists(clip))
	{
		const QStringList all = dir.entryList({ "*.opus" }, QDir::Files);
		if (all.isEmpty()) return;
		clip = dir.filePath(all[QRandomGenerator::global()->bounded(all.size())]);
	}
	m_player->stop();
	m_player->setSource(QUrl::fromLocalFile(clip));
	m_player->play();
}

void MainWindow::editHealthSettings()
{
	if (m_data.disks().isEmpty()) return;
	const DiskView& d = m_data.disks()[m_selected];
	const QString k = QString(d.modelSerial).replace('/', '_');
	DiskThresholds t = thresholdsFor(d.modelSerial);

	QDialog dlg(this);
	dlg.setWindowTitle(m_lang.tr("WindowTitle", "HEALTH_STATUS_SETTING"));
	auto* form = new QFormLayout(&dlg);
	form->addRow(new QLabel(QString("<b>%1</b>").arg(d.model.toHtmlEscaped())));
	form->addRow(new QLabel(m_lang.tr("HealthStatus", "THRESHOLD_OF_CAUTION")));
	auto spin = [](int lo, int hi, int v) { auto* s = new QSpinBox; s->setRange(lo, hi); s->setValue(v); return s; };
	QSpinBox* s05 = spin(0, 255, t.caution05);
	QSpinBox* sC5 = spin(0, 255, t.cautionC5);
	QSpinBox* sC6 = spin(0, 255, t.cautionC6);
	QSpinBox* sFF = spin(0, 100, t.cautionFF);
	QSpinBox* sT = spin(20, 80, t.alarmTemperature >= 0 ? t.alarmTemperature : d.alarmTemperature);
	if (!d.isSsd && !d.isNvme)
	{
		form->addRow("[05] " + m_lang.tr("Smart", "05"), s05);
		form->addRow("[C5] " + m_lang.tr("Smart", "C5"), sC5);
		form->addRow("[C6] " + m_lang.tr("Smart", "C6"), sC6);
	}
	else
	{
		form->addRow("[FF] " + m_lang.tr("SmartSsd", "FF"), sFF);
	}
	sT->setSuffix(" °C");
	form->addRow(m_lang.tr("Alarm", "ALARM_TEMPERATURE"), sT);
	auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
	connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	connect(bb->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, &dlg, [=] {
		s05->setValue(1); sC5->setValue(1); sC6->setValue(1); sFF->setValue(10);
		sT->setValue(d.isSsd ? 60 : 50);
	});
	form->addRow(bb);
	if (dlg.exec() != QDialog::Accepted) return;

	m_settings.setValue("ThreasholdOfCaution05/" + k, s05->value());
	m_settings.setValue("ThreasholdOfCautionC5/" + k, sC5->value());
	m_settings.setValue("ThreasholdOfCautionC6/" + k, sC6->value());
	m_settings.setValue("ThreasholdOfCautionFF/" + k, sFF->value());
	m_settings.setValue("AlarmTemperature/" + k, sT->value());
	// Re-evaluate the same snapshot with the new thresholds.
	m_data.refresh([this](const QString& ms) { return thresholdsFor(ms); }, m_greenMode);
	selectDisk(m_selected);
}

void MainWindow::about()
{
	const Edition& ed = m_theme.edition();
	QMessageBox::about(this, m_lang.tr("WindowTitle", "ABOUT"),
		QString("<h3>CrystalDiskInfo %1</h3><p>%2 &mdash; Linux port</p>"
		        "<p>&copy; 2008-2026 hiyohiyo &mdash; MIT License<br>"
		        "<a href='https://crystalmark.info/'>Crystal Dew World</a></p>"
		        "<p>Theme: %3</p>"
		        "<p>The Linux port reuses upstream's S.M.A.R.T. logic (AtaSmart.cpp) unchanged and reads drives "
		        "through SG_IO / NVMe admin passthrough.</p>")
			.arg(kVersion, ed.name, m_theme.name().toHtmlEscaped()));
}
