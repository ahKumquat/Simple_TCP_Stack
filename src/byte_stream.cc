#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : queue_(), view_queue_(),capacity_size_( capacity ), written_size_(0), read_size_(0), buffered_size_(0), is_closed_(false), error_(false){}

void Writer::push( string data )
{
  
  if(is_closed() || available_capacity() == 0 || data.empty()) return;

  uint64_t write_size = min(data.size(), available_capacity());
  
  written_size_ += write_size;
  buffered_size_ += write_size;
  
  if (write_size < data.size()) {
    data = data.substr(0, write_size);
  }
  queue_.emplace_back(std::move(data));
  view_queue_.emplace_back(queue_.back());
}

void Writer::close()
{
  is_closed_ = true;
}


void Writer::set_error()
{
  error_ = true;
}

bool Writer::is_closed() const
{
  return is_closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_size_ - buffered_size_ ;
}

uint64_t Writer::bytes_pushed() const
{
  return written_size_;
}

string_view Reader::peek() const
{
  return view_queue_.empty() ? string_view{} : view_queue_.front();
}

bool Reader::is_finished() const
{
  return is_closed_ && !buffered_size_;
}

bool Reader::has_error() const
{
  return error_;
}

void Reader::pop( uint64_t len )
{
  uint64_t pop_size = min(len, bytes_buffered());
  while ( pop_size > 0 ) {
    uint64_t sz = view_queue_.front().size();
    if ( pop_size < sz ) {
      view_queue_.front().remove_prefix( pop_size );
      buffered_size_ -= pop_size;
      read_size_ += pop_size;
      return;
    }
    view_queue_.pop_front();
    queue_.pop_front();
    pop_size -= sz;
    buffered_size_ -= sz;
    read_size_ += sz;
  }  
}

uint64_t Reader::bytes_buffered() const
{
  return buffered_size_;
}

uint64_t Reader::bytes_popped() const
{
  return read_size_;
}
