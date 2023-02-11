#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 只接受第一个 seg 的 _syn 所以要满足 !_init_seq.has_value()
    if (seg.header().syn && !_init_seq.has_value()) {
        _init_seq = seg.header().seqno;
    }

    if (_init_seq.has_value()) {
        auto data = seg.payload().copy();

        auto checkpt = _reassembler.next_ass_idx();
        auto isn = _init_seq.value();
        auto abs_seq = unwrap(seg.header().seqno, isn, checkpt);

        auto eof = seg.header().fin;

        _reassembler.push_substring(data, abs_seq, eof);
        _ackno = wrap(_reassembler.next_ass_idx(), isn);
    }

    // _fin 标志位进行迭代
    _fin = _fin ? _fin : seg.header().fin;

    // 这个判断只会进入一次，(!_syn)保证了这个判断只会执行一次
    // 这里的代码的主要目的是将 _init_seq 设置为带 Payload 的第一个 Sequence Number
    if (!_syn && seg.header().syn && _ackno.has_value()) {
        _ackno = WrappingInt32(_ackno.value().raw_value() + 1);
        _init_seq = _ackno;
        _syn = true;
    }

    // 只有建立了连接，同时 _fin 来过，而且字符流重组完成，才更新 _ackno
    if (_init_seq.has_value() && _fin && _reassembler.stream_out().input_ended()) {
        _ackno = WrappingInt32(_ackno.value().raw_value() + 1);
    }
    
    return;
}

optional<WrappingInt32> TCPReceiver::ackno() const { return _ackno; }

size_t TCPReceiver::window_size() const { return this->stream_out().remaining_capacity(); }
