//
// Created by Vojtěch on 19.05.2025.
//

#include "fmt/format.h"

#include "Database.hpp"

void Database::createTable(const std::string& table_name) {
    m_database << "CREATE TABLE IF NOT EXISTS " + table_name + " ("
    "_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
    "patient_id TEXT UNIQUE NOT NULL,"
    "pseudoname TEXT UNIQUE NOT NULL);";
    m_currentTableName = table_name;
    fmt::print("Created table {}\n", m_currentTableName);
}

void Database::setTableName(const std::string& table_name) {
    m_currentTableName = table_name;
}

std::string Database::queryPseudoname(const std::string& query_id) {
    std::string pseudoname{};

    m_database << "SELECT pseudoname FROM " + m_currentTableName + " WHERE patient_id=?;"
            << query_id
            >> [&](const std::string &psn) {
                pseudoname = psn;
            };

    if (pseudoname.empty()) {
        fmt::print("Table {}: row for patient_id={} not found\n", m_currentTableName, query_id);
    } else {
        fmt::print("Table {}: patient_id={}, pseudoname={}\n", m_currentTableName, query_id, pseudoname);
    }

    return pseudoname;
}

void Database::insertRow(const std::string &patient_id, const std::string& pseudoname) {
    const std::string existingPseudoname = queryPseudoname(patient_id);

    if (existingPseudoname.empty()) {
        // fmt::print("creating pseudoname for patient_id={}\n", insert_id);
        // pseudoname = this->createPseudoname();
        m_database << "INSERT INTO " + m_currentTableName + " (patient_id, pseudoname) VALUES (?, ?);"
                << patient_id << pseudoname;
        fmt::print("Table {}: added row patient_id={}, pseudoname={}\n", m_currentTableName, patient_id, pseudoname);
    } else {
        fmt::print("Table: row exists patient_id={}, pseudoname={}\n", m_currentTableName, patient_id, pseudoname);
    }
}

std::string Database::createPseudoname() {
    std::string pseudoname{};
    m_database << "SELECT pseudoname FROM " + m_currentTableName + " ORDER BY pseudoname DESC LIMIT 1;"
            >> [&](const std::string &psn) {
                pseudoname = psn;
            };
    int lastIndex = 0;
    if (!pseudoname.empty()) {
        std::size_t uspos = pseudoname.find('_');
        lastIndex = std::stoi(pseudoname.substr(++uspos));
    }
    return fmt::format("{}_{}", m_currentTableName, ++lastIndex);
}