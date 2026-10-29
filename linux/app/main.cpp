#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>

// Installed layout:  <prefix>/bin/crystaldiskinfo
//                    <prefix>/libexec/crystaldiskinfo/cdi-probe
//                    <prefix>/share/crystaldiskinfo/{CdiResource,icons}
// Build tree / portable: everything next to the executable.
static AppPaths FindPaths()
{
	const QString exeDir = QCoreApplication::applicationDirPath();
	AppPaths p;
	const QStringList shares = { exeDir, exeDir + "/../share/crystaldiskinfo", "/usr/share/crystaldiskinfo", "/usr/local/share/crystaldiskinfo" };
	const QString envRes = qEnvironmentVariable("CDI_RESOURCE_DIR");
	for (const QString& s : shares)
	{
		if (QFileInfo::exists(s + "/CdiResource/themes"))
		{
			p.resources = QDir(s + "/CdiResource").canonicalPath();
			p.icons = QDir(s + "/icons").canonicalPath();
			break;
		}
	}
	if (!envRes.isEmpty()) p.resources = envRes;
	for (const QString& c : { exeDir + "/cdi-probe", exeDir + "/../libexec/crystaldiskinfo/cdi-probe", QStringLiteral("/usr/libexec/crystaldiskinfo/cdi-probe") })
	{
		if (QFileInfo(c).isExecutable())
		{
			p.probe = QFileInfo(c).canonicalFilePath();
			break;
		}
	}
	return p;
}

int main(int argc, char** argv)
{
	QApplication app(argc, argv);
	QApplication::setApplicationName("CrystalDiskInfo");
	QApplication::setOrganizationName("CrystalDiskInfo");
	QApplication::setApplicationVersion("9.9.3");
	QApplication::setDesktopFileName("crystaldiskinfo");

	QCommandLineParser cli;
	cli.setApplicationDescription("CrystalDiskInfo for Linux");
	cli.addHelpOption();
	cli.addVersionOption();
	QCommandLineOption edition("edition", "Preferred edition on first start: standard, shizuku, kureikei, aoi.", "id");
	cli.addOption(edition);
	QCommandLineOption snapshot("snapshot", "Show a saved cdi-probe snapshot instead of scanning (for bug reports).", "file");
	cli.addOption(snapshot);
	cli.process(app);

	AppPaths paths = FindPaths();
	if (paths.resources.isEmpty())
	{
		qCritical("CdiResource not found (set CDI_RESOURCE_DIR).");
		return 1;
	}
	if (cli.isSet(snapshot)) paths.snapshotFile = cli.value(snapshot);
	MainWindow w(paths, cli.value(edition));
	w.show();
	return app.exec();
}
