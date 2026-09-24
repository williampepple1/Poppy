#include "YamlNode.h"
#include <QFile>
#include <QTextStream>
#include <QStringList>

namespace poppy::core {

const YamlNode YamlNode::s_nullNode;

const YamlNode& YamlNode::operator[](const QString& key) const {
    if (type == YamlNodeType::Mapping) {
        auto it = mapping.find(key);
        if (it != mapping.end()) {
            return it.value();
        }
    }
    return s_nullNode;
}

const YamlNode& YamlNode::operator[](int index) const {
    if (type == YamlNodeType::Sequence && index >= 0 && index < sequence.size()) {
        return sequence[index];
    }
    return s_nullNode;
}

namespace {

int getIndent(const QString& line) {
    int count = 0;
    for (const QChar& ch : line) {
        if (ch == ' ') count++;
        else if (ch == '\t') count += 2;
        else break;
    }
    return count;
}

QString unquote(QString s) {
    s = s.trimmed();
    if (s.size() >= 2) {
        QChar q = s[0];
        if ((q == '\'' || q == '"') && s[s.size() - 1] == q) {
            QString inner = s.mid(1, s.size() - 2);
            if (q == '\'') {
                inner.replace("''", "'");
                return inner;
            } else {
                QString result;
                for (int i = 0; i < inner.size(); ++i) {
                    if (inner[i] == '\\' && i + 1 < inner.size()) {
                        QChar next = inner[++i];
                        if (next == 'n') result += '\n';
                        else if (next == 'r') result += '\r';
                        else if (next == 't') result += '\t';
                        else if (next == '"') result += '"';
                        else if (next == '\\') result += '\\';
                        else result += next;
                    } else {
                        result += inner[i];
                    }
                }
                return result;
            }
        }
    }
    return s;
}

QString stripComment(const QString& str) {
    QString s = str.trimmed();
    if (s.startsWith('\'') || s.startsWith('"')) {
        return s;
    }
    int hashIdx = s.indexOf('#');
    if (hashIdx >= 0) {
        s = s.left(hashIdx).trimmed();
    }
    return s;
}

YamlNode parseScalarOrFlow(const QString& rawValue) {
    QString s = stripComment(rawValue);
    if (s.startsWith('[') && s.endsWith(']')) {
        YamlNode seq(YamlNodeType::Sequence);
        QString inside = s.mid(1, s.size() - 2).trimmed();
        if (!inside.isEmpty()) {
            QStringList tokens = inside.split(',');
            for (const QString& tok : tokens) {
                QString t = tok.trimmed();
                if (!t.isEmpty()) {
                    seq.sequence.append(YamlNode(unquote(t)));
                }
            }
        }
        return seq;
    }
    return YamlNode(unquote(s));
}

bool isBlockScalarIndicator(const QString& val) {
    QString v = stripComment(val);
    return v == "|-" || v == "|" || v == ">" || v == ">-";
}

struct ParserState {
    QStringList lines;
    int index = 0;
};

YamlNode parseBlock(ParserState& state, int minIndent);

YamlNode parseSequence(ParserState& state, int baseIndent) {
    YamlNode seq(YamlNodeType::Sequence);

    while (state.index < state.lines.size()) {
        const QString& line = state.lines[state.index];
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) {
            state.index++;
            continue;
        }

        int ind = getIndent(line);
        if (ind < baseIndent) {
            break;
        }
        if (ind > baseIndent) {
            // Unexpected indentation, skip
            state.index++;
            continue;
        }

        QString rest = line.mid(ind);
        if (!rest.startsWith("- ") && rest != "-") {
            break;
        }

        QString valPart = rest.mid(1).trimmed();
        state.index++;

        if (valPart.isEmpty()) {
            // Child block underneath
            seq.sequence.append(parseBlock(state, baseIndent + 1));
        } else if (valPart.contains(':') && !valPart.startsWith('"') && !valPart.startsWith('\'')) {
            // Sequence item is a mapping, e.g.:
            // - name: foo
            //   value: bar
            YamlNode mapNode(YamlNodeType::Mapping);
            int colonIdx = valPart.indexOf(':');
            QString k = unquote(valPart.left(colonIdx).trimmed());
            QString v = valPart.mid(colonIdx + 1).trimmed();

            if (isBlockScalarIndicator(v)) {
                // Collect block scalar lines
                QStringList scalarLines;
                int blockIndent = -1;
                while (state.index < state.lines.size()) {
                    const QString& bl = state.lines[state.index];
                    QString bt = bl.trimmed();
                    if (bt.isEmpty()) {
                        scalarLines.append(QString());
                        state.index++;
                        continue;
                    }
                    int bind = getIndent(bl);
                    if (bind <= baseIndent + 1) break;
                    if (blockIndent < 0) blockIndent = bind;
                    scalarLines.append(bl.mid(blockIndent));
                    state.index++;
                }
                mapNode.mapping[k] = YamlNode(scalarLines.join('\n'));
            } else if (v.isEmpty()) {
                mapNode.mapping[k] = parseBlock(state, baseIndent + 2);
            } else {
                mapNode.mapping[k] = parseScalarOrFlow(v);
            }

            // Read sibling keys of this mapping element
            while (state.index < state.lines.size()) {
                const QString& nl = state.lines[state.index];
                QString nt = nl.trimmed();
                if (nt.isEmpty() || nt.startsWith('#')) {
                    state.index++;
                    continue;
                }
                int nind = getIndent(nl);
                if (nind <= baseIndent) break;

                QString nrest = nl.mid(nind);
                if (nrest.startsWith("- ") || nrest == "-") break;

                int ncol = nrest.indexOf(':');
                if (ncol < 0) {
                    state.index++;
                    continue;
                }

                state.index++;
                QString nk = unquote(nrest.left(ncol).trimmed());
                QString nv = nrest.mid(ncol + 1).trimmed();

                if (isBlockScalarIndicator(nv)) {
                    QStringList scalarLines;
                    int blockIndent = -1;
                    while (state.index < state.lines.size()) {
                        const QString& bl = state.lines[state.index];
                        QString bt = bl.trimmed();
                        if (bt.isEmpty()) {
                            scalarLines.append(QString());
                            state.index++;
                            continue;
                        }
                        int bind = getIndent(bl);
                        if (bind <= nind) break;
                        if (blockIndent < 0) blockIndent = bind;
                        scalarLines.append(bl.mid(blockIndent));
                        state.index++;
                    }
                    mapNode.mapping[nk] = YamlNode(scalarLines.join('\n'));
                } else if (nv.isEmpty()) {
                    mapNode.mapping[nk] = parseBlock(state, nind + 1);
                } else {
                    mapNode.mapping[nk] = parseScalarOrFlow(nv);
                }
            }

            seq.sequence.append(mapNode);
        } else {
            seq.sequence.append(parseScalarOrFlow(valPart));
        }
    }

    return seq;
}

YamlNode parseMapping(ParserState& state, int baseIndent) {
    YamlNode mapNode(YamlNodeType::Mapping);

    while (state.index < state.lines.size()) {
        const QString& line = state.lines[state.index];
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) {
            state.index++;
            continue;
        }

        int ind = getIndent(line);
        if (ind < baseIndent) {
            break;
        }
        if (ind > baseIndent) {
            state.index++;
            continue;
        }

        QString rest = line.mid(ind);
        if (rest.startsWith("- ") || rest == "-") {
            break;
        }

        int colonIdx = rest.indexOf(':');
        if (colonIdx < 0) {
            state.index++;
            continue;
        }

        QString k = unquote(rest.left(colonIdx).trimmed());
        QString v = rest.mid(colonIdx + 1).trimmed();
        state.index++;

        if (isBlockScalarIndicator(v)) {
            QStringList scalarLines;
            int blockIndent = -1;
            while (state.index < state.lines.size()) {
                const QString& bl = state.lines[state.index];
                QString bt = bl.trimmed();
                if (bt.isEmpty()) {
                    scalarLines.append(QString());
                    state.index++;
                    continue;
                }
                int bind = getIndent(bl);
                if (bind <= baseIndent) break;
                if (blockIndent < 0) blockIndent = bind;
                scalarLines.append(bl.mid(blockIndent));
                state.index++;
            }
            mapNode.mapping[k] = YamlNode(scalarLines.join('\n'));
        } else if (v.isEmpty()) {
            mapNode.mapping[k] = parseBlock(state, baseIndent + 1);
        } else {
            mapNode.mapping[k] = parseScalarOrFlow(v);
        }
    }

    return mapNode;
}

YamlNode parseBlock(ParserState& state, int minIndent) {
    while (state.index < state.lines.size()) {
        const QString& line = state.lines[state.index];
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) {
            state.index++;
            continue;
        }

        int ind = getIndent(line);
        if (ind < minIndent) {
            return YamlNode(YamlNodeType::Null);
        }

        QString rest = line.mid(ind);
        if (rest.startsWith("- ") || rest == "-") {
            return parseSequence(state, ind);
        } else if (rest.contains(':')) {
            return parseMapping(state, ind);
        } else {
            state.index++;
            return YamlNode(unquote(rest));
        }
    }

    return YamlNode(YamlNodeType::Null);
}

} // namespace

YamlNode YamlNode::parse(const QString& yamlText) {
    ParserState state;
    state.lines = yamlText.split('\n');
    return parseBlock(state, 0);
}

YamlNode YamlNode::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return YamlNode(YamlNodeType::Null);
    }
    QTextStream in(&file);
    return parse(in.readAll());
}

} // namespace poppy::core
