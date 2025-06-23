#pragma once
#include <set>
#include <ygm/container/map.hpp>
#include <ygm/comm.hpp>

namespace scc {

struct VertexInfo {
    std::set<int> forward_edges;
    std::set<int> backward_edges;
    int vin;
    int vout;
    VertexInfo() : vin(0), vout(0) {}
    VertexInfo(int v) : vin(v), vout(v) {}
    template<class Archive>
    void serialize(Archive & ar) {
        ar(forward_edges, backward_edges, vin, vout);
    }
};

ygm::container::map<int, VertexInfo> create_vertex_map(ygm::comm &world, const std::string& edgelist_file);

} // namespace scc 