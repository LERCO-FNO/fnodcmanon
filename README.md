# fnodcmanon
A script for de-identifying a bulk of DICOM studies. It iterates over a directory containing studies in 
individual directories and assigns <pseudoname_index>, where index is the order of de-identified study.

## Usage
```
fnodcmanon in-directory pseudoname [options]
```

## Requirements
* fmt v11.1 or newer
* dcmtk v3.6.8 or newer

