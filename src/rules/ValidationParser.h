#ifndef _validation_parser_h
#define _validation_parser_h

#include "ValidationRule.h"
#include <unordered_map>

std::unordered_map<std::string, std::vector<ValidationRule>> parseRules(std::unordered_map<std::string, std::string> rules);
std::string GenerateColumnType(std::string column_name, std::vector<ValidationRule> rules);

#endif