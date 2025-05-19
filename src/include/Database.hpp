//
// Created by Vojtěch on 19.05.2025.
//


#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>

#include "sqlite_modern_cpp.h"

struct StudySQLFields {
    std::string patientID{};
    std::string studyInstanceUID{};
    std::string studyDate{};
    std::string modality{};
};

class Database {
public:
    explicit Database(const std::string &database_path) : m_database(database_path),
                                                          m_databasePath(database_path) {};
    ~Database() = default;

    void createTable(const std::string& table_name);
    void setTableName(const std::string& table_name);
    void insertRow(const StudySQLFields& , const std::string& pseudoname);
    std::string queryPseudoname(const std::string& query_id);
    std::string createPseudoname();


private:
    sqlite::database m_database;
    std::string m_currentTableName{};
    std::string m_databasePath{};
};

#endif //DATABASE_HPP
