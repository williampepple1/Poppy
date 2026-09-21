#pragma once

#include <QString>

namespace poppy::core {

struct AssertionRule {
    QString target;      // e.g. "res.status", "res.responseTime", "res.body.id", "res.header(Content-Type)"
    QString op{"eq"};    // "eq", "neq", "gt", "gte", "lt", "lte", "contains", "not_contains"
    QString expected;    // expected value as string
    bool enabled{true};

    bool operator==(const AssertionRule& other) const = default;
};

} // namespace poppy::core
