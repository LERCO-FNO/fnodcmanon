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

  ~StudyAnonymizer() = default;

  OFCondition findDicomFiles(const std::filesystem::path &study_directory);

  OFCondition anonymizeStudy(const std::set<E_ADDIT_ANONYM_METHODS> &methods,
                             const char *root = nullptr);
  void anonymizeBasicProfile();
  void anonymizePatientCharacteristicsProfile();
  void anonymizeInstitutionProfile();
  void anonymizeDeviceProfile();
  void setPseudoname(const char *prefix);
  void setPseudoname(const char *prefix, int count, int count_width);
  void setPseudoname(const char *prefix, const std::string &pseudoname);

  std::string getSeriesUids(const std::string &old_series_uid,
                            const char *root = nullptr);

  OFCondition removeInvalidTags() const;
  OFCondition setBasicTags();
  OFCondition writeTags() const;

  E_FILENAMES m_filenameType{F_HEX};
  E_PSEUDONAME_TYPE m_pseudonameType{P_RANDOM_STRING};
  std::string m_pseudoname{};
  OFString m_oldName{};
  OFString m_oldID{};
  OFString m_old_studyuid{};
  OFString m_studydate{};
  std::string m_outputStudyDir{};
  std::string m_patientListFilename{};

private:
  int m_numberOfFiles{0};
  std::vector<std::string> m_dicom_files{};
  std::unordered_map<std::string, std::string>
      m_series_uids{}; // unordered_map[old_uid, new_uid]
  DcmFileFormat m_fileformat;
  DcmDataset *m_dataset{nullptr};
};

#endif // DICOMANONYMIZER_HPP
