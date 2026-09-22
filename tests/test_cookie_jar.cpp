#include <iostream>
#include <cassert>
#include <QDir>
#include <QFile>
#include <core/CookieJar.h>

using namespace poppy::core;

int main() {
    std::cout << "Running test_cookie_jar..." << std::endl;

    // 1. Test parsing Netscape line
    QString line1 = ".httpbin.org\tTRUE\t/\tFALSE\t1735689600\tsession_id\tabc123xyz";
    Cookie c1;
    bool ok1 = CookieJar::parseNetscapeLine(line1, &c1);
    assert(ok1);
    assert(c1.domain == ".httpbin.org");
    assert(c1.includeSubdomains == true);
    assert(c1.path == "/");
    assert(c1.secure == false);
    assert(c1.expires == 1735689600);
    assert(c1.name == "session_id");
    assert(c1.value == "abc123xyz");
    assert(c1.httpOnly == false);

    // 2. Test HttpOnly parsing
    QString line2 = "#HttpOnly_api.example.com\tFALSE\t/auth\tTRUE\t0\tauth_token\tsecret999";
    Cookie c2;
    bool ok2 = CookieJar::parseNetscapeLine(line2, &c2);
    assert(ok2);
    assert(c2.domain == "api.example.com");
    assert(c2.includeSubdomains == false);
    assert(c2.path == "/auth");
    assert(c2.secure == true);
    assert(c2.expires == 0);
    assert(c2.name == "auth_token");
    assert(c2.value == "secret999");
    assert(c2.httpOnly == true);

    // 3. Test comment lines
    QString comment = "# Just a standard Netscape comment";
    Cookie cComment;
    assert(!CookieJar::parseNetscapeLine(comment, &cComment));

    // 4. Test formatting Netscape line
    QString formatted1 = CookieJar::formatNetscapeLine(c1);
    assert(formatted1.contains(".httpbin.org"));
    assert(formatted1.contains("session_id"));
    assert(formatted1.contains("abc123xyz"));
    assert(!formatted1.startsWith("#HttpOnly_"));

    QString formatted2 = CookieJar::formatNetscapeLine(c2);
    assert(formatted2.startsWith("#HttpOnly_api.example.com"));
    assert(formatted2.contains("auth_token"));

    // 5. Test CookieJar add, domain filter, and remove
    CookieJar jar;
    jar.addOrUpdateCookie(c1);
    jar.addOrUpdateCookie(c2);
    assert(jar.count() == 2);

    auto httpbinCookies = jar.cookiesForDomain("sub.httpbin.org");
    assert(httpbinCookies.size() == 1);
    assert(httpbinCookies[0].name == "session_id");

    auto exampleCookies = jar.cookiesForDomain("api.example.com");
    assert(exampleCookies.size() == 1);
    assert(exampleCookies[0].name == "auth_token");

    Cookie suffixFalsePositive;
    suffixFalsePositive.domain = "example.com";
    suffixFalsePositive.includeSubdomains = true;
    suffixFalsePositive.name = "sid";
    suffixFalsePositive.value = "1";
    jar.addOrUpdateCookie(suffixFalsePositive);
    assert(jar.cookiesForDomain("notexample.com").isEmpty());
    assert(jar.cookiesForDomain("www.example.com").size() == 1);

    // Remove c1
    bool removed = jar.removeCookie(".httpbin.org", "/", "session_id");
    assert(removed);
    assert(jar.count() == 2);

    // 6. Test File persistence roundtrip
    QString tmpFile = QDir::tempPath() + "/poppy_test_cookies.txt";
    QFile::remove(tmpFile);

    CookieJar saveJar;
    saveJar.addOrUpdateCookie(c1);
    saveJar.addOrUpdateCookie(c2);
    bool saveOk = saveJar.saveToFile(tmpFile);
    assert(saveOk);

    CookieJar loadedJar;
    bool loadOk = loadedJar.loadFromFile(tmpFile);
    assert(loadOk);
    assert(loadedJar.count() == 2);

    auto loadedC1 = loadedJar.cookiesForDomain("httpbin.org");
    assert(loadedC1.size() == 1);
    assert(loadedC1[0].value == "abc123xyz");

    QFile::remove(tmpFile);

    std::cout << "test_cookie_jar PASSED!" << std::endl;
    return 0;
}
