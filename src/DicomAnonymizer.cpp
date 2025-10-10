//
// Created by Vojtěch on 18.03.2025.
//
// #include <dcmtk/ofstd/ofconsol.h>
// #include <dcmtk/ofstd/oftypes.h>
#include <fstream>
#include <random>

#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dctagkey.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/oflog/oflog.h"

#include "dcmtk/ofstd/ofcond.h"
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

  fmt::print("Found {} files\n", m_dicom_files.size());
  return EC_Normal;
}

OFCondition
StudyAnonymizer::anonymizeStudy(const std::set<E_ADDIT_ANONYM_METHODS> &methods,
                                const char *root) {

  OFCondition cond{};

  char newStudyUID[65];
  dcmGenerateUniqueIdentifier(newStudyUID, root);

  int fileNumber{0};
  for (const auto &file : m_dicom_files) {
    OFCondition cond = m_fileformat.loadFile(file.c_str());
    if (cond.bad()) {
      OFLOG_ERROR(mainLogger, "Unable to load file " << file.c_str());
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
      // clean patient characteristics tags, others are kept as is
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

    const char *oldSeriesUID{nullptr};
    m_dataset->findAndGetString(DCM_SeriesInstanceUID, oldSeriesUID);

    const std::string newSeriesUID = this->getSeriesUids(oldSeriesUID, root);
    m_dataset->putAndInsertString(DCM_SeriesInstanceUID, newSeriesUID.c_str());

    char newSOPInstanceUID[65];
    dcmGenerateUniqueIdentifier(newSOPInstanceUID, root);
    m_dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, newSOPInstanceUID);

    m_dataset->putAndInsertOFStringArray(DCM_StudyInstanceUID, newStudyUID);

    cond = this->removeInvalidTags();

    if (cond.bad()) {
      OFLOG_ERROR(mainLogger, "Error occured while removing invalid tags");
      return cond;
    }
    const E_TransferSyntax xfer = m_dataset->getCurrentXfer();
    m_dataset->chooseRepresentation(xfer, nullptr);
    m_fileformat.loadAllDataIntoMemory();

    std::string outputName{};
    if (m_filenameType == F_HEX) {
      outputName = fmt::format("{:08X}", fileNumber);
    } else if (m_filenameType == F_MODALITY_SOPINSTUID) {
      const char *modality;
      m_dataset->findAndGetString(DCM_Modality, modality);
      outputName = fmt::format("{}{}", modality, newSOPInstanceUID);
    }
    const std::string outputPath =
        fmt::format("{}/DICOM/{}", m_outputStudyDir, outputName);
    cond = m_fileformat.saveFile(outputPath.c_str(), xfer);

    if (cond.bad()) {
      fmt::print("Unable to save file {}\n", file);
      fmt::print("Reason: {}\n", cond.text());
      return cond;
    }
    ++fileNumber;
  }

  // this->writeTags();

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

void StudyAnonymizer::setPseudoname(const char *prefix) {
  m_pseudoname = fmt::format("{}{}", prefix, generate_random_string());
};

void StudyAnonymizer::setPseudoname(const char *prefix, int count,
                                    int count_width) {
  m_pseudoname = fmt::format("{0}{1:0{2}}", prefix, count, count_width);
};

void StudyAnonymizer::setPseudoname(const char *prefix,
                                    const std::string &pseudoname) {

};

std::string StudyAnonymizer::getSeriesUids(const std::string &old_series_uid,
                                           const char *root) {

  // add old-new series uid map if there isn't one
  // otherwise return existing new uid
  if (!m_series_uids.contains(old_series_uid)) {
    char uid[65];
    dcmGenerateUniqueIdentifier(uid, root);
    m_series_uids[old_series_uid] = std::string(uid);
    return m_series_uids[old_series_uid];
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

  return cond;
};

OFCondition StudyAnonymizer::setBasicTags() {
  OFCondition cond = m_fileformat.loadFile(m_dicom_files[0].c_str());
  if (cond.bad()) {
    OFLOG_ERROR(mainLogger, "Unable to load file " << m_dicom_files[0].c_str());
    OFLOG_ERROR(mainLogger, cond.text());
    return cond;
  }

  DcmDataset *ds = m_fileformat.getDataset();
  ds->findAndGetOFString(DCM_PatientID, m_oldID);
  ds->findAndGetOFString(DCM_PatientName, m_oldName);
  ds->findAndGetOFString(DCM_StudyInstanceUID, m_old_studyuid);
  ds->findAndGetOFString(DCM_StudyDate, m_studydate);
  ds = nullptr;
  m_fileformat.clear();
  return cond;
}

OFCondition StudyAnonymizer::writeTags() const {
  std::ofstream csvfile{m_outputStudyDir + "/tags.csv", std::ios::out};
  if (!csvfile.is_open()) {
    OFLOG_ERROR(mainLogger, "error while creating `tags.csv`");
    return {0, 0, OF_error, "error while creating `tags.csv`"};
  }

  csvfile << "PatientID,PatientName,Pseudoname,StudyInstanceUID,StudyDate\n";
  csvfile << fmt::format("{},{},{},{},{}\n", m_oldID, m_oldName, m_pseudoname,
                         m_old_studyuid, m_studydate);

  csvfile.close();
  return {EC_Normal};
};