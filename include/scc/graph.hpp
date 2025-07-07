#pragma once
#include <set>
#include <ygm/container/map.hpp>
#include <ygm/comm.hpp>
#include <ygm/io/line_parser.hpp>
#include <sstream>
#include <iostream>

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

inline void create_vertex_map(ygm::comm &world, const std::string& edgelist_file, ygm::container::map<int, VertexInfo>& vertex_map) {
    if (world.rank0()) {
        std::cout << "Reading edges from " << edgelist_file << " using parallel I/O" << std::endl;
    }
    ygm::io::line_parser lp(world, {edgelist_file});
    lp.for_all([&vertex_map](const std::string& line) {
        if (line.empty() || line[0] == '#') {
            return;
        }
        std::istringstream iss(line);
        int src, dst;
        if (iss >> src >> dst) {
            auto process_edge = [src, dst](auto pmap, const int& vertex, VertexInfo& info) {
                if (vertex == src) {
                    info.forward_edges.insert(dst);
                }
                if (vertex == dst) {
                    info.backward_edges.insert(src);
                }
            };
            vertex_map.async_insert(src, VertexInfo{src});
            vertex_map.async_insert(dst, VertexInfo{dst});
            vertex_map.async_visit(src, process_edge);
            vertex_map.async_visit(dst, process_edge);
        }
    });
}

} // namespace scc 