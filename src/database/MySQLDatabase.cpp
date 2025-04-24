#include "MySQLDatabase.h"
#include "../entrypoint.h"
#include "../utils.h"
#include <thread>

extern bool threadStarted;
void DriverThink();

void MySQLDatabase::SetConnectionConfig(std::map<std::string, std::string> connection_details)
{
    m_hostname = connection_details["hostname"];
    m_username = connection_details["username"];
    m_password = connection_details["password"];
    m_port = V_StringToUint16(connection_details["port"].c_str(), 3306);
    m_database = connection_details["database"];
}

bool MySQLDatabase::Connect()
{
    if (this->connected)
        return true;

    if (mysql_library_init(0, nullptr, nullptr) != 0)
    {
        this->error = "Couldn't initialize MySQL Client Library.";
        return false;
    }
    this->connection = mysql_init(nullptr);
    if (this->connection == nullptr)
    {
        this->error = mysql_error(this->connection);
        return false;
    }

    my_bool my_true = true;
    mysql_options(this->connection, MYSQL_OPT_RECONNECT, &my_true);

    unsigned int timeout = 60;
    mysql_options(this->connection, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(this->connection, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(this->connection, MYSQL_OPT_WRITE_TIMEOUT, &timeout);

    if (mysql_real_connect(this->connection, this->m_hostname.c_str(), this->m_username.c_str(), this->m_password.c_str(), this->m_database.c_str(), this->m_port, nullptr, 0) == nullptr)
    {
        this->Close(true);
        return false;
    }

    mysql_set_character_set(this->connection, "utf8mb4");
    this->connected = true;

    this->Query(string_format("ALTER DATABASE %s CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;", this->m_database.c_str()).c_str());
    m_version = explode(std::any_cast<std::string>(this->Query("select @@version;")[0]["@@version"]), "-")[0];

    return true;
}

void MySQLDatabase::Close(bool setError)
{
    if (setError) this->error = mysql_error(this->connection);

    mysql_close(this->connection);
    this->connected = false;
}

bool MySQLDatabase::IsConnected()
{
    return this->connected;
}

bool MySQLDatabase::HasError()
{
    return (this->error != nullptr);
}

std::string MySQLDatabase::GetError()
{
    if (!this->error) return "";

    std::string err = this->error;
    this->error = nullptr;
    return err;
}

static constexpr int MYSQL_JSON = 245;

std::any ParseFieldType(enum_field_types type, const char* value, uint32_t length)
{
    if (type == enum_field_types::MYSQL_TYPE_FLOAT || type == enum_field_types::MYSQL_TYPE_DOUBLE || type == enum_field_types::MYSQL_TYPE_DECIMAL)
        return atof(value);
    else if (type == enum_field_types::MYSQL_TYPE_SHORT || type == enum_field_types::MYSQL_TYPE_TINY || type == enum_field_types::MYSQL_TYPE_INT24 || type == enum_field_types::MYSQL_TYPE_LONG || type == enum_field_types::MYSQL_TYPE_NEWDECIMAL || type == enum_field_types::MYSQL_TYPE_YEAR || type == enum_field_types::MYSQL_TYPE_BIT)
        return atoi(value);
    else if (
        type == enum_field_types::MYSQL_TYPE_VARCHAR ||
        type == enum_field_types::MYSQL_TYPE_VAR_STRING ||
        type == enum_field_types::MYSQL_TYPE_BLOB ||
        type == MYSQL_JSON ||
        type == enum_field_types::MYSQL_TYPE_TIMESTAMP ||
        type == enum_field_types::MYSQL_TYPE_DATE ||
        type == enum_field_types::MYSQL_TYPE_TIME ||
        type == enum_field_types::MYSQL_TYPE_DATETIME ||
        type == enum_field_types::MYSQL_TYPE_NEWDATE ||
        type == enum_field_types::MYSQL_TYPE_ENUM ||
        type == enum_field_types::MYSQL_TYPE_SET ||
        type == enum_field_types::MYSQL_TYPE_STRING ||
        type == enum_field_types::MYSQL_TYPE_TINY_BLOB ||
        type == enum_field_types::MYSQL_TYPE_MEDIUM_BLOB ||
        type == enum_field_types::MYSQL_TYPE_LONG_BLOB ||
        type == enum_field_types::MYSQL_TYPE_GEOMETRY
        ) {
        return std::string(value, length + 1);
    }
    else if (type == enum_field_types::MYSQL_TYPE_LONGLONG)
        return strtoll(value, nullptr, 10);
    else
    {
        g_SMAPI->ConPrintf("[MySQL - ParseFieldType] Invalid field type: %d, falling back to string.\n", type);
        return std::string(value, length + 1);
    }
}

std::vector<std::map<std::string, std::any>> MySQLDatabase::Query(std::any query)
{
    const char* q = std::any_cast<const char*>(query);
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::map<std::string, std::any>> values;

    if (!this->connected)
        return {};

    if (mysql_ping(this->connection))
    {
        this->error = mysql_error(this->connection);
        return {};
    }

    mysql_set_character_set(this->connection, "utf8mb4");
    if (mysql_real_query(this->connection, q, strlen(q)))
    {
        this->error = mysql_error(this->connection);
        return {};
    }

    MYSQL_RES* result = mysql_use_result(this->connection);
    if (result == nullptr)
    {
        std::map<std::string, std::any> value;

        if (mysql_field_count(this->connection) == 0)
        {
            value.insert(std::make_pair("warningCounts", (uint64)mysql_warning_count(this->connection)));
            value.insert(std::make_pair("affectedRows", mysql_affected_rows(this->connection)));
            value.insert(std::make_pair("insertId", mysql_insert_id(this->connection)));
            values.push_back(value);
        }
        else
        {
            this->error = std::string("Invalid query type.\nQuery: " + std::string(q)).c_str();
            return {};
        }
    }
    else
    {
        MYSQL_ROW row;
        MYSQL_FIELD* fields = mysql_fetch_fields(result);
        int num_fields = mysql_num_fields(result);

        while ((row = mysql_fetch_row(result))) {
            std::map<std::string, std::any> value;
            unsigned long* lengths = mysql_fetch_lengths(result);

            for (int i = 0; i < num_fields; i++) {
                value.insert({ fields[i].name, row[i] ? ParseFieldType(fields[i].type, row[i], lengths[i]) : nullptr });
            }

            values.push_back(value);
        }

        mysql_free_result(result);
    }

    return values;
}

std::string MySQLDatabase::EscapeValue(std::string query)
{
    char* newQuery = new char[query.size() * 2 + 1];
    mysql_real_escape_string(this->connection, newQuery, query.c_str(), query.size());
    std::string str(newQuery);
    delete[] newQuery;
    return str;
}

std::string MySQLDatabase::GetVersion()
{
    return m_version;
}

std::string MySQLDatabase::GetKind()
{
    return "mysql";
}

void MySQLDatabase::AddQueryQueue(DatabaseQueryQueue data)
{
    queryQueue.push_back(data);

    if (!threadStarted) {
        threadStarted = true;
        std::thread(DriverThink).detach();
    }
}

const char* MySQLDatabase::ProvideQueryBuilderTable()
{
    return "MySQL_QB";
}