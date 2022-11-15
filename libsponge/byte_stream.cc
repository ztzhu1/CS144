#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity): _capacity(capacity), _buf(0)
{
    _buf.reserve(_capacity);
    _buf.resize(0);
}

size_t ByteStream::write(const string &data)
{
    if (_buf_open == false)
        return 0;

    size_t written_size = min(data.size(), remaining_capacity());
    std::vector<char>::iterator begin = _buf.begin() + _buf.size();
    _buf.resize(_buf.size() + written_size);
    std::copy(data.c_str(),
              data.c_str() + written_size,
              begin);
    _bytes_w += written_size;
    return written_size;
}

size_t ByteStream::write(const char data)
{
    if (_buf_open == false)
        return 0;

    size_t written_size = min(static_cast<size_t>(1), remaining_capacity());
    std::vector<char>::iterator begin = _buf.begin() + _buf.size();
    _buf.resize(_buf.size() + written_size);
    std::copy(&data,
              &data + written_size,
              begin);
    _bytes_w += written_size;
    return written_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const
{
    size_t out_size = min(len, _buf.size());
    string out(_buf.begin(), _buf.begin() + out_size);
    return out;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len)
{
    size_t real_len = min(len, _buf.size());
    _buf.erase(_buf.begin(), _buf.begin() + real_len);
    _bytes_r += real_len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len)
{
    size_t real_len = min(len, _buf.size());
    string out = peek_output(real_len);
    pop_output(real_len);
    return out;
}

void ByteStream::end_input() { _buf_open = false; }

bool ByteStream::input_ended() const { return _buf_open == false; }

size_t ByteStream::buffer_size() const { return _buf.size(); }

bool ByteStream::buffer_empty() const { return _buf.size() == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_w; }

size_t ByteStream::bytes_read() const { return _bytes_r; }

size_t ByteStream::remaining_capacity() const
{
    return _capacity - _buf.size();
}
