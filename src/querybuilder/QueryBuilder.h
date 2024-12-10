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

public:
    IQueryBuilder* Table(std::string tableName);
    IQueryBuilder* Create(std::map<std::string, std::string> columns);
    IQueryBuilder* Alter(std::map<std::string, std::string> columns);
    IQueryBuilder* Drop();

    IQueryBuilder* Select(std::vector<std::string> columns);
    IQueryBuilder* Insert(std::map<std::string, std::string> data);
    IQueryBuilder* Update(std::map<std::string, std::string> data);
    IQueryBuilder* Delete();

    IQueryBuilder* Where(std::string column, std::string operator_, std::string value);
    IQueryBuilder* OrWhere(std::string column, std::string operator_, std::string value);
    IQueryBuilder* Join(std::string table, std::string onCondition, std::string joinType = "INNER");
    IQueryBuilder* OrderBy(std::vector<std::pair<std::string, std::string>> columns);
    IQueryBuilder* Limit(int count);
    IQueryBuilder* GroupBy(std::vector<std::string> columns);
    IQueryBuilder* OnDuplicate(std::map<std::string, std::string> data);
    IQueryBuilder* Having(std::string condition);
    IQueryBuilder* Distinct();
    IQueryBuilder* Offset(int count);
    IQueryBuilder* Union(std::string query, bool all);

    std::any PrepareQuery();

private:
    template<typename T>
    std::string join(const std::vector<T>& vec, const std::string& delimiter);
};

#endif