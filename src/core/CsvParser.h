#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QRegularExpression>

namespace poppy::core {

inline QStringList parseCsvLine(const QString& line) {
    QStringList fields;
    QString current;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line[i];
        if (inQuotes) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    current += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current += ch;
            }
            continue;
        }
        if (ch == '"') {
            inQuotes = true;
            continue;
        }
        if (ch == ',') {
            fields.append(current.trimmed());
            current.clear();
            continue;
        }
        current += ch;
    }
    fields.append(current.trimmed());
    return fields;
}

inline QList<QMap<QString, QString>> parseCsvTable(const QString& text) {
    QList<QMap<QString, QString>> rows;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
    if (lines.isEmpty()) return rows;
    const QStringList headers = parseCsvLine(lines.first());
    for (int i = 1; i < lines.size(); ++i) {
        const QStringList cols = parseCsvLine(lines[i]);
        QMap<QString, QString> row;
        for (int c = 0; c < headers.size() && c < cols.size(); ++c) {
            if (headers[c].isEmpty()) continue;
            row.insert(headers[c], cols[c]);
        }
        if (!row.isEmpty()) rows.append(row);
    }
    return rows;
}

} // namespace poppy::core
