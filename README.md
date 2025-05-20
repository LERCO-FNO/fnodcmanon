# fnodcmanon
A script for de-identifying a bulk of DICOM studies. It iterates over a directory containing studies in 
individual directories and assigns <pseudoname_index>, where index is the order of de-identified study.

## Usage
```
fnodcmanon in-directory pseudoname [options]
```

De-identification is based on methods explained in [PS3.16 2025a](https://dicom.nema.org/medical/dicom/current/output/chtml/part16/chapter_D.html#DCM_113100). 

## Requirements
* fmt v11.1 or newer
* dcmtk v3.6.8 or newer
* [sql_modern_cpp](https://github.com/SqliteModernCpp/sqlite_modern_cpp)
