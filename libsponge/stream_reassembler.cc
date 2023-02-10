#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _window(), _next_ass_idx(0), _uass_bytes(0)
    , _eof_idx(0), _eof(0), _output(capacity) , _capacity(capacity) {}

void StreamReassembler::insert_pair(const string &data, const size_t idx) {
    size_t len = data.length();
    if (_window[idx].length() < len) {
        _window[idx] = data;
    }

    auto _iter = _window.begin();

    // 线段扫描
    size_t total = _iter->second.length();
    size_t ed = _iter->first + total - 1;

    for ( /* nop */ ; _iter != _window.end(); _iter ++ ) {
        size_t data_len = _iter->second.length();

        if(_iter->first + data_len - 1 <= ed) {
            continue;
        }

        if(_iter->first > ed) {
            total += data_len;
            ed = max(_iter->first + data_len - 1, ed);
            continue;
        }
    }

    _uass_bytes = max(_uass_bytes, total);
}

void StreamReassembler::write_substring() {
    auto _iter = _window.begin();
    for ( /* nop */ ; _iter != _window.end(); _iter ++ ) {
        size_t data_len = _iter->second.length();

        // 1. 不连续， break
        if (_iter->first > _next_ass_idx) {
            break;
        }

        // 2. 已经写入过 ByteStream 
        if ((data_len + _iter->first) <= _next_ass_idx) {
            continue;
        }

        size_t sub_len = _iter->first + data_len - _next_ass_idx;
        auto sub = _iter->second.substr(_next_ass_idx - _iter->first, sub_len);
        
        auto write_bytes = _output.write(sub);
        _next_ass_idx += write_bytes;
        _uass_bytes -= write_bytes;
    }

    _window.erase(_window.begin(), _iter);

    if (_eof && _eof_idx <= _next_ass_idx) {
        _output.end_input();
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof = true;
        _eof_idx = index + data.length();
    }

    // 1. 有重叠的部分 或者 index刚好是 _next_ass_idx 
    if (index <= _next_ass_idx && data.length() + index >= _next_ass_idx) {
        insert_pair(data, index);
        write_substring();
    } 
    // 2. 没有重叠的部分，先存起来
    else if (index > _next_ass_idx) {
        insert_pair(data, index);
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _uass_bytes; }

bool StreamReassembler::empty() const { return _eof; }
