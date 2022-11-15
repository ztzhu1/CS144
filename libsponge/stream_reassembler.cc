#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

#define ASSERT(cond)                                            \
    do {                                                        \
        if (!(cond)) {                                            \
            printf("\033[1;31m");                               \
            printf("Assert: %s(%d): ", __FILE__, __LINE__);     \
            printf("ASSERT(%s) failed!\n", #cond);              \
            printf("Aborted.\n");                               \
            printf("\033[0m");                                  \
            exit(1);                                            \
        }                                                       \
    } while(0)

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    
    // input is ended if
    // 1. received eof.
    // 2. has written all the data
    //    into the stream (they may
    //    be not read yet).
    if (_output.input_ended()) return;

    size_t end = index + data.size();

    if (eof)
    {
        _eof = true;
        _eof_index = end;
    }

    size_t uasm = _first_unasm_idx();
    size_t uacc = _first_unacc_idx();

    if (end <= uasm || index >= uacc)
    {
        if (_eof && _eof_index == _output.bytes_written())
            _output.end_input();

        return;
    }
    else if (index <= uasm)
        _write_output(_writable_data(index, data));
    else
        _insert_entry(index, data);
}

void StreamReassembler::_insert_entry(size_t index, const string &data)
{
    size_t end = min(index + data.size(), _first_unacc_idx());
    size_t size = end - index;
    for (size_t i = 0; i < size; i++)
    {
        if (_unasm_str.count(index + i) == 0)
            _unasm_str.insert(pair<size_t, char>(index + i, data[i]));
    }
}

void StreamReassembler::_write_output(const string &data)
{
    size_t index = _first_unasm_idx();
    size_t end = index + data.size();
    ASSERT(end <= _first_unacc_idx());

    _output.write(data);

    map<size_t, char>::iterator it;
    while ((it = _unasm_str.begin()) != _unasm_str.end())
    {
        index = _first_unasm_idx();
        if (it->first < index)
        {
            _unasm_str.erase(it);
        }
        else if (it->first == index)
        {
            _output.write(it->second);
            _unasm_str.erase(it);
        }
        else
        {
            break;
        }
    }

    if (_eof && _eof_index == _output.bytes_written())
        _output.end_input();
}

inline size_t StreamReassembler::_first_unread_idx() const
{
    return _output.bytes_read();
}

inline size_t StreamReassembler::_first_unasm_idx() const
{
    return _output.bytes_written();
}

inline size_t StreamReassembler::_first_unacc_idx() const
{
    return _first_unread_idx() + _capacity;
}

string StreamReassembler::_writable_data(size_t index, const string &data)
{
    size_t uasm = _first_unasm_idx();
    ASSERT(index <= uasm && index + data.size() > uasm);
    size_t begin = uasm - index;
    size_t length = _first_unacc_idx() - uasm;
    return data.substr(begin, length);
}

size_t StreamReassembler::unassembled_bytes() const { return _unasm_str.size(); }

bool StreamReassembler::empty() const { return _unasm_str.size() == 0; }
