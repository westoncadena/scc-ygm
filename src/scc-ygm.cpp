#include <iostream>
#include <ygm/comm.hpp>
#include <ygm/container/bag.hpp>

ygm::container::bag<std::pair<int, int>> ecl_scc_ygm(ygm::comm &world, int n, const ygm::container::bag<std::pair<int, int>> &edges)
{
    ygm::container::bag<int> vin(world);
    ygm::container::bag<int> vout(world);

    bool converged = false;

    while (!converged)
    {
        // Initialize vertex signatures
        for (int v = 0; v < n; ++v)
        {
            vin.async_insert(v);
            vout.async_insert(v);
        }
    }
}

int main(int argc, char **argv)
{
    ygm::comm world(&argc, &argv);

    ygm::container::bag<std::pair<int, int>> bbag(world);

    if (world.rank0())
    {
        std::cout << "Running parallel ECL-SCC algorithm on " << world.size() << " ranks" << std::endl;

        bbag.async_insert(std::make_pair(2, 9));
        bbag.async_insert(std::make_pair(9, 0));
        bbag.async_insert(std::make_pair(0, 5));
        bbag.async_insert(std::make_pair(5, 1));
        bbag.async_insert(std::make_pair(3, 7));
        bbag.async_insert(std::make_pair(3, 11));
        bbag.async_insert(std::make_pair(7, 6));
        bbag.async_insert(std::make_pair(7, 4));
        bbag.async_insert(std::make_pair(6, 7));
        bbag.async_insert(std::make_pair(4, 10));
        bbag.async_insert(std::make_pair(10, 4));
        bbag.async_insert(std::make_pair(8, 3));
        bbag.async_insert(std::make_pair(8, 10));
        bbag.async_insert(std::make_pair(11, 8));
        bbag.async_insert(std::make_pair(11, 3));
    }

    world.barrier();

    for (int i = 0; i < world.size(); i++)
    {
        if (i == world.rank())
        {
            std::cout << "Rank " << i << std::endl;
            bbag.local_for_all([](std::pair<int, int> &p)
                               { std::cout << p.first << " " << p.second << std::endl; });
            std::cout << std::endl;
        }
        world.barrier();
    }

    return 0;
}
