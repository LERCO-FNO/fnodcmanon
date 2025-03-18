
#include <string>
#include <filesystem>

#include <pstl/algorithm_fwd.h>

#include "fmt/format.h"
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/ofstd/ofconapp.h"

#include "DicomAnonymizer.hpp"

int main(int argc, char* argv[]) {
    const char *FNO_CONSOLE_APPLICATION{"fnodcmanon"};
    const char *APP_VERSION{"0.3.0"};
    const char *RELEASE_DATE{"2024-11-19"};
    const std::string rcsid = fmt::format("${}: ver. {} rel. {}\n$dcmtk: ver. {} rel.",
        FNO_CONSOLE_APPLICATION,
        APP_VERSION,
        RELEASE_DATE,
        OFFIS_DCMTK_VERSION,
        OFFIS_DCMTK_RELEASEDATE);

    OFLogger mainLooger = OFLog::getLogger(
        fmt::format("dcmtk.apps.{}", FNO_CONSOLE_APPLICATION).c_str()
        );

    OFConsoleApplication    app(FNO_CONSOLE_APPLICATION, "DICOM anonymization tool", rcsid.c_str());
    OFCommandLine           cmd;

    // required params
    const char * opt_inDirectory{nullptr};
    const char * opt_anonymizedPrefix{nullptr};

    // optional params
    const char *    FNO_UID_ROOT = "1.2.840.113619.2";
    const char *    opt_outDirectory{"./anonymized_output"};
    const char *    opt_rootUID{nullptr};
    int             opt_anonymizationProfile{0};
    bool            opt_writeKeyList{true};
    const char *    opt_keyListFilename{"./anonymized_patients.txt"};
    bool            opt_generateDicomDir{false};

    constexpr int LONGCOL{20};
    constexpr int SHORTCOL{4};
    cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
    cmd.addParam("in-directory",        "input directory with DICOM files");
    cmd.addParam("anonymized-prefix",   "patient prefix overwriting PatientID, PatientName");

    cmd.setOptionColumns(LONGCOL, SHORTCOL);
    cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
    cmd.addOption("--help",     "-h",   "print this help text and exit", OFCommandLine::AF_Exclusive);
    cmd.addOption("--version",          "print version information and exit",
                  OFCommandLine::AF_Exclusive);
    OFLog::addOptions(cmd);

    cmd.addGroup("anonymization options:");
    cmd.addOption("--fno-uid-root",     "-f-uid",       fmt::format("use FNO UID root: {} (default)", FNO_UID_ROOT).c_str());
    cmd.addOption("--offis-uid-root",   "-o-uid",       "use OFFIS UID root: " OFFIS_UID_ROOT);
    cmd.addOption("--custom-uid-root",  "-c-uid", 1,    "uid root: string", "use custom UID root");

    cmd.addGroup("output options:");
    cmd.addOption("--out-directory",    "-od", 1,   R"(directory: string (default "./anonymized_output")", "write modified files to output directory");
    cmd.addOption("--key-list",         "-k",       R"(write pre/post anonymization patient information to text file "./anonymized_patients.txt)");
    cmd.addOption("--key-list-custom",  "-kl", 1,   "filename: string", "write list of patient information pre/post anonymization to named text file");
    cmd.addOption("--gen-dicomdir",     "-dd",      "generate new DICOMDIR file (default false)");

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

        if (cmd.findOption("--fno-uid-root")) opt_rootUID = FNO_UID_ROOT;
        if (cmd.findOption("--offis-uid-root")) opt_rootUID = OFFIS_UID_ROOT;

        if (cmd.findOption("--custom-uid-root")) {
            app.checkValue(cmd.getValue(opt_rootUID));
        }

        if (cmd.findOption("--out-directory")) {
            app.checkValue(cmd.getValue(opt_outDirectory));
        }

        if (cmd.findOption("--key-list")) opt_writeKeyList = true;
        if (cmd.findOption("--key-list-custom")) {
            opt_writeKeyList = true;
            app.checkValue(cmd.getValue(opt_keyListFilename));
        }

        if (cmd.findOption("--gen-dicomdir")) {
            opt_generateDicomDir = true;
        }

        OFLOG_DEBUG(mainLooger, rcsid.c_str() << OFendl);
    }

    if (!std::filesystem::exists(opt_inDirectory)) {
        fmt::print("Directory {} does not exist\n", opt_inDirectory);
        return 1;
    }
    // TODO implement deidentification methods?
    // https://dicom.nema.org/medical/dicom/current/output/chtml/part16/sect_CID_7050.html
    DicomAnonymizer anonymizer;

    int index{1};
    for (const auto& studyDir : std::filesystem::directory_iterator(opt_inDirectory)) {
        if (anonymizer.getDicomFilenames(studyDir.path())) continue;

        fmt::print("Anonymizing study {}\n", studyDir.path().string());

        const std::string pseudoname = fmt::format("{}_{:0{}}", opt_anonymizedPrefix,
                                                   index++,
                                                   anonymizer.m_number_of_files);
        bool cond = anonymizer.anonymizeStudy(pseudoname);

        // something bad happened
        if (!cond)
            continue;
    }







    return 0;
}
