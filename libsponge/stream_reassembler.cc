#include "stream_reassembler.hh"

#include <cassert>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _unass()
    , _next_ass_idx(0)
    , _unass_bytes(0)
    , _eof_idx(-1)
    , _output(capacity)
    , _capacity(capacity) {}

//! \details This function calls at first in the push_substring 
//! It aims to get the first index that doesn't overlap with 
//! the substring in the unassembled string in the map
size_t StreamReassembler::get_idx(const size_t index) {
    //!< Gets the first iterator pointer in _unass that is greater than index
    auto pos_iter = _unass.upper_bound(index);
    //!< Try to get an iterator pointer that is less than or equal to index
    if (pos_iter != _unass.begin()) {
        pos_iter -- ;
    }

    //!< Gets the new starting position of the current substring
    size_t new_idx = index;
    //!< If there is a substring in front of it
    if (pos_iter != _unass.end() && pos_iter->first <= index) {
        const size_t front_idx = pos_iter->first;

        //!< If there is an overlap in front of the current substring
        if (index < front_idx + pos_iter->second.size()) {
            new_idx = front_idx + pos_iter->second.size();
        }
    } 
    //!< If there is no substring in front of it, compare it with the currently read pos
    else if (index < _next_ass_idx) {
        new_idx = _next_ass_idx;
    }

    return new_idx;
}

//! \details When getting the next substring of the current substring, 
//! we need to consider that new_idx may coincide with back_idx
//! This function is aims to truncate the overlapping area
void StreamReassembler::truncate(const size_t new_idx, ssize_t& data_size) {
    auto pos_iter = _unass.lower_bound(new_idx);
    //!< If it conflicts with the following substring, truncate the length
    while (pos_iter != _unass.end() && new_idx <= pos_iter->first) {
        const size_t data_end_pos = new_idx + data_size;

        //!< If there is no overlapping area
        if (pos_iter->first >= data_end_pos) {
            break;
        }

        //!< If it's a partial overlap
        if (data_end_pos < pos_iter->first + pos_iter->second.size()) {
            data_size = pos_iter->first - new_idx;
            break;
        } 
        //!< If it all overlaps
        else {
            _unass_bytes -= pos_iter->second.size();
            pos_iter = _unass.erase(pos_iter);
            continue;
        }
    }
}

//! \details This function is aim at determine whether any data is independent 
//! and check whether the current substring is completely 
//! contained by the previous substring.
void StreamReassembler::save(const std::string &data, const size_t new_idx, 
                             const ssize_t data_size, const size_t data_start_pos) {
    if (data_size <= 0) {
        return;
    }

    const string new_data = data.substr(data_start_pos, data_size);
        
    //!< If the new string can be written directly
    if (new_idx == _next_ass_idx) {
        const size_t write_byte = _output.write(new_data);
        _next_ass_idx += write_byte;
        
        //!< _output is full that can't be written, insert into the _unass
        if (write_byte < new_data.size()) {
            size_t len = new_data.size() - write_byte;
            const string data_to_store = new_data.substr(write_byte, len);
            _unass_bytes += data_to_store.size();
            _unass.insert(make_pair(_next_ass_idx, std::move(data_to_store)));
        }
    } else {
        const string data_to_store = new_data.substr(0, new_data.size());
        _unass_bytes += data_to_store.size();
        _unass.insert(make_pair(new_idx, std::move(data_to_store)));
    }
}

//! \details This functions calls just after pushing a substring into the
//! _output stream. It aims to check if there exists any contiguous substrings
//! recorded earlier can be push into the stream.
void StreamReassembler::check_contiguous() {
    for (auto iter = _unass.begin(); iter != _unass.end(); /* nop */) { 
        assert(_next_ass_idx <= iter->first);

        if (iter->first != _next_ass_idx) {
            break;
        }

        //!< If it happens to be a message that can be received
        const size_t write_num = _output.write(iter->second);
        _next_ass_idx += write_num;
        
        //!< If it is not fully written, it means that it is full
        //!< keep the rest and exit.
        if (write_num < iter->second.size()) {  
            _unass_bytes += iter->second.size() - write_num;

            auto str = std::move(iter->second.substr(write_num));
            auto new_pair = make_pair(_next_ass_idx, str);
            
            _unass.insert(new_pair);
            _unass_bytes -= iter->second.size();
            _unass.erase(iter);

            break;
        } else {
            //!< If it is all written, delete the original iterator and update it
            _unass_bytes -= iter->second.size();
            iter = _unass.erase(iter);
        }
        
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t new_idx = get_idx(index);
    
    //!< The data index to which the new start position of the substring corresponds
    const size_t data_start_pos = new_idx - index;
    //!< The length of the data to be saved by the current substring
    ssize_t data_size = data.size() - data_start_pos;
    
    truncate(new_idx, data_size);
    
    size_t _1st_unac_idx = _next_ass_idx + _capacity - _output.buffer_size();

    //!< Detect whether there is data out of capacity.
    //!< Note that the capacity here does not refer to the number of bytes that
    //!< can be saved but to the size of the window that can be saved.
    if (_1st_unac_idx <= new_idx) {        
        return;
    }
    
    save(data, new_idx, data_size, data_start_pos);

    check_contiguous();
    
    if (eof) {
        _eof_idx = index + data.size();
    }

    if (_eof_idx <= _next_ass_idx) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unass_bytes; }

bool StreamReassembler::empty() const { return _unass_bytes == 0; }