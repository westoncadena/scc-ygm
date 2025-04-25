#include <iostream>
#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>
#include <utility>

int main(int argc, char **argv)
{
    ygm::comm world(&argc, &argv);

    // Create a bag to store pairs of integers
    ygm::container::bag<std::pair<int, int>> int_pairs_bag(world);

    // Each rank will insert different pairs
    int rank = world.rank();
    for (int i = 0; i < 3; i++)
    {
        // Insert pairs where first value is the rank and second is a counter
        int_pairs_bag.async_insert(std::make_pair(rank, i));

        // Also insert some pairs with rank as second value
        int_pairs_bag.async_insert(std::make_pair(i, rank));
    }

    // Wait for all insertions to complete
    world.barrier();

    // Print the local contents of each rank's bag
    for (int i = 0; i < world.size(); i++)
    {
        if (i == world.rank())
        {
            std::cout << "Rank " << i << " local pairs:" << std::endl;
            int_pairs_bag.local_for_all([](const std::pair<int, int> &p)
                                        { std::cout << "(" << p.first << ", " << p.second << ")" << std::endl; });
            std::cout << std::endl;
        }
        world.barrier();
    }

    // Gather all pairs to rank 0 and print them
    std::vector<std::pair<int, int>> all_pairs;
    int_pairs_bag.gather(all_pairs, 0);

    if (world.rank0())
    {
        std::cout << "All pairs gathered at rank 0:" << std::endl;
        for (const auto &pair : all_pairs)
        {
            std::cout << "(" << pair.first << ", " << pair.second << ")" << std::endl;
        }
    }

    // Count the total number of pairs and print
    size_t total_pairs = int_pairs_bag.size();
    if (world.rank0())
    {
        std::cout << "\nTotal number of pairs across all ranks: " << total_pairs << std::endl;
    }

    world.barrier();
    return 0;
}