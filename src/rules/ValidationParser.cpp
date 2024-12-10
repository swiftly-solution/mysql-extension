#include "ValidationParser.h"
#include "../utils.h"
#include "../entrypoint.h"
#include <set>

ValidationRule ParseRule(std::string ruleString)
{
    ValidationRule rule;
    auto parts = explode(ruleString, ":");
    rule.name = parts[0];
    parts.erase(parts.begin());
    rule.param = implode(parts, ":");
    return rule;
}

std::unordered_map<std::string, std::vector<ValidationRule>> parseRules(std::unordered_map<std::string, std::string> rules)
{
    std::unordered_map<std::string, std::vector<ValidationRule>> parsedRules;

    for(auto it = rules.begin(); it != rules.end(); ++it) {
        auto rls = explode(it->second, "|");
        std::vector<ValidationRule> validatedRules;

        for(auto rl : rls)
            validatedRules.push_back(ParseRule(rl));

        parsedRules.insert({ it->first, validatedRules });
    }

    return parsedRules;
}

std::string GenerateColumnType(std::string column_name, std::vector<ValidationRule> rules)
{
    std::string ret_type = "VARCHAR(255)";
    std::string defaultValue = "";

    bool defaultSet = false;
    bool isNullable = false;
    bool isInteger = false;
    bool unique = false;
    bool primary_key = false;
    bool index = false;
    bool auto_incremenet = false;

    for(auto rule : rules) {
        if(rule.name == "nullable")
            isNullable = true;
        else if(rule.name == "integer") {
            ret_type = "INT";
            isInteger = true;
        } else if(rule.name == "string") {
            ret_type = "VARCHAR(255)";
        } else if(rule.name == "boolean") {
            ret_type = "TINYINT(1)";
        } else if(rule.name == "date") {
            ret_type = "DATE";
        } else if(rule.name == "datetime") {
            ret_type = "DATETIME";
        } else if(rule.name == "min" && !rule.param.empty()) {
            int val = V_StringToInt32(rule.param.c_str(), 0);
            if(isInteger && val >= 0) {
                ret_type = "UNSIGNED INT";
            }
        } else if((rule.name == "max" || rule.name == "size") && !rule.param.empty()) {
            int maxVal = V_StringToInt32(rule.param.c_str(), 0);
            if(ret_type.find("VARCHAR") != std::string::npos)
                ret_type = string_format("VARCHAR(%d)", maxVal);
        } else if(rule.name == "json") {
            ret_type = "JSON";
            defaultValue = "{}";
            defaultSet = true;
        } else if(rule.name == "float") {
            ret_type = "FLOAT";
        } else if(rule.name == "default") {
            defaultValue = rule.param;
            defaultSet = true;
        } else if(rule.name == "unique") {
            unique = true;
            primary_key = false;
            index = false;
        } else if(rule.name == "primary") {
            primary_key = true;
            unique = false;
            index = false;
        } else if(rule.name == "index") {
            primary_key = false;
            unique = false;
            index = true;
        } else if(rule.name == "autoincrement") {
            auto_incremenet = true;
        }
    }

    if(isNullable) {
        ret_type += " NULL";
    } else {
        ret_type += " NOT NULL";
    }

    if(defaultSet)
        ret_type += " DEFAULT '"+ defaultValue +"'";

    if(auto_incremenet)
        ret_type += " AUTO_INCREMENT";

    if(primary_key)
        ret_type += ", PRIMARY KEY (`" + column_name + "`)";

    if(unique)
        ret_type += ", UNIQUE (`" + column_name + "`)";

    if(index)
        ret_type += ", INDEX (`" + column_name + "`)";

    return ret_type;
}