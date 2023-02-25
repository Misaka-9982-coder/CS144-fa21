#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    route_rule r(route_prefix,prefix_length,next_hop,interface_num);
    _rules.push_back(move(r));
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    uint32_t dst = dgram.header().dst;
    optional<Address> next_hop{};
    size_t interface_num{};
    optional<uint8_t> best_by_far{};

    for(auto& r: _rules) {
        if((!best_by_far.has_value() || r.prefix_length>best_by_far.value()) && prefix_equal(dst,r.route_prefix,r.prefix_length)){
            next_hop = r.next_hop;
            interface_num = r.interface_num;
            best_by_far = r.prefix_length;
        }
    }

    if(best_by_far.has_value()){
        if(dgram.header().ttl>1) {
            dgram.header().ttl--;

            if(next_hop.has_value()) {
                interface(interface_num).send_datagram(dgram,next_hop.value());
            } 
            // if not have next_hop, the next_hop is dgram's destination hop
            else {
                interface(interface_num).send_datagram(dgram,Address::from_ipv4_numeric(dst));
            }        
        }
    }
    
    return;
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}

bool Router::prefix_equal(uint32_t ip1, uint32_t ip2, uint8_t prefix_length) {
    if(prefix_length==0){
        return true;
    } else {
        return (ip1 >> (32-prefix_length)) == (ip2 >> (32-prefix_length));
    }
}