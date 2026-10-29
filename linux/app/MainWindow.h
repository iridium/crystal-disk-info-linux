#pragma once

#include "DiskData.h"
#include "Lang.h"
#include "Theme.h"

#include <QMainWindow>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QTimer>

class QActionGroup;
class QMediaPlayer;
class QAudioOutput;
class QTableWidget;
class Canvas;

struct AppPaths
{
	QString resources;   // .../CdiResource
	QString icons;       // small status icons (good.ico ...)
	QString probe;       // cdi-probe helper
	QString snapshotFile; // --snapshot: replay instead of scanning
};

class MainWindow : public QMainWindow
{
	Q_OBJECT
	friend class Canvas;

public:
	MainWindow(const AppPaths& paths, const QString& editionHint, QWidget* parent = nullptr);

	void refresh(bool rescan = true);

protected:
	void resizeEvent(QResizeEvent* e) override;
	void closeEvent(QCloseEvent* e) override;

private:
	// layout
	int sizeX() const { return theme().edition().character ? 1000 : 672; }
	int headerY() const { return theme().edition().character ? 288 : 260; }
	int minY() const { return theme().edition().character ? 500 : 260; }
	int offsetX() const { return theme().edition().character && theme().characterPosition == 0 ? 328 : 0; }
	int characterX() const { return theme().characterPosition == 0 ? 0 : sizeX() - 328; }
	double zoom() const { return m_zoom; }
	const Theme& theme() const { return m_theme; }

	void buildMenus();
	void retranslate();
	void applyTheme(const QString& name);
	void applyLanguage(const QString& name);
	void applyZoom();
	void updateTable();
	void selectDisk(int index);
	void runProbe(bool elevated);
	void onProbeFinished(int exitCode, QProcess::ExitStatus status);
	void reportCaptureProblems();
	void restoreSelection();
	void copyText();
	void saveText();
	void playVoice();
	void editHealthSettings();
	void about();

	QString textReport() const;
	QString diskStatusText(Health h) const;
	QString diskStatusClass(Health h) const;
	QString temperatureText(int celsius) const;
	QString temperatureClass(const DiskView& d) const;
	DiskThresholds thresholdsFor(const QString& modelSerial) const;

	AppPaths m_paths;
	QSettings m_settings;
	Lang m_lang;
	Theme m_theme;
	const Edition* m_preferredEdition = nullptr;
	DiskData m_data;
	int m_selected = 0;
	int m_drivePage = 0;
	double m_zoom = 1.0;
	int m_rawValues = 0;
	bool m_fahrenheit = false;
	bool m_hideSerial = false;
	bool m_greenMode = false;
	bool m_elevate = false;          // use pkexec for the helper this session
	bool m_warnedAccess = false;
	QString m_snapshot;

	Canvas* m_canvas = nullptr;
	QTableWidget* m_table = nullptr;
	QTimer m_autoRefresh;
	QPointer<QProcess> m_probe;
	QMediaPlayer* m_player = nullptr;
	QAudioOutput* m_audio = nullptr;

	QMenu* m_menuFile = nullptr;
	QMenu* m_menuEdit = nullptr;
	QMenu* m_menuFunction = nullptr;
	QMenu* m_menuTheme = nullptr;
	QMenu* m_menuDisk = nullptr;
	QMenu* m_menuHelp = nullptr;
	QMenu* m_menuLanguage = nullptr;
	QMenu* m_menuAutoRefresh = nullptr;
	QMenu* m_menuRaw = nullptr;
	QMenu* m_menuAdvanced = nullptr;
	QAction* m_actSave = nullptr;
	QAction* m_actExit = nullptr;
	QAction* m_actCopy = nullptr;
	QAction* m_actRefresh = nullptr;
	QAction* m_actRescan = nullptr;
	QAction* m_actHideSerial = nullptr;
	QAction* m_actFahrenheit = nullptr;
	QAction* m_actGreen = nullptr;
	QAction* m_actHealth = nullptr;
	QAction* m_actAbout = nullptr;
	QAction* m_actHelpWeb = nullptr;
	QAction* m_actAboutSmart = nullptr;
	QAction* m_actElevate = nullptr;
};
