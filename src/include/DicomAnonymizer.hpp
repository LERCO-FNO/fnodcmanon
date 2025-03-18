//
// Created by Vojtěch on 18.03.2025.
//

#ifndef DICOMANONYMIZER_HPP
#define DICOMANONYMIZER_HPP

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcdeftag.h"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "fmt/color.h"

class DicomAnonymizer {
public:
    DicomAnonymizer() = default;
    ~DicomAnonymizer() = default;

    bool getDicomFilenames(const std::filesystem::path &study_directory);
    bool anonymizeStudy(const std::string &pseudoname, const char* root = nullptr);
    std::string getSeriesUids(const std::string &old_series_uid, const char* root = nullptr);

    unsigned int m_number_of_files{0};
    std::string m_out_study_directory{};
private:
    std::vector<std::string> m_dicom_files{};
    std::unordered_map<std::string, std::string> m_series_uids{}; // map <old_uid, new_uid>
    DcmFileFormat m_fileformat;
    DcmDataset *m_dataset{nullptr};
};

#endif //DICOMANONYMIZER_HPP
