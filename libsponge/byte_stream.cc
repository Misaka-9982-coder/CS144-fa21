#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
ByteStream::ByteStream(const size_t capacity)
    : _buffer()
    , _size_capacity(capacity)
    , _bytes_written(0)
    , _bytes_read(0)
    , _input_ended(false)
    , _error(false) {}

size_t ByteStream::write(const string &data) {
    if (_input_ended) {
        return 0;
    }

    // The size of the room for ByteStream can wirte for the bytes
    size_t write_size = min(data.size(), _size_capacity - _buffer.size());
    _bytes_written += write_size;

    for (size_t i = 0; i < write_size; i ++ ) {
        _buffer.push_back(data[i]);
    }

    return write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    return string(_buffer.begin(), _buffer.begin() + len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    _bytes_read += len;
    for (size_t i = 0; i < len; i ++ ) {
        _buffer.pop_front();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string data = this -> peek_output(len);
    this -> pop_output(len);
    return data;
}

void ByteStream::end_input() { _input_ended = true; }

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.empty(); }

bool ByteStream::eof() const { return _input_ended && _buffer.empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _size_capacity - _buffer.size(); }