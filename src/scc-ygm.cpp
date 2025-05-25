#include <iostream>
#include <ygm/comm.hpp>
#include <ygm/container/map.hpp>
#include <ygm/container/bag.hpp>
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

    template<class Archive>
    void serialize(Archive & ar) {
        ar(forward_edges, backward_edges, vin, vout);
    }
};

struct propagate_vin {
    template<typename Map>
    void operator()(ygm::ygm_ptr<Map> pmap, const int &key, VertexInfo &value, int new_vin, int depth){
        if (value.vin < new_vin){
            value.vin = new_vin;
            for (int neighbor : value.forward_edges) {
                pmap->async_visit(neighbor, propagate_vin(), new_vin, depth + 1);
            }
        }
    }
};

struct propagate_vout {
    template<typename Map>
    void operator()(ygm::ygm_ptr<Map> pmap, const int &key, VertexInfo &value, int new_vout, int depth){
        if (value.vout < new_vout){
            value.vout = new_vout;
            for (int neighbor : value.backward_edges) {
                pmap->async_visit(neighbor, propagate_vout(), new_vout, depth + 1);
            }
        }
    }
};

struct remove_forward_edges {
    template<typename Map>
    void operator()(ygm::ygm_ptr<Map> pmap, const int &key, VertexInfo &value, int vertex){
        size_t removed = value.forward_edges.erase(vertex);
        // if (removed > 0) {
        //     std::cout << "Successfully removed forward edge " << key << " -> " << vertex << std::endl;
        // } else {
        //     std::cout << "Failed to remove forward edge " << key << " -> " << vertex << " (edge not found)" << std::endl;
        // }
    }
};

struct collect_edges_to_remove {
    template<typename Map>
    void operator()(ygm::ygm_ptr<Map> pmap, const int &key, VertexInfo &value, int vertex, int vin, int vout, ygm::ygm_ptr<ygm::container::bag<std::pair<int,int>>> pbag){
        if(vin != value.vin || vout != value.vout) {
            pbag->async_insert({vertex, key});
        }
    }
};

struct remove_forward_edge {
    template<typename Map>
    void operator()(ygm::ygm_ptr<Map> pmap, const int &key, VertexInfo &value, int to){
        value.forward_edges.erase(to);
    }
};

struct remove_backward_edge {
    template<typename Map>
    void operator()(ygm::ygm_ptr<Map> pmap, const int &key, VertexInfo &value, int from){
        value.backward_edges.erase(from);
    }
};

// Function to read edgelist file and create the vertex map
ygm::container::map<int, VertexInfo> create_vertex_map(ygm::comm &world, const std::string& edgelist_file) {
    ygm::container::map<int, VertexInfo> vertex_map(world);
    std::set<int> vertices;

    if (world.rank0()) {
        std::cout << "Reading edges from " << edgelist_file << std::endl;
        std::ifstream file(edgelist_file);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << edgelist_file << std::endl;
            return vertex_map;
        }

        // First pass: collect all vertices
        std::string line;
        int src, dst;
        while (std::getline(file, line)) {
            // Skip comment lines
            if (line.empty() || line[0] == '#') {
                continue;
            }
            std::istringstream iss(line);
            if (iss >> src >> dst) {
                vertices.insert(src);
                vertices.insert(dst);
            }
        }
        file.clear();
        file.seekg(0);

        // Initialize vertex information
        for (int v : vertices) {
            VertexInfo info;
            info.vin = v;
            info.vout = v;
            vertex_map.async_insert(v, info);
        }

        // Second pass: process edges
        auto update_edges = [](auto pmap, const int &vertex, VertexInfo &info, int src, int dst) {
            if (vertex == src) {
                info.forward_edges.insert(dst);
            }
            if (vertex == dst) {
                info.backward_edges.insert(src);
            }
        };

        while (std::getline(file, line)) {
            // Skip comment lines
            if (line.empty() || line[0] == '#') {
                continue;
            }
            std::istringstream iss(line);
            if (iss >> src >> dst) {
                vertex_map.async_visit(src, update_edges, src, dst);
                vertex_map.async_visit(dst, update_edges, src, dst);
            }
        }
        file.close();
    }

    world.barrier();
    return vertex_map;
}

// Function to print edges
void print_edges(ygm::container::map<int, VertexInfo>& vertex_map, ygm::comm& world) {
    for (int i = 0; i < world.size(); i++) {
        if (i == world.rank()) {
            vertex_map.local_for_all([](const int &vertex, const VertexInfo &info) {
                std::cout << "Vertex " << vertex << " edges:" << std::endl;
                std::cout << "  Forward edges: ";
                for (int edge : info.forward_edges) {
                    std::cout << edge << " ";
                }
                std::cout << std::endl;
                std::cout << "  Backward edges: ";
                for (int edge : info.backward_edges) {
                    std::cout << edge << " ";
                }
                std::cout << std::endl;
            });
        }
        world.barrier();
    }
}

ygm::container::map<int, VertexInfo> ecl_scc_ygm(ygm::comm &world, const std::string& edgelist_file)
{
    // Create the vertex map from the edgelist file
    auto vertex_map = create_vertex_map(world, edgelist_file);

    bool global_converged = false;

    while (!global_converged) {

        // Initialize vertex signatures (vin and vout)
        vertex_map.for_all([](const int &vertex, VertexInfo &info) {
            info.vin = vertex;
            info.vout = vertex;
        });

        // Propagate values
        vertex_map.for_all([&vertex_map](const int &vertex, VertexInfo &info) {
            if(vertex == info.vin) {
                for (int neighbor : info.forward_edges) {
                    vertex_map.async_visit(neighbor, propagate_vin(), info.vin, 0);
                }
            }
            if(vertex == info.vout) {
                for (int neighbor : info.backward_edges) {
                    vertex_map.async_visit(neighbor, propagate_vout(), info.vout, 0);
                }
            }
        });

        world.barrier();

        if (world.rank0()) {
            std::cout << "\nRemoving edges" << std::endl;
        }

        // First pass: collect edges to remove
        auto bag = ygm::container::bag<std::pair<int,int>>(world);
        auto pbag = world.make_ygm_ptr(bag);
        vertex_map.for_all([&vertex_map, pbag](const int &vertex, VertexInfo &info) {
            for (int neighbor : info.forward_edges) {
                vertex_map.async_visit(neighbor, collect_edges_to_remove(), vertex, info.vin, info.vout, pbag);
            }
        });

        world.barrier();

        // Second pass: remove the collected edges
        pbag->for_all([&vertex_map](const std::pair<int,int>& edge) {
            const auto& [from, to] = edge;
            vertex_map.async_visit(from, remove_forward_edge(), to);
            vertex_map.async_visit(to, remove_backward_edge(), from);
        });

        world.barrier();

        // check if for every vertex, vin == vout
        bool local_converged = true;
        vertex_map.local_for_all([&local_converged](const int &vertex, const VertexInfo &info) {
            if (info.vin != info.vout) {
                local_converged = false;
            }
        });

        
        global_converged = world.all_reduce_min(local_converged);
        
        if (world.rank0()) {
            if (global_converged) {
                std::cout << "\nAlgorithm has converged - all vertices have matching vin and vout values" << std::endl;
            } else {
                std::cout << "\nAlgorithm has not converged - some vertices have different vin and vout values" << std::endl;
            }
        }

        world.barrier();
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
    return world.all_reduce_sum(local_count);
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
