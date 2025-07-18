#include <iostream>
#include <ygm/comm.hpp>
#include <ygm/container/map.hpp>
#include <ygm/container/bag.hpp>
#include <ygm/io/line_parser.hpp>
#include <ygm/detail/collective.hpp>
#include <vector>
#include <cereal/types/vector.hpp>
#include <cereal/types/set.hpp>
#include <fstream>
#include <string>
#include <set>
#include <sstream>
#include <map>

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

// Function to read edgelist file and create the vertex map using parallel I/O
ygm::container::map<int, VertexInfo> create_vertex_map(ygm::comm &world, const std::string& edgelist_file) {
    ygm::container::map<int, VertexInfo> vertex_map(world);
    
    if (world.rank0()) {
        std::cout << "Reading edges from " << edgelist_file << " using parallel I/O" << std::endl;
    }

    // Create line parser for the edge list file
    ygm::io::line_parser lp(world, {edgelist_file});
    
    // Single pass: process edges and build adjacency lists
    lp.for_all([&vertex_map](const std::string& line) {
        // Skip comment lines and empty lines
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
    
    return vertex_map;
}

// SCC algorithm using YGM
ygm::container::map<int, VertexInfo> ecl_scc_ygm(ygm::comm &world, const std::string& edgelist_file)
{
    // Create the vertex map from the edgelist file
    auto vertex_map = create_vertex_map(world, edgelist_file);
    static auto p_vertex_map = &vertex_map;

    bool global_converged = false;

    while (!global_converged) {

        // Initialize vertex signatures (vin and vout)
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

        // Propagate values
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

        // First pass: collect edges to remove
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

        // Second pass: remove the collected edges
        p_bag->for_all([](const std::pair<int,int>& edge) {
            const auto& [from, to] = edge;
            p_vertex_map->async_visit(from, remove_forward_edge(), to);
            p_vertex_map->async_visit(to, remove_backward_edge(), from);
        });

        // check if for every vertex, vin == vout
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

    return vertex_map;
}

// Print vertex information
void print_results(ygm::container::map<int, VertexInfo>& vertex_map, ygm::comm& world) {
    for (int i = 0; i < world.size(); i++) {
        if (i == world.rank()) {
            vertex_map.local_for_all([](const int &vertex, const VertexInfo &info) {
                std::cout << "Vertex " << vertex << ": vin=" << info.vin << ", vout=" << info.vout << std::endl;
            });
        }
        world.barrier();
    }
}

// Count strongly connected components
int count_sccs(ygm::container::map<int, VertexInfo>& vertex_map, ygm::comm& world) {
    int local_count = 0;
    vertex_map.local_for_all([&local_count](const int &vertex, const VertexInfo &info) {
        if (info.vin == vertex && info.vout == vertex) {
            local_count++;
        }
    });
    
    // Sum up counts from all processes
    return ygm::sum(local_count, world);
}

// Count size of largest SCC
int count_largest_scc(ygm::container::map<int, VertexInfo>& vertex_map, ygm::comm& world) {
    // Create a distributed map to count vertices in each SCC
    ygm::container::map<int, int> scc_sizes(world);
    
    // First, count vertices in each SCC
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

int main(int argc, char **argv)
{
    ygm::comm world(&argc, &argv);

    if (argc != 2) {
        if (world.rank0()) {
            std::cerr << "Usage: " << argv[0] << " <edgelist_file>" << std::endl;
        }
        return 1;
    }

    std::string edgelist_file = argv[1];

    // Run the SCC algorithm
    auto result = ecl_scc_ygm(world, edgelist_file);

    // Count SCCs
    int num_sccs = count_sccs(result, world);
    if (world.rank0()) {
        std::cout << "\nNumber of Strongly Connected Components: " << num_sccs << std::endl;
    }

    // Count size of largest SCC
    int largest_scc_size = count_largest_scc(result, world);
    if (world.rank0()) {
        std::cout << "Size of largest Strongly Connected Component: " << largest_scc_size << std::endl;
    }

    // Print detailed results
    // print_results(result, world);

    return 0;
}