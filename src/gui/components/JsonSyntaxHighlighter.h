#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QList>

namespace poppy::gui {

class JsonSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit JsonSyntaxHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<HighlightRule> m_rules;
};

} // namespace poppy::gui
