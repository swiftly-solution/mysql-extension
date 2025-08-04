#include "../driver/DBDriver.h"
#include "../entrypoint.h"

#include <swiftly-ext/event.h>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <thread>
#include <chrono>

bool threadStarted = false;

std::string QueryToJSON(const std::vector<std::map<std::string, std::any>>& data)
{
    rapidjson::Document document(rapidjson::kArrayType);

    for (const auto& map : data)
    {
        rapidjson::Value entry(rapidjson::kObjectType);

        for (const auto& pair : map)
        {
            const char* key = pair.first.c_str();
            const std::any& value = pair.second;

            if (value.type() == typeid(const char*))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetString(std::any_cast<const char*>(value), document.GetAllocator()), document.GetAllocator());
            else if (value.type() == typeid(std::string))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetString(std::any_cast<std::string>(value).c_str(), document.GetAllocator()), document.GetAllocator());
            else if (value.type() == typeid(uint64_t))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetUint64(std::any_cast<uint64_t>(value)), document.GetAllocator());
            else if (value.type() == typeid(uint32_t))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetUint(std::any_cast<uint32_t>(value)), document.GetAllocator());
            else if (value.type() == typeid(uint16_t))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetUint(std::any_cast<uint16_t>(value)), document.GetAllocator());
            else if (value.type() == typeid(uint8_t))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetUint(std::any_cast<uint8_t>(value)), document.GetAllocator());
            else if (value.type() == typeid(int64_t))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetInt64(std::any_cast<int64_t>(value)), document.GetAllocator());
            else if (value.type() == typeid(int32_t))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetInt(std::any_cast<int32_t>(value)), document.GetAllocator());
            else if (value.type() == typeid(int16_t))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetInt(std::any_cast<int16_t>(value)), document.GetAllocator());
            else if (value.type() == typeid(int8_t))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetInt(std::any_cast<int8_t>(value)), document.GetAllocator());
            else if (value.type() == typeid(bool))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetBool(std::any_cast<bool>(value)), document.GetAllocator());
            else if (value.type() == typeid(float))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetFloat(std::any_cast<float>(value)), document.GetAllocator());
            else if (value.type() == typeid(double))
                entry.AddMember(rapidjson::Value().SetString(key, document.GetAllocator()), rapidjson::Value().SetDouble(std::any_cast<double>(value)), document.GetAllocator());
        }

        document.PushBack(entry, document.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);
    return std::string(buffer.GetString());
}

void DatabaseCallback(std::vector<std::any> res)
{
    std::string reqID = std::any_cast<std::string>(res[0]);
    std::string result = std::any_cast<std::string>(res[1]);
    std::string err = std::any_cast<std::string>(res[2]);
    std::string plugin_name = std::any_cast<std::string>(res[3]);

    std::any ares;
    TriggerEvent("mysql.ext", "OnDatabaseActionPerformed", { reqID, result, err }, ares, plugin_name);
}

void DriverThink()
{
    while (true) {
        auto dbs = g_dbDriver.GetDatabases();
        for (auto rdb : dbs) {
            MySQLDatabase* db = (MySQLDatabase*)rdb;
            if (!db->IsConnected()) continue;

            while (!db->queryQueue.empty()) {
                auto queue = db->queryQueue.front();

                auto queryResult = db->Query(queue.query);
                auto error = db->GetError();
                if (error == "MySQL server has gone away") {
                    if (db->Connect())
                        continue;
                    else
                        error = db->GetError();
                }

                std::string result = QueryToJSON(queryResult);
                g_Ext.NextFrame(DatabaseCallback, { queue.requestID, result, error, queue.plugin_name });

                free((void*)(std::any_cast<const char*>(queue.query)));
                db->queryQueue.pop_front();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}