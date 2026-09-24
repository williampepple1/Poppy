#pragma once

#include <QString>
#include <core/RequestModel.h>

namespace poppy::core {

enum class TargetLanguage {
    PythonRequests,
    JavaScriptFetch,
    JavaScriptAxios,
    GoHttp,
    CppCurl,
    RustReqwest,
    CSharpHttpClient,
    JavaHttpClient,
    Curl,
    PhpCurl,
    PowerShell
};

class CodeGenerator {
public:
    static QString generate(TargetLanguage lang, const RequestModel& req);
    static QString languageName(TargetLanguage lang);

private:
    static QString generatePython(const RequestModel& req);
    static QString generateJsFetch(const RequestModel& req);
    static QString generateJsAxios(const RequestModel& req);
    static QString generateGo(const RequestModel& req);
    static QString generateCpp(const RequestModel& req);
    static QString generateRust(const RequestModel& req);
    static QString generateCSharp(const RequestModel& req);
    static QString generateJava(const RequestModel& req);
    static QString generatePhp(const RequestModel& req);
    static QString generatePowerShell(const RequestModel& req);
};

} // namespace poppy::core
