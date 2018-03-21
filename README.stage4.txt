Name : Poorva Sampat

Note: program needs to be run as root to allow raw_socket creation

a) Reused Code: The checksum function in router.c is copied from RFC 1071. The helper function display is from stack exchange, however, this was only used for debugging purposes and is not used in the actual program.

b) Complete: Yes, this stage is completed.

c) Router Selection: 
  i) The mod operation divides the input set(set of all possible destination IPs of targets) to equally map to values in the output set. This ensures that the following approach is statistically load balanced over many flows to many targets, as based on the input set a hop to any router in the output set is equally probable.
  ii) Yes
  iii) During a DoS to a specific destination ip, all flows would be going to the same target and thus, following the same route. (Since choosing the route in this approach deterministically depends on the destination IP in the flow)