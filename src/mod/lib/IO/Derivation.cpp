#include "Derivation.h"

#include <mod/Config.h>
#include <mod/rule/Rule.h>
#include <mod/lib/Algorithm.h>
#include <mod/lib/DG/Hyper.h>
#include <mod/lib/Graph/Single.h>
#include <mod/lib/Graph/Properties/Stereo.h>
#include <mod/lib/Graph/Properties/String.h>
#include <mod/lib/GraphMorphism/LabelledMorphism.h>
#include <mod/lib/GraphMorphism/VF2Finder.hpp>
#include <mod/lib/IO/FileHandle.h>
#include <mod/lib/IO/IO.h>
#include <mod/lib/IO/MorphismConstraints.h>
#include <mod/lib/IO/Rule.h>
#include <mod/lib/LabelledUnionGraph.h>
#include <mod/lib/RC/ComposeRuleReal.h>
#include <mod/lib/RC/MatchMaker/Sub.h>
#include <mod/lib/RC/MatchMaker/Super.h>
#include <mod/lib/Rules/GraphToRule.h>
#include <mod/lib/Rules/Real.h>
#include <mod/lib/Rules/Properties/Depiction.h>
#include <mod/lib/Rules/Properties/Term.h>

#include <jla_boost/graph/morphism/Predicates.hpp>

#include <boost/lexical_cast.hpp>

namespace mod {
namespace lib {
namespace IO {
namespace Derivation {
namespace Write {
namespace {
namespace GM = jla_boost::GraphMorphism;

std::vector<lib::Rules::Real*> findCompleteRules(const lib::DG::NonHyper &dg, lib::DG::HyperVertex v, const lib::Rules::Real &rReal) {
	const lib::DG::HyperGraphType &dgGraph = dg.getHyper().getGraph();
	assert(v != dgGraph.null_vertex());
	using Vertex = lib::DG::HyperVertex;
	assert(dgGraph[v].kind == lib::DG::HyperVertexKind::Edge);
	LabelledUnionGraph<lib::Graph::LabelledGraph> eductUnion, productUnion;
	for(const auto vAdj : asRange(inv_adjacent_vertices(v, dgGraph)))
		eductUnion.push_back(&dgGraph[vAdj].graph->getLabelledGraph());
	for(const auto vAdj : asRange(adjacent_vertices(v, dgGraph)))
		productUnion.push_back(&dgGraph[vAdj].graph->getLabelledGraph());
	const auto identifyL = lib::Rules::graphToRule(eductUnion, lib::Rules::Membership::Context, "G");
	const auto identifyR = lib::Rules::graphToRule(productUnion, lib::Rules::Membership::Context, "H");
	std::vector<lib::Rules::Real*> matchingLR;
	{
		std::vector<lib::Rules::Real*> matchingL;
		{
			//			IO::log() << "Derivation: compose identifyL -> rReal" << std::endl;
			auto reporter = [&matchingL, &dg] (std::unique_ptr<lib::Rules::Real> r) {
				auto *rPtr = r.release();
				auto labelType = dg.getLabelSettings().type;
				auto withStereo = dg.getLabelSettings().withStereo;
				auto p = findAndInsert(matchingL, rPtr, lib::Rules::makeIsomorphismPredicate(labelType, withStereo));
				if(!p.second) delete rPtr;
			};
			lib::RC::Super mm(false, true);
			lib::RC::composeRuleRealByMatchMaker(*identifyL, rReal, mm, reporter, dg.getLabelSettings());
		}
		for(auto *r : matchingL) {
			//			IO::log() << "Derivation: compose matchingL -> identifyR" << std::endl;
			auto reporter = [&matchingLR, &dg] (std::unique_ptr<lib::Rules::Real> r) {
				//				IO::log() << "Derivation: got result" << std::endl;
				auto *rPtr = r.release();
				auto labelType = dg.getLabelSettings().type;
				auto withStereo = dg.getLabelSettings().withStereo;
				auto p = findAndInsert(matchingLR, rPtr, lib::Rules::makeIsomorphismPredicate(labelType, withStereo));
				//				IO::log() << "Derivation: findAndInsert = " << std::boolalpha << p.second << std::endl;
				if(!p.second) delete rPtr;
			};
			assert(r);
			// TODO: we should do isomorphism here instead
			lib::RC::Sub mm(false);
			lib::RC::composeRuleRealByMatchMaker(*r, *identifyR, mm, reporter, dg.getLabelSettings());
			delete r;
		}
	}
	return matchingLR;
}

template<typename F>
void forEachMatch(const lib::DG::NonHyper &dg, lib::DG::HyperVertex v, const lib::Rules::Real &rReal, F f) {
	const lib::DG::HyperGraphType &dgGraph = dg.getHyper().getGraph();
	bool derivationFound = false;
	std::vector<lib::Rules::Real*> matchingLR = findCompleteRules(dg, v, rReal);
	for(const lib::Rules::Real *rLower : matchingLR) {
		auto mr = [&f, &derivationFound, rLower, matchCount = 0](auto &&m, const lib::Rules::GraphType &gUpper, const lib::Rules::GraphType & gLower) mutable{
			derivationFound = true;
			std::string strMatch = boost::lexical_cast<std::string>(matchCount);
			f(*rLower, gUpper, gLower, m, strMatch);
			++matchCount;
			return true;
		};
		//		mod::postSection("Bah");
		//		lib::IO::Graph::Write::Options options;
		//		options.withRawStereo = true;
		//		options.withColour = true;
		//		options.edgesAsBonds = true;
		//		options.withIndex = true;
		//		lib::IO::Rules::Write::summary(rReal, options, options);
		//		lib::IO::Rules::Write::summary(*rLower, options, options);
		//		IO::log() << "morphismSelectByLabelSettings: " << dg.getLabelSettings() << std::endl;
		lib::GraphMorphism::morphismSelectByLabelSettings(rReal.getDPORule(), rLower->getDPORule(), dg.getLabelSettings(),
				lib::GraphMorphism::VF2Monomorphism(), std::ref(mr), lib::Rules::MembershipPredWrapper());
		//		IO::log() << "morphismSelectByLabelSettings done" << std::endl;
		delete rLower;
	}
	if(!derivationFound) IO::log() << "WARNING: derivation " << get(boost::vertex_index_t(), dgGraph, v) << " does not exist." << std::endl;
}

} // namespace

void summary(const lib::DG::NonHyper &dg, lib::DG::HyperVertex v, const IO::Graph::Write::Options &options, const std::string &nomatchColour, const std::string &matchColour) {
	const lib::DG::HyperGraphType &dgGraph = dg.getHyper().getGraph();
	if(dgGraph[v].rules.empty()) {
		IO::log() << "Warning: no rules on derivation to be printed." << std::endl;
		return;
	}
	for(const lib::Rules::Real *rPtr : dgGraph[v].rules) {
		const lib::Rules::Real &rReal = *rPtr;
		if(!getConfig().dg.dryDerivationPrinting.get()) {
			IO::post() << "summarySectionNoEscape \"Derivation " << get(boost::vertex_index_t(), dgGraph, v);
			IO::post() << ", $r_{" << rReal.getId() << "}$";
			IO::post() << ", \\{";
			bool first = true;
			for(auto vIn : asRange(inv_adjacent_vertices(v, dgGraph))) {
				if(!first) IO::post() << ", ";
				else first = false;
				IO::post() << "\\texttt{\\ensuremath{\\mathrm{" << dgGraph[vIn].graph->getName() << "}}}";
			}
			IO::post() << "\\} \\texttt{->} \\{";
			first = true;
			for(auto vOut : asRange(adjacent_vertices(v, dgGraph))) {
				if(!first) IO::post() << ", ";
				else first = false;
				IO::post() << "\\texttt{\\ensuremath{\\mathrm{" << dgGraph[vOut].graph->getName() << "}}}";
			}
			IO::post() << "\\}\"" << std::endl;
			{
				std::string file = getUniqueFilePrefix() + "der_constraints.tex";
				FileHandle s(file);
				s << "\\begin{verbatim}" << std::endl;
				auto vis = lib::IO::MatchConstraint::Write::makeTexPrintVisitor(s, get_left(rReal.getDPORule()));
				for(const auto &c : rReal.getDPORule().leftMatchConstraints) {
					c->accept(vis);
				}
				s << "\\end{verbatim}" << std::endl;
				IO::post() << "summaryInput \"" << file << "\"" << std::endl;
			}
		}

		// a copy where we change the coordinates at each match
		lib::Rules::Real rUpperCopy(lib::Rules::LabelledRule(rReal.getDPORule(), false), rReal.getLabelType());
		auto f = [&options, &nomatchColour, &matchColour, &rUpperCopy](
				const lib::Rules::Real &rLower,
				const lib::Rules::GraphType &gUpper, const lib::Rules::GraphType &gLower,
				const auto &m, const std::string & strMatch) {
			//		const bool printInstantiated = false;
			if(!getConfig().dg.dryDerivationPrinting.get()) {
				const auto visibleRule = jla_boost::AlwaysTrue();
				const auto vColourRule = jla_boost::Nop<std::string>();
				const auto eColourRule = jla_boost::Nop<std::string>();
				const auto visibleInstantiation = jla_boost::AlwaysTrue();
				const auto vColourInstantiation = [&nomatchColour, &matchColour, &gUpper, &gLower, &m] (lib::Rules::Vertex v) -> std::string {
					const auto vUpper = get_inverse(m, gUpper, gLower, v);
					const bool matched = vUpper != boost::graph_traits<lib::Rules::GraphType>::null_vertex();
					return matched ? matchColour : nomatchColour;
				};
				const auto eColourInstantiation = [&nomatchColour, &matchColour, &gUpper, &gLower, &m] (lib::Rules::Edge e) -> std::string {
					const auto vSrc = source(e, gLower);
					const auto vTar = target(e, gLower);
					const auto vSrcUpper = get_inverse(m, gUpper, gLower, vSrc);
					const auto vTarUpper = get_inverse(m, gUpper, gLower, vTar);
					if(vSrcUpper == boost::graph_traits<lib::Rules::GraphType>::null_vertex()) return nomatchColour;
					if(vTarUpper == boost::graph_traits<lib::Rules::GraphType>::null_vertex()) return nomatchColour;
					const auto matched = edge(vSrcUpper, vTarUpper, gUpper).second;
					return matched ? matchColour : nomatchColour;
				};
				using Vertex = boost::graph_traits<lib::Rules::GraphType>::vertex_descriptor;
				std::map<Vertex, Vertex> map;
				for(const auto vUpper : asRange(vertices(gUpper)))
					map.emplace(vUpper, get(m, gUpper, gLower, vUpper));
				rUpperCopy.getDepictionData().copyCoords(rLower.getDepictionData(), map);
				std::string fileNoExt1 = IO::Rules::Write::pdf(rUpperCopy, options, strMatch + "_derL", strMatch + "_derK", strMatch + "_derR",
						IO::Rules::Write::BaseArgs{visibleRule, vColourRule, eColourRule});
				std::string fileNoExt2 = IO::Rules::Write::pdf(rLower, options, strMatch + "_derG", strMatch + "_derD", strMatch + "_derH",
						IO::Rules::Write::BaseArgs{visibleInstantiation, vColourInstantiation, eColourInstantiation});
				IO::post() << "summaryDerivation \"" << fileNoExt1 << "_" << strMatch << "\" \"" << fileNoExt2 << "_" << strMatch << "\"" << std::endl;
			}
			IO::Rules::Write::gml(rLower, false);
		};
		forEachMatch(dg, v, rReal, f);
	}
}

void summaryTransitionState(const lib::DG::NonHyper &dg, lib::DG::HyperVertex v, const IO::Graph::Write::Options &options) { }

} // namespace Write
} // namespace Derivation
} // namespace IO
} // namespace lib
} // namespace mod
