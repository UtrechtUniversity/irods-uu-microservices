testRule {
   msiGenerateRandomID(*length, *randomID);
   writeLine("stdout", *randomID);
}
input *length=6
output ruleExecOut
