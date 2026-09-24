#pragma once

#include <QString>
#include <QList>
#include <QMap>

namespace poppy::core {

enum class YamlNodeType {
    Null,
    Scalar,
    Sequence,
    Mapping
};

class YamlNode {
public:
    YamlNodeType type = YamlNodeType::Null;
    QString scalarValue;
    QList<YamlNode> sequence;
    QMap<QString, YamlNode> mapping;

    YamlNode() = default;
    explicit YamlNode(YamlNodeType t) : type(t) {}
    explicit YamlNode(QString val) : type(YamlNodeType::Scalar), scalarValue(std::move(val)) {}

    bool isNull() const { return type == YamlNodeType::Null; }
    bool isScalar() const { return type == YamlNodeType::Scalar; }
    bool isSequence() const { return type == YamlNodeType::Sequence; }
    bool isMapping() const { return type == YamlNodeType::Mapping; }

    QString asString(const QString& fallback = QString()) const {
        if (type == YamlNodeType::Scalar) return scalarValue;
        return fallback;
    }

    int asInt(int fallback = 0) const {
        if (type == YamlNodeType::Scalar) {
            bool ok = false;
            int v = scalarValue.toInt(&ok);
            if (ok) return v;
        }
        return fallback;
    }

    bool asBool(bool fallback = false) const {
        if (type == YamlNodeType::Scalar) {
            QString s = scalarValue.trimmed().toLower();
            if (s == "true" || s == "yes" || s == "on" || s == "1") return true;
            if (s == "false" || s == "no" || s == "off" || s == "0") return false;
        }
        return fallback;
    }

    bool hasKey(const QString& key) const {
        return type == YamlNodeType::Mapping && mapping.contains(key);
    }

    const YamlNode& operator[](const QString& key) const;
    const YamlNode& operator[](int index) const;

    static YamlNode parse(const QString& yamlText);
    static YamlNode parseFile(const QString& filePath);

private:
    static const YamlNode s_nullNode;
};

} // namespace poppy::core
