//
// Created by Vojtěch on 18.03.2025.
//

#include "DicomAnonymizer.hpp"

#include <dcmtk/dcmdata/dcuid.h>

bool DicomAnonymizer::getDicomFilenames(const std::filesystem::path &study_directory) {

    // remove previous filenames when iterating over new study directory
    if (!this->m_dicom_files.empty()) this->m_dicom_files.clear();

    for (const auto& entry: std::filesystem::recursive_directory_iterator(study_directory)) {
        if (entry.is_directory() || entry.path().filename() == "DICOMDIR") continue;

        this->m_dicom_files.push_back(entry.path().string());
    }

    if (this->m_dicom_files.empty()) return false;

    this->m_number_of_files = static_cast<unsigned int>(this->m_dicom_files.size());
    fmt::print("Found {} files\n", m_number_of_files);
    return true;
}

bool DicomAnonymizer::anonymizeStudy(const std::string &pseudoname, const char* root) {

    char newStudyUID[65];
    dcmGenerateUniqueIdentifier(newStudyUID, root);

    OFCondition cond;
    for (const auto& file : this->m_dicom_files) {
        cond = m_fileformat.loadFile(file);
        if (cond.bad()) {
            fmt::print("Unable to load file {}\n", file);
            fmt::print("Reason: {}\n", cond.text());
            return false;
        }
        m_dataset = m_fileformat.getDataset();

        m_dataset->putAndInsertOFStringArray(DCM_PatientName, pseudoname);
        m_dataset->putAndInsertOFStringArray(DCM_PatientID, pseudoname);
        m_dataset->putAndInsertOFStringArray(DCM_PatientAge, "000Y");
        m_dataset->putAndInsertOFStringArray(DCM_PatientSex, "O");
        m_dataset->putAndInsertOFStringArray(DCM_PatientAddress, "");

        //TODO remove institution, operator and relevant tags

        std::string oldSeriesUID{};
        m_dataset->findAndGetOFString(DCM_SeriesInstanceUID, oldSeriesUID);
        m_dataset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, getSeriesUids(oldSeriesUID, root));

        char newSOPInstanceUID[65];
        dcmGenerateUniqueIdentifier(newSOPInstanceUID, root);
        m_dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, newSOPInstanceUID);

        m_dataset->putAndInsertOFStringArray(DCM_StudyInstanceUID, newStudyUID);

        cond = m_fileformat.saveFile(file);
        if (cond.bad()) {
            fmt::print("Unable to save file {}\n", file);
            fmt::print("Reason: {}\n", cond.text());
            return false;
        }

    }


    return true;
}

std::string DicomAnonymizer::getSeriesUids(const std::string &old_series_uid, const char* root) {

    // add old-new series uid map if there isn't one
    // otherwise return existing new uid
    if (!this->m_series_uids.contains(old_series_uid)) {
        char uid[65];
        dcmGenerateUniqueIdentifier(uid, root);
        this->m_series_uids[old_series_uid] = std::string(uid);
        return this->m_series_uids[old_series_uid];
    }

    return this->m_series_uids[old_series_uid];
}