//
// Created by Vojtěch on 18.03.2025.
//

#ifndef DICOMANONYMIZER_HPP
#define DICOMANONYMIZER_HPP

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <set>

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcddirif.h"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "fmt/color.h"

enum E_FILENAMES {
    F_HEX,
    F_MODALITY_SOPINSTUID
};

enum E_ANONYM_METHODS {
    // https://dicom.nema.org/medical/dicom/current/output/chtml/part16/chapter_D.html#DCM_113100
    M_113100, // Basic Application Confidentiality Profile
    M_113108, // Retain Patient Characteristics Option
    M_113109, // Retain Device Identity Option
    M_113112 // Retain Institution Identity Option
};

class StudyAnonymizer {
public:
    StudyAnonymizer() = default;

    ~StudyAnonymizer() = default;

    bool getStudyFilenames(const std::filesystem::path &study_directory);

    bool anonymizeStudy(const std::string &pseudoname, const std::set<E_ANONYM_METHODS> &methods, const char *root = nullptr);

    std::string getSeriesUids(const std::string &old_series_uid, const char *root = nullptr);

    bool removeInvalidTags() const;

    // void generateDicomDir(const std::string &dicomdir_path);

    int         m_numberOfStudies{0};
    E_FILENAMES m_filenameType{F_HEX};
    OFString    m_oldName{};
    OFString    m_oldID{};
    std::string m_outputStudyDir{};
    std::string m_patientListFilename{};


private:
    int m_numberOfFiles{0};
    std::vector<std::string>                     m_dicom_files{};
    std::unordered_map<std::string, std::string> m_series_uids{}; // unmap[old_uid, new_uid]
    DcmFileFormat                                m_fileformat;
    DcmDataset *                                 m_dataset{nullptr};
};

// static DicomDirInterface::E_ApplicationProfile
// selectApplicationProfile(const char *modality);

#endif //DICOMANONYMIZER_HPP
