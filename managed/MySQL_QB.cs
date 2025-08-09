using System.Collections;
using System.Text.Json;
using static SwiftlyS2.API.Scripting.Database;

namespace SwiftlyS2.API.Extensions
{
    public class QueryBuilderMySQL
    {
        private static readonly string[] oldServerVersions = ["5.0", "5.1", "5.2", "5.3", "5.4", "5.5", "5.6", "5.7", "8.0"];
        private static readonly Dictionary<string, bool> oldServerCache = [];
        private static readonly Dictionary<string, Dictionary<string, string>> defaultValuesCacheTbl = [];

        private DB? m_db;
        private string tableName = "";
        private string query = "";
        private List<string> whereClauses = new();
        private List<string> orWhereClauses = new();
        private List<string> joinClauses = new();
        private List<string> groupByClauses = new();
        private List<string> orderByClauses = new();
        private List<string> onDuplicateClauses = new();
        private List<string> havingClauses = new();
        private List<string> unionClauses = new();
        private bool isDistinct = false;
        private int limitCount = -1;
        private int offsetCount = -1;

        private string FormatSQLValue(object? value)
        {
            if (value == null)
                return "NULL";

            if (value is string s)
                return "\"" + m_db!.EscapeString(s) + "\"";

            if (value is IDictionary || (value is IEnumerable && !(value is string)))
            {
                var json = JsonSerializer.Serialize(value);
                return "\"" + m_db!.EscapeString(json) + "\"";
            }

            if (value is bool b)
                return b ? "1" : "0";

            return value.ToString() ?? "NULL";
        }

        private class Rule
        {
            public string Name { get; set; } = "";
            public string Param { get; set; } = "";
        }

        private class ColumnRule
        {
            public string ColumnName { get; set; } = "";
            public List<Rule> Rules { get; set; } = [];
        }

        private static Rule ParseRule(string rule)
        {
            var parts = rule.Split([':'], 2);
            return new Rule
            {
                Name = parts.Length > 0 ? parts[0] : "",
                Param = parts.Length > 1 ? parts[1] : ""
            };
        }

        private static List<ColumnRule> ParseRules(Dictionary<string, string>? columns)
        {
            var ret = new List<ColumnRule>();
            if (columns == null) return ret;

            foreach (var kv in columns)
            {
                var rulesRaw = (kv.Value ?? "").Split('|', StringSplitOptions.RemoveEmptyEntries);
                var validatedRules = new List<Rule>();
                foreach (var r in rulesRaw)
                {
                    validatedRules.Add(ParseRule(r));
                }

                ret.Add(new ColumnRule
                {
                    ColumnName = kv.Key,
                    Rules = validatedRules
                });
            }

            return ret;
        }

        private static string GenerateColumnType(string tblName, string columnName, List<Rule>? columnRules, string version)
        {
            string ret_type = "VARCHAR(255)";
            string defaultValue = "";
            bool defaultSet = false;
            bool isNullable = false;
            bool isInteger = false;
            bool unique = false;
            bool primary_key = false;
            bool index = false;
            bool autoIncrement = false;

            if (!oldServerCache.TryGetValue(version, out var oldServer))
            {
                oldServer = false;
                foreach (var v in oldServerVersions)
                {
                    if (!string.IsNullOrEmpty(version) && version.StartsWith(v, StringComparison.Ordinal))
                    {
                        oldServer = true;
                        break;
                    }
                }
                oldServerCache[version] = oldServer;
            }

            if (columnRules != null)
            {
                foreach (var r in columnRules)
                {
                    var name = r.Name;
                    var param = r.Param ?? "";

                    if (name == "nullable")
                    {
                        isNullable = true;
                    }
                    else if (name == "integer")
                    {
                        ret_type = "INT";
                        isInteger = true;
                    }
                    else if (name == "string")
                    {
                        ret_type = "VARCHAR(255)";
                    }
                    else if (name == "boolean")
                    {
                        ret_type = "TINYINT(1)";
                    }
                    else if (name == "date")
                    {
                        ret_type = "DATE";
                    }
                    else if (name == "datetime")
                    {
                        ret_type = "DATETIME";
                    }
                    else if (name == "min" && !string.IsNullOrEmpty(param))
                    {
                        if (double.TryParse(param, out var dval))
                        {
                            int val = (int)Math.Floor(dval);
                            if (isInteger && val >= 0)
                                ret_type = "UNSIGNED INT";
                        }
                    }
                    else if ((name == "max" || name == "size") && !string.IsNullOrEmpty(param))
                    {
                        if (double.TryParse(param, out var dval))
                        {
                            int val = (int)Math.Floor(dval);
                            if (ret_type.Contains("VARCHAR") && val > 0)
                                ret_type = $"VARCHAR({val})";
                        }
                    }
                    else if (name == "json")
                    {
                        ret_type = "TEXT";
                        defaultValue = "{}";
                        defaultSet = true;
                    }
                    else if (name == "float")
                    {
                        ret_type = "FLOAT";
                    }
                    else if (name == "default" && !string.IsNullOrEmpty(param))
                    {
                        defaultValue = param;
                        defaultSet = true;
                    }
                    else if (name == "unique")
                    {
                        unique = true;
                        primary_key = false;
                        index = false;
                    }
                    else if (name == "primary")
                    {
                        unique = false;
                        primary_key = true;
                        index = false;
                    }
                    else if (name == "index")
                    {
                        unique = false;
                        primary_key = false;
                        index = true;
                    }
                    else if (name == "autoincrement")
                    {
                        autoIncrement = true;
                        unique = false;
                        primary_key = true;
                        index = false;
                    }
                }
            }

            ret_type = ret_type + (isNullable ? " NULL" : " NOT NULL");

            if (defaultSet)
            {
                if (ret_type.StartsWith("TEXT", StringComparison.Ordinal) || ret_type.StartsWith("VARCHAR", StringComparison.Ordinal))
                {
                    if (!defaultValuesCacheTbl.TryGetValue(tblName, out var tblDefaults))
                    {
                        tblDefaults = new Dictionary<string, string>();
                        defaultValuesCacheTbl[tblName] = tblDefaults;
                    }
                    tblDefaults[columnName] = defaultValue;
                }
                else
                {
                    ret_type = ret_type + " DEFAULT " + defaultValue;
                }
            }

            if (autoIncrement)
                ret_type = ret_type + " AUTO_INCREMENT";

            if (primary_key)
                ret_type = ret_type + ", PRIMARY KEY (`" + columnName + "`)";

            if (unique)
                ret_type = ret_type + ", UNIQUE (`" + columnName + "`)";

            if (index)
                ret_type = ret_type + ", INDEX (`" + columnName + "`)";

            return ret_type;
        }

        public QueryBuilder(DB db)
        {
            m_db = db;
        }
        public QueryBuilder Table(string table_name)
        {
            tableName = table_name ?? "";
            return this;
        }
        public QueryBuilder Select(string[] columns)
        {
            if (string.IsNullOrEmpty(tableName))
                throw new ArgumentException("Table name must be set before executing the query.");

            if (columns == null || columns.Length == 0)
                query = "SELECT * FROM " + tableName;
            else
                query = "SELECT " + string.Join(", ", columns) + " FROM " + tableName;

            return this;

        }
        public QueryBuilder Insert(Dictionary<string, object> values)
        {
            if (string.IsNullOrEmpty(tableName))
                throw new ArgumentException("Table name must be set before executing the query.");

            if (values == null || values.Count == 0)
                throw new ArgumentException("Insert requires at least one column-value pair.");

            var cols = new List<string>();
            var vals = new List<string>();
            var registeredCols = new Dictionary<string, bool>();

            foreach (var kv in values)
            {
                cols.Add(kv.Key);
                registeredCols[kv.Key] = true;
                vals.Add(FormatSQLValue(kv.Value));
            }

            if (defaultValuesCacheTbl.TryGetValue(tableName, out var tblDefaults))
            {
                foreach (var kv in tblDefaults)
                {
                    if (!registeredCols.ContainsKey(kv.Key))
                    {
                        registeredCols[kv.Key] = true;
                        cols.Add(kv.Key);
                        vals.Add(FormatSQLValue(kv.Value));
                    }
                }
            }

            query = "INSERT INTO " + tableName + " (" + string.Join(", ", cols) + ") VALUES (" + string.Join(", ", vals) + ")";
            return this;

        }
        public QueryBuilder Update(Dictionary<string, object> values)
        {
            if (string.IsNullOrEmpty(tableName))
                throw new ArgumentException("Table name must be set before executing the query.");

            if (values == null || values.Count == 0)
                throw new ArgumentException("Update requires at least one column-value pair.");

            var updates = values.Select(kv => kv.Key + " = " + FormatSQLValue(kv.Value));
            query = "UPDATE " + tableName + " SET " + string.Join(", ", updates);
            return this;

        }
        public QueryBuilder Delete()
        {
            if (string.IsNullOrEmpty(tableName))
                throw new ArgumentException("Table name must be set before executing the query.");

            query = "DELETE FROM " + tableName;
            return this;

        }
        public QueryBuilder Where(string column, string oper, object value)
        {
            if (string.IsNullOrEmpty(column) || string.IsNullOrEmpty(oper))
                throw new ArgumentException("Where requires the column and the operator to be a string.");

            whereClauses.Add(column + " " + oper + " " + FormatSQLValue(value));
            return this;
        }
        public QueryBuilder OrWhere(string column, string oper, object value)
        {
            if (string.IsNullOrEmpty(column) || string.IsNullOrEmpty(oper))
                throw new ArgumentException("OrWhere requires the column and the operator to be a string.");

            orWhereClauses.Add(column + " " + oper + " " + FormatSQLValue(value));
            return this;
        }
        public QueryBuilder Join(string table_name, string condition, string join_type)
        {
            if (string.IsNullOrEmpty(table_name) || string.IsNullOrEmpty(condition) || string.IsNullOrEmpty(join_type))
                throw new ArgumentException("Join requires the table, the condition and the join type to be a string.");

            joinClauses.Add(join_type + " JOIN " + table_name + " ON " + condition);
            return this;
        }
        public QueryBuilder OrderBy(Dictionary<string, string> columns)
        {
            if (columns == null || columns.Count == 0)
                throw new ArgumentException("OrderBy requires at least one column-order pair.");

            foreach (var kv in columns)
                orderByClauses.Add(kv.Key + " " + kv.Value);

            return this;
        }
        public QueryBuilder Limit(int count)
        {
            limitCount = count;
            return this;
        }
        public QueryBuilder GroupBy(string[] columns)
        {
            if (columns == null || columns.Length == 0)
                throw new ArgumentException("GroupBy requires at least one column.");

            groupByClauses = columns.ToList();
            return this;
        }
        public QueryBuilder Create(Dictionary<string, string> values)
        {
            if (string.IsNullOrEmpty(tableName))
                throw new ArgumentException("Table name must be set before executing the query.");

            if (values == null || values.Count == 0)
                throw new ArgumentException("Create requires at least one column-value pair.");

            var rules = ParseRules(values);
            var definitions = new List<string>();

            foreach (var cr in rules)
            {
                var typ = GenerateColumnType(tableName, cr.ColumnName, cr.Rules, m_db!.GetVersion());
                if (!string.IsNullOrEmpty(typ))
                    definitions.Add(cr.ColumnName + " " + typ);
            }

            query = "CREATE TABLE IF NOT EXISTS " + tableName + " (" + string.Join(", ", definitions) + ")";

            return this;

        }
        public QueryBuilder Alter(Dictionary<string, string> add_columns, Dictionary<string, string> remove_columns, Dictionary<string, string> modify_columns)
        {
            if (string.IsNullOrEmpty(tableName))
                throw new ArgumentException("Table name must be set before executing the query.");

            add_columns ??= [];
            remove_columns ??= [];
            modify_columns ??= [];

            var addRules = ParseRules(add_columns);
            var modifyRules = ParseRules(modify_columns);
            var definitions = new List<string>();

            foreach (var cr in addRules)
            {
                var typ = GenerateColumnType(tableName, cr.ColumnName, cr.Rules, m_db!.GetVersion());
                if (!string.IsNullOrEmpty(typ))
                    definitions.Add("ADD COLUMN " + cr.ColumnName + " " + typ);
            }

            foreach (var cr in modifyRules)
            {
                var typ = GenerateColumnType(tableName, cr.ColumnName, cr.Rules, m_db!.GetVersion());
                if (!string.IsNullOrEmpty(typ))
                    definitions.Add("MODIFY COLUMN " + cr.ColumnName + " " + typ);
            }

            foreach (var col in remove_columns.Keys)
            {
                definitions.Add("DROP COLUMN " + col);
            }

            if (definitions.Count == 0)
                throw new ArgumentException("Alter requires at least one added or one removed column.");

            query = "ALTER TABLE " + tableName + " " + string.Join(", ", definitions);
            return this;

        }
        public QueryBuilder Drop()
        {
            if (string.IsNullOrEmpty(tableName))
                throw new ArgumentException("Table name must be set before executing the query.");

            query = "DROP TABLE IF EXISTS " + tableName;
            return this;

        }
        public QueryBuilder OnDuplicate(Dictionary<string, object> update_value)
        {
            if (update_value == null || update_value.Count == 0)
                throw new ArgumentException("OnDuplicate requires at least one column-value pair.");

            foreach (var kv in update_value)
                onDuplicateClauses.Add(kv.Key + " = " + FormatSQLValue(kv.Value));

            return this;
        }
        public QueryBuilder Having(string condition)
        {
            if (string.IsNullOrEmpty(condition))
                throw new ArgumentException("Having needs the condition to be a string and to not be empty.");

            havingClauses.Add(condition);
            return this;
        }
        public QueryBuilder Distinct()
        {
            if (string.IsNullOrEmpty(tableName))
                throw new ArgumentException("Table name must be set before executing the query.");

            isDistinct = true;
            return this;
        }
        public QueryBuilder Offset(int offset)
        {
            if (offset < 0)
                throw new ArgumentException("Offset needs a count to be a number and positive.");

            offsetCount = offset;
            return this;
        }
        public QueryBuilder Union(string query, bool all)
        {
            unionClauses.Add("UNION" + (all ? " ALL" : "") + " " + query);
            return this;
        }
        public void Execute(Action<string?, Dictionary<string, object>[]> callback)
        {
            var final = PrepareQuery();
            m_db!.Query(final, callback);
        }
        private string PrepareQuery()
        {
            var finalStr = query;

            if (!string.IsNullOrEmpty(finalStr) && finalStr.StartsWith("SELECT", StringComparison.OrdinalIgnoreCase) && isDistinct)
                finalStr = finalStr.Insert(6, " DISTINCT");

            if (joinClauses.Count > 0)
                finalStr = finalStr + " " + string.Join(" ", joinClauses);

            if (whereClauses.Count > 0 || orWhereClauses.Count > 0)
            {
                var combined = "";
                if (whereClauses.Count > 0)
                    combined = string.Join(" AND ", whereClauses);

                if (orWhereClauses.Count > 0)
                {
                    if (combined.Length > 0)
                        combined = combined + " OR ";
                    combined = combined + string.Join(" OR ", orWhereClauses);
                }

                finalStr = finalStr + " WHERE " + combined;
            }

            if (groupByClauses.Count > 0)
                finalStr = finalStr + " GROUP BY " + string.Join(", ", groupByClauses);

            if (havingClauses.Count > 0)
                finalStr = finalStr + " HAVING " + string.Join(" AND ", havingClauses);

            if (orderByClauses.Count > 0)
                finalStr = finalStr + " ORDER BY " + string.Join(", ", orderByClauses);

            if (limitCount >= 0)
                finalStr = finalStr + " LIMIT " + limitCount;

            if (offsetCount >= 0)
                finalStr = finalStr + " OFFSET " + offsetCount;

            if (onDuplicateClauses.Count > 0)
                finalStr = finalStr + " ON DUPLICATE KEY UPDATE " + string.Join(", ", onDuplicateClauses);

            if (unionClauses.Count > 0)
                finalStr = finalStr + " " + string.Join(" ", unionClauses);

            return finalStr;
        }
    }
}
