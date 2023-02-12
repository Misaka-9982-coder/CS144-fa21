#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , _first_uass(0)
    , _unassembled_bytes(0)
    , _eof(false)
    , _eof_idx(0)
    , _blocks() {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // the data that have been reassembled
    if (index + data.size() < _first_uass) {
        return;
    }

    if (eof && !_eof) {
        _eof = true;
        _eof_idx = index + data.size();
    }

    StreamBlock blk(index, move(string(data)));

    // if a part of the data have been reassembled
    if (index < _first_uass) {
        blk.buffer().remove_prefix(_first_uass - index);
    }

    // if a part of the data out of the capacity
    if (index + data.size() > _capacity + _first_uass) {
        blk.buffer().remove_suffix(index + data.size() - _capacity - _first_uass);
    }

    add_block(blk);
    write_to_stream();
    EOFcheck();
}

void StreamReassembler::push_substring(const Buffer &data, const size_t index, const bool eof) {
    // the data that have been reassembled
    if (index + data.size() < _first_uass) {
        return;
    }

    if (eof && !_eof) {
        _eof = true;
        _eof_idx = index + data.size();
    }

    StreamBlock blk(index, data);

    // if a part of the data have been reassembled
    if (index < _first_uass) {
        blk.buffer().remove_prefix(_first_uass - index);
    }

    // if a part of the data out of the capacity
    if (index + data.size() > _capacity + _first_uass) {
        blk.buffer().remove_suffix(index + data.size() - _capacity - _first_uass);
    }

    add_block(blk);
    write_to_stream();
    EOFcheck();
}

//! \details This function check if eof is written to the stream
inline void StreamReassembler::EOFcheck() {
    if (!_eof) {
        return; 
    }
    
    if (static_cast<size_t>(_eof_idx) == _first_uass) {
        _output.end_input();
    }
}

//! \details This function write the first block into the stream,
//! the first block should begin at '_first_uass'
inline void StreamReassembler::write_to_stream() {
    while (!_blocks.empty()) {
        auto block = *_blocks.begin();
        if (block.begin() != _first_uass) {
            return;
        }

        size_t bytes_written = _output.write(block.buffer());

        if (bytes_written == 0) {
            return;
        }

        _first_uass += bytes_written;
        _unassembled_bytes -= bytes_written;
        _blocks.erase(_blocks.begin());

        // partially written
        if (bytes_written != block.len()) {
            block.buffer().remove_prefix(bytes_written);
            _blocks.insert(block);
        }
    }
}

//! \details This function add "to_add" blocks to set blocks
// merge all the blocks mergeable
inline void StreamReassembler::add_block(StreamBlock &new_block) {
    if (new_block.len() == 0) {
        return;
    }

    vector<StreamBlock> blks_to_add;
    blks_to_add.emplace_back(new_block);

    if (!_blocks.empty()) {
        auto nblk = blks_to_add.begin();
        auto iter = _blocks.lower_bound(*nblk);
        auto prev = iter;

        while (iter != _blocks.end() && overlap(*iter, *nblk)) {
            if ((*iter).end() >= (*nblk).end()) {
                (*nblk).buffer().remove_suffix((*nblk).end() - (*iter).begin());
                break;
            }
            StreamBlock last(*nblk);
            (*nblk).buffer().remove_suffix((*nblk).end() - (*iter).begin());
            last.buffer().remove_prefix((*iter).end() - (*nblk).begin());
            blks_to_add.push_back(last);
            nblk = blks_to_add.end();
            nblk -- ;
            iter ++ ;
        }

        // compare with prevs
        // check one previous block is enough
        if (prev != _blocks.begin()) {
            prev -- ;
            nblk = blks_to_add.begin();
            
            if (overlap(*nblk, *prev)) {
                (*nblk).buffer().remove_prefix((*prev).end() - (*nblk).begin());
            }
        }
    }

    for (auto &blk : blks_to_add) {
        if (blk.len() != 0) {
            _blocks.emplace(blk);
            _unassembled_bytes += blk.len();
        }
    }
}

//! \details This function check if the two blocks have overlap part
bool StreamReassembler::overlap(const StreamBlock &blk, const StreamBlock &new_blk) const {
    if (blk.begin() < new_blk.begin()) {
        return overlap(new_blk, blk);
    }

    return (blk.begin() < new_blk.end());
}

uint64_t StreamReassembler::first_unassembled() const { return _first_uass; }

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
