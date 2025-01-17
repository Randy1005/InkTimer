#include <ot/timer/timer.hpp>

int main(int argc, char *argv[]) {

  // create a timer object
  ot::Timer timer;
  
  // Read design
  timer.read_celllib("ac97_ctrl_Early.lib", ot::MIN)
       .read_celllib("ac97_ctrl_Late.lib", ot::MAX)
       .read_verilog("ac97_ctrl.v")
       .read_spef("ac97_ctrl.spef");

  // get the top-5 worst critical paths
  auto paths = timer.report_timing(10);

  for(size_t i=0; i<paths.size(); ++i) {
    std::cout << "----- Critical Path " << i << " -----\n";
    std::cout << paths[i] << '\n';
  }

	
	std::ofstream ofs("ac97_ctrl.edges");
  timer.dump_edge_insertions(ofs);

  return 0;
}



