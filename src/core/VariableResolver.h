#pragma once

#include <QString>
#include <QMap>
#include "RequestModel.h"
#include "EnvironmentModel.h"

namespace poppy::core {

class VariableResolver {
public:
    VariableResolver() = default;

    void setGlobalVariables(const QMap<QString, QString>& vars) { m_globals = vars; }
    void setCollectionVariables(const QMap<QString, QString>& vars) { m_collectionVars = vars; }
    void setEnvironment(const EnvironmentModel& env);
    void setRuntimeVariable(const QString& name, const QString& value) { m_runtimeVars[name] = value; }
    void clearRuntimeVariables() { m_runtimeVars.clear(); }

    // Interpolates {{varName}} in string
    QString resolveString(const QString& input) const;

    // Produces a fully resolved copy of a RequestModel
    RequestModel resolveRequest(const RequestModel& req) const;

private:
    QString lookupVariable(const QString& name) const;

    QMap<QString, QString> m_globals;
    QMap<QString, QString> m_collectionVars;
    QMap<QString, QString> m_envVars;
    QMap<QString, QString> m_runtimeVars;
};

} // namespace poppy::core
