#include "DBDriver.h"

IDatabase* DBDriver::RegisterDatabase()
{
    IDatabase* db = new MySQLDatabase();
    databases.push_back(db);
    return db;
}

std::string DBDriver::GetKind()
{
    return "mysql";
}