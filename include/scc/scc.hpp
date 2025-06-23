#pragma once
#include "scc/graph.hpp"
#include <ygm/comm.hpp>

namespace scc {

ygm::container::map<int, VertexInfo> ecl_scc_ygm(ygm::comm &world, const std::string& edgelist_file);
void print_results(ygm::container::map<int, VertexInfo>& vertex_map, ygm::comm& world);
int count_sccs(ygm::container::map<int, VertexInfo>& vertex_map, ygm::comm& world);
int count_largest_scc(ygm::container::map<int, VertexInfo>& vertex_map, ygm::comm& world);

} // namespace scc 