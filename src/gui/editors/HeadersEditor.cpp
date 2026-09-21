#include "HeadersEditor.h"
#include <QVBoxLayout>

namespace poppy::gui {

HeadersEditor::HeadersEditor(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_table = new KeyValueTable(true, this);
    
    // Auto-completion for common HTTP headers
    QStringList commonHeaders = {
        "Accept", "Accept-Charset", "Accept-Encoding", "Accept-Language",
        "Authorization", "Cache-Control", "Content-Disposition", "Content-Encoding",
        "Content-Length", "Content-Type", "Cookie", "Host", "If-Match",
        "If-Modified-Since", "If-None-Match", "Origin", "Referer", "User-Agent",
        "X-Requested-With", "X-Api-Key", "X-Auth-Token", "X-Forwarded-For"
    };
    m_table->setKeyCompleterWords(commonHeaders);

    connect(m_table, &KeyValueTable::dataChanged, this, &HeadersEditor::headersChanged);
    layout->addWidget(m_table);
}

void HeadersEditor::loadFromRequest(const core::RequestModel& req) {
    m_table->setHeaders(req.headers);
}

void HeadersEditor::saveToRequest(core::RequestModel& req) const {
    req.headers = m_table->headers();
}

} // namespace poppy::gui
