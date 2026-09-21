#include "JsonSyntaxHighlighter.h"

namespace poppy::gui {

JsonSyntaxHighlighter::JsonSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
    // 1. JSON Keys (e.g. "key":)
    QTextCharFormat keyFormat;
    keyFormat.setForeground(QColor("#93c5fd")); // light blue
    keyFormat.setFontWeight(QFont::DemiBold);
    m_rules.append({QRegularExpression(R"("[^"\\]*(\\.[^"\\]*)*"(?=\s*:))"), keyFormat});

    // 2. String values (e.g. : "string")
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor("#86efac")); // light green
    m_rules.append({QRegularExpression(R"(:\s*"[^"\\]*(\\.[^"\\]*)*")"), stringFormat});

    // 3. Numbers
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor("#fca5a5")); // light coral/red
    m_rules.append({QRegularExpression(R"(\b-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?\b)"), numberFormat});

    // 4. Booleans & Null
    QTextCharFormat boolFormat;
    boolFormat.setForeground(QColor("#c084fc")); // purple
    boolFormat.setFontWeight(QFont::Bold);
    m_rules.append({QRegularExpression(R"(\b(true|false|null)\b)"), boolFormat});

    // 5. Brackets & Braces
    QTextCharFormat punctFormat;
    punctFormat.setForeground(QColor("#e2e8f0")); // slate
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
