#include "scc/scc.hpp"
#include <iostream>

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
    auto result = scc::ecl_scc_ygm(world, edgelist_file);

    // Count SCCs
    int num_sccs = scc::count_sccs(result, world);
    if (world.rank0()) {
        std::cout << "\nNumber of Strongly Connected Components: " << num_sccs << std::endl;
    }

    // Count size of largest SCC
    int largest_scc_size = scc::count_largest_scc(result, world);
    if (world.rank0()) {
        std::cout << "Size of largest Strongly Connected Component: " << largest_scc_size << std::endl;
    }

    // Print detailed results
    // scc::print_results(result, world);

    return 0;
}
