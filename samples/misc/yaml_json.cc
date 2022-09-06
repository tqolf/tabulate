#include <map>
#include <vector>
#include <json/json.h>
#include <yaml-cpp/yaml.h>
#include <google/protobuf/message.h>
#include <google/protobuf/unknown_field_set.h>

bool YamlToJson(const YAML::Node &ynode, Json::Value &jnode)
{
    try {
        if (ynode.IsScalar()) {
            Json::Value v(ynode.Scalar());
            jnode.swapPayload(v);
            return true;
        } else if (ynode.IsSequence()) {
            for (size_t i = 0; i < ynode.size(); ++i) {
                Json::Value v;
                if (YamlToJson(ynode[i], v)) {
                    jnode.append(std::move(v));
                } else {
                    return false;
                }
            }
        } else if (ynode.IsMap()) {
            for (auto it = ynode.begin(); it != ynode.end(); ++it) {
                Json::Value v;
                if (YamlToJson(it->second, v)) {
                    jnode[it->first.Scalar()] = std::move(v);
                } else {
                    return false;
                }
            }
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool JsonToYaml(const Json::Value &jnode, YAML::Node &ynode)
{
    try {
        if (jnode.isArray()) {
            int n = jnode.size();
            for (int i = 0; i < n; ++i) {
                YAML::Node v;
                if (JsonToYaml(jnode[i], v)) {
                    ynode.push_back(std::move(v));
                } else {
                    return false;
                }
            }
        } else if (jnode.isObject()) {
            for (auto it = jnode.begin(); it != jnode.end(); ++it) {
                YAML::Node v;
                if (JsonToYaml(*it, v)) {
                    ynode[it.name()] = std::move(v);
                } else {
                    return false;
                }
            }
        } else {
            ynode = jnode.asString();
        }
    } catch (...) {
        return false;
    }
    return true;
}

static void MessageToJson_UnknowFieldSet(const google::protobuf::UnknownFieldSet &ufs, Json::Value &jnode)
{
    std::map<int, std::vector<Json::Value>> kvs;
    for (int i = 0; i < ufs.field_count(); ++i) {
        const auto &uf = ufs.field(i);
        switch ((int)uf.type()) {
            case google::protobuf::UnknownField::TYPE_VARINT:
                kvs[uf.number()].push_back((Json::Int64)uf.varint());
                break;
            case google::protobuf::UnknownField::TYPE_FIXED32:
                kvs[uf.number()].push_back((Json::UInt)uf.fixed32());
                break;
            case google::protobuf::UnknownField::TYPE_FIXED64:
                kvs[uf.number()].push_back((Json::UInt64)uf.fixed64());
                break;
            case google::protobuf::UnknownField::TYPE_LENGTH_DELIMITED:
                google::protobuf::UnknownFieldSet tmp;
                auto &v = uf.length_delimited();
                if (!v.empty() && tmp.ParseFromString(v)) {
                    Json::Value vv;
                    MessageToJson_UnknowFieldSet(tmp, vv);
                    kvs[uf.number()].push_back(std::move(vv));
                } else {
                    kvs[uf.number()].push_back(v);
                }
                break;
        }
    }

    for (auto &i : kvs) {
        if (i.second.size() > 1) {
            for (auto &n : i.second) {
                jnode[std::to_string(i.first)].append(std::move(n));
            }
        } else {
            jnode[std::to_string(i.first)] = std::move(i.second[0]);
        }
    }
}

void MessageToJson(const google::protobuf::Message &message, Json::Value &jnode)
{
    const google::protobuf::Descriptor *descriptor = message.GetDescriptor();
    const google::protobuf::Reflection *reflection = message.GetReflection();

    for (int i = 0; i < descriptor->field_count(); ++i) {
        const google::protobuf::FieldDescriptor *field = descriptor->field(i);

        if (field->is_repeated()) {
            if (!reflection->FieldSize(message, field)) {
                continue;
            }
        } else {
            if (!reflection->HasField(message, field)) {
                continue;
            }
        }

        // Repeated fields
        if (field->is_repeated()) {
            switch (field->cpp_type()) {
#define XX(cpptype, method, jsontype)                                                                  \
    case google::protobuf::FieldDescriptor::CPPTYPE_##cpptype: {                                       \
        int size = reflection->FieldSize(message, field);                                              \
        for (int n = 0; n < size; ++n) {                                                               \
            jnode[field->name()].append((jsontype)reflection->GetRepeated##method(message, field, n)); \
        }                                                                                              \
        break;                                                                                         \
    }
                XX(INT32, Int32, Json::Int);
                XX(UINT32, UInt32, Json::UInt);
                XX(FLOAT, Float, double);
                XX(DOUBLE, Double, double);
                XX(BOOL, Bool, bool);
                XX(INT64, Int64, Json::Int64);
                XX(UINT64, UInt64, Json::UInt64);
#undef XX
                case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
                    int size = reflection->FieldSize(message, field);
                    for (int n = 0; n < size; ++n) {
                        jnode[field->name()].append(reflection->GetRepeatedEnum(message, field, n)->number());
                    }
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                    int size = reflection->FieldSize(message, field);
                    for (int n = 0; n < size; ++n) {
                        jnode[field->name()].append(reflection->GetRepeatedString(message, field, n));
                    }
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                    int size = reflection->FieldSize(message, field);
                    for (int n = 0; n < size; ++n) {
                        Json::Value vv;
                        MessageToJson(reflection->GetRepeatedMessage(message, field, n), vv);
                        jnode[field->name()].append(vv);
                    }
                    break;
                }
            }
            continue;
        }

        switch (field->cpp_type()) {
#define XX(cpptype, method, jsontype)                                             \
    case google::protobuf::FieldDescriptor::CPPTYPE_##cpptype: {                  \
        jnode[field->name()] = (jsontype)reflection->Get##method(message, field); \
        break;                                                                    \
    }
            XX(INT32, Int32, Json::Int);
            XX(UINT32, UInt32, Json::UInt);
            XX(FLOAT, Float, double);
            XX(DOUBLE, Double, double);
            XX(BOOL, Bool, bool);
            XX(INT64, Int64, Json::Int64);
            XX(UINT64, UInt64, Json::UInt64);
#undef XX
            case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
                jnode[field->name()] = reflection->GetEnum(message, field)->number();
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                jnode[field->name()] = reflection->GetString(message, field);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                MessageToJson(reflection->GetMessage(message, field), jnode[field->name()]);
                break;
            }
        }
    }

    MessageToJson_UnknowFieldSet(reflection->GetUnknownFields(message), jnode);
}

#include "cxxopt.h"
#include <iostream>

int main()
{
    std::vector<std::string> from_ = cxxopt::details::split<std::string>(getarg("json:file", "--from"), ":");
    std::vector<std::string> to_ = cxxopt::details::split<std::string>(getarg("proto:file", "--to"), ":");

    std::cout << "from: "
              << "format=" << from_[0] << ", file=" << from_[1] << std::endl;
    std::cout << "to: "
              << "format=" << to_[0] << ", file=" << to_[1] << std::endl;
    if (from_[0] == "json" && to_[0] == "yaml") {
        Json::Value jnode;
        YAML::Node ynode;
        if (JsonToYaml(jnode, ynode)) {
            std::cout << "success" << std::endl;
        }
    } else if (from_[0] == "yaml" && to_[0] == "json") {
        Json::Value jnode;
        YAML::Node ynode;
        if (YamlToJson(ynode, jnode)) {
            std::cout << "success" << std::endl;
        }
    } else if (from_[0] == "proto" && to_[0] == "json") {
        Json::Value jnode;
        // google::protobuf::Message message;
        // MessageToJson(message, jnode);
    }

    return 0;
}
