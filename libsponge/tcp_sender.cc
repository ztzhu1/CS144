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
    , _rto{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _flight_bytes; }

void TCPSender::fill_window()
{
    size_t window_size = _window_size == 0 ? 1 : _window_size;
    while (window_size > _flight_bytes)
    {
        TCPSegment seg;

        seg.header().seqno = next_seqno();

        if (!_set_syn)
        {
            seg.header().syn = true;
            _set_syn = true;
        }

        size_t payload_bytes = min(
            window_size - _flight_bytes - seg.header().syn,
            TCPConfig::MAX_PAYLOAD_SIZE);
        seg.payload() = Buffer(_stream.read(payload_bytes));
        payload_bytes = seg.payload().size();

        if (!_set_fin && _stream.eof() &&  window_size - payload_bytes - _flight_bytes - seg.header().syn > 0)
        {
            seg.header().fin = true;
            _set_fin = true;
        }

        if (seg.length_in_sequence_space() == 0)
            break;

        if (_flight_segs.empty()) {
            _retransmission_num = _initial_retransmission_timeout;
            _time = 0;
        }

        // send segment
        _segments_out.push(seg);
        // update next absolute seqno
        _next_seqno += seg.length_in_sequence_space();
        // update flighte bytes
        _flight_bytes += seg.length_in_sequence_space();
        // backup
        _flight_segs.push(make_pair(_next_seqno, seg));

        if (seg.header().fin)
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    size_t abs_ackno = unwrap(ackno, _isn, _next_seqno == 0 ? 0 : _next_seqno - 1);
    if (abs_ackno > _next_seqno)
        return;

    while (!_flight_segs.empty())
    {
        const auto &seg_pair = _flight_segs.front();

        // data received successfully
        if (seg_pair.first + seg_pair.second.length_in_sequence_space() <= abs_ackno)
        {
            _flight_bytes -= seg_pair.second.length_in_sequence_space();
            _rto = _initial_retransmission_timeout;
            _time = 0;
            _retransmission_num = 0;
            _flight_segs.pop();
        }
        else
        {
            break;
        }
    }

    _window_size = window_size;
    fill_window();

}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick)
{
    _time += ms_since_last_tick;
    if (_time >= _rto)
    {
        _segments_out.push(_flight_segs.front().second);
        if (_window_size != 0)
        {
            _retransmission_num++;
            _rto *= 2;
        }
    }
    _time = 0;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_num; }

void TCPSender::send_empty_segment()
{
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
