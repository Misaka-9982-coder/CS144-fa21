#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) 
    , _retransmission_timeout{retx_timeout}
    , _timer()
    , _window_size(1)
    , _bytes_in_flight(0)
    , _segments_in_flight() {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    size_t window_size = _window_size == 0 ? 1 : _window_size;

    /**
     * @note CLOSED state
     * @def next_seqno_absolute() == 0
     */
    if (_state == CLOSED) {
        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = wrap(_next_seqno, _isn);

        //! seg.length_in_sequence_space() > 0
        //! next_seqno_absolute() > 0
        //! next_seqno_absolute() == bytes_in_flight()
        _next_seqno += seg.length_in_sequence_space();
        _bytes_in_flight += seg.length_in_sequence_space();

        _segments_in_flight.push(seg);
        _segments_out.emplace(move(seg));

        if (!_timer.activated()) {
            _timer.reset(_retransmission_timeout);
        }
        
        /**
         * @note SYN_SENT state
         * @def next_seqno_absolute() > 0
         * and next_seqno_absolute() == bytes_in_flight()
         */
        _state = SYN_SENT;
    } else if (_state == SYN_ACKED) {

        size_t bytes_sent = 0;

        if (_bytes_in_flight >= window_size) {
            return;
        }

        size_t max_tobe_sent = window_size - _bytes_in_flight;

        while (bytes_sent < max_tobe_sent && !_stream.buffer_empty()) {
            TCPSegment seg;
            seg.payload() = Buffer(move(_stream.read(
                min(TCPConfig::MAX_PAYLOAD_SIZE, max_tobe_sent - bytes_sent)
            )));
            bytes_sent += seg.payload().size();

            /**
             * @note FIN_SENT state
             * @def stream_in().eof() 
             * and next_seqno_absolute() == stream_in().bytes_written() + 2
             * and sender.bytes_in_flight() > 0
             */
            if (_stream.eof() && bytes_sent < max_tobe_sent) {
                seg.header().fin = true;
                _state = FIN_SENT;
            }

            seg.header().seqno = wrap(_next_seqno, _isn);

            _next_seqno += seg.length_in_sequence_space();
            _bytes_in_flight += seg.length_in_sequence_space();

            _segments_in_flight.push(seg);

            _segments_out.emplace(move(seg));

            if (!_timer.activated()) {
                _timer.reset(_retransmission_timeout);
            }
        }

        /**
         * @note FIN_SENT state
         * @def stream_in().eof() 
         * and next_seqno_absolute() == stream_in().bytes_written() + 2
         * and sender.bytes_in_flight() > 0
         */
        if (window_size - _bytes_in_flight >= 1 && _stream.eof() && _state == SYN_ACKED) {
            TCPSegment fin_seg;
            fin_seg.header().fin = true;
            fin_seg.header().seqno = wrap(_next_seqno, _isn);
            _bytes_in_flight += fin_seg.length_in_sequence_space();
            _next_seqno += fin_seg.length_in_sequence_space();
            _segments_in_flight.push(fin_seg);
            _segments_out.emplace(move(fin_seg));

            if (!_timer.activated()) {
                _timer.reset(_retransmission_timeout);
            }

            _state = FIN_SENT;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (unwrap(ackno, _isn, _next_seqno) > _next_seqno) {
        return;
    }

    _window_size = window_size;

    if (_state == SYN_SENT && ackno == wrap(1, _isn)) {
        _state = SYN_ACKED;
    }

    if (_segments_in_flight.empty()) {
        return;
    }

    TCPSegment seg = _segments_in_flight.front();
    bool rec_success = false;

    auto seq = unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space();
    auto ack = unwrap(ackno, _isn, _next_seqno);

    while (seq <= ack) {
        _bytes_in_flight -= seg.length_in_sequence_space();
        _segments_in_flight.pop();
        rec_success = true;

        if (_segments_in_flight.empty()) {
            break;
        }

        seg = _segments_in_flight.front();

        seq = unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space();
        ack = unwrap(ackno, _isn, _next_seqno);
    }

    if (rec_success) {
        _retransmission_timeout = _initial_retransmission_timeout;
        _consecutive_retransmission_count = 0;

        if (!_segments_in_flight.empty()) {
            _timer.reset(_retransmission_timeout);
        } else {
            _timer.stop();
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if (_timer.activated() && _timer.passing(ms_since_last_tick)) {

        TCPSegment seg = _segments_in_flight.front();

        if (_window_size != 0) {
            _consecutive_retransmission_count ++ ;
            _retransmission_timeout *= 2;
        }

        if (_consecutive_retransmission_count <= TCPConfig::MAX_RETX_ATTEMPTS) {
            _segments_out.push(seg);
            _timer.reset(_retransmission_timeout);
        } else {
            _timer.stop();
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission_count; }

void TCPSender::send_empty_ack() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.emplace(move(seg));
}

void TCPSender::send_empty_rst() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    seg.header().rst = true;
    _segments_out.emplace(move(seg));
}
