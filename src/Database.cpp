//
// Created by Vojtěch on 19.05.2025.
//

#include "fmt/format.h"

#include "Database.hpp"

#include <DicomAnonymizer.hpp>

void Database::createTable() {
    m_database << "CREATE TABLE IF NOT EXISTS patients ("
            "_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
            "patient_id TEXT NOT NULL,"
            "pseudoname TEXT NOT NULL,"
            "group_name TEXT NOT NULL,"
            "UNIQUE(patient_id, group_name)"
            ");";

    m_database << "CREATE TABLE IF NOT EXISTS studies ("
            "_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
            "patient_id TEXT NOT NULL,"
            "group_name TEXT NOT NULL,"
            "study_inst_uid TEXT NOT NULL,"
            "study_date TEXT NOT NULL,"
            "modality TEXT NOT NULL,"
            "FOREIGN KEY(patient_id, group_name) REFERENCES patients(patient_id, group_name), "
            "UNIQUE(patient_id, group_name, study_inst_uid)"
            ");";
    m_database << "PRAGMA foreign_keys = ON;";
}

std::string Database::queryPseudoname(const std::string& query_id, const std::string& group_name) {
    std::string pseudoname{};

    m_database << "SELECT pseudoname FROM patients WHERE patient_id=? AND group_name=?;"
            << query_id
            << group_name
            >> [&](const std::string &psn) {
                pseudoname = psn;
            };

    if (pseudoname.empty()) {
        fmt::print("row for patient_id={} not found in group={}\n", query_id, group_name);
    } else {
        fmt::print("patient_id={}, pseudoname={} exists in group={}\n", query_id, pseudoname, group_name);
    }

    return pseudoname;
}

void Database::insertRow(const StudySQLFields &sql_fields,
                         const std::string &   pseudoname,
                         const std::string&    group_name) {

    m_database << "INSERT OR IGNORE INTO patients (patient_id, pseudoname, group_name) VALUES (?, ?, ?);"
            << sql_fields.patientID
            << pseudoname
            << group_name;
    m_database << "INSERT OR IGNORE INTO studies (patient_id, group_name, study_inst_uid, study_date, modality) VALUES (?, ?, ?, ?, ?);"
            << sql_fields.patientID
            << group_name
            << sql_fields.studyInstanceUID
            << sql_fields.studyDate
            << sql_fields.modality;
    fmt::print("Added patient row patient_id={}, pseudoname={} to group={}\n",
               sql_fields.patientID,
               pseudoname,
               group_name);
    fmt::print("Added study row to patient_id={}, group={}\n", sql_fields.patientID, group_name);
    fmt::print("\tstudy_inst_uid={}, study_date={}, modality={}\n",
               sql_fields.studyInstanceUID,
               sql_fields.studyDate,
               sql_fields.modality);
}

std::string Database::createPseudoname(const std::string& group_name) {
    std::string pseudoname{};
    m_database << "SELECT pseudoname FROM patients WHERE group_name=? ORDER BY pseudoname DESC LIMIT 1;"
            << group_name
            >> [&](const std::string &psn) {
                pseudoname = psn;
            };
    int lastIndex = 0;
    if (!pseudoname.empty()) {
        std::size_t uspos = pseudoname.find('_');
        lastIndex = std::stoi(pseudoname.substr(++uspos));
    }
    return fmt::format("{}_{}", group_name, ++lastIndex);
}