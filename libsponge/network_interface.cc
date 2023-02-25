#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

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
    
    // if destination Ehernet address known
    if (_arp_table.count(next_hop_ip)) {
        EthernetFrame to_send;
        to_send.payload() = dgram.serialize();
        to_send.header().dst = _arp_table[next_hop_ip].first;
        to_send.header().src = _ethernet_address;
        to_send.header().type = EthernetHeader::TYPE_IPv4;
        _frames_out.emplace(to_send);
    } 
    // if destination Ethernet address unkown
    else {
        // no arp sent yet
        if (!get<bool>(_arp_retransmission_timer)) {
            send_arp_request(next_hop_ip);
        }
        //  queue the IP datagram
        _dgrames_queue.push({dgram, next_hop});
    }
    
    resend();
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return {};
    }

    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ret;
        if (ret.parse(Buffer(frame.payload())) == ParseResult::NoError) {
            return ret;
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        // parse
        ARPMessage arp_packet;
        if (arp_packet.parse(Buffer(frame.payload())) == ParseResult::NoError) {
            // record the sender's info
            _arp_table[arp_packet.sender_ip_address] = {arp_packet.sender_ethernet_address, _curr_time + 30 * 1000};
            // turn off timer
            if (get<bool>(_arp_retransmission_timer) && get<uint32_t>(_arp_retransmission_timer) == arp_packet.sender_ip_address) {
                _arp_retransmission_timer = make_tuple(false, 0, 0);
            }

            _arp_failure_time.push({_curr_time + 30 * 1000, arp_packet.sender_ip_address});

            // send reply
            if (arp_packet.target_ip_address == _ip_address.ipv4_numeric() && arp_packet.opcode == ARPMessage::OPCODE_REQUEST) {
                EthernetFrame arp_to_send;
                // header
                arp_to_send.header().dst = arp_packet.sender_ethernet_address;
                arp_to_send.header().src = _ethernet_address;
                arp_to_send.header().type = EthernetHeader::TYPE_ARP;
                // payload
                ARPMessage arp_reply;
                arp_reply.opcode = ARPMessage::OPCODE_REPLY;
                arp_reply.sender_ethernet_address = _ethernet_address;
                arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
                arp_reply.target_ethernet_address = arp_packet.sender_ethernet_address;
                arp_reply.target_ip_address = arp_packet.sender_ip_address;
                arp_to_send.payload() = BufferList(move(arp_reply.serialize()));
                // send reply
                // cerr<< "send reply" << arp_reply.to_string() <<endl;
                _frames_out.emplace(arp_to_send);
            }
        }

        resend();
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const time_t ms_since_last_tick) {
    _curr_time += ms_since_last_tick;

    // Expire any IP-to-Ethernet mappings that have expired.
    while (!_arp_failure_time.empty()) {
        auto arp_entry = _arp_failure_time.top();
        if (arp_entry.first <= _curr_time) {
            _arp_failure_time.pop();
            
            if (_arp_table[arp_entry.second].second <= _curr_time) {
                _arp_table.erase(arp_entry.second);
            }
        } else {
            break;
        }
    }

    // Resend arp if no response
    if (get<bool>(_arp_retransmission_timer) && _curr_time - get<time_t>(_arp_retransmission_timer) > 5 * 1000) {
        auto ip_to_find = get<uint32_t>(_arp_retransmission_timer);
        send_arp_request(ip_to_find);
    }
}

void NetworkInterface::send_arp_request(const uint32_t ip_to_find) {
    EthernetFrame arp_to_send;
    // header
    arp_to_send.header().dst = ETHERNET_BROADCAST;
    arp_to_send.header().src = _ethernet_address;
    arp_to_send.header().type = EthernetHeader::TYPE_ARP;
    // payload
    ARPMessage arp_packet;
    arp_packet.opcode = ARPMessage::OPCODE_REQUEST;
    arp_packet.sender_ethernet_address = _ethernet_address;
    arp_packet.sender_ip_address = _ip_address.ipv4_numeric();
    // arp_packet.target_ethernet_address =
    arp_packet.target_ip_address = ip_to_find;
    arp_to_send.payload() = BufferList(move(arp_packet.serialize()));

    // send & reset timer
    _frames_out.emplace(arp_to_send);
    _arp_retransmission_timer = make_tuple(true, _curr_time, ip_to_find);
}

void NetworkInterface::resend() {
    while (!_dgrames_queue.empty()) {
        auto dgrams = _dgrames_queue.front();
        const uint32_t next_hop_ip = dgrams.second.ipv4_numeric();
        // if destination Ehernet address known
        if (_arp_table.count(next_hop_ip)) {
            _dgrames_queue.pop();
            send_datagram(dgrams.first, dgrams.second);
        } else {
            break;
        }
    }
}