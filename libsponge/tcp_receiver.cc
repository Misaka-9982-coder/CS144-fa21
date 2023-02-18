#include "tcp_receiver.hh"
#include "util.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    /**
     * @note Listen state
     * @def not ackno().has_value() 
     */
    if (!_ackno.has_value()) {
        // Handshake
        if (seg.header().syn) {
            auto rd = get_random_generator();
            _isn = WrappingInt32(rd());
            _seq = seg.header().seqno;

            _reassembler.push_substring(move(seg.payload().copy()), 0, seg.header().fin);
            
            // SYN or FIN make _ackno+1
            auto ctrl = seg.length_in_sequence_space() - seg.payload().size();
            _ackno = WrappingInt32(_seq) + ctrl + _reassembler.first_unassembled();
        }

        return;
    } 

    /**
     * @note FIN_RECV state
     * @def stream_out.input_ended()
     */
    if (_ackno.has_value() && !stream_out().input_ended()) {
        
        /**
         * @note SYN_RECV state
         * @def ackno.has_value() and not stream_out.input_ended()
         * @code 48 - 54
         */
        auto index = unwrap(seg.header().seqno, _seq + 1, _checkpt);  // "+ 1" for the "SYN"
        
        // data too far, considered out of data
        if (index > _checkpt && ((index - _checkpt) & 0x80000000)) {
            return;
        }
        
        // data too far, considered out of data
        if (index < _checkpt && ((_checkpt - index) & 0x80000000)) {
            return;
        }

        _reassembler.push_substring(move(Buffer(move(seg.payload().copy()))), index, seg.header().fin);
        _ackno = _ackno.value() + _reassembler.first_unassembled() - _checkpt;

        // FIN should make _ackno + 1
        if (stream_out().input_ended()) {
            _ackno = _ackno.value() + 1;
        }

        _checkpt = _reassembler.first_unassembled();
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { return _ackno; }

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
