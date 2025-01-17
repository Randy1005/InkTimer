#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <ink/ink.hpp>
const float eps = 0.0001f;
bool float_equal(const float f1, const float f2) {
	return std::fabs(f1 - f2) < eps;
}


TEST_CASE("Update Edges 1" * doctest::timeout(300)) {
	ink::Ink ink;
	ink.insert_edge("v0", "v1", -1);
	ink.insert_edge("v0", "v2", 3);
	ink.insert_edge("v2", "v5", 1);
	ink.insert_edge("v2", "v6", 2);
	ink.insert_edge("v5", "v1", 1);
	ink.insert_edge("v1", "v3", 1);
	ink.insert_edge("v1", "v4", 2);
	ink.insert_edge("v3", "v7", -4);
	ink.insert_edge("v3", "v8", 8);
	
	// report once with incsfxt, save the pfxt nodes
	ink.report_incsfxt(4, true, false);

	// modify v0 -> v2 weight
	// v0->v2 is still a prefix tree edge
	// enable save_pfxt_nodes
	ink.insert_edge("v0", "v2", 5);
	ink.report_incremental(10, true, false, false);
  auto costs = ink.get_path_costs();

	REQUIRE(float_equal(costs[0], -4));
	REQUIRE(float_equal(costs[1], 1));
	REQUIRE(float_equal(costs[2], 4));
	REQUIRE(float_equal(costs[3], 7));
	REQUIRE(float_equal(costs[4], 8));
	REQUIRE(float_equal(costs[5], 9));
	REQUIRE(float_equal(costs[6], 16));

	// modify v0 -> v2 weight
	// v0->v2 became a suffix tree edge
	ink.insert_edge("v0", "v2", -4);
	ink.report_incremental(10, true, false, false);
	
  costs = ink.get_path_costs();

  REQUIRE(float_equal(costs[0], -5));
	REQUIRE(float_equal(costs[1], -4));
	REQUIRE(float_equal(costs[2], -2));
	REQUIRE(float_equal(costs[3], 0));
	REQUIRE(float_equal(costs[4], 1));
	REQUIRE(float_equal(costs[5], 7));
	REQUIRE(float_equal(costs[6], 8));
}


TEST_CASE("Update Edges 2" * doctest::timeout(300)) {
	ink::Ink ink;
	ink.insert_edge("A", "B", -1);
	ink.insert_edge("A", "C", 3);
	ink.insert_edge("C", "D", 1);
	ink.insert_edge("C", "E", 2);
	ink.insert_edge("D", "B", 3);

	ink.insert_edge("B", "F", 1);
	ink.insert_edge("B", "G", 2);
	ink.insert_edge("F", "H", -4);
	ink.insert_edge("F", "I", 8);
	ink.insert_edge("I", "L", 4);
	ink.insert_edge("I", "M", 11);

	ink.insert_edge("G", "J", 5);
	ink.insert_edge("G", "K", 7);

  ink.report_incsfxt(20, true);

	// modify A->C's weight to 0, A->C remains a prefix tree edge
	ink.insert_edge("A", "C", 0);
	ink.report_incremental(20, true, false, false);

  auto costs = ink.get_path_costs();

	REQUIRE(float_equal(costs[0], -4));
	REQUIRE(float_equal(costs[1], 1));
	REQUIRE(float_equal(costs[2], 2));
	REQUIRE(float_equal(costs[3], 6));
	REQUIRE(float_equal(costs[4], 8));
	REQUIRE(float_equal(costs[5], 11));
	REQUIRE(float_equal(costs[6], 12));
	REQUIRE(float_equal(costs[7], 13));
	REQUIRE(float_equal(costs[8], 17));
	REQUIRE(float_equal(costs[9], 19));
	REQUIRE(float_equal(costs[10], 24));

	// modify A->B's weight to 5, A->C becomes a suffix tree edge
	ink.insert_edge("A", "B", 5);
	ink.report_incremental(20, true, false, false);
	costs = ink.get_path_costs();
 
  REQUIRE(float_equal(costs[0], -4));
	REQUIRE(float_equal(costs[1], -3));
	REQUIRE(float_equal(costs[2], -3));
	REQUIRE(float_equal(costs[3], 6));
	REQUIRE(float_equal(costs[4], 7));
	REQUIRE(float_equal(costs[5], 8));
	REQUIRE(float_equal(costs[6], 9));
	REQUIRE(float_equal(costs[7], 12));
	REQUIRE(float_equal(costs[8], 13));
	REQUIRE(float_equal(costs[9], 19));
	REQUIRE(float_equal(costs[10], 20));
}

TEST_CASE("Update Edges 2 - Report More Costs, Then Report Less" * doctest::timeout(300)) {
	ink::Ink ink;
	ink.insert_edge("A", "B", -1);
	ink.insert_edge("A", "C", 3);
	ink.insert_edge("C", "D", 1);
	ink.insert_edge("C", "E", 2);
	ink.insert_edge("D", "B", 3);

	ink.insert_edge("B", "F", 1);
	ink.insert_edge("B", "G", 2);
	ink.insert_edge("F", "H", -4);
	ink.insert_edge("F", "I", 8);
	ink.insert_edge("I", "L", 4);
	ink.insert_edge("I", "M", 11);

	ink.insert_edge("G", "J", 5);
	ink.insert_edge("G", "K", 7);

  ink.report_incsfxt(20, true);

	// modify A->C's weight to 0, A->C remains a prefix tree edge
	ink.insert_edge("A", "C", 0);
	ink.report_incremental(20, true, false, false);

  auto costs = ink.get_path_costs();

	REQUIRE(float_equal(costs[0], -4));
	REQUIRE(float_equal(costs[1], 1));
	REQUIRE(float_equal(costs[2], 2));
	REQUIRE(float_equal(costs[3], 6));
	REQUIRE(float_equal(costs[4], 8));
	REQUIRE(float_equal(costs[5], 11));
	REQUIRE(float_equal(costs[6], 12));
	REQUIRE(float_equal(costs[7], 13));
	REQUIRE(float_equal(costs[8], 17));
	REQUIRE(float_equal(costs[9], 19));
	REQUIRE(float_equal(costs[10], 24));

	// modify A->B's weight to 5, A->C becomes a suffix tree edge
	ink.insert_edge("A", "B", 5);
	ink.report_incremental(5, true, false, false);
	costs = ink.get_path_costs();
 
  REQUIRE(float_equal(costs[0], -4));
	REQUIRE(float_equal(costs[1], -3));
	REQUIRE(float_equal(costs[2], -3));
	REQUIRE(float_equal(costs[3], 6));
	REQUIRE(float_equal(costs[4], 7));
}

TEST_CASE("Update Edges 2 (Update Different Edges)" * doctest::timeout(300)) {
	ink::Ink ink;
	ink.insert_edge("A", "B", -1);
	ink.insert_edge("A", "C", 3);
	ink.insert_edge("C", "D", 1);
	ink.insert_edge("C", "E", 2);
	ink.insert_edge("D", "B", 3);

	ink.insert_edge("B", "F", 1);
	ink.insert_edge("B", "G", 2);
	ink.insert_edge("F", "H", -4);
	ink.insert_edge("F", "I", 8);
	ink.insert_edge("I", "L", 4);
	ink.insert_edge("I", "M", 11);

	ink.insert_edge("G", "J", 5);
	ink.insert_edge("G", "K", 7);

  ink.report_incsfxt(20, true);

	// modify F->I weight to -9, F->I becomes a suffix tree edge
  ink.insert_edge("F", "I", -9);
	ink.report_incremental(20, true, false, false);

  auto costs = ink.get_path_costs();

	REQUIRE(float_equal(costs[0], -5));
	REQUIRE(float_equal(costs[1], -4));
	REQUIRE(float_equal(costs[2], 2));
	REQUIRE(float_equal(costs[3], 3));
	REQUIRE(float_equal(costs[4], 4));
	REQUIRE(float_equal(costs[5], 5));
	REQUIRE(float_equal(costs[6], 6));
	REQUIRE(float_equal(costs[7], 8));
	REQUIRE(float_equal(costs[8], 10));
	REQUIRE(float_equal(costs[9], 14));
	REQUIRE(float_equal(costs[10], 16));
}

TEST_CASE("Update Edges 2 (Reporting Less Than All Possible Costs (11 Costs))" * doctest::timeout(300)) {
	ink::Ink ink;
	ink.insert_edge("A", "B", -1);
	ink.insert_edge("A", "C", 3);
	ink.insert_edge("C", "D", 1);
	ink.insert_edge("C", "E", 2);
	ink.insert_edge("D", "B", 3);

	ink.insert_edge("B", "F", 1);
	ink.insert_edge("B", "G", 2);
	ink.insert_edge("F", "H", -4);
	ink.insert_edge("F", "I", 8);
	ink.insert_edge("I", "L", 4);
	ink.insert_edge("I", "M", 11);

	ink.insert_edge("G", "J", 5);
	ink.insert_edge("G", "K", 7);

  ink.report_incsfxt(8, true);

	// modify F->I weight to -9, F->I becomes a suffix tree edge
  ink.insert_edge("F", "I", -9);
	ink.report_incremental(4, true, false, false);

  auto costs = ink.get_path_costs();
	REQUIRE(float_equal(costs[0], -5));
	REQUIRE(float_equal(costs[1], -4));
	REQUIRE(float_equal(costs[2], 2));
	REQUIRE(float_equal(costs[3], 3));
}

TEST_CASE("Update Edges 3 (Update All Edges)" * doctest::timeout(300)) {
	ink::Ink ink;
	ink.insert_edge("A", "B", -1);
	ink.insert_edge("A", "C", 3);
	ink.insert_edge("C", "D", 1);
	ink.insert_edge("C", "E", 2);
	ink.insert_edge("D", "B", 3);

	ink.insert_edge("B", "F", 1);
	ink.insert_edge("B", "G", 2);
	ink.insert_edge("F", "H", -4);
	ink.insert_edge("F", "I", 8);
	ink.insert_edge("I", "L", 4);
	ink.insert_edge("I", "M", 11);

	ink.insert_edge("G", "J", 5);
	ink.insert_edge("G", "K", 7);

  ink.report_incsfxt(4, true, false);
  auto costs = ink.get_path_costs();


  ink.insert_edge("A", "B", -1);
	ink.insert_edge("A", "C", 3);
	ink.insert_edge("C", "D", 1);
	ink.insert_edge("C", "E", 2);
	ink.insert_edge("D", "B", 3);

	ink.insert_edge("B", "F", 1);
	ink.insert_edge("B", "G", 2);
	ink.insert_edge("F", "H", -4);
	ink.insert_edge("F", "I", 8);
	ink.insert_edge("I", "L", 4);
	ink.insert_edge("I", "M", 11);

	ink.insert_edge("G", "J", 5);
	ink.insert_edge("G", "K", 7);

	ink.report_incremental(11, true, false, false);
  costs = ink.get_path_costs();

  REQUIRE(float_equal(costs[0], -4));
	REQUIRE(float_equal(costs[1], 4));
	REQUIRE(float_equal(costs[2], 5));
	REQUIRE(float_equal(costs[3], 6));
	REQUIRE(float_equal(costs[4], 8));
	REQUIRE(float_equal(costs[5], 12));
	REQUIRE(float_equal(costs[6], 14));
	REQUIRE(float_equal(costs[7], 16));
	REQUIRE(float_equal(costs[8], 19));
	REQUIRE(float_equal(costs[9], 20));
	REQUIRE(float_equal(costs[10], 27));
}

TEST_CASE("Update Edges 4" * doctest::timeout(300)) {
	ink::Ink ink;
	ink.insert_edge("A", "B", -1);
	ink.insert_edge("A", "C", 3);
	ink.insert_edge("C", "D", 1);
	ink.insert_edge("C", "E", 2);
	ink.insert_edge("D", "B", 3);

	ink.insert_edge("B", "F", 1);
	ink.insert_edge("B", "G", 2);
	ink.insert_edge("F", "H", -4);
	ink.insert_edge("F", "I", 8);
	ink.insert_edge("I", "L", 4);
	ink.insert_edge("I", "M", 11);

	ink.insert_edge("G", "J", 5);
	ink.insert_edge("G", "K", 7);

  ink.report_incsfxt(4, true, false);
  auto costs = ink.get_path_costs();

	ink.insert_edge("A", "C", 9);
	ink.insert_edge("C", "E", 5);
	ink.report_incremental(4, true, false, false);
  costs = ink.get_path_costs();

  REQUIRE(float_equal(costs[0], -4));
  REQUIRE(float_equal(costs[1], 6));
  REQUIRE(float_equal(costs[2], 8));
  REQUIRE(float_equal(costs[3], 10));
}



//TEST_CASE("Update Edges 3" * doctest::timeout(300)) {
//	ink::Ink ink;
//	
//	ink.insert_edge("S", "K", 0);
//	ink.insert_edge("S", "A", 1);
//
//	ink.insert_edge("A", "L", 0);
//	ink.insert_edge("A", "B", 19);
//	ink.insert_edge("A", "C", 18);
//	ink.insert_edge("A", "D", 27);
//
//
//	ink.insert_edge("B", "M", 0);
//	ink.insert_edge("B", "E", 1);
//	ink.insert_edge("B", "F", 2);
//
//	ink.insert_edge("E", "N", 0);
//	ink.insert_edge("E", "I", 1);
//
//	ink.insert_edge("F", "P", 0);
//	ink.insert_edge("F", "J", 1);
//
//	ink.insert_edge("C", "O", 0);
//	ink.insert_edge("C", "G", 6);
//	ink.insert_edge("C", "H", 8);
//
//	auto costs_a = ink.report_incsfxt(20, true);
//	auto costs_b = ink.report_incremental(7, true, true);
//
//	REQUIRE(float_equal(costs_a[0].weight, paths1[0].weight));	
//	REQUIRE(float_equal(costs_a[1].weight, paths1[1].weight));	
//	REQUIRE(float_equal(costs_a[2].weight, paths1[2].weight));	
//	REQUIRE(float_equal(costs_a[3].weight, paths1[3].weight));	
//	REQUIRE(float_equal(costs_a[4].weight, paths1[4].weight));	
//	REQUIRE(float_equal(costs_a[5].weight, paths1[5].weight));	
//	REQUIRE(float_equal(costs_a[6].weight, paths1[6].weight));	
//
//	//
//	//auto paths2 = ink.report_incremental(6, true, true);
//	//REQUIRE(paths2.size() == 6);
//
//	//REQUIRE(float_equal(paths0[0].weight, paths2[0].weight));	
//	//REQUIRE(float_equal(paths0[1].weight, paths2[1].weight));	
//	//REQUIRE(float_equal(paths0[2].weight, paths2[2].weight));	
//	//REQUIRE(float_equal(paths0[3].weight, paths2[3].weight));	
//	//REQUIRE(float_equal(paths0[4].weight, paths2[4].weight));	
//	//REQUIRE(float_equal(paths0[5].weight, paths2[5].weight));	
//
//	//auto paths3 = ink.report_incremental(5, true, true);
//	//REQUIRE(paths3.size() == 5);
//	//
//	//REQUIRE(float_equal(paths0[0].weight, paths3[0].weight));	
//	//REQUIRE(float_equal(paths0[1].weight, paths3[1].weight));	
//	//REQUIRE(float_equal(paths0[2].weight, paths3[2].weight));	
//	//REQUIRE(float_equal(paths0[3].weight, paths3[3].weight));	
//	//REQUIRE(float_equal(paths0[4].weight, paths3[4].weight));	
//}

TEST_CASE("Update Edges 4" * doctest::timeout(300)) {
	ink::Ink ink;
	ink.insert_edge("A", "B", -1);
	ink.insert_edge("A", "C", 3);
	ink.insert_edge("C", "D", 1);
	ink.insert_edge("C", "E", 2);
	ink.insert_edge("D", "B", 3);

	ink.insert_edge("B", "F", 1);
	ink.insert_edge("B", "G", 2);
	ink.insert_edge("F", "H", -4);
	ink.insert_edge("F", "I", 8);
	ink.insert_edge("I", "L", 4);
	ink.insert_edge("I", "M", 11);

	ink.insert_edge("G", "J", 5);
	ink.insert_edge("G", "K", 7);

	ink.report_incsfxt(4, true, false);

	// modify A->C's weight to 9, A->C remains a prefix tree edge
	ink.insert_edge("A", "C", 9);
	ink.report_incremental(8, true, true, false);
	auto costs = ink.get_path_costs();

	//REQUIRE(float_equal(costs[0], -4));
	//REQUIRE(float_equal(costs[1], 10));
	//REQUIRE(float_equal(costs[2], 12));
	//REQUIRE(float_equal(costs[3], 15));
	//REQUIRE(float_equal(costs[4], 16));
	//REQUIRE(float_equal(costs[5], 16));
	//REQUIRE(float_equal(costs[6], 25));
	//REQUIRE(float_equal(costs[7], 31));


	
	// modify A->B's weight to 5, A->C becomes a suffix tree edge
	ink.insert_edge("A", "B", 5);
	//paths = ink.report_incremental(20, true, true);
	//REQUIRE(paths.size() == 11);
	//REQUIRE(float_equal(paths[0].weight, 1));
	//REQUIRE(float_equal(paths[1].weight, 2));
	//REQUIRE(float_equal(paths[2].weight, 2));
	//REQUIRE(float_equal(paths[3].weight, 11));
	//REQUIRE(float_equal(paths[4].weight, 12));
	//REQUIRE(float_equal(paths[5].weight, 13));
	//REQUIRE(float_equal(paths[6].weight, 14));
	//REQUIRE(float_equal(paths[7].weight, 17));
	//REQUIRE(float_equal(paths[8].weight, 18));
	//REQUIRE(float_equal(paths[9].weight, 24));
	//REQUIRE(float_equal(paths[10].weight, 25));
}


