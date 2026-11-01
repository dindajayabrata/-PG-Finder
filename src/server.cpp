#include <httplib.h>
#include <nlohmann/json.hpp>

#include <sql.h>
#include <sqlext.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

std::string odbcError(SQLSMALLINT handleType, SQLHANDLE handle, const std::string& action) {
    SQLCHAR state[6]{};
    SQLCHAR message[1024]{};
    SQLINTEGER nativeError = 0;
    SQLSMALLINT messageLength = 0;
    if (SQLGetDiagRecA(handleType, handle, 1, state, &nativeError,
                       message, sizeof(message), &messageLength) == SQL_SUCCESS) {
        return action + " failed (" + reinterpret_cast<char*>(state) + "): " +
               reinterpret_cast<char*>(message);
    }
    return action + " failed.";
}

void requireSuccess(SQLRETURN result, SQLSMALLINT handleType, SQLHANDLE handle,
                    const std::string& action) {
    if (result != SQL_SUCCESS && result != SQL_SUCCESS_WITH_INFO) {
        throw std::runtime_error(odbcError(handleType, handle, action));
    }
}

class Connection {
public:
    Connection() {
        const char* connectionString = std::getenv("PGFINDER_ORACLE_CONNECTION");
        if (!connectionString || std::strlen(connectionString) == 0) {
            throw std::runtime_error(
                "Set PGFINDER_ORACLE_CONNECTION before starting the backend. See README.md.");
        }
        requireSuccess(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment_),
                       SQL_HANDLE_ENV, environment_, "Create ODBC environment");
        requireSuccess(SQLSetEnvAttr(environment_, SQL_ATTR_ODBC_VERSION,
                                     reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0),
                       SQL_HANDLE_ENV, environment_, "Set ODBC version");
        requireSuccess(SQLAllocHandle(SQL_HANDLE_DBC, environment_, &connection_),
                       SQL_HANDLE_ENV, environment_, "Create ODBC connection");

        SQLCHAR output[1024]{};
        SQLSMALLINT outputLength = 0;
        const SQLRETURN result = SQLDriverConnectA(
            connection_, nullptr,
            reinterpret_cast<SQLCHAR*>(const_cast<char*>(connectionString)), SQL_NTS,
            output, sizeof(output), &outputLength, SQL_DRIVER_NOPROMPT);
        requireSuccess(result, SQL_HANDLE_DBC, connection_, "Connect to Oracle");
    }

    ~Connection() {
        if (connection_ != SQL_NULL_HDBC) {
            SQLDisconnect(connection_);
            SQLFreeHandle(SQL_HANDLE_DBC, connection_);
        }
        if (environment_ != SQL_NULL_HENV) SQLFreeHandle(SQL_HANDLE_ENV, environment_);
    }

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    SQLHDBC handle() const { return connection_; }

    void commit() {
        requireSuccess(SQLEndTran(SQL_HANDLE_DBC, connection_, SQL_COMMIT),
                       SQL_HANDLE_DBC, connection_, "Commit transaction");
    }

private:
    SQLHENV environment_ = SQL_NULL_HENV;
    SQLHDBC connection_ = SQL_NULL_HDBC;
};

class Statement {
public:
    Statement(Connection& connection, const std::string& sql) {
        requireSuccess(SQLAllocHandle(SQL_HANDLE_STMT, connection.handle(), &statement_),
                       SQL_HANDLE_DBC, connection.handle(), "Create SQL statement");
        const SQLRETURN result = SQLPrepareA(
            statement_, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())), SQL_NTS);
        requireSuccess(result, SQL_HANDLE_STMT, statement_, "Prepare SQL statement");
    }

    ~Statement() {
        if (statement_ != SQL_NULL_HSTMT) SQLFreeHandle(SQL_HANDLE_STMT, statement_);
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    void bindText(SQLUSMALLINT position, const std::string& value) {
        textValues_.push_back(value);
        textIndicators_.push_back(SQL_NTS);
        std::string& stored = textValues_.back();
        requireSuccess(SQLBindParameter(statement_, position, SQL_PARAM_INPUT,
            SQL_C_CHAR, SQL_VARCHAR, std::max<std::size_t>(1, stored.size()), 0,
            stored.data(), static_cast<SQLLEN>(stored.size() + 1), &textIndicators_.back()),
            SQL_HANDLE_STMT, statement_, "Bind text parameter");
    }

    void bindInt(SQLUSMALLINT position, SQLINTEGER& value) {
        requireSuccess(SQLBindParameter(statement_, position, SQL_PARAM_INPUT,
            SQL_C_SLONG, SQL_INTEGER, 10, 0, &value, 0, nullptr),
            SQL_HANDLE_STMT, statement_, "Bind integer parameter");
    }

    void bindDouble(SQLUSMALLINT position, SQLDOUBLE& value) {
        requireSuccess(SQLBindParameter(statement_, position, SQL_PARAM_INPUT,
            SQL_C_DOUBLE, SQL_DOUBLE, 15, 2, &value, 0, nullptr),
            SQL_HANDLE_STMT, statement_, "Bind numeric parameter");
    }

    void bindOutputInt(SQLUSMALLINT position, SQLINTEGER& value) {
        requireSuccess(SQLBindParameter(statement_, position, SQL_PARAM_OUTPUT,
            SQL_C_SLONG, SQL_INTEGER, 10, 0, &value, 0, nullptr),
            SQL_HANDLE_STMT, statement_, "Bind output ID");
    }

    void bindOutputText(SQLUSMALLINT position, char* value, SQLLEN capacity) {
        requireSuccess(SQLBindParameter(statement_, position, SQL_PARAM_OUTPUT,
            SQL_C_CHAR, SQL_VARCHAR, static_cast<SQLULEN>(capacity), 0,
            value, capacity, nullptr),
            SQL_HANDLE_STMT, statement_, "Bind output text");
    }

    void execute() {
        requireSuccess(SQLExecute(statement_), SQL_HANDLE_STMT, statement_, "Execute SQL");
    }

    SQLHSTMT handle() const { return statement_; }

private:
    SQLHSTMT statement_ = SQL_NULL_HSTMT;
    // ODBC keeps these addresses until SQLExecute, so use containers with stable references.
    std::deque<std::string> textValues_;
    std::deque<SQLLEN> textIndicators_;
};

std::string columnText(SQLHSTMT statement, SQLUSMALLINT column) {
    char buffer[4096]{};
    SQLLEN indicator = 0;
    const SQLRETURN result = SQLGetData(statement, column, SQL_C_CHAR,
                                        buffer, sizeof(buffer), &indicator);
    if (result == SQL_NO_DATA || indicator == SQL_NULL_DATA) return "";
    requireSuccess(result, SQL_HANDLE_STMT, statement, "Read text column");
    return std::string(buffer);
}

SQLINTEGER columnInt(SQLHSTMT statement, SQLUSMALLINT column) {
    SQLINTEGER value = 0;
    SQLLEN indicator = 0;
    const SQLRETURN result = SQLGetData(statement, column, SQL_C_SLONG,
                                        &value, sizeof(value), &indicator);
    if (indicator == SQL_NULL_DATA) return 0;
    requireSuccess(result, SQL_HANDLE_STMT, statement, "Read integer column");
    return value;
}

double columnDouble(SQLHSTMT statement, SQLUSMALLINT column) {
    double value = 0;
    SQLLEN indicator = 0;
    const SQLRETURN result = SQLGetData(statement, column, SQL_C_DOUBLE,
                                        &value, sizeof(value), &indicator);
    if (indicator == SQL_NULL_DATA) return 0;
    requireSuccess(result, SQL_HANDLE_STMT, statement, "Read numeric column");
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

void sendJson(httplib::Response& response, int status, const json& body) {
    response.status = status;
    response.set_content(body.dump(), "application/json; charset=utf-8");
}

json requestJson(const httplib::Request& request) {
    try {
        return json::parse(request.body);
    } catch (...) {
        throw std::invalid_argument("Request body must contain valid JSON.");
    }
}

std::string requiredText(const json& body, const char* key) {
    if (!body.contains(key) || !body[key].is_string()) {
        throw std::invalid_argument(std::string("Missing or invalid field: ") + key);
    }
    std::string value = body[key].get<std::string>();
    if (value.empty()) throw std::invalid_argument(std::string("Please enter ") + key + ".");
    return value;
}

SQLINTEGER requiredPositiveId(const json& body, const char* key) {
    if (!body.contains(key) || !body[key].is_number_integer()) {
        throw std::invalid_argument(std::string("Missing or invalid field: ") + key);
    }
    const auto value = body[key].get<long long>();
    if (value < 1 || value > 2147483647) {
        throw std::invalid_argument(std::string("Invalid ID: ") + key);
    }
    return static_cast<SQLINTEGER>(value);
}

std::string cityFromLocation(const std::string& location) {
    const std::size_t comma = location.find_last_of(',');
    if (comma == std::string::npos) return location;
    std::string city = location.substr(comma + 1);
    const std::size_t first = city.find_first_not_of(" \t");
    const std::size_t last = city.find_last_not_of(" \t");
    if (first == std::string::npos) return location;
    return city.substr(first, last - first + 1);
}

void registerRoutes(httplib::Server& server) {
    server.Get("/api/rooms", [](const httplib::Request&, httplib::Response& response) {
        try {
            Connection connection;
            Statement statement(connection,
                "SELECT room_id, title, location, city, price, room_type, image_url, description "
                "FROM rooms ORDER BY created_at DESC");
            statement.execute();
            json rooms = json::array();
            while (SQLFetch(statement.handle()) == SQL_SUCCESS) {
                rooms.push_back({
                    {"id", columnInt(statement.handle(), 1)},
                    {"title", columnText(statement.handle(), 2)},
                    {"location", columnText(statement.handle(), 3)},
                    {"city", columnText(statement.handle(), 4)},
                    {"price", columnDouble(statement.handle(), 5)},
                    {"type", columnText(statement.handle(), 6)},
                    {"image", columnText(statement.handle(), 7)},
                    {"desc", columnText(statement.handle(), 8)}
                });
            }
            sendJson(response, 200, rooms);
        } catch (const std::exception& error) {
            sendJson(response, 500, {{"error", error.what()}});
        }
    });

    server.Post("/api/register", [](const httplib::Request& request, httplib::Response& response) {
        try {
            const json body = requestJson(request);
            const std::string name = requiredText(body, "name");
            const std::string email = lower(requiredText(body, "email"));
            const std::string password = requiredText(body, "password");
            Connection connection;
            Statement statement(connection,
                "BEGIN sp_register_user(?, ?, ?, 'user', ?); END;");
            statement.bindText(1, name);
            statement.bindText(2, email);
            statement.bindText(3, password);
            SQLINTEGER userId = 0;
            statement.bindOutputInt(4, userId);
            statement.execute();
            sendJson(response, 201, {{"message", "Account registered successfully."}, {"id", userId}});
        } catch (const std::invalid_argument& error) {
            sendJson(response, 400, {{"error", error.what()}});
        } catch (const std::exception& error) {
            const std::string message = error.what();
            const bool duplicate = message.find("-20001") != std::string::npos ||
                                   message.find("ORA-20001") != std::string::npos;
            sendJson(response, duplicate ? 409 : 500,
                     {{"error", duplicate ? "That email is already registered." : message}});
        }
    });

    server.Post("/api/login", [](const httplib::Request& request, httplib::Response& response) {
        try {
            const json body = requestJson(request);
            const std::string email = lower(requiredText(body, "email"));
            const std::string password = requiredText(body, "password");
            Connection connection;
            Statement statement(connection,
                "SELECT user_id, full_name, email, role FROM users "
                "WHERE LOWER(email) = ? AND password_hash = ?");
            statement.bindText(1, email);
            statement.bindText(2, password);
            statement.execute();
            if (SQLFetch(statement.handle()) != SQL_SUCCESS) {
                sendJson(response, 401, {{"error", "Email or password is incorrect."}});
                return;
            }
            json user = {
                {"id", columnInt(statement.handle(), 1)},
                {"name", columnText(statement.handle(), 2)},
                {"email", columnText(statement.handle(), 3)},
                {"role", columnText(statement.handle(), 4)}
            };
            sendJson(response, 200, {{"user", user}});
        } catch (const std::invalid_argument& error) {
            sendJson(response, 400, {{"error", error.what()}});
        } catch (const std::exception& error) {
            sendJson(response, 500, {{"error", error.what()}});
        }
    });

    server.Get(R"(/api/favorites/([0-9]+))",
        [](const httplib::Request& request, httplib::Response& response) {
        try {
            const SQLINTEGER userId = static_cast<SQLINTEGER>(std::stol(request.matches[1]));
            if (userId < 1) throw std::invalid_argument("Invalid user ID.");
            Connection connection;
            Statement statement(connection,
                "SELECT room_id FROM favorites WHERE user_id = ? ORDER BY created_at DESC");
            SQLINTEGER bindUserId = userId;
            statement.bindInt(1, bindUserId);
            statement.execute();
            json favorites = json::array();
            while (SQLFetch(statement.handle()) == SQL_SUCCESS) {
                favorites.push_back(columnInt(statement.handle(), 1));
            }
            sendJson(response, 200, favorites);
        } catch (const std::exception& error) {
            sendJson(response, 500, {{"error", error.what()}});
        }
    });

    server.Post("/api/favorites/toggle",
        [](const httplib::Request& request, httplib::Response& response) {
        try {
            const json body = requestJson(request);
            SQLINTEGER userId = requiredPositiveId(body, "userId");
            SQLINTEGER roomId = requiredPositiveId(body, "roomId");
            Connection connection;
            Statement statement(connection, "BEGIN sp_toggle_favorite(?, ?, ?); END;");
            statement.bindInt(1, userId);
            statement.bindInt(2, roomId);
            char status[32]{};
            statement.bindOutputText(3, status, sizeof(status));
            statement.execute();
            sendJson(response, 200, {{"status", status}});
        } catch (const std::invalid_argument& error) {
            sendJson(response, 400, {{"error", error.what()}});
        } catch (const std::exception& error) {
            sendJson(response, 500, {{"error", error.what()}});
        }
    });

    server.Post("/api/rooms", [](const httplib::Request& request, httplib::Response& response) {
        try {
            const json body = requestJson(request);
            const std::string title = requiredText(body, "title");
            const std::string location = requiredText(body, "location");
            const std::string city = body.contains("city") && body["city"].is_string()
                ? body["city"].get<std::string>() : cityFromLocation(location);
            const std::string type = requiredText(body, "type");
            const std::string image = requiredText(body, "image");
            const std::string description = body.value("desc", std::string{});
            const double rentValue = body.value("price", 0.0);
            if (rentValue <= 0) throw std::invalid_argument("Monthly rent must be greater than zero.");
            SQLDOUBLE rent = rentValue;
            SQLINTEGER ownerId = requiredPositiveId(body, "ownerId");
            Connection connection;
            Statement statement(connection,
                "INSERT INTO rooms (title, location, city, price, room_type, gender_pref, "
                "description, image_url, owner_id) VALUES (?, ?, ?, ?, ?, 'any', ?, ?, ?)");
            statement.bindText(1, title);
            statement.bindText(2, location);
            statement.bindText(3, city);
            statement.bindDouble(4, rent);
            statement.bindText(5, type);
            statement.bindText(6, description);
            statement.bindText(7, image);
            statement.bindInt(8, ownerId);
            statement.execute();
            connection.commit();
            sendJson(response, 201, {{"message", "Property listed successfully."}});
        } catch (const std::invalid_argument& error) {
            sendJson(response, 400, {{"error", error.what()}});
        } catch (const std::exception& error) {
            sendJson(response, 500, {{"error", error.what()}});
        }
    });

    server.Post("/api/contact", [](const httplib::Request& request, httplib::Response& response) {
        try {
            const json body = requestJson(request);
            const std::string name = requiredText(body, "name");
            const std::string email = requiredText(body, "email");
            const std::string subject = body.value("subject", std::string{});
            const std::string message = requiredText(body, "message");
            Connection connection;
            Statement statement(connection,
                "INSERT INTO contact_messages (name, email, subject, message) VALUES (?, ?, ?, ?)");
            statement.bindText(1, name);
            statement.bindText(2, email);
            statement.bindText(3, subject);
            statement.bindText(4, message);
            statement.execute();
            connection.commit();
            sendJson(response, 201, {{"message", "Your message has been saved."}});
        } catch (const std::invalid_argument& error) {
            sendJson(response, 400, {{"error", error.what()}});
        } catch (const std::exception& error) {
            sendJson(response, 500, {{"error", error.what()}});
        }
    });
}

} // namespace

int main() {
    httplib::Server server;
    registerRoutes(server);

    const std::string publicFolder = "public";
    server.Get("/", [](const httplib::Request&, httplib::Response& response) {
        response.status = 302;
        response.set_header("Location", "/pages/index.html");
    });
    if (!server.set_mount_point("/", publicFolder)) {
        std::cerr << "Could not find the public folder. Start this program from the project folder.\n";
        return 1;
    }
    server.set_error_handler([](const httplib::Request&, httplib::Response& response) {
        if (response.body.empty()) response.set_content("Not found", "text/plain; charset=utf-8");
    });

    std::cout << "PG Finder server: http://localhost:3000\n";
    std::cout << "Press Ctrl+C to stop.\n";
    if (!server.listen("0.0.0.0", 3000)) {
        std::cerr << "Could not start the server on port 3000. Is another program using it?\n";
        return 1;
    }
    return 0;
}
