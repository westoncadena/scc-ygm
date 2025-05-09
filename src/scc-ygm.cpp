#include <iostream>
#include <ygm/comm.hpp>
#include <ygm/container/map.hpp>
#include <vector>
#include <cereal/types/vector.hpp>
#include <cereal/types/set.hpp>
#include <fstream>
#include <string>
#include <set>

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
    ygm::container::map<int, VertexInfo>* pmap;

    propagate_vin(ygm::container::map<int, VertexInfo>* map) : pmap(map) {}

    void operator()(const int &key, VertexInfo &value, int new_vin, int depth = 0) {
        if (value.vin < new_vin) {
            value.vin = new_vin;
            for (int neighbor : value.forward_edges) {
                pmap->async_visit(neighbor, propagate_vin(pmap), new_vin, depth + 1);
            }
        }
    }
};

struct propagate_vout {
    ygm::container::map<int, VertexInfo>* pmap;

    propagate_vout(ygm::container::map<int, VertexInfo>* map) : pmap(map) {}

    void operator()(const int &key, VertexInfo &value, int new_vout, int depth = 0) {
        if (value.vout < new_vout) {
            value.vout = new_vout;
            for (int neighbor : value.backward_edges) {
                pmap->async_visit(neighbor, propagate_vout(pmap), new_vout, depth + 1);
            }
        }
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
        int src, dst;
        while (file >> src >> dst) {
            vertices.insert(src);
            vertices.insert(dst);
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
        while (file >> src >> dst) {
            vertex_map.async_visit(src, update_edges, src, dst);
            vertex_map.async_visit(dst, update_edges, src, dst);
        }
        file.close();
    }

    world.barrier();
    return vertex_map;
}

ygm::container::map<int, VertexInfo> ecl_scc_ygm(ygm::comm &world, const std::string& edgelist_file)
{
    // Create the vertex map from the edgelist file
    auto vertex_map = create_vertex_map(world, edgelist_file);

    // Initialize vertex signatures (vin and vout)
    vertex_map.for_all([](const int &vertex, VertexInfo &info) {
        info.vin = vertex;
        info.vout = vertex;
    });

    // Propagate values
    vertex_map.for_all([&vertex_map](const int &vertex, VertexInfo &info) {
        for (int neighbor : info.forward_edges) {
            vertex_map.async_visit(neighbor, propagate_vin(&vertex_map), info.vin, 0);
        }
        for (int neighbor : info.backward_edges) {
            vertex_map.async_visit(neighbor, propagate_vout(&vertex_map), info.vout, 0);
        }
    });

    world.barrier();
    return vertex_map;
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

    // Print final results
    if (world.rank0()) {
        std::cout << "\nFinal results:" << std::endl;
    }
    
    for (int i = 0; i < world.size(); i++) {
        if (i == world.rank()) {
            result.local_for_all([](const int &vertex, const VertexInfo &info) {
                std::cout << "Vertex " << vertex << ": vin=" << info.vin << ", vout=" << info.vout << std::endl;
            });
        }
        world.barrier();
    }

    return 0;
}
