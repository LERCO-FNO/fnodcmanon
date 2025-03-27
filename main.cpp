#include <string>
#include <filesystem>
#include <set>

#include "fmt/format.h"
#include "fmt/os.h"

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/ofstd/ofconapp.h"
#include "dcmtk/oflog/oflog.h"

#include "DicomAnonymizer.hpp"

void checkConflict(OFConsoleApplication &app, const char *first_opt, const char *second_opt) {
    const std::string str = fmt::format("{} not allowed with {}", first_opt, second_opt);
    app.printError(str.c_str(), EXITCODE_COMMANDLINE_SYNTAX_ERROR);
};

void checkConflict(OFConsoleApplication &app, const char *first_opt, const char *second_opt, const char *third_opt) {
    const std::string str = fmt::format("{}, {} and {} not allowed with together", first_opt, second_opt, third_opt);
    app.printError(str.c_str(), EXITCODE_COMMANDLINE_SYNTAX_ERROR);
};

int main(int argc, char *argv[]) {
    constexpr auto    FNO_CONSOLE_APPLICATION{"fnodcmanon"};
    constexpr auto    APP_VERSION{"0.3.0"};
    constexpr auto    RELEASE_DATE{"2024-11-19"};
    const std::string rcsid = fmt::format("${}: ver. {} rel. {}\n$dcmtk: ver. {} rel.",
                                          FNO_CONSOLE_APPLICATION,
                                          APP_VERSION,
                                          RELEASE_DATE,
                                          OFFIS_DCMTK_VERSION,
                                          OFFIS_DCMTK_RELEASEDATE);

    OFLogger mainLogger = OFLog::getLogger(fmt::format("fno.apps.{}", FNO_CONSOLE_APPLICATION).c_str());

    OFConsoleApplication app(FNO_CONSOLE_APPLICATION, "DICOM anonymization tool", rcsid.c_str());
    OFCommandLine        cmd;

    // required params
    const char *opt_inDirectory{nullptr};
    const char *opt_anonymizedPrefix{nullptr};

    // optional params
    const char *FNO_UID_ROOT{"1.2.840.113619.2"};
    const char *opt_outDirectory{"./anonymized_output"};
    const char *opt_rootUID{nullptr};
    E_FILENAMES opt_filenameType = F_HEX;
    const char *opt_patientListFilename{"anonymized_patients.txt"};

    std::set<E_ANONYM_METHODS> opt_anonymizationMethods{M_113100};
    //TODO implement this in future
    // bool        opt_generateDicomDir{false};

    constexpr int LONGCOL{20};
    constexpr int SHORTCOL{4};
    cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
    cmd.addParam("in-directory", "input directory with DICOM files");
    cmd.addParam("anonymized-prefix", "patient prefix overwriting PatientID, PatientName");

    cmd.setOptionColumns(LONGCOL, SHORTCOL);
    cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
    cmd.addOption("--help", "-h", "print this help text and exit", OFCommandLine::AF_Exclusive);
    cmd.addOption("--version",
                  "print version information and exit",
                  OFCommandLine::AF_Exclusive);
    OFLog::addOptions(cmd);

    cmd.addGroup("anonymization options:");
    cmd.addOption("--anonym-method",
                  "-m",
                  1,
                  "method: string",
                  "add anonymization method to list (default DCM_113100");

    cmd.addOption("--fno-uid-root", "-fuid", fmt::format("use FNO UID root: {} (default)", FNO_UID_ROOT).c_str());
    cmd.addOption("--offis-uid-root", "-ouid", "use OFFIS UID root: " OFFIS_UID_ROOT);
    cmd.addOption("--custom-uid-root", "-cuid", 1, "uid root: string", "use custom UID root");

    cmd.addGroup("output options:");
    cmd.addOption("--out-directory",
                  "-od",
                  1,
                  R"(directory: string (default "./anonymized_output")",
                  "write modified files to output directory");
    cmd.addOption("--filename-hex", "-f", "use integers in hex format as filenames (default)");
    cmd.addOption("--filename-modality-sop", "+f", "use Modality and SOPInstanceUIDs as filenames");
    cmd.addOption("--key-list",
                  "-k",
                  1,
                  "filename: string",
                  R"(write anonymized patients to text file (default "<out-directory>/anonymized_patients.txt))");
    cmd.addOption("--gen-dicomdir", "-dd", "generate new DICOMDIR file (default false)");

    prepareCmdLineArgs(argc, argv, FNO_CONSOLE_APPLICATION);
    if (app.parseCommandLine(cmd, argc, argv)) {
        if (cmd.hasExclusiveOption()) {
            if (cmd.findOption("--version")) {
                app.printHeader(OFTrue);
                return 0;
            }
        }

        cmd.getParam(1, opt_inDirectory);
        cmd.getParam(2, opt_anonymizedPrefix);

        OFLog::configureFromCommandLine(cmd, app);

        if (cmd.findOption("--fno-uid-root") &&
            cmd.findOption("--offis-uid-root") &&
            cmd.findOption("--custom-uid-root")) {
            checkConflict(app, "--fno-uid-root", "--offis-uid-root", "--custom-uid-root");
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
        if (cmd.findOption("--fno-uid-root")) opt_rootUID = FNO_UID_ROOT;
        if (cmd.findOption("--offis-uid-root")) opt_rootUID = OFFIS_UID_ROOT;
        if (cmd.findOption("--custom-uid-root")) app.checkValue(cmd.getValue(opt_rootUID));
        cmd.endOptionBlock();

        if (cmd.findOption("--out-directory")) {
            app.checkValue(cmd.getValue(opt_outDirectory));
        }

        if (cmd.findOption("--key-list")) {
            app.checkValue(cmd.getValue(opt_patientListFilename));
        }

        if (cmd.findOption("--filename-hex") && cmd.findOption("--filename-modality-sop")) {
            checkConflict(app, "--filename-hex", "--filename-modality-sop");
        }

        cmd.beginOptionBlock();
        if (cmd.findOption("--filename-hex")) opt_filenameType = F_HEX;
        if (cmd.findOption("--filename-modality-sop")) opt_filenameType = F_MODALITY_SOPINSTUID;
        cmd.endOptionBlock();

        if (cmd.findOption("--anonym-method", 0, OFCommandLine::FOM_FirstFromLeft)) {
            do {
                const char *method{nullptr};
                app.checkValue(cmd.getValue(method));
                if (method == "DCM_113108") opt_anonymizationMethods.insert(M_113108);
                if (method == "DCM_113109") opt_anonymizationMethods.insert(M_113109);
                if (method == "DCM_113112") opt_anonymizationMethods.insert(M_113112);
            } while (cmd.findOption("--anonym-method", 0, OFCommandLine::FOM_NextFromLeft));


        }
        //TODO see if possible to add in future
        // if (cmd.findOption("--gen-dicomdir")) {
        //     opt_generateDicomDir = true;
        // }

        OFLOG_DEBUG(mainLogger, rcsid.c_str() << OFendl);
    }

    if (std::filesystem::exists(opt_inDirectory)) {
        if (!std::filesystem::is_directory(opt_inDirectory)) {
            OFLOG_ERROR(mainLogger, fmt::format(R"(Path "{}" is not directory)", opt_inDirectory));
            return 1;
        }

        if (std::filesystem::is_empty(opt_inDirectory)) {
            OFLOG_ERROR(mainLogger, fmt::format(R"(Directory "{}" is empty)"));
            return 1;
        }
    } else {
        OFLOG_ERROR(mainLogger, fmt::format(R"(Directory "{}" does not exist)", opt_inDirectory));
        return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
    }

    // TODO implement deidentification methods?
    // https://dicom.nema.org/medical/dicom/current/output/chtml/part16/sect_CID_7050.html
    StudyAnonymizer anonymizer{};

    std::vector<std::string> studyDirectories{};
    for (const auto &studyDir : std::filesystem::directory_iterator(opt_inDirectory)) {
        if (!studyDir.is_directory()) continue;
        if (std::filesystem::is_empty(studyDir)) {
            fmt::print("Directory {} is empty\n", studyDir.path().stem().string());
            continue;
        }
        studyDirectories.emplace_back(studyDir.path().string());
        ++anonymizer.m_numberOfStudies;
    }

    anonymizer.m_patientListFilename = opt_patientListFilename;
    anonymizer.m_filenameType = opt_filenameType;

    (void)std::filesystem::create_directories(opt_outDirectory);
    fmt::ostream keyListFile = fmt::output_file(fmt::format("{}/{}",
                                                opt_outDirectory,
                                                opt_patientListFilename));
    keyListFile.print("old name, old id, new name/id\n");

    int index{1};
    for (const auto &studyDir : std::filesystem::directory_iterator(opt_inDirectory)) {
        bool cond = anonymizer.getStudyFilenames(studyDir.path());
        if (!cond) {
            fmt::print("Failed to get study file names from: {}\n", studyDir.path().stem().string());
            continue;
        }

        fmt::print("Anonymizing study {}\n", studyDir.path().stem().string());

        const std::string pseudoname = fmt::format("{}_{:0{}}",
                                                   opt_anonymizedPrefix,
                                                   index++,
                                                   anonymizer.m_numberOfStudies);


        anonymizer.m_outputStudyDir = fmt::format("{}/{}/DATA", opt_outDirectory, pseudoname);
        if (std::filesystem::exists(anonymizer.m_outputStudyDir)) {
            fmt::print("Directory {} exists, overwriting files\n", anonymizer.m_outputStudyDir);
        } else {
            (void)std::filesystem::create_directories(anonymizer.m_outputStudyDir);
            fmt::print("Created directory {}\n", anonymizer.m_outputStudyDir);
        }

        cond = anonymizer.anonymizeStudy(pseudoname, opt_anonymizationMethods, opt_rootUID);

        // something bad happened
        if (!cond) {
            fmt::print("Failed to anonymize study: {}\n", studyDir.path().stem().string());
            continue;
        }
        //TODO see if possible to add in future
        // if (opt_generateDicomDir) {
        //     const std::string dicomDirPath = fmt::format("{}/{}", opt_outDirectory, pseudoname);
        //     anonymizer.generateDicomDir(dicomDirPath);
        // }

        keyListFile.print("{},{},{}\n", anonymizer.m_oldName, anonymizer.m_oldID, pseudoname);
    }
    keyListFile.close();


    return 0;
}
