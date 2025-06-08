// File: database_logger.hpp
#ifndef DATABASE_LOGGER_HPP
#define DATABASE_LOGGER_HPP

#include <string>
#include <sqlite3.h>
#include <stdexcept>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

class DatabaseLogger {
private:
    sqlite3* db_;
    std::string db_path_;
    
public:
    DatabaseLogger(const std::string& db_path) : db_path_(db_path), db_(nullptr) {}
    
    ~DatabaseLogger() {
        close();
    }
    
    void open() {
        if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
            throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db_)));
        }
        
        const char* create_table_sql = 
            "CREATE TABLE IF NOT EXISTS door_events ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "state TEXT NOT NULL,"
            "source TEXT NOT NULL,"
            "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
            ");";
        
        char* err_msg = nullptr;
        if (sqlite3_exec(db_, create_table_sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
            std::string error = "Failed to create table: " + std::string(err_msg);
            sqlite3_free(err_msg);
            throw std::runtime_error(error);
        }
    }
    
    void close() {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }
    
    void logEvent(const std::string& state, const std::string& source) {
        const char* insert_sql = "INSERT INTO door_events (state, source) VALUES (?, ?);";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement");
        }
        
        sqlite3_bind_text(stmt, 1, state.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, source.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert event");
        }
        
        sqlite3_finalize(stmt);
        
        // Also log to text file
        logToFile(state, source);
    }
    
private:
    void logToFile(const std::string& state, const std::string& source) {
        // Get current date for filename
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
        std::string filename = ss.str() + ".txt";
        
        // Get full timestamp for log entry
        ss.str("");
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        std::string timestamp = ss.str();
        
        // Append to log file
        std::ofstream log_file(filename, std::ios::app);
        if (log_file.is_open()) {
            log_file << timestamp << " - State: " << state 
                     << ", Source: " << source << std::endl;
            log_file.close();
        }
    }
};