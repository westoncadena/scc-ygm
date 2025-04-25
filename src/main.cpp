#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <utility>

// Parallel implementation of ECL-SCC algorithm using YGM
std::vector<int> ecl_scc_parallel(ygm::comm &world, int n,
                                  const std::vector<std::pair<int, int>> &edges)
{
    // Create bags to store vertex signatures
    ygm::container::bag<std::tuple<int, int, int>> vin_updates(world);  // (vertex, new_vin, iteration)
    ygm::container::bag<std::tuple<int, int, int>> vout_updates(world); // (vertex, new_vout, iteration)
    ygm::container::bag<std::pair<int, int>> current_edges(world);

    // Initialize with all edges
    for (const auto &edge : edges)
    {
        current_edges.async_insert(edge);
    }
    world.barrier();

    // Create arrays to store vertex signatures locally
    std::vector<int> local_vin(n), local_vout(n);

    bool global_converged = false;
    int iteration = 0;

    while (!global_converged)
    {
        iteration++;

        // Initialize vertex signatures
        for (int v = 0; v < n; ++v)
        {
            local_vin[v] = local_vout[v] = v;
        }

        // Gather all edges to all ranks for processing
        std::vector<std::pair<int, int>> all_edges;
        current_edges.gather(all_edges);
        world.barrier();

        // Propagate max values until convergence
        bool local_updated = true;
        int propagation_round = 0;

        while (local_updated)
        {
            propagation_round++;
            local_updated = false;

            // Process edges locally
            for (const auto &[u, v] : all_edges)
            {
                // Propagate max on outgoing paths
                if (local_vout[u] < local_vout[v])
                {
                    local_vout[u] = local_vout[v];
                    vout_updates.async_insert(std::make_tuple(u, local_vout[u], iteration));
                    local_updated = true;
                }

                // Propagate max on incoming paths
                if (local_vin[v] < local_vin[u])
                {
                    local_vin[v] = local_vin[u];
                    vin_updates.async_insert(std::make_tuple(v, local_vin[v], iteration));
                    local_updated = true;
                }
            }

            world.barrier();

            // Apply updates from other ranks
            vin_updates.for_all([&local_vin, iteration](const std::tuple<int, int, int> &update)
                                {
                int vertex = std::get<0>(update);
                int new_vin = std::get<1>(update);
                int iter = std::get<2>(update);
                
                if (iter == iteration && local_vin[vertex] < new_vin) {
                    local_vin[vertex] = new_vin;
                } });

            vout_updates.for_all([&local_vout, iteration](const std::tuple<int, int, int> &update)
                                 {
                int vertex = std::get<0>(update);
                int new_vout = std::get<1>(update);
                int iter = std::get<2>(update);
                
                if (iter == iteration && local_vout[vertex] < new_vout) {
                    local_vout[vertex] = new_vout;
                } });

            world.barrier();

            // Check if any rank made updates
            bool any_rank_updated = world.all_reduce_max(local_updated);
            local_updated = any_rank_updated;

            if (!local_updated)
            {
                break;
            }
        }

        // Clear update bags for next iteration
        vin_updates.local_clear();
        vout_updates.local_clear();
        world.barrier();

        // Remove edges that span SCCs
        ygm::container::bag<std::pair<int, int>> new_edges(world);

        for (const auto &[u, v] : all_edges)
        {
            if (local_vin[u] == local_vin[v] && local_vout[u] == local_vout[v])
            {
                new_edges.async_insert(std::make_pair(u, v));
            }
        }

        world.barrier();

        // Update current edges
        current_edges = std::move(new_edges);

        // Check for global convergence
        bool local_converged = true;
        for (int v = 0; v < n; ++v)
        {
            if (local_vin[v] != local_vout[v])
            {
                local_converged = false;
                break;
            }
        }

        global_converged = world.all_reduce_min(local_converged);
    }

    // Ensure all ranks have the same final result
    std::vector<int> global_vin(n);
    for (int v = 0; v < n; ++v)
    {
        global_vin[v] = world.all_reduce_max(local_vin[v]);
    }

    return global_vin;
}

std::vector<std::pair<int, int>> figure3_edges()
{
    std::vector<std::pair<int, int>> edges = {
        {2, 9}, {9, 0}, {0, 5}, {5, 1}, {3, 7}, {3, 11}, {7, 6}, {7, 4}, {6, 7}, {4, 10}, {10, 4}, {8, 3}, {8, 10}, {11, 8}, {11, 3}};

    return edges;
}

int main(int argc, char **argv)
{
    ygm::comm world(&argc, &argv);

    const int N = 12;
    auto edges = figure3_edges();

    if (world.rank0())
    {
        std::cout << "Running parallel ECL-SCC algorithm on " << world.size() << " ranks" << std::endl;
        std::cout << "Graph has " << N << " vertices and " << edges.size() << " edges" << std::endl;
    }

    world.barrier();

    auto start_time = std::chrono::high_resolution_clock::now();
    auto labels = ecl_scc_parallel(world, N, edges);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    if (world.rank0())
    {
        std::cout << "\nComputation completed in " << duration.count() << " ms" << std::endl;
        std::cout << "\nVertex : SCC‑label (vin)\n";
        for (int v = 0; v < N; ++v)
        {
            std::cout << std::setw(3) << v << "    :   " << labels[v] << '\n';
        }

        // Count the number of SCCs
        std::set<int> unique_components;
        for (int label : labels)
        {
            unique_components.insert(label);
        }
        std::cout << "\nTotal number of SCCs: " << unique_components.size() << std::endl;
    }

    return 0;
}