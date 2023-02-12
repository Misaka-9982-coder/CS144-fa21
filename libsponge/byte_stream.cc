#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t bytes = data.size();
    bytes = min(bytes, remaining_capacity());
    _bytes_written += bytes;

    if (bytes == data.size()) {
        _buffer.emplace_back(move(Buffer(move(string(data)))));
    } else {
        _buffer.emplace_back(move(data.substr(0, bytes)));
    }

    return bytes;
}

size_t ByteStream::write(string &&data) {
    size_t bytes = data.size();
    bytes = min(bytes, remaining_capacity());
    _bytes_written += bytes;

    if (bytes == data.size()) {
        _buffer.emplace_back(move(Buffer(move(data))));
    } else {
        _buffer.emplace_back(move(data.substr(0, bytes)));
    }

    return bytes;
}

size_t ByteStream::write(BufferPlus& data) {
    size_t bytes = data.size();
    bytes = min(bytes, remaining_capacity());
    _bytes_written += bytes;

    if (bytes != data.size()) {
        data.remove_suffix(data.size() - bytes);
    }

    if (data.size()) {
        _buffer.emplace_back(move(data));
    }

    return bytes;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t bytes = min(len, buffer_size());
    string res;
    res.reserve(bytes);

    for (const auto &buffer : _buffer) {
        if (bytes >= buffer.size()) {
            res.append(buffer);
            bytes -= buffer.size();
            if (bytes == 0) {
                break;
            }
        } else {
            BufferPlus tmp(buffer);
            tmp.remove_suffix(buffer.size() - bytes);
            res.append(tmp);
            break;
        }
    }

    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    size_t bytes = min(len, buffer_size());
    _bytes_read += bytes;

    while (bytes > 0) {
        if (bytes > _buffer.front().size()) {
            bytes -= _buffer.front().size();
            _buffer.pop_front();
        } else {
            _buffer.front().remove_prefix(bytes);
            break;
        }
    }

    return;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() { _input_ended = true; }

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return _bytes_written - _bytes_read; }

bool ByteStream::buffer_empty() const { return _bytes_written - _bytes_read == 0; }

bool ByteStream::eof() const { return _input_ended && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
