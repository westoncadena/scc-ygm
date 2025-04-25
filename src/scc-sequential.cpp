#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>

std::vector<int> ecl_scc_sequential(int n,
                                    const std::vector<std::pair<int, int>> &edges)
{
    std::vector<int> vin(n), vout(n);
    std::vector<std::pair<int, int>> current_edges = edges;

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
            // Iterate through all edges
            for (const auto &[u, v] : current_edges)
            {
                // propagate max on outgoing paths (u_out ← max(u_out, v_out))
                if (vout[u] < vout[v])
                {
                    vout[u] = vout[v];
                    updated = true;
                }

                // propagate max on incoming paths (v_in ← max(u_in, v_in))
                if (vin[v] < vin[u])
                {
                    vin[v] = vin[u];
                    updated = true;
                }
            }
        }

        // remove edges that span SCCs
        std::vector<std::pair<int, int>> new_edges;
        for (const auto &[u, v] : current_edges)
        {
            if (vin[u] == vin[v] && vout[u] == vout[v])
            {
                new_edges.emplace_back(u, v);
            }
        }

        // Update current edges
        current_edges = new_edges;

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

std::vector<std::pair<int, int>> figure3_edges()
{
    std::vector<std::pair<int, int>> edges = {
        {2, 9}, {9, 0}, {0, 5}, {5, 1}, {3, 7}, {3, 11}, {7, 6}, {7, 4}, {6, 7}, {4, 10}, {10, 4}, {8, 3}, {8, 10}, {11, 8}, {11, 3}};

    return edges;
}

int main()
{
    const int N = 12;
    auto edges = figure3_edges();

    auto labels = ecl_scc_sequential(N, edges);

    std::cout << "Vertex : SCC‑label (vin)\n";
    for (int v = 0; v < N; ++v)
        std::cout << std::setw(3) << v << "    :   " << labels[v] << '\n';
    return 0;
}
