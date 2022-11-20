#include "network_interface.hh"


#include <iostream>
#include <cassert>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto it = _ip_eth_map.find(next_hop_ip);
    if (it != _ip_eth_map.end())
    // found
    {
        EthernetFrame ef;
        ef.header().type = EthernetHeader::TYPE_IPv4;
        ef.header().src  = _ethernet_address;
        ef.header().dst  = it->second.first;
        ef.payload() = dgram.serialize();
        _frames_out.push(ef);
    }
    else
    // not found
    {
        // if hasn't sent arp request
        if (_wait_for_eth.count(next_hop_ip) == 0)
        {
            _send_arp_req(next_hop_ip);
        }
        _wait_for_sending.push_back(make_pair(next_hop, dgram));
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address &&
        frame.header().dst != ETHERNET_BROADCAST)
        return nullopt;

    // IP
    if (frame.header().type == EthernetHeader::TYPE_IPv4)
    {
        InternetDatagram datagram;
        if (datagram.parse(frame.payload()) == ParseResult::NoError)
            return datagram;
    }
    // ARP
    else
    {
        assert(frame.header().type == EthernetHeader::TYPE_ARP);

        ARPMessage msg;
        if (msg.parse(frame.payload()) == ParseResult::NoError)
        {
            // request
            if ( msg.target_ip_address == _ip_address.ipv4_numeric() &&
                msg.opcode == ARPMessage::OPCODE_REQUEST)
            {
                _send_arp_reply_and_update_map(msg);
            }
            // reply
            else
            {
                _update_map(msg.sender_ip_address, msg.sender_ethernet_address);
            }
        }
    }

    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick)
{
    for (auto it = _ip_eth_map.begin(); it != _ip_eth_map.end(); )
    {
        if (it->second.second <= ms_since_last_tick)
            it = _ip_eth_map.erase(it);
        else
        {
            it->second.second -= ms_since_last_tick;
            it++;
        }
    }

    for (auto it = _wait_for_eth.begin(); it != _wait_for_eth.end(); it++)
    {
        if (it->second <= ms_since_last_tick)
        {
            _send_arp_req(it->first);
        }
        else
        {
            it->second -= ms_since_last_tick;
        }
    }
}

void NetworkInterface::_send_arp_req(uint32_t next_hop_ip)
{
    ARPMessage arp_msg;
    arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
    arp_msg.sender_ethernet_address = _ethernet_address;
    arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
    arp_msg.target_ethernet_address = {};
    arp_msg.target_ip_address = next_hop_ip;

    EthernetFrame ef;
    ef.header().type = EthernetHeader::TYPE_ARP;
    ef.header().src  = _ethernet_address;
    ef.header().dst  = ETHERNET_BROADCAST;
    ef.payload() = arp_msg.serialize();

    _wait_for_eth[next_hop_ip] = _eth_req_interval;

    _frames_out.push(ef);
}

void NetworkInterface::_send_arp_reply_and_update_map(ARPMessage &req_msg)
{
    ARPMessage reply_msg;
    reply_msg.opcode = ARPMessage::OPCODE_REPLY;
    reply_msg.sender_ethernet_address = _ethernet_address;
    reply_msg.sender_ip_address = _ip_address.ipv4_numeric();
    reply_msg.target_ethernet_address = req_msg.sender_ethernet_address;
    reply_msg.target_ip_address = req_msg.sender_ip_address;

    EthernetFrame ef;
    ef.header().type = EthernetHeader::TYPE_ARP;
    ef.header().src  = _ethernet_address;
    ef.header().dst  = req_msg.sender_ethernet_address;
    ef.payload() = reply_msg.serialize();

    _frames_out.push(ef);

    _update_map(req_msg.sender_ip_address, req_msg.sender_ethernet_address);
}

void NetworkInterface::_update_map(uint32_t ip, EthernetAddress eth)
{
    _ip_eth_map[ip] = make_pair(eth, _mapping_ttl);

    _wait_for_eth.erase(ip);

    for (auto it = _wait_for_sending.begin(); it != _wait_for_sending.end(); )
    {
        if (it->first.ipv4_numeric() == ip)
        {
            send_datagram(it->second, it->first);
            it = _wait_for_sending.erase(it);
        }
        else
        {
            it++;
        }
    }
}