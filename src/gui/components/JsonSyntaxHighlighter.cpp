#include "JsonSyntaxHighlighter.h"

namespace poppy::gui {

JsonSyntaxHighlighter::JsonSyntaxHighlighter(QTextDocument* parent, bool isDark)
    : QSyntaxHighlighter(parent), m_isDark(isDark) {
    initRules();
}

void JsonSyntaxHighlighter::setDarkTheme(bool isDark) {
    if (m_isDark != isDark) {
        m_isDark = isDark;
        initRules();
        rehighlight();
    }
}

void JsonSyntaxHighlighter::initRules() {
    m_rules.clear();

    // 1. JSON Keys (e.g. "key":)
    QTextCharFormat keyFormat;
    keyFormat.setForeground(m_isDark ? QColor("#93c5fd") : QColor("#1d4ed8")); // light blue vs dark blue
    keyFormat.setFontWeight(QFont::DemiBold);
    m_rules.append({QRegularExpression(R"("[^"\\]*(\\.[^"\\]*)*"(?=\s*:))"), keyFormat});

    // 2. String values (e.g. : "string")
    QTextCharFormat stringFormat;
    stringFormat.setForeground(m_isDark ? QColor("#86efac") : QColor("#15803d")); // mint green vs forest green
    m_rules.append({QRegularExpression(R"(:\s*"[^"\\]*(\\.[^"\\]*)*")"), stringFormat});

    // 3. Numbers
    QTextCharFormat numberFormat;
    numberFormat.setForeground(m_isDark ? QColor("#fca5a5") : QColor("#b91c1c")); // light coral vs dark red
    m_rules.append({QRegularExpression(R"(\b-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?\b)"), numberFormat});

    // 4. Booleans & Null
    QTextCharFormat boolFormat;
    boolFormat.setForeground(m_isDark ? QColor("#c084fc") : QColor("#7e22ce")); // lavender vs purple
    boolFormat.setFontWeight(QFont::Bold);
    m_rules.append({QRegularExpression(R"(\b(true|false|null)\b)"), boolFormat});

    // 5. Brackets & Braces
    QTextCharFormat punctFormat;
    punctFormat.setForeground(m_isDark ? QColor("#e2e8f0") : QColor("#334155")); // slate
    m_rules.append({QRegularExpression(R"([\[\]\{\},:])"), punctFormat});
}

void JsonSyntaxHighlighter::highlightBlock(const QString& text) {
    for (const auto& rule : m_rules) {
        auto matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            auto match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

} // namespace poppy::gui
