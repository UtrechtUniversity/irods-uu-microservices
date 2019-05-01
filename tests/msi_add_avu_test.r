# Call with
#
# irule -F msi_add_avu_test.r
#

test() {
    # type, path, attribute, value, unit
    msi_add_avu("-d", "/nlmumc/home/rods/docker-compose.yml", "foo", "bar", "xyz");
}


INPUT null
OUTPUT ruleExecOut