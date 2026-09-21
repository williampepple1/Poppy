#pragma once

#include <QString>
#include <QList>
#include <QMap>

namespace poppy::core {

struct EnvironmentVariable {
    QString name;
    QString value;
    bool isSecret{false};
    bool enabled{true};

    bool operator==(const EnvironmentVariable& other) const = default;
};

class EnvironmentModel {
public:
    EnvironmentModel() = default;
    explicit EnvironmentModel(QString name) : m_name(std::move(name)) {}

    const QString& name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    const QList<EnvironmentVariable>& variables() const { return m_variables; }
    QList<EnvironmentVariable>& variables() { return m_variables; }

    void addOrUpdateVariable(const QString& name, const QString& value, bool isSecret = false, bool enabled = true);
    void removeVariable(const QString& name);
    QString variableValue(const QString& name) const;
    bool hasVariable(const QString& name) const;

    // Load / Save standard .env file format
    static EnvironmentModel loadFromEnvFile(const QString& filePath, const QString& envName = {});
    bool saveToEnvFile(const QString& filePath) const;

    // Load / Save secret variables to .secret.env
    void loadSecretsFromEnvFile(const QString& filePath);
    bool saveSecretsToEnvFile(const QString& filePath) const;

    QMap<QString, QString> toMap() const;

private:
    QString m_name;
    QList<EnvironmentVariable> m_variables;
};

} // namespace poppy::core
