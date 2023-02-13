#include "tcp_receiver.hh"
#include "util.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (!_ackno.has_value() && !seg.header().syn) {
        return;
    } 
    
    // Handshake
    if (!_ackno.has_value() && seg.header().syn) {
        auto rd = get_random_generator();
        _isn = WrappingInt32(rd());
        _seq = seg.header().seqno;

        auto data = seg.payload().copy();
        auto eof = seg.header().fin;
        _reassembler.push_substring(data, 0, eof);
        
        // SYN or FIN make _ackno+1
        auto ctrl = seg.length_in_sequence_space() - seg.payload().size();
        _ackno = WrappingInt32(_seq) + ctrl + _reassembler.first_unassembled();

        return;
    }

    if (!_ackno.has_value() || stream_out().input_ended()) {
        return;
    }

    auto data = Buffer(move(seg.payload().copy()));
    auto index = unwrap(seg.header().seqno, _seq + 1, _checkpt);  // "+ 1" for the "SYN"
    auto eof = seg.header().fin;

    _reassembler.push_substring(data, index, eof);
    _ackno = _ackno.value() + _reassembler.first_unassembled() - _checkpt;

    // FIN should make _ackno + 1
    if (stream_out().input_ended()) {
        _ackno = _ackno.value() + 1;
    }

    _checkpt = _reassembler.first_unassembled();
}

optional<WrappingInt32> TCPReceiver::ackno() const { return _ackno; }

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
