#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if(!isn_.has_value()) {
    if(!message.SYN) {
      return;
    }
    isn_ = message.seqno;
  }
  //convert seqno to abs seqno
  auto const abs_ackno = inbound_stream.bytes_pushed() + 1; // + 1: convert stream index to absolute seqno 
  uint64_t abs_seqno = message.seqno.unwrap(isn_.value(), abs_ackno); 

  //convert abs seqno to stream index
  auto const first_idx = message.SYN ? 0: abs_seqno - 1;
  reassembler.insert(first_idx, message.payload.release(), message.FIN, inbound_stream);
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  TCPReceiverMessage msg{};
  auto const win_size = inbound_stream.available_capacity();
  msg.window_size = win_size < UINT16_MAX ? win_size :  UINT16_MAX;
  if(isn_.has_value()) {
    // convert stream index to abs seqno
    auto const abs_ackno = inbound_stream.bytes_pushed() + 1; // + 1: for SYN legnth
    uint64_t const abs_seqno = abs_ackno + inbound_stream.is_closed(); // + inbound_stream.is_closed() for FIN legnth
    msg.ackno = Wrap32::wrap(abs_seqno, isn_.value() );
  }
  return msg;
}
