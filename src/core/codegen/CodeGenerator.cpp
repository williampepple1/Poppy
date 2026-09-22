#include "CodeGenerator.h"
#include <QTextStream>

namespace poppy::core {

namespace {
QString snippetBody(const RequestModel& req) {
    if (req.bodyType == BodyType::GraphQL) {
        return QString::fromUtf8(req.effectiveBody());
    }
    if (req.bodyType == BodyType::MultipartForm) {
        QStringList parts;
        for (const auto& p : req.formDataParams) {
            if (p.enabled && !p.key.isEmpty()) {
                parts.append(p.key + "=" + (p.isFile ? ("@" + p.value) : p.value));
            }
        }
        return parts.join("&");
    }
    if (req.bodyType == BodyType::FormUrlEncoded) {
        return QString::fromUtf8(req.effectiveBody());
    }
    return req.bodyContent;
}

bool snippetHasBody(const RequestModel& req) {
    return req.bodyType != BodyType::None && !snippetBody(req).trimmed().isEmpty();
}
}

QString CodeGenerator::languageName(TargetLanguage lang) {
    switch (lang) {
        case TargetLanguage::PythonRequests: return "Python (requests)";
        case TargetLanguage::JavaScriptFetch: return "JavaScript (fetch)";
        case TargetLanguage::JavaScriptAxios: return "JavaScript (axios)";
        case TargetLanguage::GoHttp: return "Go (net/http)";
        case TargetLanguage::CppCurl: return "C++ (libcurl)";
        case TargetLanguage::RustReqwest: return "Rust (reqwest)";
        case TargetLanguage::CSharpHttpClient: return "C# (HttpClient)";
        case TargetLanguage::JavaHttpClient: return "Java (java.net.http)";
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
        case TargetLanguage::RustReqwest: return generateRust(req);
        case TargetLanguage::CSharpHttpClient: return generateCSharp(req);
        case TargetLanguage::JavaHttpClient: return generateJava(req);
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
    bool hasBody = (snippetHasBody(req));
    if (hasBody) {
        if (req.bodyType == BodyType::Json) {
            ts << "payload = " << snippetBody(req) << "\n\n";
        } else {
            ts << "payload = \"\"\"" << snippetBody(req) << "\"\"\"\n\n";
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

    if (snippetHasBody(req)) {
        if (req.bodyType == BodyType::Json) {
            ts << "  body: JSON.stringify(" << snippetBody(req) << "),\n";
        } else {
            ts << "  body: `" << snippetBody(req) << "`,\n";
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

    if (snippetHasBody(req)) {
        if (req.bodyType == BodyType::Json) {
            ts << "  data: " << snippetBody(req) << ",\n";
        } else {
            ts << "  data: `" << snippetBody(req) << "`,\n";
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
    if (snippetHasBody(req)) {
        ts << "    \"strings\"\n";
    }
    ts << ")\n\n";

    ts << "func main() {\n";
    ts << "    url := \"" << req.effectiveUrl() << "\"\n";

    bool hasBody = (snippetHasBody(req));
    if (hasBody) {
        QString escaped = snippetBody(req);
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

    if (snippetHasBody(req)) {
        QString escaped = snippetBody(req);
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

QString CodeGenerator::generateRust(const RequestModel& req) {
    QString out;
    QTextStream ts(&out);

    ts << "// Dependencies in Cargo.toml:\n";
    ts << "// reqwest = { version = \"0.11\", features = [\"json\"] }\n";
    ts << "// tokio = { version = \"1\", features = [\"full\"] }\n\n";
    ts << "use reqwest::header::{HeaderMap, HeaderName, HeaderValue};\n\n";
    ts << "#[tokio::main]\n";
    ts << "async fn main() -> Result<(), Box<dyn std::error::Error>> {\n";
    ts << "    let client = reqwest::Client::new();\n";

    auto headers = req.effectiveHeaders();
    if (!headers.isEmpty()) {
        ts << "    let mut headers = HeaderMap::new();\n";
        for (const auto& h : headers) {
            if (h.enabled && !h.name.isEmpty()) {
                ts << "    headers.insert(HeaderName::from_static(\"" << h.name.toLower() << "\"), HeaderValue::from_str(\"" << h.value << "\")?);\n";
            }
        }
        ts << "\n";
    }

    QString methodStr = methodToString(req.method).toLower();
    ts << "    let response = client\n";
    ts << "        ." << (methodStr == "delete" ? "delete" : methodStr) << "(\"" << req.effectiveUrl() << "\")\n";
    if (!headers.isEmpty()) {
        ts << "        .headers(headers)\n";
    }

    if (snippetHasBody(req)) {
        QString escaped = snippetBody(req);
        escaped.replace("\"", "\\\"");
        escaped.replace("\n", "\\n");
        ts << "        .body(\"" << escaped << "\")\n";
    }

    ts << "        .send()\n";
    ts << "        .await?;\n\n";
    ts << "    println!(\"Status: {}\", response.status());\n";
    ts << "    let body = response.text().await?;\n";
    ts << "    println!(\"Body: {}\", body);\n";
    ts << "    Ok(())\n";
    ts << "}\n";

    return out;
}

QString CodeGenerator::generateCSharp(const RequestModel& req) {
    QString out;
    QTextStream ts(&out);

    ts << "using System;\n";
    ts << "using System.Net.Http;\n";
    ts << "using System.Text;\n";
    ts << "using System.Threading.Tasks;\n\n";
    ts << "class Program {\n";
    ts << "    static async Task Main() {\n";
    ts << "        using var client = new HttpClient();\n";

    QString methodStr = methodToString(req.method);
    ts << "        var request = new HttpRequestMessage(new HttpMethod(\"" << methodStr << "\"), \"" << req.effectiveUrl() << "\");\n";

    auto headers = req.effectiveHeaders();
    for (const auto& h : headers) {
        if (h.enabled && !h.name.isEmpty()) {
            if (h.name.compare("Content-Type", Qt::CaseInsensitive) != 0) {
                ts << "        request.Headers.TryAddWithoutValidation(\"" << h.name << "\", \"" << h.value << "\");\n";
            }
        }
    }

    if (snippetHasBody(req)) {
        QString escaped = snippetBody(req);
        escaped.replace("\"", "\\\"");
        escaped.replace("\n", "\\n");
        QString mediaType = (req.bodyType == BodyType::Json) ? "application/json" : "text/plain";
        ts << "        request.Content = new StringContent(\"" << escaped << "\", Encoding.UTF8, \"" << mediaType << "\");\n";
    }

    ts << "\n        var response = await client.SendAsync(request);\n";
    ts << "        Console.WriteLine((int)response.StatusCode);\n";
    ts << "        var responseBody = await response.Content.ReadAsStringAsync();\n";
    ts << "        Console.WriteLine(responseBody);\n";
    ts << "    }\n";
    ts << "}\n";

    return out;
}

QString CodeGenerator::generateJava(const RequestModel& req) {
    QString out;
    QTextStream ts(&out);

    ts << "import java.net.URI;\n";
    ts << "import java.net.http.HttpClient;\n";
    ts << "import java.net.http.HttpRequest;\n";
    ts << "import java.net.http.HttpResponse;\n\n";
    ts << "public class Main {\n";
    ts << "    public static void main(String[] args) throws Exception {\n";
    ts << "        HttpClient client = HttpClient.newHttpClient();\n";
    ts << "        HttpRequest.Builder builder = HttpRequest.newBuilder()\n";
    ts << "            .uri(URI.create(\"" << req.effectiveUrl() << "\"));\n\n";

    auto headers = req.effectiveHeaders();
    for (const auto& h : headers) {
        if (h.enabled && !h.name.isEmpty()) {
            ts << "        builder.header(\"" << h.name << "\", \"" << h.value << "\");\n";
        }
    }

    QString methodStr = methodToString(req.method);
    if (snippetHasBody(req)) {
        QString escaped = snippetBody(req);
        escaped.replace("\"", "\\\"");
        escaped.replace("\n", "\\n");
        ts << "        builder.method(\"" << methodStr << "\", HttpRequest.BodyPublishers.ofString(\"" << escaped << "\"));\n";
    } else if (req.method == HttpMethod::POST || req.method == HttpMethod::PUT || req.method == HttpMethod::PATCH) {
        ts << "        builder.method(\"" << methodStr << "\", HttpRequest.BodyPublishers.noBody());\n";
    } else if (req.method == HttpMethod::GET) {
        ts << "        builder.GET();\n";
    } else {
        ts << "        builder.method(\"" << methodStr << "\", HttpRequest.BodyPublishers.noBody());\n";
    }

    ts << "        HttpRequest request = builder.build();\n";
    ts << "        HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());\n\n";
    ts << "        System.out.println(response.statusCode());\n";
    ts << "        System.out.println(response.body());\n";
    ts << "    }\n";
    ts << "}\n";

    return out;
}

} // namespace poppy::core
