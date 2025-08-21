# Call with
# irule -F msi_archive_create_test.r
# Or call specifically with: 
# /bin/irule -r irods_rule_engine_plugin-irods_rule_language-instance -F msi_archive_create_test.r

testArchiveCreation {
    *sourceCollection = "/nlmumc/home/rods/test-data-collection";
    *archiveTargetPath = "/nlmumc/home/rods/msi_archive_backup/archive.tar";
    *status = 0;
    # Target path, source collection path
    msiArchiveCreate(*archiveTargetPath, *sourceCollection, "", *status);

    # Error logging
    if (*status != 0) {
        writeLine("stdout", "Archive creation failed with status: *status");
    } else {
        writeLine("stdout", "Archive created successfully at *archiveTargetPath");
    }
}
INPUT null
OUTPUT ruleExecOut
