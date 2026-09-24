#include "EnvironmentModel.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QRegularExpression>

namespace poppy::core {

namespace {

bool parseEnvAssignment(const QString& rawLine, QString* key, QString* value, bool* enabled) {
    QString line = rawLine.trimmed();
    if (line.isEmpty()) return false;

    bool isEnabled = true;
    if (line.startsWith(QLatin1String("#@disabled"))) {
        isEnabled = false;
        line = line.mid(QStringLiteral("#@disabled").size()).trimmed();
    } else if (line.startsWith('#')) {
        // Previously disabled variables were saved as "# name=value".
        const QString rest = line.mid(1).trimmed();
        const int eq = rest.indexOf('=');
        if (eq <= 0) return false;
        const QString candidate = rest.left(eq).trimmed();
        static const QRegularExpression ident(QStringLiteral("^[A-Za-z_][A-Za-z0-9_.-]*$"));
        if (!ident.match(candidate).hasMatch()) return false;
        isEnabled = false;
        line = rest;
    }

    const int eqIdx = line.indexOf('=');
    if (eqIdx <= 0) return false;
    QString parsedKey = line.left(eqIdx).trimmed();
    QString parsedVal = line.mid(eqIdx + 1).trimmed();
    if (parsedKey.isEmpty()) return false;
    if ((parsedVal.startsWith('"') && parsedVal.endsWith('"') && parsedVal.size() >= 2)
        || (parsedVal.startsWith('\'') && parsedVal.endsWith('\'') && parsedVal.size() >= 2)) {
        parsedVal = parsedVal.mid(1, parsedVal.size() - 2);
    }
    *key = parsedKey;
    *value = parsedVal;
    *enabled = isEnabled;
    return true;
}

} // namespace

void EnvironmentModel::addOrUpdateVariable(const QString& name, const QString& value, bool isSecret, bool enabled) {
    for (auto& v : m_variables) {
        if (v.name == name) {
            v.value = value;
            v.isSecret = isSecret;
            v.enabled = enabled;
            return;
        }
    }
    m_variables.append(EnvironmentVariable{
        .name = name,
        .value = value,
        .isSecret = isSecret,
        .enabled = enabled
    });
}

void EnvironmentModel::setVariableValue(const QString& name, const QString& value) {
    for (auto& v : m_variables) {
        if (v.name == name) {
            v.value = value;
            return;
        }
    }
    addOrUpdateVariable(name, value, false, true);
}

void EnvironmentModel::removeVariable(const QString& name) {
    for (int i = 0; i < m_variables.size(); ++i) {
        if (m_variables[i].name == name) {
            m_variables.removeAt(i);
            return;
        }
    }
}

QString EnvironmentModel::variableValue(const QString& name) const {
    for (const auto& v : m_variables) {
        if (v.name == name && v.enabled) {
            return v.value;
        }
    }
    return {};
}

bool EnvironmentModel::hasVariable(const QString& name) const {
    for (const auto& v : m_variables) {
        if (v.name == name) return true;
    }
    return false;
}

bool EnvironmentModel::isSecretVariable(const QString& name) const {
    for (const auto& v : m_variables) {
        if (v.name == name) return v.isSecret;
    }
    return false;
}

QMap<QString, QString> EnvironmentModel::toMap() const {
    QMap<QString, QString> map;
    for (const auto& v : m_variables) {
        if (v.enabled) {
            map[v.name] = v.value;
        }
    }
    return map;
}

EnvironmentModel EnvironmentModel::loadFromEnvFile(const QString& filePath, const QString& envName) {
    QFileInfo fi(filePath);
    QString name = envName.isEmpty() ? fi.baseName() : envName;
    EnvironmentModel model(name);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return model;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString key;
        QString val;
        bool enabled = true;
        if (!parseEnvAssignment(in.readLine(), &key, &val, &enabled)) continue;
        model.addOrUpdateVariable(key, val, false, enabled);
    }

    return model;
}

bool EnvironmentModel::saveToEnvFile(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << "# Poppy Environment: " << m_name << "\n";
    for (const auto& v : m_variables) {
        if (v.isSecret) continue; // secrets are saved separately
        if (!v.enabled) out << "#@disabled ";
        out << v.name << "=" << v.value << "\n";
    }
    return true;
}

void EnvironmentModel::loadSecretsFromEnvFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString key;
        QString val;
        bool enabled = true;
        if (!parseEnvAssignment(in.readLine(), &key, &val, &enabled)) continue;
        addOrUpdateVariable(key, val, true, enabled);
    }
}

bool EnvironmentModel::saveSecretsToEnvFile(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << "# Poppy Secrets: " << m_name << "\n";
    for (const auto& v : m_variables) {
        if (!v.isSecret) continue;
        if (!v.enabled) out << "#@disabled ";
        out << v.name << "=" << v.value << "\n";
    }
    return true;
}

} // namespace poppy::core
