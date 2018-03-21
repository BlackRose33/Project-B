Name : Poorva Sampat

Note: program needs to be run as root to allow raw_socket creation

a) Reused Code: The checksum function in router.c is copied from RFC 1071. The helper function display is from stack exchange, however, this was only used for debugging purposes and is not used in the actual program.

b) Complete: Yes, this stage is completed.

c) Addressing on the way out of your router: The source IP is rewritten to the router's associated IP before sending the packet out, so that the echo reply can be received at the right location. The raw socket at the router is configured to listen at a specific IP:Port pair. If the source IP is not changed before the packet is sent, then the raw socket will not be able to pick up the response from the destination. By changing the IP, we are specifying where the virtual network connects with the physical network.

d) Addressing on the way in to the VM: The routers in the VM use the physical interfaces associated with the host server to connect the virtual network to the physical network. A router that lacks an assigned physical interface is an internal virtual switch that cannot communicate with other virtual or physical machines outside of the host. Thus, it is necessary to assign each router their independent physical interface to allow communication to the physical network.

e) Addressing from the VM to the host: Both your routers and the VM use private addresses to communicate with each other within the network that has been created to exist inside our host OS. When devices on the VM attempt to communicate with the outside world, the host OS uses NAT service the same way it is used in home routers to publicly only display a common public ip from the host OS, while maintaining the private address translations for each communication. 