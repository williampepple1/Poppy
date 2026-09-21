#include "CodeGenerator.h"
#include <QTextStream>

namespace poppy::core {

QString CodeGenerator::languageName(TargetLanguage lang) {
    switch (lang) {
        case TargetLanguage::PythonRequests: return "Python (requests)";
        case TargetLanguage::JavaScriptFetch: return "JavaScript (fetch)";
        case TargetLanguage::JavaScriptAxios: return "JavaScript (axios)";
        case TargetLanguage::GoHttp: return "Go (net/http)";
        case TargetLanguage::CppCurl: return "C++ (libcurl)";
        case TargetLanguage::Curl: return "cURL";
    }
    return "Python (requests)";
}

QString CodeGenerator::generate(TargetLanguage lang, const RequestModel& req) {
    switch (lang) {
        case TargetLanguage::PythonRequests: return generatePython(req);
        case TargetLanguage::JavaScriptFetch: return generateJsFetch(req);
        case TargetLanguage::JavaScriptAxios: return generateJsAxios(req);
        case TargetLanguage::GoHttp: return generateGo(req);
        case TargetLanguage::CppCurl: return generateCpp(req);
        case TargetLanguage::Curl: return req.toCurlCommand();
    }
    return generatePython(req);
}

QString CodeGenerator::generatePython(const RequestModel& req) {
    QString out;
    QTextStream ts(&out);

    ts << "import requests\n\n";
    ts << "url = \"" << req.effectiveUrl() << "\"\n\n";

    // Headers
    auto headers = req.effectiveHeaders();
    if (!headers.isEmpty()) {
        ts << "headers = {\n";
        for (const auto& h : headers) {
            if (h.enabled && !h.name.isEmpty()) {
                ts << "    \"" << h.name << "\": \"" << h.value << "\",\n";
            }
        }
        ts << "}\n\n";
    }

    // Body
    bool hasBody = (req.bodyType != BodyType::None && !req.bodyContent.isEmpty());
    if (hasBody) {
        if (req.bodyType == BodyType::Json) {
            ts << "payload = " << req.bodyContent << "\n\n";
        } else {
            ts << "payload = \"\"\"" << req.bodyContent << "\"\"\"\n\n";
        }
    }

    QString methodStr = methodToString(req.method);
    ts << "response = requests.request(\"" << methodStr << "\", url";
    if (!headers.isEmpty()) ts << ", headers=headers";
    if (hasBody) {
        if (req.bodyType == BodyType::Json) ts << ", json=payload";
        else ts << ", data=payload";
    }
    ts << ")\n\n";

    ts << "print(response.status_code)\n";
    ts << "print(response.text)\n";

    return out;
}

QString CodeGenerator::generateJsFetch(const RequestModel& req) {
    QString out;
    QTextStream ts(&out);

    ts << "const url = \"" << req.effectiveUrl() << "\";\n\n";

    ts << "const options = {\n";
    ts << "  method: \"" << methodToString(req.method) << "\",\n";

    auto headers = req.effectiveHeaders();
    if (!headers.isEmpty()) {
        ts << "  headers: {\n";
        for (const auto& h : headers) {
            if (h.enabled && !h.name.isEmpty()) {
                ts << "    \"" << h.name << "\": \"" << h.value << "\",\n";
            }
        }
        ts << "  },\n";
    }

    if (req.bodyType != BodyType::None && !req.bodyContent.isEmpty()) {
        if (req.bodyType == BodyType::Json) {
            ts << "  body: JSON.stringify(" << req.bodyContent << "),\n";
        } else {
            ts << "  body: `" << req.bodyContent << "`,\n";
        }
    }
    ts << "};\n\n";

    ts << "fetch(url, options)\n";
    ts << "  .then(response => response.json())\n";
    ts << "  .then(data => console.log(data))\n";
    ts << "  .catch(error => console.error('Error:', error));\n";

    return out;
}

QString CodeGenerator::generateJsAxios(const RequestModel& req) {
    QString out;
    QTextStream ts(&out);

    ts << "const axios = require('axios');\n\n";
    ts << "const config = {\n";
    ts << "  method: '" << methodToString(req.method).toLower() << "',\n";
    ts << "  url: '" << req.effectiveUrl() << "',\n";

    auto headers = req.effectiveHeaders();
    if (!headers.isEmpty()) {
        ts << "  headers: {\n";
        for (const auto& h : headers) {
            if (h.enabled && !h.name.isEmpty()) {
                ts << "    '" << h.name << "': '" << h.value << "',\n";
            }
        }
        ts << "  },\n";
    }

    if (req.bodyType != BodyType::None && !req.bodyContent.isEmpty()) {
        if (req.bodyType == BodyType::Json) {
            ts << "  data: " << req.bodyContent << ",\n";
        } else {
            ts << "  data: `" << req.bodyContent << "`,\n";
        }
    }
    ts << "};\n\n";

    ts << "axios(config)\n";
    ts << "  .then(response => console.log(response.data))\n";
    ts << "  .catch(error => console.error(error));\n";

    return out;
}

QString CodeGenerator::generateGo(const RequestModel& req) {
    QString out;
    QTextStream ts(&out);

    ts << "package main\n\n";
    ts << "import (\n";
    ts << "    \"fmt\"\n";
    ts << "    \"io\"\n";
    ts << "    \"net/http\"\n";
    if (req.bodyType != BodyType::None && !req.bodyContent.isEmpty()) {
        ts << "    \"strings\"\n";
    }
    ts << ")\n\n";

    ts << "func main() {\n";
    ts << "    url := \"" << req.effectiveUrl() << "\"\n";

    bool hasBody = (req.bodyType != BodyType::None && !req.bodyContent.isEmpty());
    if (hasBody) {
        QString escaped = req.bodyContent;
        escaped.replace("\"", "\\\"");
        escaped.replace("\n", "\\n");
        ts << "    payload := strings.NewReader(\"" << escaped << "\")\n";
        ts << "    req, err := http.NewRequest(\"" << methodToString(req.method) << "\", url, payload)\n";
    } else {
        ts << "    req, err := http.NewRequest(\"" << methodToString(req.method) << "\", url, nil)\n";
    }
    ts << "    if err != nil {\n        panic(err)\n    }\n\n";

    for (const auto& h : req.effectiveHeaders()) {
        if (h.enabled && !h.name.isEmpty()) {
            ts << "    req.Header.Add(\"" << h.name << "\", \"" << h.value << "\")\n";
        }
    }

    ts << "\n    res, err := http.DefaultClient.Do(req)\n";
    ts << "    if err != nil {\n        panic(err)\n    }\n";
    ts << "    defer res.Body.Close()\n\n";
    ts << "    body, _ := io.ReadAll(res.Body)\n";
    ts << "    fmt.Println(res.StatusCode)\n";
    ts << "    fmt.Println(string(body))\n";
    ts << "}\n";

    return out;
}

QString CodeGenerator::generateCpp(const RequestModel& req) {
    QString out;
    QTextStream ts(&out);

    ts << "#include <curl/curl.h>\n";
    ts << "#include <iostream>\n\n";
    ts << "int main() {\n";
    ts << "    CURL* curl = curl_easy_init();\n";
    ts << "    if (curl) {\n";
    ts << "        curl_easy_setopt(curl, CURLOPT_URL, \"" << req.effectiveUrl() << "\");\n";

    if (req.method != HttpMethod::GET) {
        ts << "        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, \"" << methodToString(req.method) << "\");\n";
    }

    auto headers = req.effectiveHeaders();
    if (!headers.isEmpty()) {
        ts << "        struct curl_slist* headers = NULL;\n";
        for (const auto& h : headers) {
            if (h.enabled && !h.name.isEmpty()) {
                ts << "        headers = curl_slist_append(headers, \"" << h.name << ": " << h.value << "\");\n";
            }
        }
        ts << "        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);\n";
    }

    if (req.bodyType != BodyType::None && !req.bodyContent.isEmpty()) {
        QString escaped = req.bodyContent;
        escaped.replace("\"", "\\\"");
        escaped.replace("\n", "\\n");
        ts << "        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, \"" << escaped << "\");\n";
    }

    ts << "        CURLcode res = curl_easy_perform(curl);\n";
    ts << "        if (res != CURLE_OK) {\n";
    ts << "            std::cerr << \"curl error: \" << curl_easy_strerror(res) << std::endl;\n";
    ts << "        }\n";
    if (!headers.isEmpty()) {
        ts << "        curl_slist_free_all(headers);\n";
    }
    ts << "        curl_easy_cleanup(curl);\n";
    ts << "    }\n";
    ts << "    return 0;\n";
    ts << "}\n";

    return out;
}

} // namespace poppy::core
