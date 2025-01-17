#include "ink.hpp"

namespace ink {

// ------------------------
// Vertex Implementations
// ------------------------
bool Vert::is_src() const {
	// no fanins
	if (num_fanins() == 0) {
		return true;
	}

	return false;
}

bool Vert::is_dst() const {
	// no fanouts
	if (num_fanouts() == 0) {
		return true;
	}

	return false;
}

void Vert::insert_fanin(Edge& e) {
	e.fanin_satellite = fanin.insert(fanin.end(), &e);
}

void Vert::insert_fanout(Edge& e) {
	e.fanout_satellite = fanout.insert(fanout.end(), &e);
}

void Vert::remove_fanin(Edge& e) {
	assert(e.fanin_satellite);
	assert(&e.to == this);
	fanin.erase(*(e.fanin_satellite));

	// invalidate satellite iterator
	e.fanin_satellite.reset();
}

void Vert::remove_fanout(Edge& e) {
	assert(e.fanout_satellite);
	assert(&e.from == this);
	fanout.erase(*(e.fanout_satellite));

	// invalidate satellite iterator
	e.fanout_satellite.reset();
}

// ------------------------
// Edge Implementations
// ------------------------
std::string Edge::name() const {
	return from.name + "->" + to.name;
}

inline size_t Edge::min_valid_weight() const {
	float v = std::numeric_limits<float>::max();
	size_t min_idx = NUM_WEIGHTS;
	for (size_t i = 0; i < NUM_WEIGHTS; i++) {
		if (weights[i] && *weights[i] < v) {
			min_idx = i;
			v = *weights[i];
		}
	}
	return min_idx;
}


// ------------------------
// Ink Implementations
// ------------------------
void Ink::read_ops(const std::string& in, const std::string& out) {
	std::ifstream ifs;
	ifs.open(in);
	if (!ifs) {
		throw std::runtime_error("Failed to open file: " + in);
	}

	std::ofstream ofs(out);

	_read_ops(ifs, ofs);
}


Vert& Ink::insert_vertex(const std::string& name) {
	// NOTE: 
	// 1. (vertex non-existent): simply insert
	// if a valid id can be popped from the free list, assign
	// it to this new vertex
	// if no free id is available, increment the size of storage
	// 2. (vertex exists): do nothing

	// check if a vertex with this name exists
	auto itr = _name2v.find(name);

	if (itr == _name2v.end()) {
		// store in name to object map
		auto id = _idxgen_vert.get();

		// NOTE: Vert is a rvalue, compiler moves it by default
		auto [iter, success] = _name2v.emplace(name, Vert{name, id});
	
		// if the index generated goes out of range, resize _vptrs 
		if (id + 1 > _vptrs.size()) {
			_vptrs.resize(id + 1, nullptr);
			
		}

		_vptrs[id] = &(iter->second); 
		return iter->second;	
	}
	else {
		return itr->second;
	}
	
	
}

void Ink::remove_vertex(const std::string& name) {
	auto itr = _name2v.find(name);

	if (itr == _name2v.end()) {
		// vertex non-existent, nothing to do
		return;
	}
	

	// iterate through this vertex's
	// fanin and fanouts and remove them
	auto& v = itr->second;

	for (auto e : v.fanin) {
		for (auto n : e->dep_nodes) {
      n->edge = nullptr;
    }
    e->dep_nodes.clear();
    
    // record each of e's fanin vertex
		// they need to be re-relaxed during 
		// incremental report
		if (!_vptrs[e->from.id]->is_in_update_list && _global_sfxt) {
			_to_update.emplace_back(e->from.id);
			_vptrs[e->from.id]->is_in_update_list = true;
		}
		
		
		// remove edge pointer mapping and recycle free id
		_eptrs[e->id] = nullptr;
		_idxgen_edge.recycle(e->id);
		e->from.remove_fanout(*e);
		assert(e->satellite);
		_edges.erase(*e->satellite);
	}
	for (auto e : v.fanout) {
		for (auto n : e->dep_nodes) {
      n->edge = nullptr;
    }
    e->dep_nodes.clear();
		
    // record each of e's fanout vertex
		// they need to be re-relaxed during 
		// incrmental report
		if (!_vptrs[e->to.id]->is_in_update_list && _global_sfxt) {
			_to_update.emplace_back(e->to.id);
			_vptrs[e->to.id]->is_in_update_list = true;
		}

		_eptrs[e->id] = nullptr;
		_idxgen_edge.recycle(e->id);
		e->to.remove_fanin(*e);
		assert(e->satellite);
		_edges.erase(*e->satellite);
	}


	// remove vertex pointer mapping and recycle free id
	_vptrs[v.id] = nullptr;
	_idxgen_vert.recycle(v.id);
	_name2v.erase(itr);

}

Edge& Ink::insert_edge(
	const std::string& from,
	const std::string& to,
	const std::optional<float> w0, 
	const std::optional<float> w1, 
	const std::optional<float> w2, 
	const std::optional<float> w3,
	const std::optional<float> w4, 
	const std::optional<float> w5, 
	const std::optional<float> w6, 
	const std::optional<float> w7) {

	std::array<std::optional<float>, NUM_WEIGHTS> ws = {
		w0, w1, w2, w3, w4, w5, w6, w7
	};
		
	Vert& v_from = insert_vertex(from);
	Vert& v_to = insert_vertex(to);

	const std::string ename = from + "->" + to;
	auto itr = _name2eit.find(ename);

	if (itr != _name2eit.end()) {
		// edge exists
		auto& e = *itr->second;

		// update weights
		for (size_t i = 0; i < e.weights.size(); i++) {
			e.weights[i] = ws[i];
		}
		
		// record from vertex
		if (_global_sfxt) {
			auto v = e.from.id;
			if (!_vptrs[v]->is_in_update_list) {
				_to_update.emplace_back(v);
				_vptrs[v]->is_in_update_list = true;
			}

			e.modified = true;
		}

		
    // initialize pruned weights
    for (size_t i = 0; i < NUM_WEIGHTS; i++) {
      e.pruned_weights[i] = false;
    }

		return e;
	}
	else {
		// edge doesn't exist
		auto id = _idxgen_edge.get();

		auto& e = _edges.emplace_front(v_from, v_to, id, std::move(ws));
		
		// cache the edge's satellite iterator
		// in the owner storage
		e.satellite = _edges.begin();
		
		// resize _eptr if the generated index goes out of range
		if (id + 1 > _eptrs.size()) {
			_eptrs.resize(id + 1, nullptr);
		}

		// store raw pointer to this edge object
		_eptrs[id] = &e;

		// update fanin, fanout
		// cache fanin, fanout satellite iterators
		// (for edge removal)
		e.from.insert_fanout(e);
		e.to.insert_fanin(e);

		// update the edge name to iterator mapping
		_name2eit.emplace(ename, _edges.begin());
	
		// record to vertex 
		if (_global_sfxt) {
			auto v = e.to.id;
			if (!_vptrs[v]->is_in_update_list) {
				_to_update.emplace_back(v);
				_vptrs[v]->is_in_update_list = true;
			}

			e.modified = true;
		}

    // initialize pruned weights
    for (size_t i = 0; i < NUM_WEIGHTS; i++) {
      e.pruned_weights[i] = false;
    }

		return e; 
	}
}

// TODO: we may wanna use std::nullopt when an edge or vertex
// doesn't exist
Edge& Ink::get_edge(const std::string& from, const std::string& to) {
	const std::string ename = from + "->" + to;
	auto itr = _name2eit.find(ename);
	return *itr->second;
}

Vert& Ink::get_vertex(const std::string& name) {
	auto itr = _name2v.find(name);
	return itr->second;
}



void Ink::remove_edge(const std::string& from, const std::string& to) {
	const std::string ename = from + "->" + to;
	auto itr = _name2eit.find(ename);
	
	if (itr == _name2eit.end()) {
		// edge non existent, nothing to do
		return;
	}

  auto& e = *itr->second;  
	// nullify dependent pfxt node edges
  for (auto n : e.dep_nodes) {
    n->edge = nullptr;
  }
  e.dep_nodes.clear();
  
  _remove_edge(e);

	// remove name to edge iterator mapping
	_name2eit.erase(itr);
}


void Ink::insert_buffer(const std::string& name, Edge& e) {
  // cache the vertices names and edge weights
  auto uname = e.from.name;
  auto vname = e.to.name;
  auto ws = e.weights;

  // simulate the buffering effect
  // delays would downscale
  for (auto& w : ws) {
    if (w) {
      *w *= 0.4f;
    }
  }

  // remove e
  remove_edge(uname, vname);

  // add an edge directed from u to buffer
  auto& e_u2b = insert_edge(uname, name, 
    ws[0], ws[1], ws[2], ws[3], ws[4], ws[5], ws[6], ws[7]); 
    
  e_u2b.to.f_name = uname; 
  e_u2b.to.t_name = vname;

  // add an edge directed from buffer to v
  auto& e_b2v = insert_edge(name, vname, 
    ws[0], ws[1], ws[2], ws[3], ws[4], ws[5], ws[6], ws[7]); 
} 

void Ink::remove_buffer(const std::string& name) {
  auto& vert = get_vertex(name);
  auto& e_f = get_edge(vert.f_name, name);
  auto& e_t = get_edge(name, vert.t_name);
  auto ws = std::move(e_f.weights);
  for (auto& w : ws) {
    *w *= 2.5f;
  }

  // undo buffering, reconnect buffered edge
  insert_edge(vert.f_name, vert.t_name, 
    ws[0], ws[1], ws[2], ws[3], ws[4], ws[5], ws[6], ws[7]); 

  remove_vertex(name);
}  

std::vector<Path> Ink::report(size_t K) {
	if (K == 0) {
		return {};
	}
	
	// TODO: K = 1
	if (K == 1) {
	
	}

	// incremental: clear endpoint storage
	_endpoints.clear();

	// scan and add the out-degree=0 vertices to the endpoint vector
	for (const auto v : _vptrs) {
		if (v != nullptr && v->is_dst()) {
			_endpoints.emplace_back(*v, 0.0f);	
		}
	}

	PathHeap heap;
	_taskflow.transform_reduce(_endpoints.begin(), _endpoints.end(), heap,
		[&] (PathHeap l, PathHeap r) mutable {
			l.merge_and_fit(std::move(r), K);
			return l;
		},
		[&] (Point& ept) {
			PathHeap heap;
			_spur(ept, K, heap);
			return heap;
		}
	);

	_executor.run(_taskflow).wait();
	_taskflow.clear();

	_clear_update_list();		
	return heap.extract();
}

std::vector<Path> Ink::report_incsfxt(
	size_t K, 
	bool save_pfxt_nodes,
	bool recover_paths) {
	if (K == 0) {
		return {};
	}
	
	// TODO: K = 1
	if (K == 1) {
	
	}

	// incremental: clear endpoint storage
	_endpoints.clear();

	// scan and add the out-degree=0 vertices to the endpoint vector
	for (const auto v : _vptrs) {
		if (v != nullptr && v->is_dst()) {
			_endpoints.emplace_back(*v, 0.0f);	
		}
	}

  pfxt_expansion_time = 0;

	PathHeap heap;
	_sfxt_cache();
	auto& sfxt = *_global_sfxt;
	auto pfxt = _pfxt_cache(sfxt);
  _spur_incsfxt(K, heap, pfxt, save_pfxt_nodes, recover_paths);	
	
	// report complete, clear the update list
	_clear_update_list();		
	
	return _extract_paths(_all_paths);
}

std::vector<Path> Ink::report_rebuild(
  size_t K,
  bool recover_paths) {
	if (K == 0) {
		return {};
	}
	
	// TODO: K = 1
	if (K == 1) {
	
	}

  // clear paths
  _all_path_costs.clear();

	// incremental: clear endpoint storage
	_endpoints.clear();

	// scan and add the out-degree=0 vertices to the endpoint vector
	for (const auto v : _vptrs) {
		if (v != nullptr && v->is_dst()) {
			_endpoints.emplace_back(*v, 0.0f);	
		}
	}

	PathHeap heap;
	auto beg = std::chrono::steady_clock::now();
  _sfxt_rebuild();
  auto end = std::chrono::steady_clock::now();
  sfxt_update_time =  
		std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();
  _spur_rebuild(K, heap, recover_paths);	


  full_time = sfxt_update_time + pfxt_expansion_time;

	// report complete, clear the update list
	_clear_update_list();		

	return heap.extract();
}

std::vector<Path> Ink::report_incremental(
	size_t K,
	bool save_pfxt_nodes,
	bool use_leaders,
	bool recover_paths) {
	if (K == 0) {
		return {};
	}
	
	// TODO: K = 1
	if (K == 1) {
	
	}

	// incremental: clear endpoint storage
	_endpoints.clear();

	// scan and add the out-degree=0 vertices to the endpoint vector
	for (const auto v : _vptrs) {
		if (v != nullptr && v->is_dst()) {
			_endpoints.emplace_back(*v, 0.0f);	
		}
	}

  auto beg = std::chrono::steady_clock::now();
	_sfxt_cache();
  auto end = std::chrono::steady_clock::now();
	
	sfxt_update_time = 
		std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();
  auto& sfxt = *_global_sfxt;

  auto pfxt = _pfxt_cache(sfxt);
	pfxt.nodes = std::move(_pfxt_nodes);
	pfxt.paths = std::move(_pfxt_paths);
  pfxt.srcs = std::move(_pfxt_srcs);

	_spur_incremental(K, pfxt, save_pfxt_nodes, use_leaders, recover_paths);	

  incremental_time = sfxt_update_time + pfxt_expansion_time;

	// report complete, clear the update list
	_clear_update_list();	
	_affected_pfxtnodes.clear();

	return _extract_paths(_all_paths);
}

std::vector<Path> Ink::report_async(
	size_t K, 
	bool recover_paths) {
	if (K == 0) {
		return {};
	}
	
	// TODO: K = 1
	if (K == 1) {
	
	}

	// incremental: clear endpoint storage
	_endpoints.clear();

	// scan and add the out-degree=0 vertices to the endpoint vector
	for (const auto v : _vptrs) {
		if (v != nullptr && v->is_dst()) {
			_endpoints.emplace_back(*v, 0.0f);	
		}
	}

  pfxt_expansion_time = 0;

	_sfxt_cache();
	auto& sfxt = *_global_sfxt;
	auto pfxt = _pfxt_cache(sfxt);
  _spur_async(K, pfxt, recover_paths);

	// report complete, clear the update list
	_clear_update_list();		
	
	return _extract_paths(_all_paths);
}

std::vector<Path> Ink::report_multiq(
  float max_dc,
	float min_dc,
  size_t K, 
  size_t N,
	bool recover_paths,
  bool enable_node_redistr,
  bool is_relaxed) {
	if (K == 0) {
		return {};
	}
	
	// TODO: K = 1
	if (K == 1) {
	
	}

	// incremental: clear endpoint storage
	_endpoints.clear();

	// scan and add the out-degree=0 vertices to the endpoint vector
	for (const auto v : _vptrs) {
		if (v != nullptr && v->is_dst()) {
			_endpoints.emplace_back(*v, 0.0f);	
		}
	}

  pfxt_expansion_time = 0;

  old_max_dc = max_dc;
  old_min_dc = min_dc;
  num_task_qs = N;
  
  _sfxt_cache();
	auto& sfxt = *_global_sfxt;
  auto pfxt = _pfxt_cache_multiq(sfxt);
  if (is_relaxed) {
    _spur_multiq_relaxed(K, pfxt, recover_paths);
  }
  else {
    _spur_multiq(K, pfxt, recover_paths, enable_node_redistr);
  }

	// report complete, clear the update list
	_clear_update_list();		
	
	return {};
}





void Ink::dump(std::ostream& os) const {
	os << num_verts() << " Vertices:\n";
	os << "------------------\n";
	for (const auto& [name, v] : _name2v) {
		os << "name=" << v.name 
			 << ", id=" << v.id << '\n'; 
		os << "... " << v.fanin.size() << " fanins=";
		for (const auto& e : v.fanin) {
			os << e->from.name << ' ';
		}
		os << '\n';
	
		os << "... " << v.fanout.size() << " fanouts=";
		for (const auto& e : v.fanout) {
			os << e->to.name << ' ';
		}
		os << '\n';
	}

	os << "------------------\n";
	os << " Vert Ptrs:\n";
	os << "------------------\n";
	for (const auto& p : _vptrs) {
		if (p != nullptr) {
			os << "ptr to " << p->name << '\n';
		}
		else {
			os << "ptr to null\n";
		}
	}

	os << "------------------\n";
	os << num_edges() << " Edges:\n";
	os << "------------------\n";
	for (const auto& e : _edges) {
		os << "id=" << e.id
			 << " ... name= " << e.from.name 
			 << "->" << e.to.name
			 << " ... weights= ";
		for (const auto& w : e.weights) {
			if (w.has_value()) {
				os << w.value() << ' ';
			}
			else {
				os << "n/a ";
			}
		}
		os << '\n';
	}

	
	os << "------------------\n";
	os << " Edges Ptrs:\n";
	os << "------------------\n";
	for (const auto& e : _eptrs) {
		if (e != nullptr) {
			os << "ptr to " << e->from.name << "->" << e->to.name << '\n';
		}
		else {
			os << "ptr to null\n";
		}
	}
}

void Ink::dump_pfxt_srcs(std::ostream& os) const {
	for (auto src : _pfxt_srcs) {
		for (auto c : src->children) {
			if (c->edge) {
				os << "insert_edge " 
					 << c->edge->from.name
					 << ' '
					 << c->edge->to.name
					 << ' ';

				for (size_t i = 0; i < NUM_WEIGHTS; i++) {
					if (c->edge->weights[i]) {
						os << *c->edge->weights[i] << ' ';
					}
					else {
						os << "n/a ";
					}
				}
				os << '\n';
			}
		}
	
	}
}

void Ink::dump_pfxt_nodes(std::ostream& os) const {
	// dump pfxt node 
	// edge id, weight sel
	for (auto& n : _pfxt_nodes) {
		if (n->edge) {
			os << n->edge->id << '\n';
		}		
	}	

}


void Ink::dump_profile(std::ostream& os) {
  os << "total expansion time=" << pfxt_expansion_time << '\n';
  os << "mark_pfxt_nodes=" << _rt_marking << '\n';
  os << "cleanup_paths=" << _rt_cleanup_paths << '\n';
  os << "cleanup_nodes=" << _rt_cleanup_nodes << '\n';
  os << "spur_loop=" << _rt_spur << '\n';

  pfxt_expansion_time = 0;
  _rt_marking = 0;
  _rt_cleanup_paths = 0;
  _rt_cleanup_nodes = 0;
  _rt_spur = 0;
}

void Ink::dump_graph_dot(std::ostream& os) const {
  os << "digraph {\n";
  for(const auto v : _vptrs) {
    if (v) {
      os << "  \"" << v->name  << "\";\n";
    }
  }

  for(const auto e : _eptrs) {
    if (e) {
      os << "  \"" << e->from.name << "\" -> \"" << e->to.name << "\"" 
         << " [label=\"" << *e->weights[0] << "\"];\n";
    }
  }
  os << "}\n";
}


float Ink::vec_diff(
	const std::vector<Path>& ps1, 
	const std::vector<Path>& ps2,
	std::vector<float>& diff) {
	
	std::vector<float> ws1;
	for (auto& p : ps1) {
		ws1.emplace_back(p.weight);
	}

	std::vector<float> ws2;
	for (auto& p : ps2) {
		ws2.emplace_back(p.weight);
	}

	std::set_difference(
		ws1.begin(), 
		ws1.end(),
		ws2.begin(),
		ws2.end(),
		std::inserter(diff, diff.begin()));


	const float diff_size = static_cast<float>(diff.size());
	return diff_size / ws1.size();
}


std::vector<float> Ink::get_path_costs() {
  std::sort(_all_path_costs.begin(), _all_path_costs.end());
  auto costs = std::move(_all_path_costs);
  _all_path_costs.clear();
  return costs;
}

std::vector<float> Ink::get_path_costs_from_cq() {
  std::vector<float> costs;
  while (_spurred_nodes.size_approx() != 0) {
    std::unique_ptr<PfxtNode> node;
    _spurred_nodes.try_dequeue(node);
    costs.push_back(node->cost);
  }
  std::sort(costs.begin(), costs.end());
  return costs;
}

void Ink::update_edges_percent(float percent) {
  size_t N = _eptrs.size() * (percent / 100.0f);
  std::cout << percent << "\% of " << _eptrs.size() << " is " << N << '\n';

  const float offset = 0.01f;
  // NOTE: we have to call insert_edge or else ink won't
  // identify the affected pfxt nodes
  //
  // TODO: does this API make sense?
  for (size_t i = 0; i < N; i++) {
    auto ws = _eptrs[i]->weights;
    for (size_t j = 0; j < NUM_WEIGHTS; j++) {
      if (ws[j]) {
        *ws[j] += offset;
      }
    }

    insert_edge(
      _eptrs[i]->from.name, 
      _eptrs[i]->to.name,
      ws[0], ws[1], ws[2], ws[3],
      ws[4], ws[5], ws[6], ws[7]
    );
  }
}


void Ink::modify_vertex(size_t vid, float offset) {
  const auto v = _vptrs[vid];
  for (const auto e : v->fanin) {
    auto ws = e->weights;
    // offset weights by a constant amount
    for (size_t i = 0; i < NUM_WEIGHTS; i++) {
      if (ws[i]) {
        *ws[i] += offset;
      }
    }
   
    // update weights 
    insert_edge(
      e->from.name, 
      e->to.name,
      ws[0], ws[1], ws[2], ws[3],
      ws[4], ws[5], ws[6], ws[7]
    );

  }

  for (const auto e : v->fanout) {
    auto ws = e->weights;
    // offset weights by a constant amount
    for (size_t i = 0; i < NUM_WEIGHTS; i++) {
      if (ws[i]) {
        *ws[i] += offset;
      }
    }
   
    // update weights 
    insert_edge(
      e->from.name, 
      e->to.name,
      ws[0], ws[1], ws[2], ws[3],
      ws[4], ws[5], ws[6], ws[7]
    );

  }
}


std::vector<std::reference_wrapper<Edge>> Ink::find_chain_edges() {
  std::vector<std::reference_wrapper<Edge>> erefs;
  for (auto e : _eptrs) {
    if (e) {
      auto f = e->from;
      auto t = e->to;
      // if I'm the only fanout of vertex f
      // and the only fanin of vertex t
      if (f.fanout.size() == 1 && t.fanin.size() == 1) {
        erefs.push_back(std::ref(*e));
      } 
    }
  }

  return erefs;
}


void Ink::_read_ops(std::istream& is, std::ostream& os) {
	std::string buf;
	while (true) {
		is >> buf;
		if (is.eof()) {
			break;
		}

		if (buf == "#") {
			std::getline(is, buf);
			continue;
		}
		
		if (buf == "report") {
			is >> buf;
			size_t num_paths = std::stoul(buf);

			// enable save_pfxt_nodes or not
			is >> buf;
			bool save_pfxt_nodes = std::stoi(buf);

      // enable recover path or not
      is >> buf;
      bool recover_paths = std::stoi(buf);

			report_incsfxt(num_paths, save_pfxt_nodes, recover_paths);
		}

		if (buf == "insert_edge") {
			std::array<std::optional<float>, 8> ws;
			is >> buf;
			std::string from(buf);
			is >> buf;
			std::string to(buf);
			
			for (size_t i = 0; i < NUM_WEIGHTS; i++) {
				is >> buf;
				if (buf == "n/a") {
					ws[i] = std::nullopt;
				}
				else {
					ws[i] = stof(buf);
				}
			}

			insert_edge(from, to,
				ws[0], ws[1], ws[2], ws[3],
				ws[4], ws[5], ws[6], ws[7]);
		}


	}
	
	std::cout << "finished reading " << num_edges() << " edges.\n";	
	std::cout << "finished reading " << num_verts() << " vertices.\n";
	
}

Edge& Ink::_insert_edge(
	Vert& from, 
	Vert& to, 
	std::array<std::optional<float>, 8>&& ws) {
	
	auto id = _idxgen_edge.get();
	auto& e = _edges.emplace_front(from, to, id, std::move(ws));
	
	// cache the edge's satellite iterator
	// in the owner storage
	e.satellite = _edges.begin();
	
	// resize _eptr if the generated index goes out of range
	if (id + 1 > _eptrs.size()) {
		_eptrs.resize(id + 1, nullptr);
	}

	// store pointer to this edge object
	_eptrs[id] = &e;

	// update fanin, fanout
	// cache fanin, fanout satellite iterators
	// (for edge removal)
	e.from.insert_fanout(e);
	e.to.insert_fanin(e);
	return e; 
}


void Ink::_remove_edge(Edge& e) {
	// update fanout of v_from, fanin of v_to
	e.from.remove_fanout(e);
	e.to.remove_fanin(e);


	// record from
	if (_global_sfxt) {
		auto v = e.from.id;
		if (!_vptrs[v]->is_in_update_list) {
			_to_update.emplace_back(v);
			_vptrs[v]->is_in_update_list = true;
		}
	}


	// remove pointer mapping and recycle free id
	_eptrs[e.id] = nullptr;
	_idxgen_edge.recycle(e.id);
	
	// remove this edge from the owner storage
	assert(e.satellite);
	_edges.erase(*e.satellite);
}

void Ink::_topologize(Sfxt& sfxt, size_t v) const {	
	// set visited to true
	sfxt.visited[v] = true;
	auto& vert = _vptrs[v];
	// base case: stop at path source
	if (!vert->is_src()) {
		for (auto& e : vert->fanin) {
			if (!sfxt.visited[e->from.id]) {
				_topologize(sfxt, e->from.id);
			}			
		}
	}
	
	sfxt.topo_order.push_back(v);
}

void Ink::_build_sfxt(Sfxt& sfxt) const {
	// NOTE: super source and target are implicitly generated
	assert(sfxt.topo_order.empty());
	// generate topological order of vertices
	if (sfxt.S > sfxt.T) {
		_topologize(sfxt, sfxt.T);
	}
	else {
		// topologizing for a global sfxt
		for (const auto& ept : _endpoints) {
			_topologize(sfxt, ept.vert.id);
    }
	}

	assert(!sfxt.topo_order.empty());
  
	// visit the topological order in reverse
	for (auto itr = sfxt.topo_order.rbegin(); 
		itr != sfxt.topo_order.rend(); 
		++itr) {
		auto v = *itr;
		auto v_ptr = _vptrs[v]; 
		assert(v_ptr != nullptr);	

		if (v_ptr->is_src()) {
			sfxt.srcs.try_emplace(v, 0.0f);
			continue;
		}


		for (auto e : v_ptr->fanin) {
			auto w_sel = e->min_valid_weight();

			// relaxation
			if (w_sel != NUM_WEIGHTS) {
				auto d = *e->weights[w_sel];
				sfxt.relax(e->from.id, v, _encode_edge(*e, w_sel), d);	
			}
		}

	}	

}

void Ink::_sfxt_cache() {
	
	// now id 0 and 1 are reserved for super source and super target
	size_t S = 0, T = 1;
  
	if (!_global_sfxt) {
		// it's our first time constructing a global sfxt
		_global_sfxt = Sfxt(S, T, _vptrs.size());
		auto& sfxt = *_global_sfxt;
		assert(!sfxt.dists[T]);
		sfxt.dists[T] = 0.0f;
	
		// relax from destinations to super target
		for (auto& p : _endpoints) {
			sfxt.relax(p.vert.id, T, std::nullopt, 0.0f);
		}
		
    _build_sfxt(sfxt);

		// relax from super source to sources
		for (auto& [src, w] : sfxt.srcs) {
      sfxt.relax(S, src, std::nullopt, *w);
		}
	}
	else {
		// we have an existing suffix tree
		auto& sfxt = *_global_sfxt;
		auto sz = _vptrs.size();
		sfxt.dists.resize(sz);
		sfxt.successors.resize(sz);
		sfxt.links.resize(sz);
		sfxt.srcs.clear();

		// update sources and destinations
		for (auto v : _vptrs) {
			if (v != nullptr && v->is_src()) {
				sfxt.srcs.try_emplace(v->id, 0.0f);
			}

      if (v != nullptr && v->is_dst()) {
        sfxt.dists[v->id] = 0.0f;
      }
		}
				
		// identify the affected region of vertices
		// generate the topological order with DFS
		std::vector<bool> checked(_vptrs.size(), false);
		std::deque<size_t> tpg;
    for (const auto& v : _to_update) {
      if (_vptrs[v] && !checked[v]) {
				_dfs(v, tpg, checked);
			}
		}
	

		// iterate through the topological order and re-relax
		for (const auto& v : tpg) {
			auto vptr = _vptrs[v];
			sfxt.dists[v].reset();

			if (vptr->is_dst()) {
				sfxt.links[v].reset();
				sfxt.successors[v] = sfxt.T;
				continue;
			}

			// for each of v's fanout we redo relaxation
			for (auto e : vptr->fanout) {
        auto t = e->to.id;
				auto w_sel = e->min_valid_weight();
				if (w_sel != NUM_WEIGHTS) {
					auto d = *e->weights[w_sel];
          sfxt.relax(v, t, _encode_edge(*e, w_sel), d);
        }
			}	
		}	
		
				
		// relax from destinations to super target
		for (auto& p : _endpoints) {
			sfxt.relax(p.vert.id, T, std::nullopt, 0.0f);
		}

		// relax from super source to sources
		sfxt.dists[S].reset();
		for (auto& [src, w] : sfxt.srcs) {
      sfxt.relax(S, src, std::nullopt, *w);
		}

	}
}

void Ink::_sfxt_rebuild() {
	// now id 0 and 1 are reserved for super source and super target
	size_t S = 0, T = 1;
	
  _global_sfxt = Sfxt(S, T, _vptrs.size());
	auto& sfxt = *_global_sfxt;
	assert(!sfxt.dists[T]);
	sfxt.dists[T] = 0.0f;
	// relax from destinations to super target
	for (auto& p : _endpoints) {
		sfxt.relax(p.vert.id, T, std::nullopt, 0.0f);
	}
  
	_build_sfxt(sfxt);
	
	// relax from super source to sources
	for (auto& [src, w] : sfxt.srcs) {
		w = 0.0f;
		sfxt.relax(S, src, std::nullopt, *w);
	}

}

Sfxt Ink::_sfxt_cache(const Point& p) const {
	auto S = _vptrs.size();
	auto to = p.vert.id;

	Sfxt sfxt(S, to, S + 1);

	assert(!sfxt.dists[to]);
	sfxt.dists[to] = 0.0f;
	
	// calculate shortest path tree with dynamic programming
	_build_sfxt(sfxt);

	// relax from super source to sources
	for (auto& [src, w] : sfxt.srcs) {
		w = 0.0f;
		sfxt.relax(S, src, std::nullopt, *w);
	}

	return sfxt;
}


Pfxt Ink::_pfxt_cache(const Sfxt& sfxt) {
	Pfxt pfxt(sfxt);
	assert(sfxt.dist());

	// generate path prefix from each source vertex
	for (const auto& [src, w] : sfxt.srcs) {
		if (!w || !sfxt.dists[src]) {
			continue;
		}

		auto cost = *sfxt.dists[src] + *w;
    auto node = pfxt.push(cost, *w, sfxt.S, src, nullptr, nullptr, std::nullopt);

		// cache pfxt src nodes
		pfxt.srcs.emplace_back(node);	
	}
	return pfxt;
}

Pfxt Ink::_pfxt_cache_multiq(const Sfxt& sfxt) {
	Pfxt pfxt(sfxt);
	assert(sfxt.dist());
  set_num_task_qs(pfxt, num_task_qs);
  assert(!pfxt.task_qs.empty());

	// generate path prefix from each source vertex
  for (const auto& [src, w] : sfxt.srcs) {
		if (!w || !sfxt.dists[src]) {
			continue;
		}

		auto cost = *sfxt.dists[src] + *w;
    pfxt.push_task(*this, cost, *w, sfxt.S, src, nullptr, nullptr, std::nullopt);
  }
	return pfxt;
}



void Ink::_update_pfxt(Pfxt& pfxt, float& max_dc) {

  // reset node markings
  for (auto& n : pfxt.paths) {
    n->reset();
  }

  for (auto& n : pfxt.nodes) {
    n->reset();
  }

	auto& ap = _affected_pfxtnodes;
	if (ap.empty()) {
    return;
	}	

  std::sort(ap.begin(), ap.end(), [](auto a, auto b) {
    return a->level < b->level;
  });
  
  std::queue<PfxtNode*> q;
	auto& sfxt = *_global_sfxt;
	for (auto p : ap) {
		if (p->updated || p->removed) {
			continue;
		}

    // add all p's siblings to the queue
    for (auto s : p->parent->children) {
      q.push(s);
    }


		while (!q.empty()) {
			bool skip{false};
      auto node = q.front();
			q.pop();

      // if my parent is marked as to be re-spured
      // that means a sibling has been marked as
      // removed before me, then mark myself as
      // removed too
      if (node->parent->to_respur) {
        node->removed = true;
      }

      if (node->removed) {
        skip = true;
      }

			if (!skip) {
				auto [eptr, w_sel] = _decode_edge(*node->link);
				if (!belongs_to_sfxt[eptr->id][w_sel]) {
          node->updated = true;
					// update cost
					auto v = node->to;
					auto u = node->from;
					auto w = *eptr->weights[w_sel];
					auto dc = *sfxt.dists[v] + w - *sfxt.dists[u];
					node->detour_cost = dc + node->parent->detour_cost; 
					node->parent->pruned.emplace_back(eptr->id, w_sel);
        }
				else {
					node->removed = true;
          if (!node->parent->to_respur) {
						_respurs.emplace_back(node->parent, std::move(node->parent->pruned));
					  node->parent->to_respur = true;
          }
        }
			}

			for (auto c : node->children) {
				q.push(c);
        if (node->removed) {
          c->removed = true;
        }
			}

		}
	}

 
	// iterate through the re-spur list
	// and spur each pfxt node with their
	// corresponding pruned edge/weights
	for (const auto& r : _respurs) {
    _spur_pruned(pfxt, *r.node, r.pruned);
  }

  // validate paths and update max detour cost
  _all_path_costs.clear();
  for (const auto& n : pfxt.paths) {
    if (!n->removed) {
      _all_path_costs.push_back(n->detour_cost + *sfxt.dist());

      // may have detour candidates now
      // spur this node
      //if (n->num_children() == 0) {
      //  _spur(pfxt, *n);
      //}   
      
      max_dc = std::max(n->detour_cost, max_dc);
    }
  }

  // TODO: bug
  // why? after paths validation, we almost have the right costs
  // but spur_incremental then seems to spur a lot of redundant nodes
  //std::ofstream ofs("allcosts-dmp");
  //std::sort(_all_path_costs.begin(), _all_path_costs.end());
  //for (auto& c : _all_path_costs) {
  //  ofs << c << '\n';
  //}
  
}

void Ink::_spur_incsfxt(
	size_t K, 
	PathHeap& heap, 
	Pfxt& pfxt, 
	bool save_pfxt_nodes,
	bool recover_paths) {
	
  auto beg = std::chrono::steady_clock::now();
  auto& sfxt = *_global_sfxt;
  while (!pfxt.num_nodes() == 0) {
    auto node = pfxt.pop();

		// no more paths to generate
		if (node == nullptr) {
			break;
		}		

		
		// expand search space
		_spur(pfxt, *node);

    if (recover_paths) {
			// recover the complete path
			auto path = std::make_unique<Path>(0.0f, nullptr);
			_recover_path(*path, sfxt, node, sfxt.T);
			path->weight = path->back().dist;
			_all_paths.push_back(std::move(path));
		}
		else {
      _all_path_costs.push_back(node->cost);
    }
		
    
    if (_all_paths.size() >= K || _all_path_costs.size() >= K) {
			break;
		}
  }

  auto end = std::chrono::steady_clock::now();
  pfxt_expansion_time = 
	  std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();

  if (save_pfxt_nodes) {
		_pfxt_srcs = std::move(pfxt.srcs);
		_pfxt_nodes = std::move(pfxt.nodes);
		_pfxt_paths = std::move(pfxt.paths);
	}	
}

void Ink::_spur_incremental(
	size_t K, 
	Pfxt& pfxt,
	bool save_pfxt_nodes,
	bool use_leaders,
	bool recover_paths) {

  float max_dc = 0.0f;  
	auto& sfxt = *_global_sfxt;
	
  auto beg = std::chrono::steady_clock::now();
  _update_pfxt(pfxt, max_dc);
  pfxt.heapify();


  bool valid{false};
	while (!(pfxt.num_nodes() == 0)) {
		auto node = pfxt.pop();  
    
    // no more paths to generate
		if (node == nullptr) {
			break;
		}

		if (valid && 
        (_all_paths.size() >= K || _all_path_costs.size() >= K)) {
			break;
		}

    if (node->removed) {
      continue;
    }

		if (recover_paths) {
			// recover the complete path
			auto path = std::make_unique<Path>(0.0f, nullptr);
			_recover_path(*path, sfxt, node, sfxt.T);
			path->weight = path->back().dist;
			_all_paths.push_back(std::move(path));
		}
		else {
			_all_path_costs.push_back(node->detour_cost + *sfxt.dist());
    }

		// expand search space
		// find children (more detours) for node
		_spur(pfxt, *node);

    if (!valid && node->detour_cost >= max_dc) {
      //std::cout << "costs.size=" << _all_path_costs.size() << '\n';
      valid = true;
    }
	}
  auto end = std::chrono::steady_clock::now();
  pfxt_expansion_time = 
		std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();


	if (save_pfxt_nodes) {
		_pfxt_srcs = std::move(pfxt.srcs);
		_pfxt_nodes = std::move(pfxt.nodes);
		_pfxt_paths = std::move(pfxt.paths);
	}
} 

void Ink::_spur_async(
	size_t K,
  Pfxt& pfxt, 
	bool recover_paths) {

  auto beg = std::chrono::steady_clock::now();  
  auto& sfxt = *_global_sfxt;
  while (!pfxt.par_prq.empty()) {
		auto node = pfxt.pop_par();
    if (node == nullptr) {
      break;
    }

    // expand search space
		_spur_async(pfxt, *node);

    if (recover_paths) {
			// recover the complete path
			auto path = std::make_unique<Path>(0.0f, nullptr);
			_recover_path(*path, sfxt, node, sfxt.T);
			path->weight = path->back().dist;
			_all_paths.push_back(std::move(path));
		}
		else {
      _all_path_costs.push_back(node->cost);
    }
		
    if (_all_paths.size() >= K || _all_path_costs.size() >= K) {
			break;
		}
  }

  auto end = std::chrono::steady_clock::now();  
  pfxt_expansion_time = 
	  std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();
}

void Ink::_spur_multiq(
	size_t K,
  Pfxt& pfxt, 
	bool recover_paths,
  bool enable_node_redistr) {
  _atom_path_cnt = 0;

  if (_num_workers == 0) {
    // then by default use maximum hardware concurrency
    _num_workers = std::thread::hardware_concurrency();
  }
  assert(_num_workers > 0);
  //std::cout << "using " << _num_workers << " workers.\n";
  tf::Executor executor{_num_workers};

  // resize workload monitor storage
  _total_workloads.resize(_num_workers);
  std::fill(_total_workloads.begin(), _total_workloads.end(), 0);

  //std::cout << "bulk size = ";
  //if (_bulk_size == 0) {
  //  std::cout << "random.\n";
  //}
  //else {
  //  std::cout << _bulk_size << '\n';
  //}
  
  auto K_overgrow = static_cast<float>(K) * overgrow_scalar; 
  //std::cout << "K overgrow=" << K_overgrow << '\n';
  //if (spur_ahead) {
  //  std::cout << "spur_ahead enabled.\n";
  //}

  auto beg = std::chrono::steady_clock::now();  
  auto& sfxt = *_global_sfxt;
  // TODO: how would we know if the N queues are "really" empty and
  // stop the loop? In real benchmarks, it's extremely unlikely to
  // run out of nodes, but we have to deal with this scenario
  std::atomic<size_t> redistr_cnt{0};
  while (_atom_path_cnt < K_overgrow) {
    executor.silent_async([=, &pfxt, &redistr_cnt, &executor]() {
      size_t q_id = 0;
      while (pfxt.task_qs[q_id].size_approx() == 0) {
        if (q_id == num_task_qs - 1) {
          break;
        }  
        q_id++;
      }
      
      if (updating_bounds && spur_ahead) {
        for (size_t id = q_id; id < num_task_qs - 1; id++) {
          executor.silent_async([=, &pfxt, &executor]() {
            auto nodes = pfxt.pop_task(id, 2);
            if (nodes.empty()) {
              return;
            }
            assert(!nodes.empty());
            size_t sz{0};
            std::for_each(nodes.begin(), nodes.end(), [&pfxt, &sz, &executor, this](auto& n){
              if (!n) {
                return;
              }
              sz++;
              _spur_multiq(pfxt, *(n.get()), executor);
              pfxt.paths_concurr.enqueue(std::move(n));
            });

            _atom_path_cnt += sz;
          }); 
        }
        return;
      }
    
      assert(q_id < num_task_qs); 
      // if we have moved on to the queue with
      // the lowest priority, we should update
      // the bounds and redistribute the nodes 
      if (q_id == num_task_qs - 1 && enable_node_redistr) {
        bool expected{false};
        if (!updating_bounds.compare_exchange_weak(expected, true)) {
          return;
        }
        redistr_cnt++;
        auto init = bounds.back();

        if (policy == PartitionPolicy::EQUAL) {
          for (size_t i = 0; i < num_task_qs - 1; i++) {
            bounds[i] = init + _width * i; 
          }
        }
        else if (policy == PartitionPolicy::GEOMETRIC) {
          for (size_t i = 0; i < num_task_qs - 1; i++) {
            bounds[i] = init + std::pow(base, i + 1);
          } 
        }
       
        // move candidate nodes to a temp. storage 
        _tmp_q = std::move(pfxt.task_qs[q_id]);

        // redistribute tasks to their corresponding queues
        while (!_tmp_q.size_approx() == 0) {
          std::unique_ptr<PfxtNode> node;
          _tmp_q.try_dequeue(node);
          auto q_idx = determine_q_idx(node->cost);
          pfxt.task_qs[q_idx].enqueue(std::move(node));
        }

        // unlock critical section
        updating_bounds = false;
        return;
      }
      
      if (_bulk_size != 1) {
        std::uniform_int_distribution<size_t> distr(2, 8);
        auto bs = (_bulk_size == 0) ? distr(rng) : _bulk_size; 
        auto nodes = pfxt.pop_task(q_id, bs);
        if (nodes.empty()) {
          return;
        }
        assert(!nodes.empty());
        size_t sz{0};
        std::for_each(nodes.begin(), nodes.end(), [&pfxt, &sz, &executor, this](auto& n){
          if (!n) {
            return;
          }
          sz++;
          _spur_multiq(pfxt, *(n.get()), executor);
          pfxt.paths_concurr.enqueue(std::move(n));
        });
        
        _atom_path_cnt += sz;
      }
      else {
        auto node = pfxt.pop_task(q_id);
        if (node == nullptr) {
          return;
        }
        _spur_multiq(pfxt, *node, executor);
        _atom_path_cnt++;
      } 
    });  
  }
  executor.wait_for_all();

  auto end = std::chrono::steady_clock::now();  
  pfxt_expansion_time = 
	  std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();
  _spurred_nodes = std::move(pfxt.paths_concurr); 
  //std::cout << "redistributed tasks " << redistr_cnt << " times.\n";
}

void Ink::_spur_multiq_relaxed(
	size_t K,
  Pfxt& pfxt, 
	bool recover_paths) {
  //_atom_path_cnt = 0;
  //if (_num_workers == 0) {
  //  // then by default use maximum hardware concurrency
  //  _num_workers = std::thread::hardware_concurrency();
  //}
  //assert(_num_workers > 0);
  //std::cout << "using " << _num_workers << " workers.\n";
  //tf::Executor executor{_num_workers};

  //std::cout << "bulk size = ";
  //if (_bulk_size == 0) {
  //  std::cout << "random.\n";
  //}
  //else {
  //  std::cout << _bulk_size << '\n';
  //}
  //
  //auto beg = std::chrono::steady_clock::now();  
  //auto& sfxt = *_global_sfxt;
  //
  //auto K_overgrow = static_cast<float>(K) * overgrow_scalar; 
 
  //std::atomic<int> occupied{0};

  //while (_atom_path_cnt < K_overgrow) {
  //  executor.silent_async([=, &pfxt, &executor, &occupied]() {

  //    // this returns 1, if first set bit is at index 0
  //    // 2, at index 1
  //    // ... etc.
  //    // 0 means no bit is set
  //    auto qid = __builtin_ffs(~occupied);
  //    qid--;
  //    
  //    if (qid == -1) {
  //      // all queues are occupied 
  //      // won't happen when workers are less than queues
  //      return;
  //    }


  //    // sometimes a worker gets assigned an unoccupied queue
  //    // with 0 nodes, we have no choice but to move on to the
  //    // next closest non-empty queue
  //    while (pfxt.task_qs[qid].size_approx() == 0) {
  //      qid++;
  //      if (qid == num_task_qs - 1) {
  //        break;
  //      }
  //    }


  //    // if qid is the last queue
  //    if (qid == num_task_qs - 1) {
  //      bool expected{false};
  //      if (!updating_bounds.compare_exchange_weak(expected, true)) {
  //        return;
  //      }
  //    
  //      // update bounds and redistribute tasks
  //      auto init = bounds.back();
  //      
  //      for (size_t i = 0; i < num_task_qs - 1; i++) {
  //        bounds[i] = init + _width * (i + 1); 
  //      }
  //     
  //      // move candidate nodes to a temp. storage 
  //      _tmp_q = std::move(pfxt.task_qs[qid]);

  //      // redistribute tasks to the responsible queues
  //      while (!_tmp_q.size_approx() == 0) {
  //        std::unique_ptr<PfxtNode> node;
  //        _tmp_q.try_dequeue(node);
  //        auto id = determine_q_idx(node->cost);
  //        pfxt.task_qs[id].enqueue(std::move(node));
  //      }
  //      
  //      // reset occupied bit vector
  //      // no one is occupied now
  //      occupied = 0;

  //      // unlock critical section
  //      updating_bounds = false;
  //      return;
  //    }



  //    auto node = pfxt.pop_task(qid);
  //    if (node == nullptr) {
  //      return;
  //    }
  //    manipulate_bit(occupied, qid, set_bit);
  //    _spur_multiq(pfxt, *node, executor);
  //    manipulate_bit(occupied, qid, clr_bit);
  //    _atom_path_cnt++;
  //  });
  //
  //} 
  //executor.wait_for_all();

  //auto end = std::chrono::steady_clock::now();  
  //pfxt_expansion_time = 
	//  std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();
  //_spurred_nodes = std::move(pfxt.paths_concurr); 
}





void Ink::_mark_pfxt_nodes(Pfxt& pfxt) { 
  auto sfxt = *_global_sfxt;
  std::queue<PfxtNode*> q;
  for (auto s : pfxt.srcs) {
    auto v = s->to;
    // NOTE: we assume weight from S to primary inputs are 0
    // but in real circuits this might change
    auto w = 0.0f;
    // update root pfxt node cost
    if (v == *sfxt.successors[sfxt.S]) {
      s->detour_cost = 0.0f;
      s->cost = *sfxt.dist();
    }
    else {
      s->detour_cost = *sfxt.dists[v] + w - *sfxt.dist();
      s->cost = s->detour_cost + *sfxt.dist(); 
    }

    for (auto c : s->children) {
      q.push(c);
    }
  }

  int spur_begin, to_spurs, total_spurs, updates_per_parent;
  PfxtNode* curr_parent{nullptr};

  while (!q.empty()) {
    auto skip{false};
    auto node = q.front();
    q.pop();
    
    auto p = node->parent;
    // if parent is already spurred
    // and we can skip to the next iteration
    if (p->spurred || p->removed) {
      node->removed = true;
      skip = true;
    }
        
    if (!skip) {
      // if parented to a different node 
      // we re-initialize spur_begin and spur_cnt
      // and record current parent
      auto u = node->from;
      auto v = node->to;
      if (p != curr_parent) {
        spur_begin = p->to;
        curr_parent = p;
        updates_per_parent = 0;

        // count non-sfxt edges (multiple weights)
        to_spurs = total_spurs = -1;
        for (auto e : _vptrs[spur_begin]->fanout) {
          for (size_t i = 0; i < NUM_WEIGHTS; i++) {
            if (e->weights[i]) {
              to_spurs++;
              total_spurs++;
            }
          }
        } 
      }
      
      auto [eptr, w_sel] = _decode_edge(*node->link);
      // if node.edge is non-sfxt edge, update deviation cost
      if (node->edge && sfxt.links[u] && *node->link != *sfxt.links[u]) {
        node->updated = true;
        updates_per_parent++;
        // update cumulative deviation cost
        auto w = *eptr->weights[w_sel];
        auto dc = *sfxt.dists[v] + w - *sfxt.dists[u];
        node->detour_cost = dc + p->detour_cost; 
        node->cost = node->detour_cost + *sfxt.dist(); 

        // finished updating a non-sfxt edge, decrement to_spurs 
        // if to_spurs == 0, spur_begin can advance to its successor
        to_spurs--;
        while (to_spurs == 0 && sfxt.successors[spur_begin]) {
          // update spur_begin
          spur_begin = *sfxt.successors[spur_begin];
          // update to_spurs and total_spurs
          to_spurs = total_spurs = -1;
          for (auto e : _vptrs[spur_begin]->fanout) {
            for (size_t i = 0; i < NUM_WEIGHTS; i++) {
              if (e->weights[i]) {
                to_spurs++;
                total_spurs++;
              }
            }
          } 
        }  

        // if I'm the last child of this parent
        // but to_spur is still non-zero, that means my
        // parent has undiscovered children 
        // spur my parent here
        if (updates_per_parent == p->num_children()) {
          _spur_midway(
            pfxt, 
            *p, 
            spur_begin, 
            total_spurs - to_spurs, 
            updates_per_parent);
        }

      }
      // node.edge is sfxt edge or null edge
      else {
        node->removed = true;
        // node disappears from prefix tree, spur parent
        p->spurred = true;
        _spur_midway(
          pfxt, 
          *p, 
          spur_begin, 
          total_spurs - to_spurs, 
          updates_per_parent);
      }
    }

    for (auto c : node->children) {
      q.push(c); 
    }
  }
}


void Ink::_spur_incremental_v2(
  size_t K,
  Pfxt& pfxt,
  bool save_pfxt_nodes,
  bool recover_paths) {
  auto& sfxt = *_global_sfxt;
  
  auto beg = std::chrono::steady_clock::now();
  _mark_pfxt_nodes(pfxt);
  auto end = std::chrono::steady_clock::now();
	_rt_marking = 
    std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();
  
  // iterate through pfxt.paths
  // clean up removed paths, spur any leaf nodes
  // record max deviation cost
  beg = std::chrono::steady_clock::now();
  float max_dc{0.0f};
  for (size_t i = 0; i < pfxt.paths.size();) {
    auto& node = pfxt.paths[i];
    auto n_obs = node.get();
    if (node->removed) {
      node = std::move(pfxt.paths.back());
      pfxt.paths.pop_back();
    } 
    else {
      i++;
      max_dc = std::max(max_dc, node->detour_cost);
      if (recover_paths) {
        // recover the complete path
        auto path = std::make_unique<Path>(0.0f, nullptr);
        _recover_path(*path, sfxt, n_obs, sfxt.T);
        path->weight = path->back().dist;
        _all_paths.push_back(std::move(path));
      }
      else {
        _all_path_costs.push_back(node->cost);
      }

      if (node->num_children() == 0) {
        _spur(pfxt, *n_obs); 
      }

      // clear spurred flag for next iteration
      node->spurred = false;
    }
  }
  end = std::chrono::steady_clock::now();
	_rt_cleanup_paths = 
    std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();

  beg = std::chrono::steady_clock::now();
  for (size_t i = 0; i < pfxt.nodes.size();) {
    auto& node = pfxt.nodes[i];
    if (node->removed) {
      node = std::move(pfxt.nodes.back());
      pfxt.nodes.pop_back();
    }
    else {
      // clear spurred flag for next iteration
      node->spurred = false;
      i++;
    }
    
  }
  pfxt.heapify();
  end = std::chrono::steady_clock::now();
  _rt_cleanup_nodes = 
		std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();


  beg = std::chrono::steady_clock::now();
  auto valid{false};
  while (!pfxt.num_nodes() == 0) {
    auto node = pfxt.pop();  
    
    // no more paths to generate
		if (node == nullptr) {
			break;
		}

    if (!valid && node->detour_cost >= max_dc) {
      valid = true;
    }

		if (node->removed) {
      continue;
    }

		// expand search space
		// find children (more detours) for node
		_spur(pfxt, *node);
    
    if (recover_paths) {
			// recover the complete path
			auto path = std::make_unique<Path>(0.0f, nullptr);
			_recover_path(*path, sfxt, node, sfxt.T);
			path->weight = path->back().dist;
			_all_paths.push_back(std::move(path));
		}
		else {
			_all_path_costs.push_back(node->cost);
    }

    if (valid && 
        (_all_paths.size() >= K || _all_path_costs.size() >= K)) {
			break;
		}

  }
  end = std::chrono::steady_clock::now();
  _rt_spur = 
		std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();
 

  pfxt_expansion_time = 
    // _rt_marking + _rt_cleanup_paths + _rt_cleanup_nodes + _rt_spur;
    _rt_marking + _rt_spur;

  if (save_pfxt_nodes) {
		_pfxt_srcs = std::move(pfxt.srcs);
		_pfxt_nodes = std::move(pfxt.nodes);
		_pfxt_paths = std::move(pfxt.paths);
	}

}

std::vector<Path> Ink::report_incremental_v2(
	size_t K,
	bool save_pfxt_nodes,
	bool recover_paths) {
	if (K == 0) {
		return {};
	}
	
	// TODO: K = 1
	if (K == 1) {
	
	}

	// incremental: clear endpoint storage
	_endpoints.clear();

	// scan and add the out-degree=0 vertices to the endpoint vector
	for (const auto v : _vptrs) {
		if (v != nullptr && v->is_dst()) {
			_endpoints.emplace_back(*v, 0.0f);	
		}
	}

  pfxt_expansion_time = 0;

	_sfxt_cache();
  auto& sfxt = *_global_sfxt;
  auto pfxt = _pfxt_cache(sfxt);
	pfxt.nodes = std::move(_pfxt_nodes);
	pfxt.paths = std::move(_pfxt_paths);
  pfxt.srcs = std::move(_pfxt_srcs);

	_spur_incremental_v2(K, pfxt, save_pfxt_nodes, recover_paths);	

	// report complete, clear the update list
	_clear_update_list();	

	return _extract_paths(_all_paths);
}


void Ink::_spur_parallel(
  size_t K,
  bool recover_paths) {
  std::atomic<bool> finished{false};
 
//  // TODO: how would I know if the prefix tree has fully grown?
//  // _standybys.size_approx() is not necessarily accurate
//
//
//  while (!finished) {
//    _executor.silent_async([&]() {
//      if (_atom_path_cnt.load() >= K) {
//        finished = true;
//        return;
//      }
//      
//      // dequeue a pfxt node from the standby queue
//      std::unique_ptr<PfxtNode> node;
//      _standbys.try_dequeue(node);
//
//      if (node && !finished) {
//        // approximate new avg.
//        auto size = _standbys.size_approx();
//        auto obs = node.get();
//        auto old_avg = _atom_avg_dc.load();
//        size_t local_avg{0};
//        if (size == 0) {
//          // I got the only node left in the queue
//          // must spur this node
//          _spur_no_pq(*obs, local_avg);
//          _paths.try_enqueue(std::move(node)); 
//          
//          // update path count
//          _atom_path_cnt.fetch_add(1); 
//
//          // update avg. = (current avg + local avg) / 2
//          while (!_atom_avg_dc.compare_exchange_weak(
//                  old_avg, 
//                  (old_avg + local_avg) / 2)
//          );
//        }
//        else {
//          auto new_avg = (old_avg * (size + 1) - node->detour_cost * AVG_DC_SCALE) / size;
//          if (new_avg >= old_avg) {
//            // I have popped a node with relatively small cost
//            // should spur this node
//            _spur_no_pq(*obs, local_avg);
//            _paths.try_enqueue(std::move(node)); 
//          
//            // update path count
//            _atom_path_cnt.fetch_add(1); 
//          
//            // update avg. = (current avg + local avg) / 2
//            while (!_atom_avg_dc.compare_exchange_weak(
//                    old_avg, 
//                    (old_avg + local_avg) / 2)
//            );
//          }
//          else {
//            // this node should go back and wait
//            _standbys.try_enqueue(std::move(node));
//          }
//        }
//        
//      }
//      else {
//        return;
//      }
//    }); 
//  }
//
//  _executor.wait_for_all();
}


std::vector<Path> Ink::report_parallel(
  size_t K,
  bool recover_paths) {
  if (K == 0) {
		return {};
	}
	
	// TODO: K = 1
	if (K == 1) {
	
	}

	// incremental: clear endpoint storage
	_endpoints.clear();

	// scan and add the out-degree=0 vertices to the endpoint vector
	for (const auto v : _vptrs) {
		if (v != nullptr && v->is_dst()) {
			_endpoints.emplace_back(*v, 0.0f);	
		}
	}

  pfxt_expansion_time = 0;

	_sfxt_cache();
	auto& sfxt = *_global_sfxt;
	
  // initialize concurrent queue of pfxt nodes
  // i.e. generate prefices for graph source
  float sum{0};
  for (const auto& [src, w] : sfxt.srcs) {
		if (!w || !sfxt.dists[src]) {
			continue;
		}
		auto cost = *sfxt.dists[src] + *w;
    _standbys.try_enqueue(
      std::make_unique<PfxtNode>(
        cost, *w, sfxt.S, src,
        nullptr, nullptr, std::nullopt)
    );
  }

  // initialize avg. deviation cost
  _atom_avg_dc = 0;

  _spur_parallel(K, recover_paths);	
	
	// report complete, clear the update list
	_clear_update_list();		
	
	return {};
}


void Ink::_spur_rebuild(size_t K, PathHeap& heap, bool recover_paths) {
 	auto& sfxt = *_global_sfxt;
	auto pfxt = _pfxt_cache(sfxt);

	auto beg = std::chrono::steady_clock::now();
	while (!pfxt.num_nodes() == 0) {
		auto node = pfxt.pop();
		// no more paths to generate
		if (node == nullptr) {
			break;
		}		

		if (heap.size() >= K || _all_path_costs.size() >= K) {
			break;
		}

		// recover the complete path
    if (recover_paths) { 
      auto path = std::make_unique<Path>(0.0f, nullptr);
      _recover_path(*path, sfxt, node, sfxt.T);

      path->weight = path->back().dist;
      heap.push(std::move(path));
      heap.fit(K);
    }
    else {
			_all_path_costs.push_back(node->detour_cost + *sfxt.dist());
    }
    

		// expand search space
		_spur(pfxt, *node);
	}
	auto end = std::chrono::steady_clock::now();
	pfxt_expansion_time = 
		std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count();
} 

void Ink::_spur(Point& endpt, size_t K, PathHeap& heap) {
	auto sfxt = _sfxt_cache(endpt);
	auto pfxt = _pfxt_cache(sfxt);

	for (size_t k = 0; k < K; k++) {
		auto node = pfxt.pop();	
		// no more paths to generate
		if (node == nullptr) {
			break;
		}		


		if (heap.size() >= K) {
			break;
		}

		// recover the complete path
		auto path = std::make_unique<Path>(0.0f, &endpt);
		_recover_path(*path, sfxt, node, sfxt.T);
		
		path->weight = path->back().dist;
		if (path->size() > 1) {
			heap.push(std::move(path));
			heap.fit(K);
		}

		// expand search space
		_spur(pfxt, *node);
	}

} 

void Ink::_spur(Pfxt& pfxt, PfxtNode& pfx) {
	auto u = pfx.to;

	while (u != pfxt.sfxt.T) {
		//assert(pfxt.sfxt.links[u]);
		auto uptr = _vptrs[u];

		for (auto edge : uptr->fanout) {
      for (size_t w_sel = 0; w_sel < NUM_WEIGHTS; w_sel++) {
				if (!edge->weights[w_sel]) {
					continue;
				}

				// skip if the edge goes outside of the suffix tree
				// which is unreachable
				auto v = edge->to.id;
				if (!pfxt.sfxt.dists[v]) {
					continue;
				}

				// skip if the edge belongs to the suffix 
				// NOTE: we're detouring, so we don't want to
				// go on the same paths explored by the suffix tree
				if (pfxt.sfxt.links[u] &&
						_encode_edge(*edge, w_sel) == *pfxt.sfxt.links[u]) {
					continue;
				}
			
				auto w = *edge->weights[w_sel];
				auto detour_cost = *pfxt.sfxt.dists[v] + w - *pfxt.sfxt.dists[u]; 
				
				auto c = detour_cost + pfx.cost;
        auto dc = detour_cost + pfx.detour_cost;
				pfxt.push(c, dc, u, v, edge, &pfx, _encode_edge(*edge, w_sel));
			}
		}

		u = *pfxt.sfxt.successors[u];
	}
}

void Ink::_spur_async(Pfxt& pfxt, PfxtNode& pfx) {
	auto u = pfx.to;
	while (u != pfxt.sfxt.T) {
		_executor.silent_async([=, &pfxt, &pfx]() {
      auto uptr = _vptrs[u];
      for (auto edge : uptr->fanout) {
        for (size_t w_sel = 0; w_sel < NUM_WEIGHTS; w_sel++) {
          if (!edge->weights[w_sel]) {
            continue;
          }

          // skip if the edge goes outside of the suffix tree
          // which is unreachable
          auto v = edge->to.id;
          if (!pfxt.sfxt.dists[v]) {
            continue;
          }

          // skip if the edge belongs to the suffix 
          // NOTE: we're detouring, so we don't want to
          // go on the same paths explored by the suffix tree
          if (pfxt.sfxt.links[u] &&
              _encode_edge(*edge, w_sel) == *pfxt.sfxt.links[u]) {
            continue;
          }
        
          auto w = *edge->weights[w_sel];
          auto detour_cost = *pfxt.sfxt.dists[v] + w - *pfxt.sfxt.dists[u]; 
          auto c = detour_cost + pfx.cost;
          auto dc = detour_cost + pfx.detour_cost;
          pfxt.push_par(c, dc, u, v, edge, &pfx, _encode_edge(*edge, w_sel));
        }
      }
 
    });
		u = *pfxt.sfxt.successors[u];
	}
  _executor.wait_for_all();
}

void Ink::_spur_multiq(Pfxt& pfxt, PfxtNode& pfx, tf::Executor& e) {
	auto u = pfx.to;
  while (u != pfxt.sfxt.T) {
		auto uptr = _vptrs[u];
    for (auto edge : uptr->fanout) {
      for (size_t w_sel = 0; w_sel < NUM_WEIGHTS; w_sel++) {
        if (!edge->weights[w_sel]) {
          continue;
        }

        // skip if the edge goes outside of the suffix tree
        // which is unreachable
        auto v = edge->to.id;
        if (!pfxt.sfxt.dists[v]) {
          continue;
        }

        // skip if the edge belongs to the suffix 
        // NOTE: we're detouring, so we don't want to
        // go on the same paths explored by the suffix tree
        if (pfxt.sfxt.links[u] &&
            _encode_edge(*edge, w_sel) == *pfxt.sfxt.links[u]) {
          continue;
        }
      
        auto w = *edge->weights[w_sel];
        auto detour_cost = *pfxt.sfxt.dists[v] + w - *pfxt.sfxt.dists[u]; 
        auto c = detour_cost + pfx.cost;
        auto dc = detour_cost + pfx.detour_cost;
        
        pfxt.push_task(*this, c, dc, u, v, edge, &pfx, _encode_edge(*edge, w_sel));
      }
    }

		u = *pfxt.sfxt.successors[u];
	}
}



void Ink::_spur_midway(
  Pfxt& pfxt, 
  PfxtNode& pfx, 
  size_t start, 
  int skip_spurs,
  size_t write_idx) {
	auto u{start};
  auto availables = pfx.children.size() - write_idx; 
	while (u != pfxt.sfxt.T) {
		auto uptr = _vptrs[u];
		for (auto edge : uptr->fanout) {
			for (size_t w_sel = 0; w_sel < NUM_WEIGHTS; w_sel++) {
				if (!edge->weights[w_sel]) {
					continue;
				}

				// skip if the edge goes outside of the suffix tree
				// which is unreachable
				auto v = edge->to.id;
				if (!pfxt.sfxt.dists[v]) {
					continue;
				}

				// skip if the edge belongs to the suffix 
				// NOTE: we're detouring, so we don't want to
				// go on the same paths explored by the suffix tree
				if (pfxt.sfxt.links[u] &&
						_encode_edge(*edge, w_sel) == *pfxt.sfxt.links[u]) {
					continue;
				}

        if (skip_spurs > 0) {
          skip_spurs--;
          continue;
        }

				auto w = *edge->weights[w_sel];
				auto detour_cost = *pfxt.sfxt.dists[v] + w - *pfxt.sfxt.dists[u]; 
				
				auto c = detour_cost + pfx.cost;
        auto dc = detour_cost + pfx.detour_cost;
        if (availables > 0) {
				  pfxt.push_inc(c, dc, u, v, edge, &pfx, _encode_edge(*edge, w_sel), write_idx++);
          availables--;
        }
        else {
				  pfxt.push(c, dc, u, v, edge, &pfx, _encode_edge(*edge, w_sel));
			  }
      }
		}

		u = *pfxt.sfxt.successors[u];
	}

  // if there are existing nodes after newly spurred nodes
  // we should directly downsize the children list
  if (write_idx < pfx.num_children()) {
    pfx.children.resize(write_idx);  
  }
}


void Ink::_spur_pruned(
	Pfxt& pfxt, 
	PfxtNode& pfx,
	const std::vector<std::pair<size_t, size_t>>& pruned) {
	
  auto u = pfx.to;
	// mark pruned edges on graph
	for (const auto& p : pruned) {
		_eptrs[p.first]->pruned_weights[p.second] = true;
	}

	while (u != pfxt.sfxt.T) {
		//assert(pfxt.sfxt.links[u]);
		auto uptr = _vptrs[u];

		for (auto edge : uptr->fanout) {
			for (size_t w_sel = 0; w_sel < NUM_WEIGHTS; w_sel++) {
				if (!edge->weights[w_sel]) {
					continue;
				}

				// skip if the edge goes outside of the suffix tree
				// which is unreachable
				auto v = edge->to.id;
				if (!pfxt.sfxt.dists[v]) {
					continue;
				}

				// skip if the edge belongs to the suffix 
				// NOTE: we're detouring, so we don't want to
				// go on the same paths explored by the suffix tree
				if (pfxt.sfxt.links[u] &&
						_encode_edge(*edge, w_sel) == *pfxt.sfxt.links[u]) {
					continue;
				}

				// skip if the [edge id, weight sel] pair is marked as pruned
				if (edge->pruned_weights[w_sel]) {
					continue;
				}

				auto w = *edge->weights[w_sel];
				auto detour_cost = *pfxt.sfxt.dists[v] + w - *pfxt.sfxt.dists[u]; 
				
				auto c = detour_cost + pfx.cost;
        auto dc = detour_cost + pfx.detour_cost;
				pfxt.push(c, dc, u, v, edge, &pfx, _encode_edge(*edge, w_sel));
			}
		}

		u = *pfxt.sfxt.successors[u];
	}

	// reset markings on pruned edges
	// NOTE: they're only pruned when we spur this
	// particular pfxt node
	for (const auto& p : pruned) {
		_eptrs[p.first]->pruned_weights[p.second] = false;
	}
}

void Ink::_spur_no_pq(PfxtNode& pfx, size_t& local_avg) {
	auto u = pfx.to;
  auto& sfxt = *_global_sfxt;
  size_t dc_sum{0};
  size_t node_cnt{0};
	while (u != sfxt.T) {
	  //assert(pfxt.sfxt.links[u]);
		auto uptr = _vptrs[u];
		for (auto edge : uptr->fanout) {
			for (size_t w_sel = 0; w_sel < NUM_WEIGHTS; w_sel++) {
				if (!edge->weights[w_sel]) {
					continue;
				}

				// skip if the edge goes outside of the suffix tree
				// which is unreachable
				auto v = edge->to.id;
				if (!sfxt.dists[v]) {
					continue;
				}

				// skip if the edge belongs to the suffix 
				// NOTE: we're detouring, so we don't want to
				// go on the same paths explored by the suffix tree
				if (sfxt.links[u] &&
						_encode_edge(*edge, w_sel) == *sfxt.links[u]) {
					continue;
				}
			
				auto w = *edge->weights[w_sel];
				auto detour_cost = *sfxt.dists[v] + w - *sfxt.dists[u]; 
				auto c = detour_cost + pfx.cost;
        auto dc = detour_cost + pfx.detour_cost;
        
        // allocate new child node for pfx
        // and push to standby queue
        node_cnt++;
        dc_sum += dc * DC_SCALE;
        _standbys.try_enqueue(
          std::make_unique<PfxtNode>(
            c, dc, u, v, edge,
            &pfx, _encode_edge(*edge, w_sel))
        );
      }
		}

		u = *sfxt.successors[u];
	}
  
  // calculate local avg. deviation cost
  if (node_cnt != 0) {
    local_avg = dc_sum / node_cnt;
  }
}





void Ink::_recover_path(
	Path& path,
	const Sfxt& sfxt,
	const PfxtNode* pfxt_node,
	size_t v) const {

	if (pfxt_node == nullptr) {
		return;
	}

	// recurse until we reach the source pfxt node
	_recover_path(path, sfxt, pfxt_node->parent, pfxt_node->from);

	auto u = pfxt_node->to;
	auto u_ptr = _vptrs[u];

	// detour at the sfxt source
	if (pfxt_node->from == sfxt.S) {
		path.emplace_back(*u_ptr, 0.0f);
	}
	// detour at non-sfxt-source nodes (internal deviation)
	else {
		assert(!path.empty());
		assert(pfxt_node->link);
		
		auto [edge, w_sel] = _decode_edge(*pfxt_node->link);
		auto d = path.back().dist + *edge->weights[w_sel];
		path.emplace_back(*u_ptr, d);
	}
	
	while (u != v) {
		if (!sfxt.links[u]) {
			break;
		}
		
		assert(sfxt.links[u]);
		auto [edge, w_sel] = _decode_edge(*sfxt.links[u]);	
		// move to the successor of u
		u = *sfxt.successors[u];
		u_ptr = _vptrs[u];
		
		auto d = path.back().dist + *edge->weights[w_sel];
		path.emplace_back(*u_ptr, d);
	}

}	

void Ink::_clear_update_list() {
	while (!_to_update.empty()) {
		auto v = _to_update.back();
		if (_vptrs[v]) {
			_vptrs[v]->is_in_update_list = false;
		}
		_to_update.pop_back();
	}
}

void Ink::_dfs(
	size_t v, 
	std::deque<size_t>& tpg, 
	std::vector<bool>& visited) {
	visited[v] = true;
	auto vptr = _vptrs[v];
	for (const auto e : vptr->fanin) {
		auto u = e->from.id;
		if (!visited[u]) {
			_dfs(u, tpg, visited);
		}
	}

	tpg.push_front(v);
}

std::vector<Path> Ink::_extract_paths(
	std::vector<std::unique_ptr<Path>>& paths) {
  std::sort(
		paths.begin(), 
		paths.end(), 
		[](std::unique_ptr<Path>& a, std::unique_ptr<Path>& b) {
			return a->weight < b->weight;	
		}
	);

	std::vector<Path> P;
	P.reserve(paths.size());

	std::transform(
		paths.begin(), paths.end(), std::back_inserter(P), 
		[](auto& ptr) {
			return std::move(*ptr);		
		}
	);

	paths.clear();
	return P;
}



// ------------------------
// Suffix Tree Implementations
// ------------------------

// NOTE:
// OpenTimer does a resize_to_fit, why not simply resize(N)?

Sfxt::Sfxt(size_t S, size_t T, size_t sz) :
	S{S},
	T{T}
{
	// resize suffix tree storages
	visited.resize(sz);
	dists.resize(sz);
	successors.resize(sz);
	links.resize(sz);
}

inline bool Sfxt::relax(
	size_t u, 
	size_t v, 
	std::optional<std::pair<size_t, size_t>> l, 
	float d) {
	if (!dists[u] || *dists[v] + d < *dists[u]) {
    dists[u] = *dists[v] + d;
		successors[u] = v;
		links[u] = l; 
		return true;
	}
	return false;
}


inline std::optional<float> Sfxt::dist() const {
	return dists[S];
}


// ------------------------
// Prefix Tree Implementations
// ------------------------
PfxtNode::PfxtNode(
	float c,
  float dc, 
	size_t f, 
	size_t t, 
	Edge* e,
	PfxtNode* p,
	std::optional<std::pair<size_t, size_t>> l) :
	cost{c},
  detour_cost{dc},
	from{f},
	to{t},
	edge{e},
	parent{p},
	link{l}
{ 
}

size_t PfxtNode::num_children() const {
	return children.size();
}

void PfxtNode::reset() {
  updated = false;
  removed = false;
  to_respur = false;
  pruned.clear(); 
}

Pfxt::Pfxt(const Sfxt& sfxt) : 
	sfxt{sfxt} 
{
}

Pfxt::Pfxt(Pfxt&& other) :
	sfxt{other.sfxt},
	comp{other.comp},
	paths{std::move(other.paths)},
	nodes{std::move(other.nodes)}
{
}

PfxtNode* Pfxt::push(
	float c,
  float dc,
	size_t f,
	size_t t,
	Edge* e,
	PfxtNode* p,
	std::optional<std::pair<size_t, size_t>> link) {
  nodes.emplace_back(std::make_unique<PfxtNode>(c, dc, f, t, e, p, link));
  
  // cache the pointer we just pushed
	auto obs = nodes.back().get();

	// store this node to its corresponding edge's
	// dependency list
	if (e) {
		e->dep_nodes.emplace_back(obs);	
	}

	// store children
	if (p) {
		p->children.emplace_back(obs);
		obs->level = p->level + 1;
	}
	else {
		obs->level = 0;	
	}
	
  // push node to heap 
  std::push_heap(nodes.begin(), nodes.end(), comp);
	
  return obs;
}

PfxtNode* Pfxt::push_inc(
	float c,
  float dc,
	size_t f,
	size_t t,
	Edge* e,
	PfxtNode* p,
	std::optional<std::pair<size_t, size_t>> link,
  size_t write_idx) {
	nodes.emplace_back(std::make_unique<PfxtNode>(c, dc, f, t, e, p, link));

	// cache the pointer we just pushed
	auto obs = nodes.back().get();

	// store this node to its corresponding edge's
	// dependency list
	if (e) {
		e->dep_nodes.emplace_back(obs);	
	}

	// store children
	if (p) {
		p->children[write_idx] = obs;
		obs->level = p->level + 1;
	}
	else {
		obs->level = 0;	
	}

	// push node to heap 
	std::push_heap(nodes.begin(), nodes.end(), comp);
	
	return obs;
}

void Pfxt::push_par(
	float c,
  float dc,
	size_t f,
	size_t t,
	Edge* e,
	PfxtNode* p,
	std::optional<std::pair<size_t, size_t>> l) {
  par_prq.emplace(std::make_unique<PfxtNode>(c, dc, f, t, e, p, l));
}

void Pfxt::push_task(
	const Ink& ink,
  float c,
  float dc,
	size_t f,
	size_t t,
	Edge* e,
	PfxtNode* p,
	std::optional<std::pair<size_t, size_t>> l) {
  auto q_idx = ink.determine_q_idx(c);
  task_qs[q_idx].enqueue(std::make_unique<PfxtNode>(c, dc, f, t, e, p, l));
}


PfxtNode* Pfxt::pop_par() {
  // a temporary node to transfer ownership
  std::unique_ptr<PfxtNode> node;

  if (par_prq.try_pop(node)) {
    auto obs = node.get();
    paths.push_back(std::move(node));
    return obs;
  }
  
  return nullptr;
}

PfxtNode* Pfxt::pop_task(size_t q_idx) {
  // a temporary node to transfer ownership
  std::unique_ptr<PfxtNode> node;

  assert(q_idx < task_qs.size());
  if (task_qs[q_idx].try_dequeue(node)) {
    auto obs = node.get();
    paths_concurr.enqueue(std::move(node));
    return obs;
  }
  
  return nullptr;
}

std::vector<std::unique_ptr<PfxtNode>> Pfxt::pop_task(size_t q_idx, size_t bulk_size) {
  // a temporary vector to transfer ownership
  std::vector<std::unique_ptr<PfxtNode>> nodes(bulk_size);

  assert(q_idx < task_qs.size());
  if (task_qs[q_idx].try_dequeue_bulk(nodes.begin(), bulk_size) != 0) {
    return nodes;
  }
 
  return {};
}


PfxtNode* Pfxt::pop() {
	if (nodes.empty()) {
		return nullptr;
	}
	
	// swap [0] and [N-1], and heapify [first, N-1)
	std::pop_heap(nodes.begin(), nodes.end(), comp);

	// get the min element from heap
	// now it's located in the back
	paths.push_back(std::move(nodes.back()));
	nodes.pop_back();

	// and return an observer poiner to this node object
	return paths.back().get();
}


PfxtNode* Pfxt::top() const {
	return nodes.empty() ? nullptr : nodes.front().get();
}

void Pfxt::heapify() {
  std::make_heap(nodes.begin(), nodes.end(), comp);

}

// ------------------------
// PfxtNodeComp Implementations
// ------------------------
bool PfxtNodeComp::operator() (
	const std::unique_ptr<PfxtNode>& a,
	const std::unique_ptr<PfxtNode>& b) {
	return a->cost > b->cost;
}	

// ------------------------
// Point Implementations
// ------------------------
Point::Point(const Vert& v, float d) :
	vert{v},
	dist{d}
{
}

// ------------------------
// Path Implementations
// ------------------------
Path::Path(float w, const Point* endpt) :
	weight{w},
	endpoint{endpt}
{
}

void Path::dump(std::ostream& os) const {
	if (empty()) {
		os << "empty path\n";
	}

	// dump the header
	os << "----------------------------------\n";
	os << "Startpoint:  " << front().vert.name << '\n';
	os << "Endpoint:    " << back().vert.name << '\n';

	// dump the path
	os << "Path:\n";
	for (const auto& p : *this) {
		os << "Vert name: " << p.vert.name << ", Dist: " << p.dist << '\n';
	}

	os << "----------------------------------\n";
}

// ------------------------
// PathHeap Implementations
// ------------------------
inline size_t PathHeap::size() const {
	return _paths.size();
}

inline bool PathHeap::empty() const {
	return _paths.empty();
}

void PathHeap::push(std::unique_ptr<Path> path) {
	_paths.push_back(std::move(path));
	std::push_heap(_paths.begin(), _paths.end(), _comp);
}

void PathHeap::pop() {
	if (_paths.empty()) {
		return;
	}

	std::pop_heap(_paths.begin(), _paths.end(), _comp);
	_paths.pop_back();
}

Path* PathHeap::top() const {
	return _paths.empty() ? nullptr : _paths.front().get();
}

void PathHeap::heapify() {
	std::make_heap(_paths.begin(), _paths.end(), _comp);
} 

void PathHeap::fit(size_t K) {
	while (_paths.size() > K) {
		pop();
	}
}

void PathHeap::merge_and_fit(PathHeap&& rhs, size_t K) {
	if (_paths.capacity() < rhs._paths.capacity()) {
		_paths.swap(rhs._paths);
	}

	std::sort_heap(_paths.begin(), _paths.end(), _comp);
	std::sort_heap(rhs._paths.begin(), rhs._paths.end(), _comp);

	// now insert rhs to the back of _paths
	auto mid = _paths.insert(
		_paths.end(),
		std::make_move_iterator(rhs._paths.begin()),
		std::make_move_iterator(rhs._paths.end())
	);

	// rhs elements are invalidated
	rhs._paths.clear();

	std::inplace_merge(_paths.begin(), mid, _paths.end(), _comp);
	if (_paths.size() > K) {
		_paths.resize(K);
	}

	heapify();

}


std::vector<Path> PathHeap::extract() {
	std::sort_heap(_paths.begin(), _paths.end(), _comp);
	std::vector<Path> P;
	P.reserve(_paths.size());

	std::transform(
		_paths.begin(), _paths.end(), std::back_inserter(P), 
		[](auto& ptr) {
			return std::move(*ptr);		
		}
	);

	_paths.clear();
	return P;
}


void PathHeap::dump(std::ostream& os) const {
	os << "Num Paths = " << _paths.size() << '\n';
	for (const auto& p : _paths) {
		p->dump(os);
	}
}

} // end of namespace ink 
