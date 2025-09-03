# Call with
# irule -F msi_archive_extract_test.r
# Or call specifically with: 
# /bin/irule -r irods_rule_engine_plugin-irods_rule_language-instance -F msi_archive_extract_test.r

testArchiveExtraction {

    *archivePath = "/nlmumc/home/rods/msi_archive_backup/archive.tar";
    *targetCollection = "/nlmumc/home/rods/test-data-extracted";
    *extractFile = "null"; # null for entire extraction
    *targetResource = "null"; # null for default resource storage
    *status = 0;

    # Archive path, target collection, specific file to be extracted (optional), target resource (optional)
    msiArchiveExtract(*archivePath, *targetCollection, *extractFile, *targetResource, *status);

    # Error logging
    if (*status != 0) {
        writeLine("stdout", "Archive extraction failed with status: *status");
    } else {
        writeLine("stdout", "Archive extracted successfully at *archivePath");
    }
}

INPUT null
OUTPUT ruleExecOut
