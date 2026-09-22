#include "DocGenerator.h"
#include <QFile>
#include <QTextStream>
#include <core/codegen/CodeGenerator.h>

namespace poppy::core {

static QString escapeHtml(const QString& str) {
    QString res = str;
    res.replace("&", "&amp;");
    res.replace("<", "&lt;");
    res.replace(">", "&gt;");
    res.replace("\"", "&quot;");
    res.replace("'", "&#39;");
    return res;
}

QString DocGenerator::generateHtml(const QList<RequestModel>& requests,
                                   const QString& collectionName,
                                   const QString& description,
                                   const VariableResolver* resolver)
{
    QString title = collectionName.isEmpty() ? "API Documentation" : collectionName;

    QString html;
    html += "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    html += "  <meta charset=\"UTF-8\">\n";
    html += "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    html += QString("  <title>%1 - Documentation</title>\n").arg(escapeHtml(title));
    html += "  <style>\n";
    html += "    :root {\n";
    html += "      --bg: #09090b; --card: #18181b; --border: #27272a; --fg: #fafafa; --muted: #a1a1aa;\n";
    html += "      --primary: #3b82f6; --success: #10b981; --warning: #f59e0b; --danger: #ef4444; --code-bg: #121214;\n";
    html += "    }\n";
    html += "    * { box-sizing: border-box; margin: 0; padding: 0; }\n";
    html += "    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background: var(--bg); color: var(--fg); display: flex; height: 100vh; overflow: hidden; }\n";
    html += "    sidebar { width: 300px; border-right: 1px solid var(--border); display: flex; flex-direction: column; background: #0c0c0e; }\n";
    html += "    .sidebar-header { padding: 1.25rem 1rem; border-bottom: 1px solid var(--border); }\n";
    html += "    .sidebar-header h1 { font-size: 1.1rem; font-weight: 700; color: var(--fg); }\n";
    html += "    .sidebar-header p { font-size: 0.8rem; color: var(--muted); margin-top: 0.25rem; }\n";
    html += "    .search-box { padding: 0.75rem 1rem; border-bottom: 1px solid var(--border); }\n";
    html += "    .search-box input { width: 100%; background: var(--card); border: 1px solid var(--border); border-radius: 6px; padding: 0.5rem 0.75rem; color: var(--fg); font-size: 0.85rem; outline: none; }\n";
    html += "    .nav-list { flex: 1; overflow-y: auto; padding: 0.5rem; }\n";
    html += "    .nav-item { display: flex; align-items: center; gap: 0.5rem; padding: 0.5rem 0.75rem; border-radius: 6px; color: var(--fg); text-decoration: none; font-size: 0.85rem; margin-bottom: 2px; transition: background 0.15s; }\n";
    html += "    .nav-item:hover { background: var(--card); }\n";
    html += "    .badge { font-size: 0.7rem; font-weight: 800; padding: 2px 6px; border-radius: 4px; text-transform: uppercase; }\n";
    html += "    .badge-GET { background: rgba(16, 185, 129, 0.15); color: #34d399; }\n";
    html += "    .badge-POST { background: rgba(59, 130, 246, 0.15); color: #60a5fa; }\n";
    html += "    .badge-PUT { background: rgba(245, 158, 11, 0.15); color: #fbbf24; }\n";
    html += "    .badge-DELETE { background: rgba(239, 68, 68, 0.15); color: #f87171; }\n";
    html += "    .badge-PATCH { background: rgba(168, 85, 247, 0.15); color: #c084fc; }\n";
    html += "    main { flex: 1; overflow-y: auto; padding: 2.5rem 3rem; scroll-behavior: smooth; }\n";
    html += "    .hero { margin-bottom: 3rem; padding-bottom: 2rem; border-bottom: 1px solid var(--border); }\n";
    html += "    .hero h1 { font-size: 2rem; font-weight: 800; letter-spacing: -0.02em; }\n";
    html += "    .hero p { color: var(--muted); margin-top: 0.5rem; font-size: 1rem; }\n";
    html += "    .endpoint-card { background: var(--card); border: 1px solid var(--border); border-radius: 8px; margin-bottom: 2.5rem; overflow: hidden; }\n";
    html += "    .endpoint-header { padding: 1.25rem 1.5rem; border-bottom: 1px solid var(--border); display: flex; align-items: center; gap: 1rem; flex-wrap: wrap; }\n";
    html += "    .endpoint-title { font-size: 1.15rem; font-weight: 700; width: 100%; margin-bottom: 0.25rem; }\n";
    html += "    .endpoint-url { font-family: monospace; font-size: 0.95rem; color: #e4e4e7; background: #0c0c0e; padding: 4px 10px; border-radius: 4px; border: 1px solid var(--border); }\n";
    html += "    .endpoint-body { padding: 1.5rem; }\n";
    html += "    h3 { font-size: 0.9rem; font-weight: 700; text-transform: uppercase; letter-spacing: 0.05em; color: var(--muted); margin: 1.25rem 0 0.5rem 0; }\n";
    html += "    h3:first-child { margin-top: 0; }\n";
    html += "    table { width: 100%; border-collapse: collapse; margin-bottom: 1rem; font-size: 0.85rem; }\n";
    html += "    th, td { text-align: left; padding: 0.6rem 0.75rem; border-bottom: 1px solid var(--border); }\n";
    html += "    th { color: var(--muted); font-weight: 600; }\n";
    html += "    pre { background: var(--code-bg); border: 1px solid var(--border); border-radius: 6px; padding: 1rem; font-family: monospace; font-size: 0.85rem; overflow-x: auto; color: #f4f4f5; }\n";
    html += "    .code-tabs { margin-top: 0.5rem; }\n";
    html += "    .tab-btn { background: none; border: none; color: var(--muted); padding: 0.4rem 0.8rem; font-size: 0.8rem; font-weight: 600; cursor: pointer; border-bottom: 2px solid transparent; }\n";
    html += "    .tab-btn.active { color: var(--primary); border-bottom-color: var(--primary); }\n";
    html += "    @media (max-width: 800px) { body { flex-direction: column; height: auto; overflow: auto; } sidebar { width: 100%; height: auto; } }\n";
    html += "  </style>\n";
    html += "</head>\n<body>\n";

    // Sidebar
    html += "<sidebar>\n";
    html += "  <div class=\"sidebar-header\">\n";
    html += QString("    <h1>%1</h1>\n").arg(escapeHtml(title));
    html += QString("    <p>%1 endpoints documented</p>\n").arg(requests.size());
    html += "  </div>\n";
    html += "  <div class=\"search-box\">\n";
    html += "    <input type=\"text\" placeholder=\"Filter endpoints...\" oninput=\"filterEndpoints(this.value)\">\n";
    html += "  </div>\n";
    html += "  <div class=\"nav-list\" id=\"navList\">\n";

    for (int i = 0; i < requests.size(); ++i) {
        const auto& req = requests[i];
        QString methodStr = methodToString(req.method);
        QString reqName = req.name.isEmpty() ? req.url : req.name;
        html += QString("    <a href=\"#endpoint-%1\" class=\"nav-item\">\n").arg(i);
        html += QString("      <span class=\"badge badge-%1\">%1</span>\n").arg(methodStr);
        html += QString("      <span class=\"nav-name\">%1</span>\n").arg(escapeHtml(reqName));
        html += "    </a>\n";
    }

    html += "  </div>\n";
    html += "</sidebar>\n";

    // Main Content
    html += "<main>\n";
    html += "  <div class=\"hero\">\n";
    html += QString("    <h1>%1</h1>\n").arg(escapeHtml(title));
    if (!description.isEmpty()) {
        html += QString("    <p>%1</p>\n").arg(escapeHtml(description));
    } else {
        html += "    <p>API documentation generated directly from Poppy collections.</p>\n";
    }
    html += "  </div>\n";

    for (int i = 0; i < requests.size(); ++i) {
        RequestModel req = resolver ? resolver->resolveRequest(requests[i]) : requests[i];
        QString methodStr = methodToString(req.method);
        QString reqName = req.name.isEmpty() ? req.url : req.name;

        html += QString("  <div class=\"endpoint-card\" id=\"endpoint-%1\">\n").arg(i);
        html += "    <div class=\"endpoint-header\">\n";
        html += QString("      <div class=\"endpoint-title\">%1</div>\n").arg(escapeHtml(reqName));
        html += QString("      <span class=\"badge badge-%1\">%1</span>\n").arg(methodStr);
        html += QString("      <span class=\"endpoint-url\">%1</span>\n").arg(escapeHtml(req.url));
        html += "    </div>\n";
        html += "    <div class=\"endpoint-body\">\n";
        if (!req.description.isEmpty()) {
            html += QString("      <p style=\"color:var(--muted); margin-bottom:1rem; font-size:0.9rem;\">%1</p>\n").arg(escapeHtml(req.description));
        }

        // Query parameters
        if (!req.queryParams.isEmpty()) {
            html += "      <h3>Query Parameters</h3>\n";
            html += "      <table>\n<tr><th>Parameter</th><th>Value</th><th>Description</th></tr>\n";
            for (const auto& q : req.queryParams) {
                if (!q.enabled) continue;
                html += QString("<tr><td><code>%1</code></td><td>%2</td><td>%3</td></tr>\n")
                            .arg(escapeHtml(q.key), escapeHtml(q.value), escapeHtml(q.description));
            }
            html += "      </table>\n";
        }

        // Headers
        if (!req.headers.isEmpty()) {
            html += "      <h3>Headers</h3>\n";
            html += "      <table>\n<tr><th>Header</th><th>Value</th><th>Description</th></tr>\n";
            for (const auto& h : req.headers) {
                if (!h.enabled) continue;
                html += QString("<tr><td><code>%1</code></td><td>%2</td><td>%3</td></tr>\n")
                            .arg(escapeHtml(h.name), escapeHtml(h.value), escapeHtml(h.description));
            }
            html += "      </table>\n";
        }

        // Body
        QString bodyText = req.bodyContent;
        if (req.bodyType == BodyType::GraphQL) {
            bodyText = QString::fromUtf8(req.effectiveBody());
        } else if (req.bodyType == BodyType::MultipartForm) {
            QStringList parts;
            for (const auto& p : req.formDataParams) {
                if (p.enabled && !p.key.isEmpty()) {
                    parts.append(p.key + "=" + p.value);
                }
            }
            bodyText = parts.join("\n");
        }
        if (!bodyText.isEmpty()) {
            html += "      <h3>Request Body</h3>\n";
            html += QString("      <pre><code>%1</code></pre>\n").arg(escapeHtml(bodyText));
        }

        // Code Snippets
        html += "      <h3>Example Requests</h3>\n";
        html += QString("      <div class=\"code-tabs\" id=\"tabs-%1\">\n").arg(i);

        QString curlSnippet = CodeGenerator::generate(TargetLanguage::Curl, req);
        QString pySnippet = CodeGenerator::generate(TargetLanguage::PythonRequests, req);
        QString jsSnippet = CodeGenerator::generate(TargetLanguage::JavaScriptFetch, req);

        html += QString("        <button class=\"tab-btn active\" onclick=\"showCode(%1, 'curl')\">cURL</button>\n").arg(i);
        html += QString("        <button class=\"tab-btn\" onclick=\"showCode(%1, 'py')\">Python</button>\n").arg(i);
        html += QString("        <button class=\"tab-btn\" onclick=\"showCode(%1, 'js')\">JavaScript</button>\n").arg(i);

        html += QString("        <pre id=\"code-%1-curl\"><code>%2</code></pre>\n").arg(i).arg(escapeHtml(curlSnippet));
        html += QString("        <pre id=\"code-%1-py\" style=\"display:none;\"><code>%2</code></pre>\n").arg(i).arg(escapeHtml(pySnippet));
        html += QString("        <pre id=\"code-%1-js\" style=\"display:none;\"><code>%2</code></pre>\n").arg(i).arg(escapeHtml(jsSnippet));
        html += "      </div>\n";

        html += "    </div>\n";
        html += "  </div>\n";
    }

    html += "</main>\n";

    // Script
    html += "<script>\n";
    html += "  function filterEndpoints(q) {\n";
    html += "    var items = document.querySelectorAll('.nav-item');\n";
    html += "    var search = q.toLowerCase();\n";
    html += "    items.forEach(function(el) {\n";
    html += "      var text = el.textContent.toLowerCase();\n";
    html += "      el.style.display = text.indexOf(search) !== -1 ? 'flex' : 'none';\n";
    html += "    });\n";
    html += "  }\n";
    html += "  function showCode(id, lang) {\n";
    html += "    var langs = ['curl', 'py', 'js'];\n";
    html += "    langs.forEach(function(l) {\n";
    html += "      var el = document.getElementById('code-' + id + '-' + l);\n";
    html += "      if (el) el.style.display = (l === lang) ? 'block' : 'none';\n";
    html += "    });\n";
    html += "    var container = document.getElementById('tabs-' + id);\n";
    html += "    if (container) {\n";
    html += "      var btns = container.querySelectorAll('.tab-btn');\n";
    html += "      btns.forEach(function(b) {\n";
    html += "        var isMatch = b.getAttribute('onclick').indexOf(lang) !== -1;\n";
    html += "        b.className = isMatch ? 'tab-btn active' : 'tab-btn';\n";
    html += "      });\n";
    html += "    }\n";
    html += "  }\n";
    html += "</script>\n";
    html += "</body>\n</html>\n";

    return html;
}

bool DocGenerator::exportToFile(const QString& filePath,
                               const QList<RequestModel>& requests,
                               const QString& collectionName,
                               const QString& description,
                               const VariableResolver* resolver,
                               QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QString content = generateHtml(requests, collectionName, description, resolver);
    QTextStream out(&file);
    out << content;
    file.close();
    return true;
}

} // namespace poppy::core
