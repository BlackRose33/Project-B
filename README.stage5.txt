Name : Poorva Sampat

Note: program needs to be run as root to allow raw_socket creation

a) Reused Code: The checksum function in router.c is copied from RFC 1071.The helper function display is from stack exchange, however, this was only used for debugging purposes and is not used in the actual program.

b) Complete: TYes, this stage is completed. Note that the current program chooses the first hop based on the destination address of the packet to be routed through the onion network(same as the algorithm in stage4). From then on the next hop is the next router number. The current program has a few hardcoded values due to which on a specific run only 1 circuit of M hops can be built where M is less than the number of routers that exist. 

c) This allows the onion proxy to ensure that there are no failures in the partially built circuit while it is still being built. Also, it adds anonymity, as routers that are more than 1 hop away do not have information about the proxy(the src/start node).