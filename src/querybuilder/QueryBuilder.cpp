#include "QueryBuilder.h"
#include "../entrypoint.h"
#include "../utils.h"
#include "../rules/ValidationParser.h"
#include <stdexcept>

#define ENSURE_TABLE_SET() \
    if (this->tableName.empty()) { \
        throw std::runtime_error("Table name must be set before executing the query."); \
    }

IQueryBuilder* QueryBuilder::Table(std::string tableName)
{
    this->tableName = tableName;
    return this;
}

IQueryBuilder* QueryBuilder::Create(std::unordered_map<std::string, std::string> columns)
{
    ENSURE_TABLE_SET();
    
    auto rules = parseRules(columns);
    std::vector<std::string> columnDefinitions;
    for (const auto& pair : rules)
    {
        auto type = GenerateColumnType(pair.first, pair.second);
        if(type == "") continue;
        columnDefinitions.push_back(pair.first + " " + type);
    }

    this->query = "CREATE TABLE IF NOT EXISTS " + this->tableName + " (" + implode(columnDefinitions, ", ") + ")";
    
    return this;
}

IQueryBuilder* QueryBuilder::Alter(std::map<std::string, std::string> columns)
{
    g_SMAPI->ConPrint("[MySQL - QueryBuilder] Alter is not currently implemented.\n");
    return this;
}

IQueryBuilder* QueryBuilder::Drop()
{
    ENSURE_TABLE_SET();
    this->query = "DROP TABLE IF EXISTS " + this->tableName;
    return this;
}

IQueryBuilder* QueryBuilder::Select(std::vector<std::string> columns)
{
    ENSURE_TABLE_SET();
    if (columns.empty()) {
        this->query = "SELECT * FROM " + this->tableName;
    } else {
        this->query = "SELECT " + implode(columns, ", ") + " FROM " + this->tableName;
    }
    return this;
}

IQueryBuilder* QueryBuilder::Insert(std::map<std::string, std::string> data)
{
    ENSURE_TABLE_SET();

    if (data.empty()) {
        throw std::invalid_argument("Insert requires at least one column-value pair.");
    }

    std::vector<std::string> columns;
    std::vector<std::string> values;

    for (const auto& pair : data) {
        columns.push_back(pair.first);
        values.push_back(pair.second);
    }

    this->query = "INSERT INTO " + tableName + " (" + implode(columns, ", ") + ") VALUES (" + implode(values, ", ") + ")";
    return this;
}

IQueryBuilder* QueryBuilder::Update(std::map<std::string, std::string> data)
{
    ENSURE_TABLE_SET();

    if (data.empty()) {
        throw std::invalid_argument("Update requires at least one column-value pair.");
    }
    std::vector<std::string> updates;
    for (const auto& pair : data) {
        updates.push_back(pair.first + " = " + pair.second);
    }

    this->query = "UPDATE " + tableName + " SET " + implode(updates, ", ");
    return this;
}

IQueryBuilder* QueryBuilder::Delete()
{
    ENSURE_TABLE_SET();

    this->query = "DELETE FROM " + tableName;
    return this;
}

IQueryBuilder* QueryBuilder::Where(std::string column, std::string operator_, std::string value)
{
    this->whereClauses.push_back(column + " " + operator_ + " " + value);
    return this;
}

IQueryBuilder* QueryBuilder::OrWhere(std::string column, std::string operator_, std::string value)
{
    this->orWhereClauses.push_back(column + " " + operator_ + " " + value);
    return this;
}

IQueryBuilder* QueryBuilder::Join(std::string table, std::string onCondition, std::string joinType)
{
    this->joinClauses.push_back(joinType + " JOIN " + table + " ON " + onCondition);
    return this;
}

IQueryBuilder* QueryBuilder::OrderBy(std::vector<std::pair<std::string, std::string>> columns)
{
    if (columns.empty())
    {
        throw std::invalid_argument("OrderBy requires at least one column-value pair.");
    }
    for (const auto& column : columns) {
        this->orderByClauses.push_back(column.first + " " + column.second);
    }
    return this;
}

IQueryBuilder* QueryBuilder::Limit(int count)
{
    this->limitCount = count;
    return this;
}

IQueryBuilder* QueryBuilder::GroupBy(std::vector<std::string> columns)
{
    this->groupByClauses = columns;
    return this;
}

IQueryBuilder* QueryBuilder::OnDuplicate(std::map<std::string, std::string> data)
{
    if (data.empty()) {
        throw std::invalid_argument("OnDuplicate requires at least one column-value pair.");
    }

    for (const auto& pair : data) {
        this->onDuplicateClauses.push_back(pair.first + " = " + pair.second);
    }
    return this;
}

IQueryBuilder* QueryBuilder::Having(std::string condition)
{
    if (condition.empty()) {
        throw std::invalid_argument("Having condition cannot be empty.");
    }
    this->havingClauses.push_back(condition);
    return this;
}

IQueryBuilder* QueryBuilder::Distinct()
{
    ENSURE_TABLE_SET();
    this->isDistinct = true;
    return this;
}

IQueryBuilder* QueryBuilder::Offset(int count)
{
    if (count < 0) {
        throw std::invalid_argument("Offset cannot be negative.");
    }
    this->offsetCount = count;
    return this;
}

IQueryBuilder* QueryBuilder::Union(std::string query, bool all)
{
    if (query.empty()) {
        throw std::invalid_argument("Union query cannot be empty.");
    }
    std::string unionType = all ? "UNION ALL" : "UNION";
    this->unionClauses.push_back(unionType + " " + query);
    return this;
}

std::any QueryBuilder::PrepareQuery()
{
    std::string finalQuery = query;

    // Add DISTINCT after SELECT if needed
    if (this->query.rfind("SELECT", 0) == 0 && this->isDistinct) {
        finalQuery.insert(6, " DISTINCT");
    }

    // Join clauses
    if (!this->joinClauses.empty()) {
        finalQuery += " " + implode(this->joinClauses, " ");
    }

    // Add WHERE clause, handle AND/OR combination
    if (!this->whereClauses.empty() || !this->orWhereClauses.empty()) {
        std::string combinedWhereClause;
        
        // Handle WHERE
        if (!this->whereClauses.empty()) {
            combinedWhereClause += implode(whereClauses, " AND ");
        }
        
        // Handle OR WHERE
        if (!this->orWhereClauses.empty()) {
            if (!combinedWhereClause.empty()) {
                combinedWhereClause += " OR ";
            }
            combinedWhereClause += implode(orWhereClauses, " OR ");
        }
        
        // Add to final query
        finalQuery += " WHERE " + combinedWhereClause;
    }

    // Add GROUP BY clauses
    if (!this->groupByClauses.empty()) {
        finalQuery += " GROUP BY " + implode(groupByClauses, ", ");
    }

    // Add HAVING clauses
    if (!this->havingClauses.empty()) {
        finalQuery += " HAVING " + implode(this->havingClauses, " AND ");
    }

    // Add ORDER BY clauses
    if (!this->orderByClauses.empty()) {
        finalQuery += " ORDER BY " + implode(this->orderByClauses, ", ");
    }

    // Add LIMIT if specified
    if (this->limitCount >= 0) {
        finalQuery += " LIMIT " + std::to_string(limitCount);
    }

    // Add OFFSET if specified
    if (this->offsetCount >= 0) {
        finalQuery += " OFFSET " + std::to_string(offsetCount);
    }

    // Add ON DUPLICATE KEY UPDATE if specified
    if (!this->onDuplicateClauses.empty()) {
        finalQuery += " ON DUPLICATE KEY UPDATE " + implode(this->onDuplicateClauses, ", ");
    }

    // Add UNION clauses if any
    if (!this->unionClauses.empty()) {
        finalQuery += " " + implode(this->unionClauses, " ");
    }

    return finalQuery;
}