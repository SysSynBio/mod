#ifndef MOD_LIB_GRAPH_STATE_STEREO_H
#define MOD_LIB_GRAPH_STATE_STEREO_H

#include <mod/lib/Graph/Properties/Property.h>
#include <mod/lib/Stereo/EdgeCategory.h>
#include <mod/lib/Stereo/Configuration/Configuration.h>

#include <jla_boost/graph/PairToRangeAdaptor.hpp>

namespace mod {
namespace lib {
namespace Graph {

struct PropStereo : Prop<PropStereo, std::unique_ptr<const lib::Stereo::Configuration>, Stereo::EdgeCategory> {
	using Base = Prop<PropStereo, std::unique_ptr<const lib::Stereo::Configuration>, Stereo::EdgeCategory>;
public:

	template<typename Inference>
	explicit PropStereo(const GraphType &g, Inference &&inference) : Base(g) {
		vertexState.reserve(num_vertices(g));
		for(const auto v : asRange(vertices(g))) {
			assert(get(boost::vertex_index_t(), g, v) == vertexState.size());
			vertexState.push_back(inference.extractConfiguration(v));
		}
		edgeState.reserve(num_edges(g));
		for(const auto e : asRange(edges(g))) {
			assert(get(boost::edge_index_t(), g, e) == edgeState.size());
			edgeState.push_back(inference.getEdgeCategory(e));
		}
		verify(&g);
	}
};

} // namespace Graph
} // namespace lib
} // namespace mod

#endif /* MOD_LIB_GRAPH_STATE_STEREO_H */