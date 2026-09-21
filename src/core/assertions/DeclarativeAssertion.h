#pragma once

#include <QString>
#include <QList>
#include "AssertionRule.h"
#include <core/ScriptRunner.h>

namespace poppy::core {

class ResponseModel;

class DeclarativeAssertionEvaluator {
public:
    static TestCaseResult evaluate(const AssertionRule& rule, const ResponseModel& res);
    static QList<TestCaseResult> evaluateAll(const QList<AssertionRule>& rules, const ResponseModel& res);

private:
    static QString resolveTargetValue(const QString& target, const ResponseModel& res);
    static bool compare(const QString& actual, const QString& op, const QString& expected);
};

} // namespace poppy::core
