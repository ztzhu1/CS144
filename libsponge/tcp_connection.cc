#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const
{
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const
{
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const
{
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const
{
    return _ms_since_last_tick;
}

void TCPConnection::segment_received(const TCPSegment &seg)
{
    if (!_active)
        return;

    _ms_since_last_tick = 0;

    if (seg.header().rst)
    {
        _unclean_shutdown();
        return;
    }

    _receiver.segment_received(seg);
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
    {
        _linger_after_streams_finish = false;
    }

    if (seg.header().ack)
    {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (seg.length_in_sequence_space() != 0)
    {
        size_t seg_num = _sender.segments_out().size();
        _sender.fill_window();
        if (seg_num == _sender.segments_out().size())
        {
            _sender.send_empty_segment();
        }
    }
    else if (_receiver.ackno().has_value() &&
             seg.header().seqno == _receiver.ackno().value() - 1)
    {
        _sender.send_empty_segment();
    }
    _send();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (!_active)
        return 0;

    size_t data_len = _sender.stream_in().write(data);
    _sender.fill_window();
    _send();
    return data_len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick)
{

    if (!_active)
        return;

    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS)
    {
        while (!_sender.segments_out().empty())
            _sender.segments_out().pop();

        TCPSegment seg;
        seg.header().rst = true;
        _sender.segments_out().push(seg);
        _send();
        _unclean_shutdown();
        return;
    }

    _ms_since_last_tick += ms_since_last_tick;

    _send();
    ASSERT(_sender.segments_out().empty());

    if (_receiver.stream_out().input_ended() &&
        _sender.stream_in().eof() &&
        _sender.bytes_in_flight() == 0 &&
        (!_linger_after_streams_finish || _ms_since_last_tick >= 10 * _cfg.rt_timeout))
    {
        _active = false;
    }
}

void TCPConnection::end_input_stream()
{
    _sender.stream_in().end_input();
    _sender.fill_window();
    _send();
}

void TCPConnection::connect()
{
    _active = true;
    _sender.fill_window();
    _send();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _unclean_shutdown();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_send()
{
    if (!_active)
        return;

    while (!_sender.segments_out().empty())
    {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value())
        {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
}

void TCPConnection::_unclean_shutdown()
{
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}