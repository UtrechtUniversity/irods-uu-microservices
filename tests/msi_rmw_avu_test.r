# Call with
#
# irule -F msi_add_avu_test.r
#

test() {
    # type, path, attribute, value, unit
    msi_rmw_avu("-d", "/nlmumc/home/rods/docker-compose.yml", "%", "%", "xyz");
}


INPUT null
OUTPUT ruleExecOut