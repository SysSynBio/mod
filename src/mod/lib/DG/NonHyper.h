#ifndef MOD_LIB_DG_NONHYPER_H
#define MOD_LIB_DG_NONHYPER_H

#include <mod/dg/ForwardDecl.h>
#include <mod/dg/DG.h>
#include <mod/graph/Graph.h>
#include <mod/lib/DG/GraphDecl.h>
#include <mod/lib/Graph/GraphDecl.h>

#include <boost/graph/adjacency_list.hpp>

#include <iosfwd>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mod {
namespace lib {
namespace Graph {
struct PropString;
} // namespace Graph
namespace DG {
class HyperCreator;

class NonHyper {
	friend class HyperCreator;
protected:
	using GraphType = NonHyperGraphType;
	using Vertex = NonHyperVertex;
	using Edge = NonHyperEdge;
public:
	// TODO: change to unordered set, when Boost >= 1.51 is installed
	typedef std::set<std::shared_ptr<graph::Graph>, graph::GraphLess> StdGraphSet;
protected:
	NonHyper(const std::vector<std::shared_ptr<graph::Graph> > &graphDatabase, LabelSettings labelSettings);
public: // general
	virtual ~NonHyper();
	std::size_t getId() const;
	std::shared_ptr<dg::DG> getAPIReference() const;
	void setAPIReference(std::shared_ptr<dg::DG> dg);
	LabelSettings getLabelSettings() const;
	virtual std::string getType() const = 0;
public: // calculation
	void calculate();
	bool getHasCalculated() const;
protected: // calculation
	virtual void calculateImpl() = 0;
	// adds the graph to the graph database
	// returns true iff it was a new graph
	bool addGraph(std::shared_ptr<graph::Graph> g);
	// adds the graph to the database if it's not there already
	// makes a vertex in the DG
	// returns true iff it was a new graph
	bool addGraphAsVertex(std::shared_ptr<graph::Graph> g);
	// searches the database for the given graph
	// if found, returns the found graph and false, additionally the given graph is deleted
	// if not found, returns the given wrapped given graph and true
	// does NOT change the graphDatabse
	std::pair<std::shared_ptr<graph::Graph>, bool> checkIfNew(std::unique_ptr<lib::Graph::Single> g) const;
	// gives a graph product status, i.e., rename it, put it in the product list and maybe print a status message
	void giveProductStatus(std::shared_ptr<graph::Graph> g);
	// adds the graph to the database if it's not there already
	// if it was not there, give it product status
	// returns true iff it was given product status
	bool addProduct(std::shared_ptr<graph::Graph> g);
	// checks if this derivation already exists
	// if it does then the edge descriptor of that derivation is returned, otherwise the edge descriptor is bogus
	std::pair<Edge, bool> isDerivation(const GraphMultiset &gmsSrc, const GraphMultiset &gmsTar, const lib::Rules::Real *r) const;
	// adds a derivation if it does not exist already
	// the edge descriptor of the derivation is returned, along with the existence status before the call
	// the rule may be nullptr
	std::pair<Edge, bool> suggestDerivation(const GraphMultiset &gmsSrc, const GraphMultiset &gmsTar, const lib::Rules::Real *r);
	const GraphType &getGraphDuringCalculation() const;
private: // calculation
	// adds the graph as a vertex, if it's not there already, and returns the vertex
	Vertex getVertex(const GraphMultiset &gms);
	void findReversiblePairs();
public: // post calculation
	void list(std::ostream &s) const;
	const GraphType &getGraph() const;
	Hyper &getHyper();
	const Hyper &getHyper() const;
	const StdGraphSet &getGraphDatabase() const;
	const std::vector<std::shared_ptr<graph::Graph> > &getProducts() const;
	void print() const;
	HyperVertex findHyperEdge(const std::vector<HyperVertex> &sources, const std::vector<HyperVertex> &targets) const;
	std::vector<dg::DG::HyperEdge> getAllHyperEdges() const;
protected:
	virtual void listImpl(std::ostream &s) const = 0;
private: // general
	std::size_t id;
	std::weak_ptr<dg::DG> apiReference;
	const LabelSettings labelSettings;
	StdGraphSet graphDatabase;
	GraphType dg;
	std::map<GraphMultiset, Vertex> multisetToVertex;
	std::unique_ptr<Hyper> hyper;
	HyperCreator *hyperCreator; // only valid during calculation
private: // calculation
	bool hasCalculated;
	unsigned int productNum;
	std::vector<std::shared_ptr<graph::Graph> > products;
public:
	static void diff(const NonHyper &dg1, const NonHyper &dg2);
};

} // namespace DG
} // namespace lib
} // namespace mod

#endif /* MOD_LIB_DG_NONHYPER_H */
