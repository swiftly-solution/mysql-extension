const oldServerVersions = [
    "5.0", "5.1", "5.2", "5.3", "5.4", "5.5", "5.6", "5.7", "8.0"
];

const oldServerCache = {};
const defaultValuesCacheTbl = {};

/**
 * Checks if an object is empty.
 * @param {Object} obj
 * @returns {boolean}
 */
function isObjectEmpty(obj) {
    return Object.keys(obj).length === 0;
}

/**
 * Parses a rule string into an array.
 * @param {string} rule
 * @returns {Array}
 */
function parseRule(rule) {
    const parts = rule.split(":");
    return parts.length > 1 ? [parts[0], parts.slice(1).join(":")] : [parts[0]];
}

/**
 * Parses rules from an object containing column definitions.
 * @param {Object} columns
 * @returns {Array}
 */
function parseRules(columns) {
    const result = [];
    
    for (const [key, value] of Object.entries(columns)) {
        const rules = value.split("|");
        const validatedRules = rules.map(parseRule);
        
        result.push([key, validatedRules]);
    }
    
    return result;
}

function GenerateColumnType(tblName, columnName, columnRules, version, db) {
    let retType = "VARCHAR(255)";
    let defaultValue = "";
    let defaultSet = false;
    let isNullable = false;
    let isInteger = false;
    let unique = false;
    let primaryKey = false;
    let index = false;
    let autoIncrement = false;

    let oldServer = oldServerCache[version];
    if (oldServer === undefined) {
        oldServer = oldServerVersions.some(v => version.startsWith(v));
        oldServerCache[version] = oldServer;
    }

    for (const [name, param] of columnRules) {
        switch (name) {
            case "nullable":
                isNullable = true;
                break;
            case "integer":
                retType = "INT";
                isInteger = true;
                break;
            case "string":
                retType = "VARCHAR(255)";
                break;
            case "boolean":
                retType = "TINYINT(1)";
                break;
            case "date":
                retType = "DATE";
                break;
            case "datetime":
                retType = "DATETIME";
                break;
            case "min":
                if (param) {
                    const val = Math.floor(Number(param) || 0);
                    if (isInteger && val >= 0) {
                        retType = "UNSIGNED INT";
                    }
                }
                break;
            case "max":
            case "size":
                if (param) {
                    const val = Math.floor(Number(param) || 0);
                    if (retType.startsWith("VARCHAR") && val > 0) {
                        retType = `VARCHAR(${val})`;
                    }
                }
                break;
            case "json":
                retType = "TEXT";
                defaultValue = "{}";
                defaultSet = true;
                break;
            case "float":
                retType = "FLOAT";
                break;
            case "default":
                if (param) {
                    defaultValue = param;
                    defaultSet = true;
                }
                break;
            case "unique":
                unique = true;
                primaryKey = false;
                index = false;
                break;
            case "primary":
                unique = false;
                primaryKey = true;
                index = false;
                break;
            case "index":
                unique = false;
                primaryKey = false;
                index = true;
                break;
            case "autoincrement":
                autoIncrement = true;
                unique = false;
                primaryKey = true;
                index = false;
                break;
        }
    }

    retType += isNullable ? " NULL" : " NOT NULL";

    if (defaultSet) {
        if(retType.startsWith("VARCHAR") || retType.startsWith("TEXT")) {
            defaultValuesCacheTbl[tblName] = defaultValuesCacheTbl[tblName] || {};
            defaultValuesCacheTbl[tblName][columnName] = (retType.startsWith("VARCHAR") || retType.startsWith("TEXT") || retType.startsWith("DATE")) ? defaultValue : Number(defaultValue);
        } else {
            retType += " DEFAULT " + defaultValue;
        }
    }

    if (autoIncrement) {
        retType += " AUTO_INCREMENT";
    }

    if (primaryKey) {
        retType += `, PRIMARY KEY (\`${columnName}\`)`;
    }

    if (unique) {
        retType += `, UNIQUE (\`${columnName}\`)`;
    }

    if (index) {
        retType += `, INDEX (\`${columnName}\`)`;
    }

    return retType;
}

function MySQL_QB(db) {
    
    return {
        db,
        tableName: "",
        query: "",
        whereClauses : [],
        orWhereClauses : [],
        joinClauses : [],
        groupByClauses : [],
        orderByClauses : [],
        onDuplicateClauses : [],
        havingClauses : [],
        unionClauses : {},
        updatePairs : {},
        isDistinct : false,
        limitCount : -1,
        offsetCount : -1,

        formatSQLValue(value) {
            if (value === null || value === undefined) return "NULL";
            else if (typeof value === "object") return `"${db.EscapeString(JSON.stringify(value))}"`;
            else if (typeof value === "string") return `"${db.EscapeString(value)}"`;
            else return String(value);
        },

        Table(tblName) {
            this.tableName = tblName;
            return this;
        },

        Create(columns){
            if (this.tableName.length <= 0) throw new Error("Table name must be set before executing the query.");
            if (isObjectEmpty(columns)) throw new Error("Create requires at least one column-value pair.");
            const rules = parseRules(columns);
            const definitions = rules.map(([columnName, columnRules]) => `${columnName} ${GenerateColumnType(this.tableName, columnName, columnRules, this.db.GetVersion(), this.db)}`);
            this.query = `CREATE TABLE IF NOT EXISTS ${this.tableName} (${definitions.join(", ")})`;
            return this;
        },

        Alter(addColumns, removeColumns, modifyColumns)
        {
            if (this.tableName.length <= 0) throw new Error("Table name must be set before executing the query.");
            if (!Array.isArray(addColumns) || !Array.isArray(removeColumns) || !Array.isArray(modifyColumns)) {
                throw new Error("Alter requires add and remove columns to be a list.");
            }

            const addRules = parseRules(addColumns);
            const modifyRules = parseRules(modifyColumns);
            let definitions = [];

            for (const [columnName, columnRules] of addRules) {
                const typ = GenerateColumnType(this.tableName, columnName, columnRules, this.db.GetVersion(), this.db);
                definitions.push(`ADD COLUMN ${columnName} ${typ}`);
            }

            for (const [columnName, columnRules] of modifyRules) {
                const typ = GenerateColumnType(this.tableName, columnName, columnRules, this.db.GetVersion(), this.db);
                definitions.push(`MODIFY COLUMN ${columnName} ${typ}`);
            }

            for (const columnName of removeColumns) {
                definitions.push(`DROP COLUMN ${columnName}`);
            }

            if (definitions.length === 0) throw new Error("Alter requires at least one added or removed column.");
            
            this.query = `ALTER TABLE ${this.tableName} ${definitions.join(", ")}`;
            return this;
        },

        Drop() {
            if (this.tableName.length <= 0) throw new Error("Table name must be set before executing the query.");

            self.query = `DROP TABLE IF EXISTS ${this.tableName}`;
            return self;
        },

        Select(columns) {
            if (this.tableName.length <= 0) throw new Error("Table name must be set before executing the query.");
            this.query = Array.isArray(columns) && columns.length > 0 ? `SELECT ${columns.join(", ")} FROM ${this.tableName}` : `SELECT * FROM ${this.tableName}`;
            return this;            
        },

        Insert(values){
            if (this.tableName.length <= 0) throw new Error("Table name must be set before executing the query.");
            if (isObjectEmpty(values)) throw new Error("Insert requires at least one column-value pair.");

            let registeredCols = []
            const cols = Object.keys(values);
            registeredCols = cols;
            const vals = cols.map(col => this.formatSQLValue(values[col]));

            if(defaultValuesCacheTbl.hasOwnProperty(this.tableName)) {
                for(const [columnName, columnValue] of Object.entries(defaultValuesCacheTbl[this.tableName])) {
                    registeredCols.push(columnName);
                    cols.push(columnName)
                    vals.push(this.formatSQLValue(columnValue));
                }
            }

            this.query = `INSERT INTO ${this.tableName} (${cols.join(", ")}) VALUES (${vals.join(", ")})`;
            return this;
        },

        Update(values) {
            if (this.tableName.length <= 0) throw new Error("Table name must be set before executing the query.");
            if (typeof values !== "object" || isObjectEmpty(values)) throw new Error("Update requires at least one column-value pair.");

            const updates = [];

            for (const [column, value] of Object.entries(values)) {
                updates.push(`${column} = ${this.formatSQLValue(value)}`);
            }

            this.query = `UPDATE ${this.tableName} SET ${updates.join(", ")}`;
            return this;
        },

        Delete() {
            if (this.tableName.length <= 0) throw new Error("Table name must be set before executing the query.");
            this.query = `DELETE FROM ${this.tableName}`;
            return this;
        },

        Limit(count) {
            if(typeof count !== "number") throw new Error("Limit requires the count to be a number.")
            this.limitCount = count;
            return this;
        },

        GroupBy(columns) {
            if (!Array.isArray(columns) || columns.length === 0) throw new Error("GroupBy requires at least one column.");
            this.groupByClauses = columns;
            return this;
        },

        Where(column, operator_, value ) {
            if( typeof column !== "string" || typeof operator_ !== "string") throw new Error("Where requires the column and the operator to be a string.");
            this.whereClauses.push(`${column} ${operator_} ${this.formatSQLValue(value)}`);
            return this;
        },

        OrWhere(column, operator_, value ) {
            if( typeof column !== "string" || typeof operator_ !== "string") throw new Error("Where requires the column and the operator to be a string.");
            this.orWhereClauses.push(`${column} ${operator_} ${this.formatSQLValue(value)}`);
            return this;
        },

        Join(tbl, onCondition, joinType) {
            if (typeof tbl !== "string" || typeof onCondition !== "string" || typeof joinType !== "string") {
                throw new Error("Join requires the table, the condition, and the join type to be strings.");
            }

            this.joinClauses.push(`${joinType} JOIN ${tbl} ON ${onCondition}`);

            return this;
        },

        Offset(count) {
            if (typeof count !== "number" || count < 0) {
                throw new Error("Offset needs a count to be a number and positive.");
            }

            this.offsetCount = count;

            return this;
        },

        Distinct() {
            if (this.tableName.length <= 0) throw new Error("Table name must be set before executing the query.");

            this.isDistinct = true;

            return this;
        },

        Having(condition) {
            if (typeof condition !== "string" || condition.length <= 0) {
                throw new Error("Having needs the condition to be a string and to not be empty.");
            }

            this.havingClauses.push(condition);

            return this;
        },

        OnDuplicate(data) {
            if (typeof data !== "object" || isObjectEmpty(data)) {
                throw new Error("OnDuplicate requires at least one column-value pair.");
            }

            for (const [col, val] of Object.entries(data)) {
                this.onDuplicateClauses.push(`${col} = ${this.formatSQLValue(val)}`);
            }

            return this;
        },

        OrderBy(columns) {
            if (typeof columns !== "object" || isObjectEmpty(data)) {
                throw new Error("OrderBy requires at least one column-order pair.");
            }

            for (const [col, val] of Object.entries(columns)) {
                this.orderByClauses.push(`${col} ${val}`);
            }

            return this;
        },

        PrepareQuery() {
            let finalStr = this.query;

            if (finalStr.startsWith("SELECT") && this.isDistinct) {
                finalStr = finalStr.replace("SELECT", "SELECT DISTINCT");
            }

            if (this.joinClauses.length > 0) {
                finalStr += " " + this.joinClauses.join(" ");
            }
            if (this.whereClauses.length > 0 || this.orWhereClauses.length > 0) {
                let combined = "";

                if (this.whereClauses.length > 0) {
                    combined = this.whereClauses.join(" AND ");
                }

                if (this.orWhereClauses.length > 0) {
                    if (combined.length > 0) {
                        combined += " OR ";
                    }
                    combined += this.orWhereClauses.join(" OR ");
                }

                finalStr += " WHERE " + combined;
            }

            if (this.groupByClauses.length > 0) {
                finalStr += " GROUP BY " + this.groupByClauses.join(", ");
            }

            if (this.havingClauses.length > 0) {
                finalStr += " HAVING " + this.havingClauses.join(" AND ");
            }

            if (this.orderByClauses.length > 0) {
                finalStr += " ORDER BY " + this.orderByClauses.join(", ");
            }

            if (this.limitCount >= 0) {
                finalStr += " LIMIT " + this.limitCount;
            }

            if (this.offsetCount >= 0) {
                finalStr += " OFFSET " + this.offsetCount;
            }

            if (this.onDuplicateClauses.length > 0) {
                finalStr += " ON DUPLICATE KEY UPDATE " + this.onDuplicateClauses.join(", ");
            }

            return finalStr;           
        },

        Execute(cb) {
            let finalStr = this.PrepareQuery()
            this.db.Query(finalStr, cb)
        }
    }
}