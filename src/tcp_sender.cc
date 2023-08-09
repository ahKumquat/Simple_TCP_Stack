#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return outstandings;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return con_retrans;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{ 
  if (outgoing_segs.empty()) return {};

  if (!timer_.is_running()) timer_.start();

  auto msg = outgoing_segs.front();
  outgoing_segs.pop();
  return msg;
}

void TCPSender::push( Reader& outbound_stream )
{
  uint64_t curr_win_size = window_size ? window_size : 1;

  while(curr_win_size > outstandings) {

    // construct a single package
    TCPSenderMessage msg;
    if (!SYN) {
      SYN = msg.SYN = true;
      outstandings++;
    }

    // set seqno
    msg.seqno = Wrap32::wrap(next_seqno_, isn_); // convert abs seqno to seqno

    // load payload
    auto payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, curr_win_size - outstandings);
    read(outbound_stream, payload_size, msg.payload);
    outstandings += msg.payload.size();

    if( !FIN && outbound_stream.is_finished() && curr_win_size > outstandings ) {
      FIN = msg.FIN = true;
      outstandings++;
    }

    // stop sending package if no data 
    if(msg.sequence_length() == 0) break;

    // send data
    outgoing_segs.push(msg);

    //keep track of which segments have been sent but not yet acknowledged by the receiver
    next_seqno_ += msg.sequence_length();
    outstanding_segs.push(msg);

    // stop filling the window if FIN flag is set
    if( msg.FIN || outbound_stream.bytes_buffered() == 0) break;
  
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return {Wrap32::wrap(next_seqno_, isn_) , false, {} , false};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size = msg.window_size;
  if (msg.ackno.has_value()) {
    uint64_t abs_seqno = msg.ackno.value().unwrap(isn_, next_seqno_);

    if(abs_seqno > next_seqno_) return;

    ackno_ = abs_seqno;

    while(!outstanding_segs.empty()) {
      auto& first_msg = outstanding_segs.front();

      // if the package is received (if the ackno is greater than all of the sequence numbers in the segment)
      if (ackno_ >= first_msg.sequence_length() + first_msg.seqno.unwrap(isn_, next_seqno_)) {
        outstandings -= first_msg.sequence_length();
        outstanding_segs.pop();
        timer_.reset_RTO(); // set the RTO back to its “initial value.”

        // if the sender has any outstanding data, restart the retransmission timer so that it
        // will expire after RTO milliseconds (for the current value of RTO).
        if(!outstanding_segs.empty()) timer_.start(); 
        
        // reset the count of “consecutive retransmissions” back to zero.
        con_retrans = 0;
      } else {
        break;
      }
    }

    if(outstanding_segs.empty()) timer_.stop();
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  timer_.tick(ms_since_last_tick);
  if (timer_.is_expired()) {
    // Retransmit the earliest segment
    outgoing_segs.push(outstanding_segs.front());

    if (window_size != 0) {
      // Keep track of the number of consecutive retransmissions, and increment it
      // because you just retransmitted something.
      con_retrans++;

      // This is called “exponential backoff”—it slows down
      // retransmissions on lousy networks to avoid further gumming up the works.
      timer_.doule_RTO();
    }

    // restart the retransmission timer
    timer_.start();
  }
}
