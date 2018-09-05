#include "Skeleton.hpp"

#include "GrowthCone.hpp"
#include "Neuron.hpp"
#include "Node.hpp"

namespace growth
{
// for plotting the neurons and axons
// TO BE EXPLAINED !
Skeleton::Skeleton() {}

Skeleton::Skeleton(const Neuron *neuron, unsigned int resolution)
{
    soma_position        = neuron->get_position();
    soma_radius          = neuron->get_soma_radius();
    axon                 = SkelNeurite();
    dendrites            = SkelNeurite();
    branching_points     = SkelNeurite();
    growth_cones         = SkelNeurite();
    SkelNeurite branches = SkelNeurite();
    //~ #ifndef NDEBUG
    //~ printf(" %lu neurites inside the neuron skeleton \n",
    //~ neuron->get_num_neurites());
    //~ #endif
    for (const auto &neurite : neuron->neurites_)
    {
        if (neurite.first == "axon")
        {
            NodePtr node = neurite.second->get_first_node();
            std::deque<TNodePtr> nodes{node->get_child(0)};
            while (not nodes.empty())
            {
                TNodePtr node   = nodes.back();
                size_t i, last(node->get_branch()->size());

                PointsArray &pp = node->get_branch()->points;

                for (i=0; i<last; i+=resolution)
                {
                    axon.first.push_back(pp[0].at(i));
                    axon.second.push_back(pp[1].at(i));
                }
                if (i != last-1)
                {
                    axon.first.push_back(pp[0].back());
                    axon.second.push_back(pp[1].back());
                }

                axon.first.push_back(NAN);
                axon.second.push_back(NAN);

                nodes.pop_back();

                if (node->has_child())
                {
                    branching_points.first.push_back(
                        node->get_position().at(0));
                    branching_points.second.push_back(
                        node->get_position().at(1));
                    NodePtr mynode = std::dynamic_pointer_cast<Node>(node);
                    nodes.push_front(mynode->get_child(0));
                    nodes.push_front(mynode->get_child(1));
                }
            }

            // if no dendrites, still mark it with NaNs
            if (neuron->neurites_.size() == 1)
            {
                dendrites.first.push_back(NAN);
                dendrites.second.push_back(NAN);
            }
        }
        else
        {
            NodePtr node = neurite.second->get_first_node();
            std::deque<TNodePtr> nodes{node->get_child(0)};
            while (not nodes.empty())
            {

                TNodePtr node   = nodes.back();
                size_t i, last(node->get_branch()->size());

                PointsArray &pp = node->get_branch()->points;

                for (i=0; i<last; i+=resolution)
                {
                    dendrites.first.push_back(pp[0].at(i));
                    dendrites.second.push_back(pp[1].at(i));
                }
                if (i != last-1)
                {
                    dendrites.first.push_back(pp[0].back());
                    dendrites.second.push_back(pp[1].back());
                }

                dendrites.first.push_back(NAN);
                dendrites.second.push_back(NAN);

                nodes.pop_back();

                if (node->has_child())
                {
                    branching_points.first.push_back(
                        node->get_position().at(0));
                    branching_points.second.push_back(
                        node->get_position().at(1));
                    NodePtr mynode = std::dynamic_pointer_cast<Node>(node);
                    nodes.push_front(mynode->get_child(0));
                    nodes.push_front(mynode->get_child(1));
                }
            }

            // if no axon, still mark it with NaNs
            if (neuron->neurites_.size() == 1)
            {
                axon.first.push_back(NAN);
                axon.second.push_back(NAN);
            }
        }
        // anyway get growth cones
        auto ite = neurite.second->gc_cend();
        for (auto it = neurite.second->gc_cbegin(); it != ite; it++)
        {
            double x = it->second->get_position().at(0);
            double y = it->second->get_position().at(1);
            growth_cones.first.push_back(x);
            growth_cones.second.push_back(y);
        }
        // add NaNs to help separate data from different neurons afterwards
        branching_points.first.push_back(NAN);
        branching_points.second.push_back(NAN);
        growth_cones.first.push_back(NAN);
        growth_cones.second.push_back(NAN);
    }
}
} // namespace growth
