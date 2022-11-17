#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg)
{
    if (seg.header().syn)
    {
        _syn = true;
        _isn = seg.header().seqno;
    }
    if (!_syn)
        return;

    size_t bw = _reassembler.stream_out().bytes_written();
    uint64_t abs_seqno = unwrap(seg.header().seqno, _isn, bw);
    if (seg.header().syn)
        abs_seqno = 1;

    _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const
{
    if (!_syn)
        return nullopt;
    size_t abs_ackno = _reassembler.stream_out().bytes_written() + 1;
    if (_reassembler.stream_out().input_ended())
        abs_ackno++;
    return WrappingInt32(wrap(abs_ackno, _isn));
}

size_t TCPReceiver::window_size() const
{
    return _capacity - _reassembler.stream_out().buffer_size();
}
