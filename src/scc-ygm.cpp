#include <iostream>
#include <ygm/comm.hpp>
#include <ygm/container/map.hpp>
#include <vector>
#include <cereal/types/vector.hpp>
#include <fstream>
#include <string>
#include <set>

struct VertexInfo {
    std::vector<int> forward_edges;  
    std::vector<int> backward_edges; 
    int vin;                         
    int vout;

    template<class Archive>
    void serialize(Archive & ar) {
        ar(forward_edges, backward_edges, vin, vout);
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
        while (file >> src >> dst) {
            auto update_edges = [](auto pmap, const int &vertex, VertexInfo &info, int src, int dst) {
                if (vertex == src) {
                    info.forward_edges.push_back(dst);
                }
                if (vertex == dst) {
                    info.backward_edges.push_back(src);
                }
            };
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

    // bool converged = false;
    // while (!converged) {
    //     // Process forward edges to update vout
    //     vertex_map.for_all([&vertex_map](const int &vertex, VertexInfo &info) {
    //         for (int neighbor : info.forward_edges) {
    //             auto update_vout = [](auto pmap, const int &v, VertexInfo &vinfo, int new_vout) {
    //                 if (vinfo.vout < new_vout) {
    //                     vinfo.vout = new_vout;
    //                 }
    //             };
    //             vertex_map.async_visit(neighbor, update_vout, info.vout);
    //         }
    //     });

    //     // Process backward edges to update vin
    //     vertex_map.for_all([&vertex_map](const int &vertex, VertexInfo &info) {
    //         for (int neighbor : info.backward_edges) {
    //             auto update_vin = [](auto pmap, const int &v, VertexInfo &vinfo, int new_vin) {
    //                 if (vinfo.vin < new_vin) {
    //                     vinfo.vin = new_vin;
    //                 }
    //             };
    //             vertex_map.async_visit(neighbor, update_vin, info.vin);
    //         }
    //     });

    //     world.barrier();

    //     // Check for convergence
    //     bool local_converged = true;
    //     vertex_map.local_for_all([&local_converged](const int &vertex, VertexInfo &info) {
    //         if (info.vin != info.vout) {
    //             local_converged = false;
    //         }
    //     });

    //     converged = world.all_reduce_min(local_converged);
    // }

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

    // Print results
    for (int i = 0; i < world.size(); i++) {
        if (i == world.rank()) {
            std::cout << "Rank " << i << " results:" << std::endl;
            result.local_for_all([](const int &vertex, const VertexInfo &info) {
                std::cout << "Vertex " << vertex << ": vin=" << info.vin << ", vout=" << info.vout << std::endl;
            });
            std::cout << std::endl;
        }
        world.barrier();
    }

    return 0;
}
