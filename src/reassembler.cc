#include "reassembler.hh"

using namespace std;

// It may achieve 10 Gbit/s.
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{	
	process_substr(first_index, data, output); 
	
	//push it to output	
	if (first_index == next_idx) {
		std::string nxt_str = std::move(unassemble_strs[first_index]);
		output.push(nxt_str);
		unorder_strs -= nxt_str.size();
		unassemble_strs.erase(first_index);	
	}	

	if(is_last_substring) {
		byte_stream_end = true;
		eof_idx = first_index + data.size();
	}

	if(byte_stream_end && output.bytes_pushed() == eof_idx) {
		output.close();
		return;
	}
	
} 

uint64_t Reassembler::bytes_pending() const
{
  return unorder_strs;
}

void Reassembler::process_substr(uint64_t& first_index, std::string& data, Writer& output) {

	if (data.empty() || output.available_capacity() == 0) return;

	uint64_t unassembled_idx = output.bytes_pushed();
	uint64_t unaccepctable_idx = unassembled_idx + output.available_capacity();
	uint64_t last_idx = first_index + data.size() - 1;
	next_idx = unassembled_idx;	
	
	if(unassembled_idx >= unaccepctable_idx //No more space in buffer
			|| first_index>= unaccepctable_idx // The first index is beyond the first unaccepctable index
			|| last_idx < unassembled_idx)  // The first unassembled index is beyond the last index
			return;

	if(first_index < unassembled_idx) {
		//remove the part of data before unassembled index
		data = data.substr(unassembled_idx - first_index); 
		first_index = unassembled_idx;
	} else if (first_index >= unassembled_idx && last_idx >= unaccepctable_idx) {
		// remove the part of data beyond unaccepectable index
		data = data.substr(0, unaccepctable_idx - first_index); 
	}
	
	remove_overlap(first_index, data);
	
	//Insert data
	unassemble_strs[first_index] = data;
	unorder_strs += data.size();

	return;

}


void Reassembler::remove_overlap(uint64_t& first_index, std::string& data) {
	//if nothing in the map
	if(unassemble_strs.size() == 0) return;
	
	//compare incoming index with every index in the map
	for(auto i = unassemble_strs.begin(); i != unassemble_strs.end();) {
		if(i -> second.size() == 0) {
			i++;
			continue;
		}
		uint64_t last_idx = first_index + data.size() - 1;
		uint64_t begin_idx = i -> first;
		uint64_t end_idx = begin_idx + i -> second.size() - 1;
		if (last_idx + 1 == begin_idx) { // No overlap.
			data += i -> second;
			unorder_strs -= i->second.size();
			i = unassemble_strs.erase(i);

		// When they have some overlaps	
		} else if (first_index <= begin_idx && begin_idx <= last_idx) {
			if(end_idx > last_idx) {
				data += i -> second.substr(last_idx - begin_idx + 1); // Add the missing back part to incoming data. 
			}	
			unorder_strs -= unassemble_strs[begin_idx].size();
			i = unassemble_strs.erase(i);
		} else if (first_index >= begin_idx && first_index <= end_idx) { 
			if (last_idx <= end_idx) {
				data = i -> second; // when new data is completely covered by old data.
			} else {
				data = i -> second.substr(0, first_index - begin_idx) + data; // Add the missing front part to incoming data.
			}
			first_index = begin_idx;
			unorder_strs -= i -> second.size();
			i = unassemble_strs.erase(i);
		} else { 
			i++;
		}
	
	}
}
