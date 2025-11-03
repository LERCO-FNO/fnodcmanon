# fnodcmanon
A script for de-identifying a bulk of DICOM studies. It iterates over a directory containing studies in 
individual directories and assigns `pseudoname`, which depends on the chosen pseudoname option with `--pseudoname-*`.

De-identification is based on methods explained in [PS3.16 2025a](https://dicom.nema.org/medical/dicom/current/output/chtml/part16/chapter_D.html#DCM_113100). 

## Usage
```
fnodcmanon in-directory [options]
```
#### Pseudoname options:
* `--prefix (-p)`: set pseudoname prefix eg. TS_, AN, , ...  

* `--pseudoname-random (-pr)` (default): apply randomly generated string from a-z, A-Z, 0-9, including duplicate characters  
* `--pseudoname-integer (-pi)`: apply incrementing integer counter starting at 1  
* `--pseudoname-file (-pf) <path/to/file>`: apply pseudonames from \*.csv/\*.txt file containing `PatientID,Pseudoname` pairs:
```
01,TS_01
02,TS_02
03,TS_03
```

Option `--pseudoname-file` additionally:
* removes alphabet characters and whitespace from PatientID column
* assigns `UN_<random_string>` as pseudoname if corresponding PatientID is not found

#### Anonymization profiles:
`--retain-patient-charac-tags` (`-rpt`) retain patient characterstic tags - patient age, patient Weight, patient height, ...  
`--retain-device-tags` (`-rdt`) retain device identity tags - device description, station name, performed station name, ...  
`--retain-institution-tags` (`-rit`) retain institution identity tags - institution address, institution name, ...  

To print out all anonymization profiles use and examples for affected tags, use `--print-anon-profiles`.



## Requirements
* fmt v11.1 or newer
* dcmtk v3.6.9 or newer, with STL support enabled
