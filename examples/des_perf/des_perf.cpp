#include <ot/timer/timer.hpp>

int main(int argc, char *argv[]) {

  // create a timer object
  ot::Timer timer;
  
  // Read design
  timer.read_celllib("des_perf_Early.lib", ot::MIN)
       .read_celllib("des_perf_Late.lib", ot::MAX)
       .read_verilog("des_perf.v")
       .read_spef("des_perf.spef");

  // get the top-5 worst critical paths
  auto paths = timer.report_timing(10);

  for(size_t i=0; i<paths.size(); ++i) {
    std::cout << "----- Critical Path " << i << " -----\n";
    std::cout << paths[i] << '\n';
  }

	
	std::ofstream ofs("des_perf.edges");
  timer.dump_edge_insertions(ofs);

  return 0;
}



