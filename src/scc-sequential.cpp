#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>

std::vector<int> ecl_scc_sequential(int n,
                                    std::map<int, std::vector<int>> &adj_list)
{
    std::vector<int> vin(n), vout(n);

    bool converged = false;
    while (!converged)
    {
        // initialize vertex signatures
        for (int v = 0; v < n; ++v)
            vin[v] = vout[v] = v;

        // propagate max values
        bool updated = true;
        while (updated)
        {
            updated = false;
            // Iterate through the adjacency list
            for (const auto &[u, neighbors] : adj_list)
            {
                for (int w : neighbors)
                {
                    // propagate max on outgoing paths
                    if (vout[u] < vout[w])
                    {
                        vout[u] = vout[w];
                        updated = true;
                    }

                    // propagate max on incoming paths
                    if (vin[w] < vin[u])
                    {
                        vin[w] = vin[u];
                        updated = true;
                    }
                }
            }
        }

        // remove edges that span SCCs
        std::map<int, std::vector<int>> new_adj_list;

        for (const auto &[u, neighbors] : adj_list)
        {
            for (int v : neighbors)
            {
                if (vin[u] == vin[v] && vout[u] == vout[v])
                {
                    new_adj_list[u].push_back(v);
                }
            }
        }

        adj_list = new_adj_list;

        // check for global convergence
        converged = true;
        for (int v = 0; v < n; ++v)
            if (vin[v] != vout[v])
            {
                converged = false;
                break;
            }
    }

    return vin;
}

std::map<int, std::vector<int>> figure3_edges()
{
    std::map<int, std::vector<int>> adj_list;

    // chain
    adj_list[2].push_back(9);
    adj_list[9].push_back(0);
    adj_list[0].push_back(5);
    adj_list[5].push_back(1);

    adj_list[3].push_back(7);
    adj_list[3].push_back(11);

    adj_list[7].push_back(6);
    adj_list[7].push_back(4);

    adj_list[6].push_back(7);

    adj_list[4].push_back(10);

    adj_list[10].push_back(4);

    adj_list[8].push_back(3);
    adj_list[8].push_back(10);

    adj_list[11].push_back(8);
    adj_list[11].push_back(3);

    return adj_list;
}

int main()
{
    const int N = 12;
    auto adj_list = figure3_edges();

    auto labels = ecl_scc_sequential(N, adj_list);

    std::cout << "Vertex : SCC‑label (vin)\n";
    for (int v = 0; v < N; ++v)
        std::cout << std::setw(3) << v << "    :   " << labels[v] << '\n';
    return 0;
}
