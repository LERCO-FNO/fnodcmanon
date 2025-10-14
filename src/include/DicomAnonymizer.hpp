//
// Created by Vojtěch on 18.03.2025.
//

#ifndef DICOMANONYMIZER_HPP
#define DICOMANONYMIZER_HPP

#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/ofstd/ofcond.h"

extern OFLogger mainLogger;

void setupLogger(std::string_view logger_name);

enum E_FILENAMES { F_HEX, F_MODALITY_SOPINSTUID };

enum E_ADDIT_ANONYM_METHODS {
  // https://dicom.nema.org/medical/dicom/current/output/chtml/part16/chapter_D.html#DCM_113100
  M_113108, // Retain Patient Characteristics Option
  M_113109, // Retain Device Identity Option
  M_113112  // Retain Institution Identity Option
};

enum E_PSEUDONAME_TYPE { P_RANDOM_STRING, P_INTEGER_ORDER, P_FROM_FILE };

class StudyAnonymizer {
public:
  StudyAnonymizer() = default;
  StudyAnonymizer(const OFString &pseudoname_prefix,
                  E_PSEUDONAME_TYPE pseudoname_type = P_RANDOM_STRING,
                  E_FILENAMES filename_type = F_HEX)
      : m_pseudoname_prefix{pseudoname_prefix},
        m_pseudoname_type{pseudoname_type}, m_filenameType{filename_type} {};

  ~StudyAnonymizer() = default;

  OFCondition findDicomFiles(const std::filesystem::path &study_directory);

  OFCondition anonymizeStudy(const std::filesystem::path &study_directory,
                             const char *output_directory,
                             const std::set<E_ADDIT_ANONYM_METHODS> &methods,
                             const char *uid_root = nullptr);
  void anonymizeBasicProfile();
  void anonymizePatientCharacteristicsProfile();
  void anonymizeInstitutionProfile();
  void anonymizeDeviceProfile();
  void setPseudoname();

  std::string getSeriesUids(const std::string &old_series_uid,
                            const char *root = nullptr);

  OFCondition readPseudonamesFromFile(const std::string &filename);
  OFCondition removeInvalidTags() const;
  OFCondition setBasicTags();
  OFCondition writeTags() const;

  E_FILENAMES m_filenameType{F_HEX};
  E_PSEUDONAME_TYPE m_pseudoname_type{P_RANDOM_STRING};
  unsigned int m_study_count{1};
  unsigned short m_count_width{2};
  std::string m_pseudoname_prefix{};

  std::string m_pseudoname{};
  OFString m_oldName{};
  OFString m_oldID{};
  OFString m_old_studyuid{};
  OFString m_new_studyuid{};
  OFString m_studydate{};
  std::string m_outputStudyDir{};

private:
  int m_numberOfFiles{0};
  std::vector<std::string> m_dicom_files{};
  std::unordered_map<std::string, std::string>
      m_series_uids{}; // unordered_map[old_uid, new_uid]
  std::unordered_map<std::string, std::string> m_id_pseudoname_map{};
  DcmFileFormat m_fileformat;
  DcmDataset *m_dataset{nullptr};
};

#endif // DICOMANONYMIZER_HPP
