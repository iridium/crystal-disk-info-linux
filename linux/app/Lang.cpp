#include "Lang.h"

#include <QDir>
#include <QFile>
#include <QStringDecoder>

Lang::Table Lang::parse(const QString& path)
{
	Table t;
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) return t;
	QByteArray raw = f.readAll();

	QString text;
	if (raw.startsWith("\xFF\xFE") || (raw.size() > 1 && raw[1] == '\0'))
		text = QStringDecoder(QStringDecoder::Utf16LE)(raw);
	else
		text = QString::fromUtf8(raw);
	if (text.startsWith(QChar(0xFEFF))) text.remove(0, 1);

	QString section;
	const auto lines = text.split('\n');
	for (QString line : lines)
	{
		line = line.trimmed();
		if (line.isEmpty() || line.startsWith(';')) continue;
		if (line.startsWith('['))
		{
			int end = line.indexOf(']');
			section = line.mid(1, end - 1).trimmed();
			continue;
		}
		int eq = line.indexOf('=');
		if (eq <= 0) continue;
		t.insert(section + '\n' + line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
	}
	return t;
}

bool Lang::load(const QString& dir, const QString& name)
{
	m_english = parse(QDir(dir).filePath("English.lang"));
	m_current = parse(QDir(dir).filePath(name + ".lang"));
	m_name = m_current.isEmpty() ? QStringLiteral("English") : name;
	return !m_english.isEmpty();
}

QString Lang::tr(const QString& section, const QString& key) const
{
	const QString k = section + '\n' + key;
	auto it = m_current.constFind(k);
	if (it != m_current.constEnd() && !it->isEmpty()) return *it;
	return m_english.value(k);
}

QStringList Lang::available(const QString& dir)
{
	QStringList out;
	for (const QString& f : QDir(dir).entryList({ "*.lang" }, QDir::Files, QDir::Name))
		out << f.chopped(5);
	return out;
}
