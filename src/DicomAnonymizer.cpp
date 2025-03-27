//
// Created by Vojtěch on 18.03.2025.
//

#include "DicomAnonymizer.hpp"

#include <set>

#include <dcmtk/dcmdata/dcuid.h>

bool StudyAnonymizer::getStudyFilenames(const std::filesystem::path &study_directory) {

    // remove previous filenames and series uids when iterating over new study directory
    if (!m_dicom_files.empty()) m_dicom_files.clear();
    if (!m_series_uids.empty()) m_series_uids.clear();

    for (const auto& entry: std::filesystem::recursive_directory_iterator(study_directory)) {
        if (entry.is_directory() || entry.path().filename() == "DICOMDIR") continue;

        m_dicom_files.push_back(entry.path().string());
    }

    if (m_dicom_files.empty()) {
        fmt::print("No files found\n");
        return false;
    }

    fmt::print("Found {} files\n", m_dicom_files.size());
    return true;
}

bool StudyAnonymizer::anonymizeStudy(const std::string &               pseudoname,
                                     const std::set<E_ANONYM_METHODS> &methods,
                                     const char *                      root) {


    char newStudyUID[65];
    dcmGenerateUniqueIdentifier(newStudyUID, root);

    int fileNumber{0};
    for (const auto& file : m_dicom_files) {
        OFCondition cond = m_fileformat.loadFile(file.c_str());
        if (cond.bad()) {
            fmt::print("Unable to load file {}\n", file);
            fmt::print("Reason: {}\n", cond.text());
            return false;
        }

        m_dataset = m_fileformat.getDataset();
        // for writing pre-post deidentification to text file
        m_dataset->findAndGetOFString(DCM_PatientName, m_oldName);
        m_dataset->findAndGetOFString(DCM_PatientID, m_oldID);

        // dicom tags anonymization specification https://dicom.nema.org/medical/dicom/current/output/chtml/part15/chapter_E.html
        // deidentification methods explained https://dicom.nema.org/medical/dicom/current/output/chtml/part16/sect_CID_7050.html

        // Basic Application Confidentiality Profile
        if (methods.contains(M_113100)) {
            m_dataset->putAndInsertOFStringArray(DCM_PatientName, pseudoname);
            m_dataset->putAndInsertOFStringArray(DCM_PatientID, pseudoname);
            m_dataset->putAndInsertOFStringArray(DCM_PatientAddress, "");
            m_dataset->putAndInsertOFStringArray(DCM_AdditionalPatientHistory, "");
            // m_dataset->remove(DCM_PatientAddress);
            // m_dataset->remove(DCM_AdditionalPatientHistory);

            // Retain Patient Characteristics Option
            if (!methods.contains(M_113108)) {
                // FIXME where applicable replace ->putAndInsertOFStringArray with ->remove
                m_dataset->putAndInsertOFStringArray(DCM_PatientAge, "000Y");
                m_dataset->putAndInsertOFStringArray(DCM_PatientSex, "O");
                m_dataset->remove(DCM_PatientInstitutionResidence);
                // m_dataset->remove(DCM_PatientAge);
                // m_dataset->remove(DCM_PatientWeight);
                // m_dataset->remove(DCM_PatientSize);
            }
        }


        // Retain Institution Identity Option
        if (!methods.contains(M_113112)) {
            // FIXME where applicable replace ->putAndInsertOFStringArray with ->remove
            // institution tags
            m_dataset->putAndInsertOFStringArray(DCM_InstitutionName, "");
            m_dataset->putAndInsertOFStringArray(DCM_InstitutionAddress, "");
            m_dataset->putAndInsertOFStringArray(DCM_InstitutionalDepartmentName, "");

            // operator, physician and other medical personel tags
            m_dataset->putAndInsertOFStringArray(DCM_OperatorsName, "");
            m_dataset->putAndInsertOFStringArray(DCM_ReferringPhysicianName, "");
            m_dataset->putAndInsertOFStringArray(DCM_PerformingPhysicianName, "");
            m_dataset->putAndInsertOFStringArray(DCM_NameOfPhysiciansReadingStudy, "");

            // m_dataset->remove(DCM_NameOfPhysiciansReadingStudy);
            // m_dataset->remove(DCM_PerformingPhysicianName);
            // m_dataset->remove(DCM_ScheduledPerformingPhysicianName);
            // m_dataset->remove(DCM_OperatorsName);
            // m_dataset->remove(DCM_InstitutionName);
            // m_dataset->remove(DCM_InstitutionAddress);
            // m_dataset->remove(DCM_InstitutionalDepartmentName);

        }


        OFString oldSeriesUID{};
        m_dataset->findAndGetOFString(DCM_SeriesInstanceUID, oldSeriesUID);
        OFString newSeriesUID = this->getSeriesUids(oldSeriesUID, root);
        m_dataset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, newSeriesUID);

        char newSOPInstanceUID[65];
        dcmGenerateUniqueIdentifier(newSOPInstanceUID, root);
        m_dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, newSOPInstanceUID);

        m_dataset->putAndInsertOFStringArray(DCM_StudyInstanceUID, newStudyUID);

        if (!this->removeInvalidTags()) return false;

        const E_TransferSyntax xfer = m_dataset->getCurrentXfer();
        m_dataset->chooseRepresentation(xfer, nullptr);
        m_fileformat.loadAllDataIntoMemory();

        const std::string outputName = fmt::format("{:08X}", fileNumber);
        const std::string outputPath = fmt::format("{}/{}", m_outputStudyDir, outputName);
        cond = m_fileformat.saveFile(outputPath.c_str(), xfer);

        if (cond.bad()) {
            fmt::print("Unable to save file {}\n", file);
            fmt::print("Reason: {}\n", cond.text());
            return false;
        }
        ++fileNumber;
    }

    return true;
}

std::string StudyAnonymizer::getSeriesUids(const std::string &old_series_uid, const char* root) {

    // add old-new series uid map if there isn't one
    // otherwise return existing new uid
    if (!m_series_uids.contains(old_series_uid)) {
        char uid[65];
        dcmGenerateUniqueIdentifier(uid, root);
        m_series_uids[old_series_uid] = std::string(uid);
        return m_series_uids[old_series_uid];
    }

    return m_series_uids[old_series_uid];
}

bool StudyAnonymizer::removeInvalidTags() const {

    // sanity check
    if (m_dataset == nullptr) return false;

    for (unsigned long i = 0; i < m_dataset->card(); ++i) {
        const DcmElement *element = m_dataset->getElement(i);
        DcmTag tag = element->getTag();
        const DcmTagKey tagKey = DcmTagKey(element->getGTag(), element->getETag());
        const std::string tagName = tag.getTagName();
        if (tagName == "Unknown Tag & Data") {
            m_dataset->remove(tagKey);
            --i; // decrement due to ->remove reducing total number of tags
        }
    }

    return true;
}

// void StudyAnonymizer::generateDicomDir(const std::string &dicomdir_path) {
//     DicomDirInterface dicomdir;
//
//     OFString ap_profile{};
//     m_fileformat.getDataset()->findAndGetOFString(DCM_RequestedMediaApplicationProfile, ap_profile);
//
//     dicomdir.
//
//     const std::string dicomdirName = fmt::format("{}/DICOMDIR", dicomdir_path);
//     OFCondition status = dicomdir.createNewDicomDir(dicomdir.AP_Default, dicomdirName.c_str(), "");
//
//     for (const auto& file : std::filesystem::directory_iterator(m_out_study_directory)) {
//         const std::string filepath = fmt::format("DATA/{}", file.path().stem().string());
//         dicomdir.addDicomFile(filepath.c_str(), dicomdir_path.c_str());
//     }
//
//
// }

// DicomDirInterface::E_ApplicationProfile
// selectApplicationProfile(const char *modality) {
//     if (modality == nullptr) return DicomDirInterface::AP_GeneralPurpose;
//
//     switch (modality) {
//         case "CT":
//             return DicomDirInterface::AP_CTandMR;
//         case "MR":
//             return DicomDirInterface::AP_CTandMR;
//         default:
//             return DicomDirInterface::AP_GeneralPurpose;
//     }
// }
