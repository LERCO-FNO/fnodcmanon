//
// Created by Vojtěch on 18.03.2025.
//
#include <fstream>
#include <random>

#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dctagkey.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/oflog/oflog.h"
#include "dcmtk/ofstd/ofcond.h"
#include "dcmtk/ofstd/ofexit.h"

#include "fmt/format.h"

#include "DicomAnonymizer.hpp"

OFLogger mainLogger = OFLog::getLogger("");

void setupLogger(std::string_view logger_name) {
  mainLogger = OFLog::getLogger(logger_name.data());
};

std::string generate_random_string() {
  static constexpr std::string_view chars{"abcdefghijklmnopqrstuvwxyz"
                                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                          "0123456789"};
  static std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<std::size_t> dist(0, chars.size() - 1);

  std::string retval(10, '\0');
  for (char &c : retval) {
    c = chars[dist(rng)];
  }

  return retval;
};

OFCondition
StudyAnonymizer::findDicomFiles(const std::filesystem::path &study_directory) {

  // remove previous filenames and series uids when iterating over new study
  // directory
  if (!m_dicom_files.empty())
    m_dicom_files.clear();
  if (!m_series_uids.empty())
    m_series_uids.clear();

  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(study_directory)) {
    if (entry.is_directory() || entry.path().filename() == "DICOMDIR")
      continue;

    m_dicom_files.push_back(entry.path().string());
  }

  OFCondition cond{};
  if (m_dicom_files.empty()) {
    const std::string msg =
        fmt::format("no dicom files found in `{}`", study_directory.string());
    OFLOG_WARN(mainLogger, msg.c_str());
    return {0, 0, OF_failure, msg.c_str()};
  }

  return EC_Normal;
}

OFCondition StudyAnonymizer::anonymizeStudy(
    const std::filesystem::path &input_study_directory,
    const std::string &output_directory,
    const std::set<E_ADDIT_ANONYM_METHODS> &methods,
    const std::string &uid_root) {

  OFCondition cond{};

  cond = this->findDicomFiles(input_study_directory);
  if (cond.bad()) {
    OFLOG_ERROR(mainLogger, "error while searching dicom files");
    OFLOG_ERROR(mainLogger, cond.text());
    return cond;
  }

  cond = this->setBasicTags();

  fmt::print("\nanonymizing study {}, {} dicom files\n", m_old_id,
             m_dicom_files.size());

  char newStudyUID[65];
  dcmGenerateUniqueIdentifier(newStudyUID, uid_root.c_str());
  m_new_studyuid = OFString(newStudyUID);

  OFLOG_INFO(mainLogger, "replacing StudyInstanceUID (old) " << m_old_studyuid
                                                             << " with (new) "
                                                             << m_new_studyuid);

  this->setPseudoname();

  fmt::print("applying pseudoname {} to ID {}\n", m_pseudoname, m_old_id);

  m_output_study_dir = fmt::format("{}/{}", output_directory, m_pseudoname);

  if (std::filesystem::exists(m_output_study_dir)) {
    OFLOG_INFO(mainLogger, "directory `" << m_output_study_dir
                                         << "` exists, overwriting files");
  } else {
    std::filesystem::create_directories(m_output_study_dir + "/DICOM");
    OFLOG_INFO(mainLogger, "created directory `" << m_output_study_dir << "`");
  }

  for (const auto &file : m_dicom_files) {
    OFCondition cond = m_fileformat.loadFile(file);
    if (cond.bad()) {
      OFLOG_ERROR(mainLogger, "unable to load file " << file.c_str());
      OFLOG_ERROR(mainLogger, cond.text());
      return cond;
    }

    m_dataset = m_fileformat.getDataset();

    // dicom tags anonymization specification
    // https://dicom.nema.org/medical/dicom/current/output/chtml/part15/chapter_E.html
    // deidentification methods explained
    // https://dicom.nema.org/medical/dicom/current/output/chtml/part16/sect_CID_7050.html

    // Basic Application Confidentiality Profile
    this->anonymizeBasicProfile();

    // Retain Patient Characteristics Option
    if (methods.contains(M_113108)) {
      // clean some patient characteristics tags, others are kept as is
      m_dataset->putAndInsertString(DCM_Allergies, "");
      m_dataset->putAndInsertString(DCM_PatientState, "");
      m_dataset->putAndInsertString(DCM_PreMedication, "");
      m_dataset->putAndInsertString(DCM_SpecialNeeds, "");
    } else {
      // remove patient characteristics tags
      this->anonymizePatientCharacteristicsProfile();
    }

    if (!methods.contains(M_113109)) {
      this->anonymizeDeviceProfile();
    }
    // Retain Institution Identity Option
    if (!methods.contains(M_113112)) {
      this->anonymizeInstitutionProfile();
    }

    std::string oldSeriesUID{};
    m_dataset->findAndGetOFString(DCM_SeriesInstanceUID, oldSeriesUID);

    const std::string newSeriesUID =
        this->getSeriesUids(oldSeriesUID, uid_root.c_str());
    m_dataset->putAndInsertString(DCM_SeriesInstanceUID, newSeriesUID.c_str());

    char newSOPInstanceUID[65];
    dcmGenerateUniqueIdentifier(newSOPInstanceUID, uid_root.c_str());
    m_dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, newSOPInstanceUID);

    m_dataset->putAndInsertOFStringArray(DCM_StudyInstanceUID, newStudyUID);

    cond = this->removeInvalidTags();

    cond = this->writeDicomFile();

    if (cond.bad()) {
      OFLOG_ERROR(mainLogger, "error while processing study `"
                                  << input_study_directory.stem().string()
                                  << "`, skipping to next study");
      OFLOG_ERROR(mainLogger, cond.text());
      return cond;
    }
  }

  // TODO: add in future?
  //  this->writeTags();
  fmt::print("finished anonymization of {}\n", m_old_id);
  return cond;
}

void StudyAnonymizer::anonymizeBasicProfile() {
  // basic patient tags
  m_dataset->putAndInsertOFStringArray(DCM_PatientName, m_pseudoname);
  m_dataset->putAndInsertOFStringArray(DCM_PatientID, m_pseudoname);
  m_dataset->putAndInsertString(DCM_PatientSex, "O");
  m_dataset->findAndDeleteElement(DCM_PatientAddress);
  m_dataset->findAndDeleteElement(DCM_AdditionalPatientHistory);
  m_dataset->findAndDeleteElement(DCM_PatientInstitutionResidence);

  // other institution staff - operator, physicians
  m_dataset->putAndInsertString(DCM_ConsultingPhysicianName, "");
  m_dataset->findAndDeleteElement(
      DCM_ConsultingPhysicianIdentificationSequence);
  m_dataset->findAndDeleteElement(DCM_OperatorsName);
  m_dataset->findAndDeleteElement(DCM_NameOfPhysiciansReadingStudy);
  m_dataset->findAndDeleteElement(DCM_PerformingPhysicianName);
  m_dataset->findAndDeleteElement(
      DCM_PerformingPhysicianIdentificationSequence);
  m_dataset->findAndDeleteElement(DCM_PhysiciansOfRecord);
  m_dataset->findAndDeleteElement(DCM_PhysiciansOfRecordIdentificationSequence);
  m_dataset->findAndDeleteElement(DCM_ReferringPhysicianName);
  m_dataset->findAndDeleteElement(DCM_ReferringPhysicianAddress);
  m_dataset->findAndDeleteElement(DCM_ReferringPhysicianIdentificationSequence);
  m_dataset->findAndDeleteElement(DCM_ReferringPhysicianTelephoneNumbers);
  m_dataset->findAndDeleteElement(DCM_RequestingPhysician);
  m_dataset->findAndDeleteElement(DCM_ScheduledPerformingPhysicianName);
  m_dataset->findAndDeleteElement(
      DCM_ScheduledPerformingPhysicianIdentificationSequence);
};

void StudyAnonymizer::anonymizePatientCharacteristicsProfile() {
  m_dataset->findAndDeleteElement(DCM_Allergies);
  m_dataset->findAndDeleteElement(DCM_PatientAge);
  m_dataset->findAndDeleteElement(DCM_PatientSexNeutered);
  m_dataset->findAndDeleteElement(DCM_PatientSize);
  m_dataset->findAndDeleteElement(DCM_PatientWeight);
  m_dataset->findAndDeleteElement(DCM_PatientState);
  m_dataset->findAndDeleteElement(DCM_PregnancyStatus);
  m_dataset->findAndDeleteElement(DCM_PreMedication);
  m_dataset->findAndDeleteElement(DCM_SmokingStatus);
  m_dataset->findAndDeleteElement(DCM_SpecialNeeds);
};

void StudyAnonymizer::anonymizeInstitutionProfile() {
  m_dataset->findAndDeleteElement(DCM_InstitutionAddress);
  m_dataset->findAndDeleteElement(DCM_InstitutionName);
  m_dataset->findAndDeleteElement(DCM_InstitutionalDepartmentName);
  m_dataset->findAndDeleteElement(DCM_InstitutionalDepartmentTypeCodeSequence);
  m_dataset->findAndDeleteElement(DCM_InstitutionCodeSequence);
};

void StudyAnonymizer::anonymizeDeviceProfile() {
  m_dataset->findAndDeleteElement(DCM_DeviceDescription);
  m_dataset->findAndDeleteElement(DCM_DeviceLabel);
  m_dataset->findAndDeleteElement(DCM_DeviceSerialNumber);
  m_dataset->findAndDeleteElement(DCM_ManufacturerDeviceIdentifier);
  m_dataset->findAndDeleteElement(DCM_PerformedStationName);
  m_dataset->findAndDeleteElement(DCM_PerformedStationNameCodeSequence);
  m_dataset->findAndDeleteElement(DCM_ScheduledStationName);
  m_dataset->findAndDeleteElement(DCM_ScheduledStationNameCodeSequence);
  m_dataset->findAndDeleteElement(DCM_SourceManufacturer);
  m_dataset->findAndDeleteElement(DCM_SourceSerialNumber);
  m_dataset->findAndDeleteElement(DCM_StationName);
};

void StudyAnonymizer::setPseudoname() {

  if (m_pseudoname_type == P_RANDOM_STRING) {
    m_pseudoname =
        fmt::format("{}{}", m_pseudoname_prefix, generate_random_string());
  } else if (m_pseudoname_type == P_INTEGER_ORDER) {
    m_pseudoname = fmt::format("{0}{1:0{2}}", m_pseudoname_prefix,
                               m_study_count, m_count_width);
    ++m_study_count;
  } else if (m_pseudoname_type == P_FROM_FILE) {
    if (m_id_pseudoname_map.contains(m_old_id)) {
      m_pseudoname = fmt::format("{}{}", m_pseudoname_prefix,
                                 m_id_pseudoname_map[m_old_id]);
      return;
    }

    m_pseudoname = fmt::format("{}{}_{}", m_pseudoname_prefix, "UN",
                               generate_random_string());
    OFLOG_WARN(mainLogger,
               "ID " << m_old_id << " not in PatientID-pseudoname file");
    OFLOG_WARN(mainLogger, "generated random string instead "
                               << m_old_id << " -> " << m_pseudoname);
  }
};

std::string StudyAnonymizer::getSeriesUids(const std::string &old_series_uid,
                                           const char *root) {

  // add old-new series uid map if there isn't one
  // otherwise return existing new uid
  if (!m_series_uids.contains(old_series_uid)) {
    char uid[65];
    dcmGenerateUniqueIdentifier(uid, root);
    m_series_uids[old_series_uid] = std::string(uid);
  }

  return m_series_uids[old_series_uid];
};

OFCondition StudyAnonymizer::removeInvalidTags() const {

  OFCondition cond{};
  // sanity check
  if (m_dataset == nullptr) {
    cond = {0, 0, OF_error, "dataset is nullptr"};
    OFLOG_ERROR(mainLogger, cond.text());
    return cond;
  }

  for (unsigned long i = 0; i < m_dataset->card(); ++i) {
    const DcmElement *element = m_dataset->getElement(i);
    DcmTag tag = element->getTag();
    const DcmTagKey tagKey = DcmTagKey(element->getGTag(), element->getETag());
    const std::string tagName = tag.getTagName();
    if (tagName == "Unknown Tag & Data") {
      m_dataset->findAndDeleteElement(tagKey);
      --i; // decrement due to deleting total number of tags
    }
  }

  if (cond.bad()) {
    OFLOG_ERROR(mainLogger, "error while removing invalid tags");
  }

  return cond;
};

OFCondition StudyAnonymizer::setBasicTags() {
  OFCondition cond = m_fileformat.loadFile(m_dicom_files[0].c_str());
  if (cond.bad()) {
    OFLOG_ERROR(mainLogger, "unable to load file " << m_dicom_files[0].c_str());
    OFLOG_ERROR(mainLogger, cond.text());
    return cond;
  }

  DcmDataset *ds = m_fileformat.getDataset();
  ds->findAndGetOFString(DCM_PatientID, m_old_id);
  ds->findAndGetOFString(DCM_PatientName, m_old_name);
  ds->findAndGetOFString(DCM_StudyInstanceUID, m_old_studyuid);
  ds->findAndGetOFString(DCM_StudyDate, m_study_date);
  ds = nullptr;
  m_fileformat.clear();
  return cond;
}

OFCondition
StudyAnonymizer::readPseudonamesFromFile(const std::string &filename) {
  OFCondition cond{};

  std::ifstream file{filename, std::ios::in};
  if (!file.is_open()) {
    OFCondition cond{0, EXITCODE_CANNOT_READ_INPUT_FILE, OF_error,
                     "error reading file with pseudonames"};
    OFLOG_ERROR(mainLogger, cond.text());
    return cond;
  }

  std::string line{};
  while (std::getline(file, line)) {

    std::erase_if(line, ::isspace);
    const std::size_t delimiter_pos = line.find(',');
    std::string patient_id = line.substr(0, delimiter_pos);
    const std::string pseudoname = line.substr(delimiter_pos + 1);

    // correct PatientID formatting
    std::erase(patient_id, '/');
    std::erase_if(patient_id, ::isalpha);

    if (!m_id_pseudoname_map.contains(patient_id)) {
      m_id_pseudoname_map.emplace(patient_id, pseudoname);
    }
  }
  file.close();
  OFLOG_INFO(mainLogger,
             "found " << static_cast<unsigned int>(m_id_pseudoname_map.size())
                      << " PatientID-pseudoname pairs to apply");

  return cond;
};

OFCondition StudyAnonymizer::writeDicomFile() {
  OFCondition cond{};

  const E_TransferSyntax xfer = m_dataset->getCurrentXfer();
  m_dataset->chooseRepresentation(xfer, nullptr);
  m_fileformat.loadAllDataIntoMemory();

  std::string path = fmt::format("{}/DICOM/", m_output_study_dir);
  switch (m_filename_type) {
  case F_HEX:
    path += fmt::format("{:08X}", m_files_processed);
    ++m_files_processed;
    break;
  case F_MODALITY_SOPINSTUID: {
    std::string modality{}, sopInstanceUid{};
    m_dataset->findAndGetOFString(DCM_Modality, modality);
    m_dataset->findAndGetOFString(DCM_SOPInstanceUID, sopInstanceUid);
    path += fmt::format("{}{}", modality, sopInstanceUid);
    break;
  }
  }

  cond = m_fileformat.saveFile(path, xfer);

  if (cond.bad()) {
    OFLOG_ERROR(mainLogger, "error writing file `" << path << "`");
    OFLOG_ERROR(mainLogger, cond.text());
  }
  return cond;
};

OFCondition StudyAnonymizer::writeTags() const {
  std::ofstream csvfile{m_output_study_dir + "/tags.csv", std::ios::out};
  if (!csvfile.is_open()) {
    OFLOG_ERROR(mainLogger, "error while creating `tags.csv`");
    return {0, 0, OF_error, "error while creating `tags.csv`"};
  }

  csvfile << "PatientID,PatientName,Pseudoname,StudyInstanceUID,StudyDate\n";
  csvfile << fmt::format("{},{},{},{},{}\n", m_old_id, m_old_name, m_pseudoname,
                         m_old_studyuid, m_study_date);

  csvfile.close();
  return EC_Normal;
};