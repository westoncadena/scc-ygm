#pragma once
#include <ygm/container/bag.hpp>
#include <ygm/container/map.hpp>
#include <ygm/detail/collective.hpp>
#include <algorithm>
#include <iostream>
#include "scc/graph.hpp"

namespace scc {

inline void ecl_scc_ygm(ygm::comm &world, const std::string& edgelist_file, ygm::container::map<int, VertexInfo>& vertex_map)
{
    scc::create_vertex_map(world, edgelist_file, vertex_map);
    static auto p_vertex_map = &vertex_map;
    bool global_converged = false;
    while (!global_converged) {
        p_vertex_map->for_all([](const int &vertex, VertexInfo &info) {
            info.vin = vertex;
            info.vout = vertex;
        });
        struct propagate_vin {
            void operator()(const int &key, VertexInfo &value, int new_vin){
                if (value.vin < new_vin){
                    value.vin = new_vin;
                    for (int neighbor : value.forward_edges) {
                        p_vertex_map->async_visit(neighbor, propagate_vin(), new_vin);
                    }
                }
            }
        };
        struct propagate_vout {
            void operator()(const int &key, VertexInfo &value, int new_vout){
                if (value.vout < new_vout){
                    value.vout = new_vout;
                    for (int neighbor : value.backward_edges) {
                        p_vertex_map->async_visit(neighbor, propagate_vout(), new_vout);
                    }
                }
            }
        };
        p_vertex_map->for_all([](const int &vertex, VertexInfo &info) {
            if(vertex == info.vin) {
                for (int neighbor : info.forward_edges) {
                    p_vertex_map->async_visit(neighbor, propagate_vin(), info.vin);
                }
            }
            if(vertex == info.vout) {
                for (int neighbor : info.backward_edges) {
                    p_vertex_map->async_visit(neighbor, propagate_vout(), info.vout);
                }
            }
        });
        if (world.rank0()) {
            std::cout << "\nRemoving edges" << std::endl;
        }
        auto bag = ygm::container::bag<std::pair<int,int>>(world);
        static auto p_bag = &bag;
        struct collect_edges_to_remove {
            void operator()(const int &key, VertexInfo &value, int vertex, int vin, int vout){
                if(vin != value.vin || vout != value.vout) {
                    p_bag->async_insert({vertex, key});
                }
            }
        };
        p_vertex_map->for_all([](const int &vertex, VertexInfo &info) {
            for (int neighbor : info.forward_edges) {
                p_vertex_map->async_visit(neighbor, collect_edges_to_remove(), vertex, info.vin, info.vout);
            }
        });
        struct remove_forward_edge {
            void operator()(const int &key, VertexInfo &value, int to){
                value.forward_edges.erase(to);
            }
        };
        struct remove_backward_edge {
            void operator()(const int &key, VertexInfo &value, int from){
                value.backward_edges.erase(from);
            }
        };
        p_bag->for_all([](const std::pair<int,int>& edge) {
            const auto& [from, to] = edge;
            p_vertex_map->async_visit(from, remove_forward_edge(), to);
            p_vertex_map->async_visit(to, remove_backward_edge(), from);
        });
        bool local_converged = true;
        p_vertex_map->local_for_all([&local_converged](const int &vertex, const VertexInfo &info) {
            if (info.vin != info.vout) {
                local_converged = false;
            }
        });
        global_converged = ygm::min(local_converged, world);
        if (world.rank0()) {
            if (global_converged) {
                std::cout << "\nAlgorithm has converged - all vertices have matching vin and vout values" << std::endl;
            } else {
                std::cout << "\nAlgorithm has not converged - some vertices have different vin and vout values" << std::endl;
            }
        }
    }
}

inline void print_results(ygm::container::map<int, VertexInfo>& vertex_map, ygm::comm& world) {
    for (int i = 0; i < world.size(); i++) {
        if (i == world.rank()) {
            vertex_map.local_for_all([](const int &vertex, const VertexInfo &info) {
                std::cout << "Vertex " << vertex << ": vin=" << info.vin << ", vout=" << info.vout << std::endl;
            });
        }
        world.barrier();
    }
}

inline int count_sccs(ygm::container::map<int, VertexInfo>& vertex_map, ygm::comm& world) {
    int local_count = 0;
    vertex_map.local_for_all([&local_count](const int &vertex, const VertexInfo &info) {
        if (info.vin == vertex && info.vout == vertex) {
            local_count++;
        }
    });
    return ygm::sum(local_count, world);
}

inline int count_largest_scc(ygm::container::map<int, VertexInfo>& vertex_map, ygm::comm& world) {
    ygm::container::map<int, int> scc_sizes(world);
    vertex_map.for_all([&scc_sizes](const int &vertex, const VertexInfo &info) {
        scc_sizes.async_visit(info.vin, [](auto pmap, const int &scc_id, int &count) {
            count++;
        });
    });
    int local_max = 0;
    scc_sizes.for_all([&local_max](const int &scc_id, const int &size) {
        local_max = std::max(local_max, size);
    });
    return ygm::max(local_max, world);
}

} // namespace scc 