#pragma once

#include <QString>
#include <QList>
#include <QDateTime>

namespace poppy::core {

struct Cookie {
    QString domain;
    bool includeSubdomains = true;
    QString path = "/";
    bool secure = false;
    qint64 expires = 0; // Unix epoch timestamp (seconds); 0 indicates session cookie
    bool httpOnly = false;
    QString name;
    QString value;

    bool isExpired() const {
        if (expires <= 0) return false;
        return QDateTime::currentSecsSinceEpoch() > expires;
    }
};

class CookieJar {
public:
    CookieJar() = default;

    bool loadFromFile(const QString& filePath);
    bool saveToFile(const QString& filePath) const;

    const QList<Cookie>& cookies() const { return m_cookies; }
    QList<Cookie> cookiesForDomain(const QString& domain) const;

    void addOrUpdateCookie(const Cookie& cookie);
    bool removeCookie(const QString& domain, const QString& path, const QString& name);
    void clear();
    void clearDomain(const QString& domain);

    int count() const { return m_cookies.size(); }

    static bool parseNetscapeLine(const QString& line, Cookie* outCookie);
    static QString formatNetscapeLine(const Cookie& cookie);

private:
    QList<Cookie> m_cookies;
};

} // namespace poppy::core
