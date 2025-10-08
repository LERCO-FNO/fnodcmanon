#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>

#include "fmt/format.h"

// #include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/ofstd/ofconapp.h"
#include "dcmtk/ofstd/ofexit.h"

#include "DicomAnonymizer.hpp"

void checkConflict(OFConsoleApplication &app, const char *first_opt,
                   const char *second_opt) {
  const std::string str =
      fmt::format("{} not allowed with {}", first_opt, second_opt);
  app.printError(str.c_str(), EXITCODE_COMMANDLINE_SYNTAX_ERROR);
};

void checkConflict(OFConsoleApplication &app, const char *first_opt,
                   const char *second_opt, const char *third_opt) {
  const std::string str = fmt::format("{}, {} and {} not allowed together",
                                      first_opt, second_opt, third_opt);
  app.printError(str.c_str(), EXITCODE_COMMANDLINE_SYNTAX_ERROR);
};

int getPseudonameLeadingZeroesWidth(const char *opt_inDirectory) {
  int studyCount{0};
  for (const auto &studyDir :
       std::filesystem::directory_iterator(opt_inDirectory)) {
    if (!studyDir.is_directory()) {
      OFLOG_WARN(mainLogger, fmt::format("Invalid path, not directory `{}`",
                                         studyDir.path().string())
                                 .c_str());
      continue;
    }

    if (studyDir.path().empty()) {
      OFLOG_WARN(mainLogger, fmt::format("Invalid directory, is empty `{}`",
                                         studyDir.path().string())
                                 .c_str());
      continue;
    }
    ++studyCount;
  }
  return static_cast<int>(std::to_string(studyCount).length());
};

void printMethods() {
  struct AnonProfiles {
    std::string_view option{};
    std::string_view profile{};
    std::string_view description{};
  };

  constexpr std::array<AnonProfiles, 4> methods{
      AnonProfiles{"<profile always used>",
                   "Basic Application Confidentiality Profile (DCM_113100)",
                   "basic tags: PatientName, PatientID, PatientSex, "
                   "physician tags, ..."},
      AnonProfiles{"--profile-patient-tags",
                   "Retain Patient Characteristics Option (DCM_113108)",
                   "optional patient tags: PatientAge, "
                   "PatientWeight, SmokingStatus, ..."},
      AnonProfiles{"--profile-device-tags",
                   "Retain Device Identity Option (DCM_113109)",
                   "device tags: DeviceLabel, StationName, ..."},
      AnonProfiles{
          "--profile-institution-tags",
          "Retain Institution Identity Option (DCM_113112)",
          "institution tags: InstitutionAddress, InstitutionName, ..."}};

  for (const auto &m : methods) {
    fmt::print("{:<30} | {:<55} | {}\n", m.option, m.profile, m.description);
  }
};

void writeAllStudyTags(std::ofstream &file, const StudyAnonymizer &anonymizer) {
  file << fmt::format("{},{},{},{},{}\n", anonymizer.m_oldID,
                      anonymizer.m_oldName, anonymizer.m_pseudoname,
                      anonymizer.m_studyuid, anonymizer.m_studydate);
};

int main(int argc, char *argv[]) {
  constexpr auto FNO_CONSOLE_APPLICATION{"fnodcmanon"};
  constexpr auto APP_VERSION{"0.4.2"};
  constexpr auto RELEASE_DATE{"2024-11-19"};
  const std::string rcsid = fmt::format(
      "${}: ver. {} rel. {}\n$dcmtk: ver. {} rel.{}", FNO_CONSOLE_APPLICATION,
      APP_VERSION, RELEASE_DATE, OFFIS_DCMTK_VERSION, OFFIS_DCMTK_RELEASEDATE);

  setupLogger(fmt::format("fno.apps.{}", FNO_CONSOLE_APPLICATION));
  OFConsoleApplication app(FNO_CONSOLE_APPLICATION, "DICOM anonymization tool",
                           rcsid.c_str());
  OFCommandLine cmd;

  // required params
  const char *opt_inDirectory{nullptr};
  const char *opt_anonymizedPrefix{nullptr};

  // optional params
  const char *FNO_UID_ROOT{"1.2.840.113619.2"};
  const char *opt_outDirectory{"./anonymized_output"};
  const char *opt_rootUID = FNO_UID_ROOT;
  E_FILENAMES opt_filenameType = F_HEX;

  std::set<E_ADDIT_ANONYM_METHODS> opt_anonymizationMethods{};

  constexpr int LONGCOL{20};
  constexpr int SHORTCOL{4};
  cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
  cmd.addParam("in-directory", "input directory with DICOM studies");
  cmd.addParam(
      "anonymized-prefix",
      "pseudoname prefix overwriting DICOM tags PatientID, PatientName");

  cmd.setOptionColumns(LONGCOL, SHORTCOL);
  cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
  cmd.addOption("--help", "-h", "print this help text and exit",
                OFCommandLine::AF_Exclusive);
  cmd.addOption("--version", "print version information and exit",
                OFCommandLine::AF_Exclusive);

  OFLog::addOptions(cmd);

  cmd.addGroup("anonymization options:");
  cmd.addOption("--profile-patient-tags", "-ppt",
                "retain patient characteristics option");
  cmd.addOption("--profile-device-tags", "-pdt",
                "retain device identity option");
  cmd.addOption("--profile-institution-tags", "-pit",
                "retain institution identity option");
  cmd.addOption("--print-anon-profiles",
                "print deidentification profiles for example tags",
                OFCommandLine::AF_Exclusive);
  cmd.addOption(
      "--fno-uid-root", "-fuid",
      fmt::format("use FNO UID root: {} (default)", FNO_UID_ROOT).c_str());
  cmd.addOption("--offis-uid-root", "-ouid",
                "use OFFIS UID root: " OFFIS_UID_ROOT);
  cmd.addOption("--custom-uid-root", "-cuid", 1, "uid root: string",
                "use custom UID root");

  cmd.addGroup("output options:");
  cmd.addOption("--out-directory", "-od", 1,
                "directory: string (default `./anonymized_output`",
                "write modified files to output directory");
  cmd.addOption("--filename-hex", "-f", "filenames in hex format (default)");
  cmd.addOption("--filename-modality-sop", "+f",
                "filenames in MODALITY_SOPINSTUID format");

  prepareCmdLineArgs(argc, argv, FNO_CONSOLE_APPLICATION);
  if (app.parseCommandLine(cmd, argc, argv)) {
    if (cmd.hasExclusiveOption()) {
      if (cmd.findOption("--version")) {
        app.printHeader(OFTrue);
        return 0;
      }

      if (cmd.findOption("--print-anon-profiles")) {
        printMethods();
        return 0;
      }
    }

    cmd.getParam(1, opt_inDirectory);
    cmd.getParam(2, opt_anonymizedPrefix);

    OFLog::configureFromCommandLine(cmd, app);

    if (cmd.findOption("--fno-uid-root") &&
        cmd.findOption("--offis-uid-root") &&
        cmd.findOption("--custom-uid-root")) {
      checkConflict(app, "--fno-uid-root", "--offis-uid-root",
                    "--custom-uid-root");
    } else if (cmd.findOption("--fno-uid-root") &&
               cmd.findOption("--offis-uid-root")) {
      checkConflict(app, "--fno-uid-root", "--offis-uid-root");
    } else if (cmd.findOption("--fno-uid-root") &&
               cmd.findOption("--custom-uid-root")) {
      checkConflict(app, "--fno-uid-root", "--custom-uid-root");
    } else if (cmd.findOption("--offis-uid-root") &&
               cmd.findOption("--custom-uid-root")) {
      checkConflict(app, "--offis-uid-root", "--custom-uid-root");
    }

    cmd.beginOptionBlock();
    if (cmd.findOption("--fno-uid-root"))
      opt_rootUID = FNO_UID_ROOT;
    if (cmd.findOption("--offis-uid-root"))
      opt_rootUID = OFFIS_UID_ROOT;
    if (cmd.findOption("--custom-uid-root"))
      app.checkValue(cmd.getValue(opt_rootUID));
    cmd.endOptionBlock();

    if (cmd.findOption("--out-directory")) {
      app.checkValue(cmd.getValue(opt_outDirectory));
    }

    if (cmd.findOption("--filename-hex") &&
        cmd.findOption("--filename-modality-sop")) {
      checkConflict(app, "--filename-hex", "--filename-modality-sop");
    }

    cmd.beginOptionBlock();
    if (cmd.findOption("--filename-hex"))
      opt_filenameType = F_HEX;
    if (cmd.findOption("--filename-modality-sop"))
      opt_filenameType = F_MODALITY_SOPINSTUID;
    cmd.endOptionBlock();

    if (cmd.findOption("--profile-patient-tags")) {
      opt_anonymizationMethods.insert(E_ADDIT_ANONYM_METHODS::M_113108);
    }

    if (cmd.findOption("--profile-device-tags")) {
      opt_anonymizationMethods.insert(E_ADDIT_ANONYM_METHODS::M_113109);
    }

    if (cmd.findOption("--profile-institution-tags")) {
      opt_anonymizationMethods.insert(E_ADDIT_ANONYM_METHODS::M_113112);
    }

    OFLOG_DEBUG(mainLogger, rcsid.c_str() << OFendl);
  }

  if (std::filesystem::exists(opt_inDirectory)) {
    if (!std::filesystem::is_directory(opt_inDirectory)) {
      OFLOG_ERROR(mainLogger, fmt::format("Invalid path, not directory `{}`",
                                          opt_inDirectory));
      return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
    }

    if (std::filesystem::is_empty(opt_inDirectory)) {
      OFLOG_ERROR(mainLogger,
                  fmt::format("Invalid directory, empty directory `{}`",
                              opt_inDirectory));
      return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
    }
  } else {
    OFLOG_ERROR(
        mainLogger,
        fmt::format("Invalid path, directory not found `{}`", opt_inDirectory));
    return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
  }

  StudyAnonymizer anonymizer{};
  anonymizer.m_filenameType = opt_filenameType;

  (void)std::filesystem::create_directories(opt_outDirectory);
  OFLOG_INFO(mainLogger,
             fmt::format("Created output directory `{}`", opt_outDirectory));

  std::ofstream outputAnonymFile{
      std::string(opt_outDirectory) + "/anonym_output.csv", std::ios::out};
  outputAnonymFile
      << "PatientID,PatientName,Pseudoname,StudyInstanceUID,StudyDate\n";

  int pseudonameCount{1};
  int pseudonameLeadingZeroesWidth =
      getPseudonameLeadingZeroesWidth(opt_inDirectory);
  for (const auto &studyDir :
       std::filesystem::directory_iterator(opt_inDirectory)) {
    fmt::print("Anonymizing study `{}`\n", studyDir.path().stem().string());

    bool cond = anonymizer.getStudyFilenames(studyDir.path());
    if (!cond) {
      OFLOG_ERROR(mainLogger,
                  fmt::format("Failed to get study file names from `{}`",
                              studyDir.path().string())
                      .c_str());
      continue;
    }

    anonymizer.m_pseudoname =
        fmt::format("{0}{1:0{2}}", opt_anonymizedPrefix, pseudonameCount,
                    pseudonameLeadingZeroesWidth);
    ++pseudonameCount;

    OFLOG_INFO(mainLogger, "Applying pseudoname " << anonymizer.m_pseudoname);

    anonymizer.m_outputStudyDir =
        fmt::format("{}/{}", opt_outDirectory, anonymizer.m_pseudoname);

    if (std::filesystem::exists(anonymizer.m_outputStudyDir)) {
      OFLOG_INFO(mainLogger,
                 fmt::format("Directory `{}` exists, overwriting files",
                             anonymizer.m_outputStudyDir)
                     .c_str());
    } else {
      if (std::filesystem::create_directories(anonymizer.m_outputStudyDir +
                                              "/DICOM"))
        OFLOG_INFO(mainLogger, fmt::format("Created directory `{}`\n",
                                           anonymizer.m_outputStudyDir)
                                   .c_str());
    }
    anonymizer.setBasicTags();
    cond = anonymizer.anonymizeStudy(opt_anonymizationMethods, opt_rootUID);

    // something bad happened
    if (!cond) {
      OFLOG_ERROR(mainLogger, fmt::format("Failed to anonymize study: `{}`\n",
                                          studyDir.path().stem().string())
                                  .c_str());
      continue;
    }

    writeAllStudyTags(outputAnonymFile, anonymizer);
  }
  outputAnonymFile.close();

  return 0;
}
