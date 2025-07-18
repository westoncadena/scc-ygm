#include <iostream>
#include <ygm/comm.hpp>
#include <ygm/container/map.hpp>
#include <ygm/io/multi_output.hpp>
#include <ygm/detail/collective.hpp>
#include <random>
#include <set>
#include <string>
#include <sstream>

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

// Function to generate random edges for a vertex
void generate_random_edges(int vertex_id, std::set<int>& edges, int max_vertices, int avg_degree, std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, max_vertices - 1);
    std::poisson_distribution<int> degree_dist(avg_degree);
    
    int num_edges = degree_dist(rng);
    num_edges = std::min(num_edges, max_vertices / 10); 
    
    for (int i = 0; i < num_edges; ++i) {
        int target = dist(rng);
        if (target != vertex_id) {
            edges.insert(target);
        }
    }
}

// Function to create a vertex map with random edges
ygm::container::map<int, VertexInfo> create_random_vertex_map(ygm::comm &world, int num_vertices, int avg_degree) {
    ygm::container::map<int, VertexInfo> vertex_map(world);
    
    if (world.rank0()) {
        std::cout << "Creating vertex map with " << num_vertices << " vertices and average degree " << avg_degree << std::endl;
    }
    
    // Seed random number generator
    std::mt19937 rng(42 + world.rank()); // Different seed per rank
    
    // Create vertices and generate random edges
    for (int vertex_id = 0; vertex_id < num_vertices; ++vertex_id) {
        // Determine which rank owns this vertex
        int owner_rank = vertex_id % world.size();
        
        if (world.rank() == owner_rank) {
            VertexInfo info(vertex_id);
            generate_random_edges(vertex_id, info.forward_edges, num_vertices, avg_degree, rng);
            
            // Insert the vertex
            vertex_map.async_insert(vertex_id, info);
        }
    }
    
    // Wait for all insertions to complete
    world.barrier();
    
    // Now add backward edges based on forward edges
    vertex_map.for_all([&vertex_map](const int &vertex, VertexInfo &info) {
        for (int neighbor : info.forward_edges) {
            // Add backward edge
            auto add_backward_edge = [vertex](auto pmap, const int &neighbor_vertex, VertexInfo &neighbor_info) {
                neighbor_info.backward_edges.insert(vertex);
            };
            vertex_map.async_visit(neighbor, add_backward_edge);
        }
    });
    
    world.barrier();
    
    if (world.rank0()) {
        std::cout << "Vertex map creation completed" << std::endl;
    }
    
    return vertex_map;
}

// Function to output edges using multi_output
void output_edges_to_files(ygm::comm &world, ygm::container::map<int, VertexInfo>& vertex_map, const std::string& output_dir) {
    if (world.rank0()) {
        std::cout << "Outputting edges to directory: " << output_dir << std::endl;
    }
    
    // Create multi_output for edge files
    ygm::io::multi_output<> edge_writer(world, output_dir, 1024 * 1024, false);
    
    // Output forward edges
    vertex_map.for_all([&edge_writer](const int &vertex, const VertexInfo &info) {
        for (int neighbor : info.forward_edges) {
            // Format: "source target" (like edges.txt)
            edge_writer.async_write_line("edges.txt", vertex, " ", neighbor);
        }
    });
    
    world.barrier();
    
    if (world.rank0()) {
        std::cout << "Edge output completed" << std::endl;
    }
}

// Function to count total edges
int count_total_edges(ygm::container::map<int, VertexInfo>& vertex_map, ygm::comm& world) {
    int local_edge_count = 0;
    
    vertex_map.local_for_all([&local_edge_count](const int &vertex, const VertexInfo &info) {
        local_edge_count += info.forward_edges.size();
    });
    
    return ygm::sum(local_edge_count, world);
}

int main(int argc, char **argv) {
    ygm::comm world(&argc, &argv);
    
    // Parameters
    int num_vertices = 1000000;  // 1 million vertices
    int avg_degree = 5;          // Average degree per vertex
    std::string output_dir = "output_edges";
    
    if (argc > 1) {
        num_vertices = std::stoi(argv[1]);
    }
    if (argc > 2) {
        avg_degree = std::stoi(argv[2]);
    }
    if (argc > 3) {
        output_dir = argv[3];
    }
    
    if (world.rank0()) {
        std::cout << "=== YGM Multi-Output Edge Generator ===" << std::endl;
        std::cout << "Number of vertices: " << num_vertices << std::endl;
        std::cout << "Average degree: " << avg_degree << std::endl;
        std::cout << "Output directory: " << output_dir << std::endl;
        std::cout << "Number of processes: " << world.size() << std::endl;
        std::cout << "=======================================" << std::endl;
    }
    
    // Create vertex map with random edges
    auto vertex_map = create_random_vertex_map(world, num_vertices, avg_degree);
    
    // Count total edges
    int total_edges = count_total_edges(vertex_map, world);
    if (world.rank0()) {
        std::cout << "Total edges generated: " << total_edges << std::endl;
    }
    
    // Output edges to files using multi_output
    output_edges_to_files(world, vertex_map, output_dir);
    
    if (world.rank0()) {
        std::cout << "=== Generation Complete ===" << std::endl;
        std::cout << "Edge files written to: " << output_dir << "/edges.txt" << std::endl;
    }
    
    return 0;
}
