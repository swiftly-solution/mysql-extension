#ifndef _querybuilder_h
#define _querybuilder_h

#include "IQueryBuilder.h"

class QueryBuilder: public IQueryBuilder
{
private:
    std::string tableName;
    std::string query;
    std::vector<std::string> selectColumns;
    std::vector<std::string> whereClauses;
    std::vector<std::string> orWhereClauses;
    std::vector<std::string> joinClauses;
    std::vector<std::string> groupByClauses;
    std::vector<std::string> orderByClauses;
    std::vector<std::string> onDuplicateClauses;
    std::vector<std::string> havingClauses;
    std::vector<std::string> unionClauses;
    std::vector<std::pair<std::string, std::string>> updatePairs;

    bool isDistinct = false;
    int limitCount = -1;
    int offsetCount = -1;

    std::string m_version;

public:
    QueryBuilder(std::string version);

    void Table(std::string tableName);
    void Create(std::unordered_map<std::string, std::string> columns);
    void Alter(std::map<std::string, std::string> columns);
    void Drop();

    void Select(std::vector<std::string> columns);
    void Insert(std::map<std::string, std::string> data);
    void Update(std::map<std::string, std::string> data);
    void Delete();

    void Where(std::string column, std::string operator_, std::string value);
    void OrWhere(std::string column, std::string operator_, std::string value);
    void Join(std::string table, std::string onCondition, std::string joinType = "INNER");
    void OrderBy(std::vector<std::pair<std::string, std::string>> columns);
    void Limit(int count);
    void GroupBy(std::vector<std::string> columns);
    void OnDuplicate(std::map<std::string, std::string> data);
    void Having(std::string condition);
    void Distinct();
    void Offset(int count);
    void Union(std::string query, bool all);

    std::any PrepareQuery();
};

#endif