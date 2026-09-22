#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QList>

namespace poppy::gui {

class JsonSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit JsonSyntaxHighlighter(QTextDocument* parent = nullptr, bool isDark = true);
    void setDarkTheme(bool isDark);
    bool isDarkTheme() const { return m_isDark; }

protected:
    void highlightBlock(const QString& text) override;

private:
    void initRules();

    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<HighlightRule> m_rules;
    bool m_isDark{true};
};

} // namespace poppy::gui
