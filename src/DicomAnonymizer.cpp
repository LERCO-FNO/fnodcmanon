//
// Created by Vojtěch on 18.03.2025.
//
#include <fstream>
#include <set>

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

bool StudyAnonymizer::getStudyFilenames(
    const std::filesystem::path &study_directory) {

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

  if (m_dicom_files.empty()) {
    OFLOG_ERROR(mainLogger, "No files found");
    return false;
  }

  fmt::print("Found {} files\n", m_dicom_files.size());
  return true;
}

bool StudyAnonymizer::anonymizeStudy(
    const std::set<E_ADDIT_ANONYM_METHODS> &methods, const char *root) {

  char newStudyUID[65];
  dcmGenerateUniqueIdentifier(newStudyUID, root);

  int fileNumber{0};
  for (const auto &file : m_dicom_files) {
    OFCondition cond = m_fileformat.loadFile(file.c_str());
    if (cond.bad()) {
      OFLOG_ERROR(mainLogger, "Unable to load file " << file.c_str());
      OFLOG_ERROR(mainLogger, cond.text());
      return false;
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

    if (!this->removeInvalidTags()) {
      OFLOG_ERROR(mainLogger, "Error occured while removing invalid tags");
      return false;
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
      return false;
    }
    ++fileNumber;
  }

  // this->writeTags();

  return true;
}

void StudyAnonymizer::anonymizeBasicProfile() {
  // basic patient tags
  m_dataset->putAndInsertOFStringArray(DCM_PatientName, m_pseudoname.c_str());
  m_dataset->putAndInsertOFStringArray(DCM_PatientID, m_pseudoname.c_str());
  m_dataset->putAndInsertString(DCM_PatientSex, "O");

  delete m_dataset->remove(DCM_PatientAddress);
  delete m_dataset->remove(DCM_AdditionalPatientHistory);
  delete m_dataset->remove(DCM_PatientInstitutionResidence);

  // other institution staff - operator, physicians
  m_dataset->putAndInsertString(DCM_ConsultingPhysicianName, "");
  delete m_dataset->remove(DCM_ConsultingPhysicianIdentificationSequence);
  delete m_dataset->remove(DCM_OperatorsName);
  delete m_dataset->remove(DCM_NameOfPhysiciansReadingStudy);
  delete m_dataset->remove(DCM_PerformingPhysicianName);
  delete m_dataset->remove(DCM_PerformingPhysicianIdentificationSequence);
  delete m_dataset->remove(DCM_PhysiciansOfRecord);
  delete m_dataset->remove(DCM_PhysiciansOfRecordIdentificationSequence);
  delete m_dataset->remove(DCM_ReferringPhysicianName);
  delete m_dataset->remove(DCM_ReferringPhysicianAddress);
  delete m_dataset->remove(DCM_ReferringPhysicianIdentificationSequence);
  delete m_dataset->remove(DCM_ReferringPhysicianTelephoneNumbers);
  delete m_dataset->remove(DCM_RequestingPhysician);
  delete m_dataset->remove(DCM_ScheduledPerformingPhysicianName);
  delete m_dataset->remove(
      DCM_ScheduledPerformingPhysicianIdentificationSequence);
};

void StudyAnonymizer::anonymizePatientCharacteristicsProfile() {
  delete m_dataset->remove(DCM_Allergies);
  delete m_dataset->remove(DCM_PatientAge);
  delete m_dataset->remove(DCM_PatientSexNeutered);
  delete m_dataset->remove(DCM_PatientSize);
  delete m_dataset->remove(DCM_PatientWeight);
  delete m_dataset->remove(DCM_PatientState);
  delete m_dataset->remove(DCM_PregnancyStatus);
  delete m_dataset->remove(DCM_PreMedication);
  delete m_dataset->remove(DCM_SmokingStatus);
  delete m_dataset->remove(DCM_SpecialNeeds);
};

void StudyAnonymizer::anonymizeInstitutionProfile() {
  delete m_dataset->remove(DCM_InstitutionAddress);
  delete m_dataset->remove(DCM_InstitutionName);
  delete m_dataset->remove(DCM_InstitutionalDepartmentName);
  delete m_dataset->remove(DCM_InstitutionalDepartmentTypeCodeSequence);
  delete m_dataset->remove(DCM_InstitutionCodeSequence);
};

void StudyAnonymizer::anonymizeDeviceProfile() {
  delete m_dataset->remove(DCM_DeviceDescription);
  delete m_dataset->remove(DCM_DeviceLabel);
  delete m_dataset->remove(DCM_DeviceSerialNumber);
  delete m_dataset->remove(DCM_ManufacturerDeviceIdentifier);
  delete m_dataset->remove(DCM_PerformedStationName);
  delete m_dataset->remove(DCM_PerformedStationNameCodeSequence);
  delete m_dataset->remove(DCM_ScheduledStationName);
  delete m_dataset->remove(DCM_ScheduledStationNameCodeSequence);
  delete m_dataset->remove(DCM_SourceManufacturer);
  delete m_dataset->remove(DCM_SourceSerialNumber);
  delete m_dataset->remove(DCM_StationName);
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

bool StudyAnonymizer::removeInvalidTags() const {

  // sanity check
  if (m_dataset == nullptr) {
    OFLOG_ERROR(mainLogger, "Dataset is nullptr");
    return false;
  }

  for (unsigned long i = 0; i < m_dataset->card(); ++i) {
    const DcmElement *element = m_dataset->getElement(i);
    DcmTag tag = element->getTag();
    const DcmTagKey tagKey = DcmTagKey(element->getGTag(), element->getETag());
    const std::string tagName = tag.getTagName();
    if (tagName == "Unknown Tag & Data") {
      delete m_dataset->remove(tagKey);
      --i; // decrement due to ->remove reducing total number of tags
    }
  }

  return true;
};

bool StudyAnonymizer::setBasicTags() {
  OFCondition cond = m_fileformat.loadFile(m_dicom_files[0].c_str());
  if (cond.bad()) {
    OFLOG_ERROR(mainLogger, "Unable to load file " << m_dicom_files[0].c_str());
    OFLOG_ERROR(mainLogger, cond.text());
    return false;
  }

  m_fileformat.getDataset()->findAndGetOFString(DCM_PatientID, m_oldID);
  m_fileformat.getDataset()->findAndGetOFString(DCM_PatientName, m_oldName);
  m_fileformat.getDataset()->findAndGetOFString(DCM_StudyInstanceUID,
                                                m_studyuid);
  m_fileformat.getDataset()->findAndGetOFString(DCM_StudyDate, m_studydate);
  m_fileformat.clear();
  return true;
}

void StudyAnonymizer::writeTags() const {
  std::ofstream csvfile{m_outputStudyDir + "/tags.csv", std::ios::out};
  if (!csvfile.is_open()) {
    OFLOG_ERROR(mainLogger, "error while creating `tags.csv`");
    return;
  }
  csvfile << "PatientID,PatientName,Pseudoname,StudyInstanceUID,StudyDate\n";

  csvfile << fmt::format("{},{},{},{},{}\n", m_oldID, m_oldName, m_pseudoname,
                         m_studyuid, m_studydate);

  csvfile.close();
};