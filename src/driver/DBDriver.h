#ifndef _dbdriver_h
#define _dbdriver_h

#include "../database/MySQLDatabase.h"
#include <vector>
#include <string>

class DBDriver
{
private:
    std::vector<IDatabase*> databases;
public:
    virtual IDatabase* RegisterDatabase();
    virtual std::string GetKind();

    std::vector<IDatabase*> GetDatabases() { return databases; }
};

extern DBDriver g_dbDriver;

#endif