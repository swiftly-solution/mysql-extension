MySQL_QB = {
    tableName = "",
    query = "",
    whereClauses = {},
    orWhereClauses = {},
    joinClauses = {},
    groupByClauses = {},
    orderByClauses = {},
    onDuplicateClauses = {},
    havingClauses = {},
    unionClauses = {},
    updatePairs = {},
    isDistinct = false,
    limitCount = -1,
    offsetCount = -1,
    db = nil
}

local oldServerVersions = {
    "5.0",
    "5.1",
    "5.2",
    "5.3",
    "5.4",
    "5.5",
    "5.6",
    "5.7",
}

local oldServerCache = {}

--- @param rule string
--- @return table
local function parseRule(rule)
    local retRule = {}
    local parts = string.split(rule, ":")
    retRule[1] = parts[1]
    if #parts > 1 then
        retRule[2] = table.concat(parts, ":", 2)
    end
    return retRule
end

--- @param tbl table
--- @return boolean
local function isTableEmpty(tbl)
    return next(tbl) == nil
end

--- @param columns table
--- @return table
local function parseRules(columns)
    local ret = {}
    for k,v in next,columns,nil do
        local rules = string.split(v, "|")
        local validatedRules = {}

        for i=1,#rules do
            validatedRules[#validatedRules + 1] = parseRule(rules[i])
        end

        ret[#ret + 1] = { k, validatedRules }
    end
    return ret
end

--- @param columnName string
--- @param columnRules table
--- @param version string
--- @return string
local function GenerateColumnType(columnName, columnRules, version)
    local ret_type = "VARCHAR(255)"
    local defaultValue = ""

    local defaultSet = false
    local isNullable = false
    local isInteger = false
    local unique = false
    local primary_key = false
    local index = false
    local autoIncrement = false

    local oldServer = oldServerCache[version]
    if oldServer == nil then
        oldServer = false
        for i=1,#oldServerVersions do
            if version:find(oldServerVersions[i]) ~= nil then
                oldServer = true
                break
            end
        end
        oldServerCache[version] = oldServer
    end

    for i=1,#columnRules do
        local name = columnRules[i][1]
        local param = columnRules[i][2]

        if name == "nullable" then
            isNullable = true
        elseif name == "integer" then
            ret_type = "INT"
            isInteger = true
        elseif name == "string" then
            ret_type = "VARCHAR(255)"
        elseif name == "boolean" then
            ret_type = "TINYINT(1)"
        elseif name == "date" then
            ret_type = "DATE"
        elseif name == "datetime" then
            ret_type = "DATETIME"
        elseif name == "min" and (param or ""):len() > 0 then
            local val = math.floor(tonumber(param) or 0)
            if isInteger and val >= 0 then
                ret_type = "UNSIGNED INT"
            end
        elseif (name == "max" or name == "size") and (param or ""):len() > 0 then
            local val = math.floor(tonumber(param) or 0)
            if ret_type:find("VARCHAR") ~= nil and val > 0 then
                ret_type = "VARCHAR("..val..")"
            end
        elseif name == "json" then
            if oldServer then ret_type = "VARCHAR(16380)"
            else ret_type = "JSON" end

            defaultValue = "{}"
            defaultSet = true
        elseif name == "float" then
            ret_type = "FLOAT"
        elseif name == "default" then
            defaultValue = "{}"
            defaultSet = true
        elseif name == "unique" then
            unique = true
            primary_key = false
            index = false
        elseif name == "primary" then
            unique = false
            primary_key = true
            index = false
        elseif name == "index" then
            unique = false
            primary_key = false
            index = true
        elseif name == "autoincrement" then
            autoIncrement = true
        end
    end

    if isNullable then
        ret_type = ret_type .. " NULL"
    else
        ret_type = ret_type .. " NOT NULL"
    end

    if defaultSet then
        if ret_type:find("^JSON") ~= nil or ret_type:find("^VARCHAR") ~= nil then
            ret_type = ret_type .. " DEFAULT '"..defaultValue.."'"
        else
            ret_type = ret_type .. " DEFAULT "..defaultValue
        end
    end

    if autoIncrement then
        ret_type = ret_type .. " AUTO_INCREMENT"
    end

    if primary_key then
        ret_type = ret_type .. ", PRIMARY KEY (`" .. columnName .. "`)"
    end

    if unique then
        ret_type = ret_type .. ", UNIQUE (`" .. columnName .. "`)"
    end

    if index then
        ret_type = ret_type .. ", INDEX (`" .. columnName .. "`)"
    end

    return ret_type
end

--- @param value any
--- @return string
function MySQL_QB:FormatSQLValue(value)
    if value == "nil" or value == nil then
        return "NULL"
    elseif type(value) == "table" then
        return json.encode(value)
    elseif type(value) == "string" then
        return "'".. self.db:EscapeString(value) .."'"
    else
        return tostring(value)
    end
end

--- @param db Database
function MySQL_QB:new(db)
    local o = {}
    setmetatable(o, self)
    self.__index = self
    self.db = db
    return o
end

--- @param tblName string
function MySQL_QB:Table(tblName)
    self.tableName = tblName
    return self
end

--- @param columns table
function MySQL_QB:Create(columns)
    if self.tableName:len() <= 0 then
        return error("Table name must be set before executing the query.")
    end

    if type(columns) ~= "table" or isTableEmpty(columns) then
        return error("Create requires at least one column-value pair.")
    end

    local rules = parseRules(columns)
    local definitions = {}

    for i=1,#rules do
        local columnName = rules[i][1]
        local columnRules = rules[i][2]

        local typ = GenerateColumnType(columnName, columnRules, self.db:GetVersion())
        if typ then
            definitions[#definitions + 1] = table.concat({columnName, typ}, " ")
        end
    end

    self.query = "CREATE TABLE IF NOT EXISTS " .. self.tableName .. " (" .. table.concat(definitions, ", ") .. ")"

    return self
end

--- @param columns table
function MySQL_QB:Alter(columns)
    print("[MySQL - QueryBuilder] Alter is not currently implemented.")
    return self
end

function MySQL_QB:Drop()
    if self.tableName:len() <= 0 then
        return error("Table name must be set before executing the query.")
    end

    self.query = "DROP TABLE IF EXISTS " .. self.tableName
    return self
end

--- @param columns table
function MySQL_QB:Select(columns)
    if self.tableName:len() <= 0 then
        return error("Table name must be set before executing the query.")
    end

    if type(columns) ~= "table" or #columns == 0 then
        self.query = "SELECT * FROM " .. self.tableName
    else
        self.query = "SELECT ".. table.concat(columns, ", ") .." FROM " .. self.tableName
    end

    return self
end

--- @param values table
function MySQL_QB:Insert(values)
    if self.tableName:len() <= 0 then
        return error("Table name must be set before executing the query.")
    end

    if type(values) ~= "table" or isTableEmpty(values) then
        return error("Insert requires at least one column-value pair.")
    end

    local cols = {}
    local vals = {}

    for k,v in next,values,nil do
        cols[#cols + 1] = k
        vals[#vals + 1] = self:FormatSQLValue(v)
    end

    self.query = "INSERT INTO " .. self.tableName .. " (" .. table.concat(cols, ", ") .. ") VALUES (" .. table.concat(vals, ", ") .. ")"

    return self
end

--- @param values table
function MySQL_QB:Update(values)
    if self.tableName:len() <= 0 then
        return error("Table name must be set before executing the query.")
    end

    if type(values) ~= "table" or isTableEmpty(values) then
        return error("Update requires at least one column-value pair.")
    end

    local updates = {}

    for k,v in next,values,nil do
        updates[#updates + 1] = k .. " = " .. MySQL_QB:FormatSQLValue(v)
    end

    self.query = "UPDATE " .. self.tableName .. " SET " .. table.concat(updates, ", ")

    return self
end

function MySQL_QB:Delete()
    if self.tableName:len() <= 0 then
        return error("Table name must be set before executing the query.")
    end

    self.query = "DELETE FROM " .. self.tableName

    return self
end

--- @param count number
function MySQL_QB:Limit(count)
    if type(count) ~= "number" then
        return error("Limit requires the count to be a number.")
    end

    self.limitCount = count

    return self
end

--- @param columns table
function MySQL_QB:GroupBy(columns)
    if type(columns) ~= "table" or #columns <= 0 then
        return error("GroupBy requires at least one column.")
    end

    self.groupByClauses = columns

    return self
end

--- @param column string
--- @param operator_ string
--- @param value any
function MySQL_QB:Where(column, operator_, value)
    if type(column) ~= "string" or type(operator_) ~= "string" then
        return error("Where requires the column and the operator to be a string.")
    end

    self.whereClauses[#self.whereClauses + 1] = column .. " " .. operator_ .. " " .. self:FormatSQLValue(value)

    return self
end

--- @param column string
--- @param operator_ string
--- @param value any
function MySQL_QB:OrWhere(column, operator_, value)
    if type(column) ~= "string" or type(operator_) ~= "string" then
        return error("OrWhere requires the column and the operator to be a string.")
    end

    self.orWhereClauses[#self.orWhereClauses + 1] = column .. " " .. operator_ .. " " .. self:FormatSQLValue(value)

    return self
end

--- @param tbl string
--- @param onCondition string
--- @param joinType string
function MySQL_QB:Join(tbl, onCondition, joinType)
    if type(tbl) ~= "string" or type(onCondition) ~= "string" or type(joinType) ~= "string" then
        return error("Join requires the table, the condition and the join type to be a string.")
    end

    self.joinClauses[#self.joinClauses + 1] = joinType .. " JOIN " .. tbl .. " ON " .. onCondition

    return self
end

--- @param count number
function MySQL_QB:Offset(count)
    if type(count) ~= "number" or count < 0 then
        return error("Offset needs a count to be a number and positive.")
    end

    self.offsetCount = count

    return self
end

function MySQL_QB:Distinct()
    if self.tableName:len() <= 0 then
        return error("Table name must be set before executing the query.")
    end

    self.isDistinct = true

    return self
end

--- @param condition string
function MySQL_QB:Having(condition)
    if type(condition) ~= "string" or condition:len() <= 0 then
        return error("Having needs the condition to be a string and to not be empty.")
    end

    self.havingClauses[#self.havingClauses + 1] = condition

    return self
end

--- @param data table
function MySQL_QB:OnDuplicate(data)
    if type(data) ~= "table" or isTableEmpty(data) then
        return error("OnDuplicate requires at least one column-value pair.")
    end

    for col,val in next,data,nil do
        self.onDuplicateClauses[#self.onDuplicateClauses + 1] = col .. " = " .. self:FormatSQLValue(val)
    end

    return self
end

--- @param columns table
function MySQL_QB:OrderBy(columns)
    if type(columns) ~= "table" or isTableEmpty(columns) then
        return error("OrderBy requires at least one column-order pair.")
    end

    for col,val in next,columns,nil do
        self.orderByClauses[#self.orderByClauses + 1] = col .. " " .. val
    end

    return self
end

function MySQL_QB:PrepareQuery()
    local finalStr = self.query

    if self.query:find("^SELECT") ~= nil and self.isDistinct then
        finalStr, _ = self.query:gsub("()", {[7] = " DISTINCT"})
    end

    if #self.joinClauses > 0 then
        finalStr = finalStr .. " " .. table.concat(self.joinClauses, " ")
    end

    if #self.whereClauses > 0 or #self.orWhereClauses > 0 then
        local combined = ""

        if #self.whereClauses > 0 then
            combined = table.concat(self.whereClauses, " AND ")
        end

        if #self.orWhereClauses > 0 then
            if combined:len() > 0 then
                combined = combined .. " OR "
            end
            combined = combined .. table.concat(self.orWhereClauses, " OR ")
        end

        finalStr = finalStr .. " WHERE " .. combined
    end

    if #self.groupByClauses > 0 then
        finalStr = finalStr .. " GROUP BY " ..  table.concat(self.groupByClauses, ", ")
    end

    if #self.havingClauses > 0 then
        finalStr = finalStr .. " HAVING " .. table.concat(self.havingClauses, " AND ")
    end

    if #self.orderByClauses > 0 then
        finalStr = finalStr .. " ORDER BY " ..  table.concat(self.orderByClauses, ", ")
    end

    if self.limitCount >= 0 then
        finalStr = finalStr .. " LIMIT " .. self.limitCount
    end

    if self.offsetCount >= 0 then
        finalStr = finalStr .. " OFFSET " .. self.offsetCount
    end

    if #self.onDuplicateClauses > 0 then
        finalStr = finalStr .. " ON DUPLICATE KEY UPDATE " .. table.concat(self.onDuplicateClauses, ", ")
    end

    return finalStr
end

--- @param cb fun(err:string,result:table)|nil
function MySQL_QB:Execute(cb)
    local finalstr = self:PrepareQuery()
    self.db:ExecuteQB(finalstr, cb)
end