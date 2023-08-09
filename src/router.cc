#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  routing_table_.push_back({route_prefix, prefix_length, next_hop, interface_num}); 
}

void Router::route() {
  for (auto& interf : interfaces_) {
    auto recv_dgram = interf.maybe_receive();
    if (recv_dgram.has_value()) {
      auto& dgram = recv_dgram.value();
      if (dgram.header.ttl > 1) {
        dgram.header.ttl--;

        dgram.header.compute_checksum();
        
        auto target = routing_table_.end();
        auto max_len = -1;
        for (auto it = routing_table_.begin(); it != routing_table_.end(); it++) {
          if (it -> prefix_length != 0 && it -> prefix_length <= 32 ) {
            uint8_t const len = 32U - it -> prefix_length;

            // compare the length of source ip and the length of target ip
            auto src_ip = dgram.header.dst >> len;
            auto tgt_ip = it -> route_prefix >> len;
            if (src_ip == tgt_ip) {
              if (it -> prefix_length > max_len) {
                max_len = it -> prefix_length;
                target = it;
              }
            }
          } else {
            if (it -> prefix_length == 0) {
              if (0 > max_len) {
                  max_len = it -> prefix_length;
                  target = it;
              }
            }
          }
        }

        if (target != routing_table_.end()) {
          auto& target_interface = interface(target -> interface_num);
          target_interface.send_datagram(dgram, target -> next_hop.value_or(Address::from_ipv4_numeric(dgram.header.dst)));
        }

      }
    }
  }
}
