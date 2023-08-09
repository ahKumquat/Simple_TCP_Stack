#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + n;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const constexpr uint64_t INT32 = 1L << 32;
  
  // we need to get the offset
  auto const checkpoint_32 = wrap(checkpoint, zero_point); 
  uint64_t offset = raw_value_ - checkpoint_32.raw_value_;

  if(checkpoint + offset < INT32 || offset < (INT32 >> 1)) {
    return checkpoint + offset; //checkpoint + x , where x is required offset. Move to right
  }
  return checkpoint + offset - INT32; // checkpoint - (2^32 - x) Move to left. 
}
