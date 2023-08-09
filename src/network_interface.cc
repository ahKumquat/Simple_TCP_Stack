#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  auto target_ip = next_hop.ipv4_numeric();

  if (ip2eth_.contains(target_ip)) {
    eframes_.push({{ip2eth_[target_ip].first, ethernet_address_, EthernetHeader::TYPE_IPv4}, serialize(dgram)});
  } else {
    if(arp_timer_.contains(target_ip)) {
      datagrams_[target_ip].push_back(dgram);
    } else {
      ARPMessage req_msg;

      req_msg.opcode = ARPMessage::OPCODE_REQUEST;
      req_msg.sender_ethernet_address = ethernet_address_;
      req_msg.sender_ip_address = ip_address_.ipv4_numeric();
      req_msg.target_ip_address = target_ip;

      eframes_.push({{ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP}, serialize(req_msg)});

      arp_timer_.emplace(next_hop.ipv4_numeric(), 0);

      datagrams_.insert({target_ip, {dgram}});
    }
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if(frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) return {};


  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram dgram;
    if (parse(dgram, frame.payload)) return dgram;
  } else if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage msg;
    if (parse(msg, frame.payload)) {
      ip2eth_.insert({msg.sender_ip_address, {msg.sender_ethernet_address, 0}});
      if (msg.opcode == ARPMessage::OPCODE_REQUEST) {
        if (msg.target_ip_address == ip_address_.ipv4_numeric()) {
          ARPMessage reply_msg;
          reply_msg.opcode = ARPMessage::OPCODE_REPLY;
          reply_msg.sender_ethernet_address = ethernet_address_;
          reply_msg.sender_ip_address = ip_address_.ipv4_numeric();
          reply_msg.target_ip_address = msg.sender_ip_address;
          reply_msg.target_ethernet_address = msg.sender_ethernet_address;

          eframes_.push({{msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP}, serialize(reply_msg)});
        }
      } else if (msg.opcode == ARPMessage::OPCODE_REPLY) {
        ip2eth_.insert({msg.sender_ip_address, {msg.sender_ethernet_address, 0}});
        auto dgrams = datagrams_[msg.sender_ip_address];
        for (auto dgram : dgrams) {
          send_datagram(dgram, Address::from_ipv4_numeric(msg.sender_ip_address));
        }
        datagrams_.erase(msg.sender_ip_address);
      }
    }
  }
  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  static constexpr size_t ARP_REQUEST_TTL = 5000; 
  static constexpr size_t IP_MAPPING_TTL = 30000;

  for (auto i = arp_timer_.begin() ; i != arp_timer_.end();) {
    i -> second += ms_since_last_tick;

    if (i -> second >= ARP_REQUEST_TTL) {
      i = arp_timer_.erase(i);
    } else {
      i++;
    }
  }
  
  for (auto i = ip2eth_.begin() ; i != ip2eth_.end();) {
    i -> second.second += ms_since_last_tick;

    if (i -> second.second >= IP_MAPPING_TTL) {
      i = ip2eth_.erase(i);
    } else {
      i++;
    }
  }

}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if(eframes_.empty()) return {};

  auto frame = std::move(eframes_.front());
  eframes_.pop();
  return frame;
}
