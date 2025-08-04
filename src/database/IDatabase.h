#ifndef _idatabase_h
#define _idatabase_h

#include <map>
#include <string>
#include <vector>
#include <any>

struct DatabaseQueryQueue
{
    std::any query;
    std::string requestID;
    std::string plugin_name;
};

class IDatabase
{
public:
    virtual void SetConnectionConfig(std::map<std::string, std::string> connection_details) = 0;

    virtual bool Connect() = 0;
    virtual void Close(bool setError) = 0;

    virtual std::string GetVersion() = 0;
    virtual std::string GetKind() = 0;

    virtual bool IsConnected() = 0;

    virtual bool HasError() = 0;
    virtual std::string GetError() = 0;

    virtual std::vector<std::map<std::string, std::any>> Query(std::any query) = 0;

    // Mostly used in SQL-kind databases (MariaDB/MySQL, SQLite, MSSQL)
    virtual std::string EscapeValue(std::string query) = 0;

    virtual void AddQueryQueue(DatabaseQueryQueue data) = 0;

    virtual const char* ProvideQueryBuilderTable() = 0;
};

#endif