#include "EnvironmentModel.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

namespace poppy::core {

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
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        int eqIdx = line.indexOf('=');
        if (eqIdx > 0) {
            QString key = line.left(eqIdx).trimmed();
            QString val = line.mid(eqIdx + 1).trimmed();
            if ((val.startsWith('"') && val.endsWith('"')) || (val.startsWith('\'') && val.endsWith('\''))) {
                val = val.mid(1, val.length() - 2);
            }
            model.addOrUpdateVariable(key, val, false, true);
        }
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
        if (!v.enabled) out << "# ";
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
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        int eqIdx = line.indexOf('=');
        if (eqIdx > 0) {
            QString key = line.left(eqIdx).trimmed();
            QString val = line.mid(eqIdx + 1).trimmed();
            if ((val.startsWith('"') && val.endsWith('"')) || (val.startsWith('\'') && val.endsWith('\''))) {
                val = val.mid(1, val.length() - 2);
            }
            addOrUpdateVariable(key, val, true, true);
        }
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
        if (!v.enabled) out << "# ";
        out << v.name << "=" << v.value << "\n";
    }
    return true;
}

} // namespace poppy::core
