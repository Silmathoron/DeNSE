#include "space_manager.hpp"

// C++ includes
#include <cmath>
#include <deque>
#include <iostream>
#include <sstream>

// Boost includes
#include <boost/geometry/algorithms/buffer.hpp>

// kernel include
#include "kernel_manager.hpp"

// lib include
#include "config_impl.hpp"
#include "growth_names.hpp"
#include "tools.hpp"

// spatial include
#include "Area.hpp"
#include "Environment.hpp"


namespace growth
{

const double epsilon = 1e-4;

void notice(const char *fmt, ...)
{
    //~ va_list ap;

    //~ printf( stdout, "NOTICE: ");

    //~ va_start (ap, fmt);
    //~ vfprintf( stdout, fmt, ap);
    //~ va_end(ap);
    //~ printf( stdout, "\n" );
}


void log_and_exit(const char *fmt, ...)
{
    //~ va_list ap;

    //~ printf( stdout, "ERROR: ");

    //~ va_start (ap, fmt);
    //~ vfprintf( stdout, fmt, ap);
    //~ va_end(ap);
    //~ printf( stdout, "\n" );
    //~ exit(1);
}


SpaceManager::SpaceManager()
    : initialized_(false)
    , interactions_(true)
    , environment_initialized_(false)
    , environment_manager_(nullptr)
    , context_handler_(nullptr)
    , points_per_circle_(DEFAULT_POINTS_PER_CIRCLE)
    , join_strategy_(DEFAULT_POINTS_PER_CIRCLE)
    , circle_strategy_(DEFAULT_POINTS_PER_CIRCLE)
{
}


void SpaceManager::initialize()
{
    context_handler_ = GEOS_init_r();

    GEOSContext_setNoticeHandler_r(context_handler_, notice);
    GEOSContext_setErrorHandler_r(context_handler_, log_and_exit);

    int num_omp = kernel().parallelism_manager.get_num_local_threads();

    geom_add_buffer_ = std::vector<space_tree_map>(num_omp);
    box_buffer_      = std::vector<std::vector<box_tree_tuple>>(num_omp);
    initialized_     = true;
}


void SpaceManager::finalize()
{
    // destroy all geometries
    map_geom_.clear();

    geom_add_buffer_.clear();
    box_buffer_.clear();

    GEOS_finish_r(context_handler_);
}


/*
 * Interacting with the STR-tree
 */

/**
 * Add a polygon to the R-tree (for obstacles)
 */
void SpaceManager::add_object(BMultiPolygonPtr geom, const ObjectInfo &info,
                              int omp_id)
{
    //~ geom_add_buffer_[omp_id][info] = geom;
}


void correct_polygon(
    const BPoint& vec_step, const BPoint& vec_ortho, const BPoint& stop,
    BPoint& lp_1, BPoint& lp_2, const BPoint &old_p1, const BPoint& old_p2,
    BRing& outer, BPolygonPtr last_segment, double diam)
{
    // lp1 is on the wrong side of old_p1->old_p2 so it disappears
    lp_1 = old_p1;
    outer.push_back(old_p1);
    outer.push_back(old_p2);

    // compute angle direction of side
    double side_angle = atan2(lp_2.y() - old_p2.y(), lp_2.x() - old_p2.x());

    // compute the intersection between the previous side (old_p2, lp2) and the
    // new front line going through lp_1 and stop.
    // Both segments are made `diameter`-long to be sure they intersect.
    lp_2 = BPoint(old_p2.x() + cos(side_angle)*diam,
                  old_p2.y() + sin(side_angle)*diam);

    BLineString side({old_p2, lp_2});

    // compute angle direction of new front
    double front_angle = atan2(stop.y() - lp_1.y(), stop.x() - lp_1.x());

    lp_2 = BPoint(lp_1.x() + cos(front_angle)*diam,
                  lp_1.y() + sin(front_angle)*diam);
    
    BLineString front({lp_1, lp_2});

    BMultiPoint mp;
    bg::intersection(side, front, mp);

    if (mp.empty())
    {
        std::cout << bg::wkt(vec_step) << std::endl;
        std::cout << bg::wkt(front) << std::endl;
        std::cout << bg::wkt(side) << std::endl;

        std::cout << bg::wkt(*(last_segment.get())) << std::endl;

        outer.push_back(lp_2);
        outer.push_back(old_p1);
        std::cout << bg::wkt(outer) << std::endl;

        lp_1 = BPoint(stop.x() + vec_ortho.x(), stop.y() + vec_ortho.y());
        lp_2 = BPoint(stop.x() - vec_ortho.x(), stop.y() - vec_ortho.y());
        std::cout << bg::wkt(lp_1) << std::endl;
        std::cout << bg::wkt(lp_2) << std::endl;

        outer.clear();
        outer.push_back(old_p1);
        outer.push_back(lp_1);
        outer.push_back(lp_2);
        outer.push_back(old_p2);
        outer.push_back(old_p1);
        std::cout << bg::wkt(outer) << std::endl;
        std::cout << bg::wkt(old_p1) << std::endl;
        std::cout << bg::wkt(old_p2) << std::endl;
        std::cout << bg::wkt(stop) << std::endl;
    }

    lp_2 = mp[0];
    
    outer.push_back(lp_2);
    outer.push_back(old_p1);
}


/**
 * Add a soma or a neurite segment to the R-tree and to the branch
 */
void SpaceManager::add_object(const BPoint &start, const BPoint &stop,
                              double diam, double length, double taper,
                              const ObjectInfo &info, BranchPtr b, int omp_id)
{
    // objects should be added only once the manager has been initialized
    if (initialized_)
    {
        BMultiPolygon geom;
        BPolygonPtr poly;

        bool is_soma = std::get<1>(info).empty() ? true : false;

        bool success = true;
        bg::validity_failure_type failure;
        std::string message;

        // set the distance strategy
        bg::strategy::buffer::distance_symmetric<double> distance_strategy(
            0.5 * diam);

        // check if soma (start = stop)
        if (is_soma)
        {
            // make disk centered on Point and of correct radius
            bg::buffer(start, geom, distance_strategy, side_strategy_,
                       join_strategy_, end_strategy_, circle_strategy_);

            poly = std::make_shared<BPolygon>(geom[0]);
        }
        else
        {
            BPoint l_vec(stop);
            bg::subtract_point(l_vec, start);

            double r_new(0.5 * (diam - length * taper));
            double norm = sqrt(l_vec.x() * l_vec.x() + l_vec.y() * l_vec.y());

            BPoint r_vec(-r_new * l_vec.y() / norm, r_new * l_vec.x() / norm);

            // get the last segment and prepare new last points
            BPolygonPtr last_segment = b->get_last_segment();

            BPoint lp_1 = BPoint(stop.x() + r_vec.x(), stop.y() + r_vec.y());
            BPoint lp_2 = BPoint(stop.x() - r_vec.x(), stop.y() - r_vec.y());

            // create an empty polygon and add the points
            poly         = std::make_shared<BPolygon>();
            BRing &outer = poly->outer();

            if (last_segment == nullptr)
            {
                outer.push_back(
                    BPoint(start.x() + r_vec.x(), start.y() + r_vec.y()));
                outer.push_back(lp_1);
                outer.push_back(lp_2);
                outer.push_back(
                    BPoint(start.x() - r_vec.x(), start.y() - r_vec.y()));
                outer.push_back(
                    BPoint(start.x() + r_vec.x(), start.y() + r_vec.y()));
            }
            else
            {
                const std::pair<BPoint, BPoint> &last_points =
                    b->get_last_points();

                outer.push_back(last_points.first);
                outer.push_back(lp_1);
                outer.push_back(lp_2);
                outer.push_back(last_points.second);
                outer.push_back(last_points.first);

                // check whether this did not create a self-crossing
                while (not bg::is_valid(*(poly.get()), failure))
                {
                    if (failure == bg::failure_self_intersections)
                    {
                        // self intersecting clear it up
                        outer.clear();
                        
                        if (bg::covered_by(stop, *(last_segment.get())))
                        {
                            printf("stop covered by last segment");
                            outer.push_back(last_points.first);
                            outer.push_back(lp_1);
                            outer.push_back(last_points.second);
                            outer.push_back(lp_2);
                            outer.push_back(last_points.first);
                        }
                        else
                        {
                            // get intersection between new and old last points
                            BLineString ls_new({lp_1, lp_2});
                            BLineString ls_old(
                                {last_points.first, last_points.second});
                            
                            BMultiPoint mp;
                            bg::intersection(ls_new, ls_old, mp);

                            // if they intersect, then the GC turned to much and
                            // we need to change the polygon
                            // otherwise it means that lp_1 and lp_2 are 
                            // inverted
                            if (mp.empty())
                            {
                                // lp_1 and lp_2 are inverted
                                outer.push_back(last_points.first);
                                outer.push_back(lp_2);
                                outer.push_back(lp_1);
                                outer.push_back(last_points.second);
                                outer.push_back(last_points.first);
                                
                                // printf("empty mp\n");
                                // std::cout << bg::wkt(*(last_segment.get())) << std::endl;

                                // std::cout << bg::wkt(lp_1) << std::endl;
                                // std::cout << bg::wkt(lp_2) << std::endl;
                                // std::cout << bg::wkt(last_points.first) << std::endl;
                                // std::cout << bg::wkt(last_points.second) << std::endl;

                                // outer.push_back(last_points.first);
                                // outer.push_back(lp_1);
                                // outer.push_back(lp_2);
                                // outer.push_back(last_points.second);
                                // outer.push_back(last_points.first);
                                // std::cout << bg::wkt(outer) << std::endl;
                                // std::cout << bg::wkt(stop) << std::endl;
                            }
                            else
                            {
                                // get which old and new point are closest to the
                                // intersection
                                double d1, d2, d1new, d2new;
                                d1    = bg::distance(mp[0], last_points.first);
                                d2    = bg::distance(mp[0], last_points.second);
                                d1new = bg::distance(mp[0], lp_1);
                                d2new = bg::distance(mp[0], lp_2);

                                // new polygon will start from closest point, go
                                // through stop, and finish someplace (computed)
                                // that way
                                if (d1 < d2)
                                {
                                    if (d1new > d2new)
                                    {
                                        std::swap(lp_1, lp_2);
                                    }

                                    correct_polygon(
                                        l_vec, r_vec, stop, lp_1, lp_2,
                                        last_points.first, last_points.second,
                                        outer, last_segment, 1.1*diam);
                                }
                                else
                                {
                                    if (d1new < d2new)
                                    {
                                        std::swap(lp_1, lp_2);
                                    }

                                    correct_polygon(
                                        l_vec, r_vec, stop, lp_2, lp_1,
                                        last_points.second, last_points.first,
                                        outer, last_segment, 1.1*diam);
                                }
                            }
                        }
                    }
                    else if (failure == bg::failure_wrong_orientation)
                    {
                        bg::correct(*(poly.get()));
                    }
                    else
                    {
                        success = false;
                    }
                }

#ifndef NDEBUG
                if (not bg::is_valid(*(poly.get()), message))
                {
                    printf("difference is empty; invalid poly %s\n",
                           message.c_str());
                    std::cout << "last points: " << bg::wkt(last_points.first)
                              << " " << bg::wkt(last_points.second)
                              << std::endl;
                    std::cout << bg::wkt(*(poly.get())) << std::endl;
                    std::cout << bg::wkt(*(last_segment.get())) << std::endl;
                    success = false;
                }
#endif
            }

            if (success)
            {
                b->add_point(stop, length, poly, lp_1, lp_2);
            }
        }

        if (success)
        {
            // add polygon
            geom_add_buffer_[omp_id][info] = poly;
            // add box, so `true` in box buffer
            BBox box = bg::return_envelope<BBox>(*(poly.get()));
            box_buffer_[omp_id].push_back(std::make_tuple(info, box, true));
        }
#ifndef NDEBUG
        else
        {
            printf("polygon addition failed: %s\n", message.c_str());
        }
#endif
    }
}


void SpaceManager::remove_object(const BBox &box, const ObjectInfo &info,
                                 int omp_id)
{
    // remove box, so `false` in box buffer
    box_buffer_[omp_id].push_back(std::make_tuple(info, box, false));
}


void SpaceManager::update_objects_branching(TNodePtr old_node, NodePtr new_node,
                                            size_t branching_point,
                                            size_t neuron,
                                            const std::string &neurite,
                                            int omp_id)
{
    BPolygonPtr poly;
    BBox box;
    ObjectInfo info;

    size_t old_id(old_node->get_nodeID()), new_id(new_node->get_nodeID());
    const BranchPtr old_branch(old_node->get_branch());
    const BranchPtr new_branch(new_node->get_branch());

    // change node ids below branching point
    for (size_t i = 0; i < branching_point; i++)
    {
        poly = new_branch->get_segment_at(i);
        box  = bg::return_envelope<BBox>(*(poly.get()));
        info = std::make_tuple(neuron, neurite, old_id, i);

        // remove old version
        remove_object(box, info, omp_id);

        // readd, associated to new node
        info = std::make_tuple(neuron, neurite, new_id, i);
        // add polygon
        geom_add_buffer_[omp_id][info] = poly;
        // add box, so `true` in box buffer
        box_buffer_[omp_id].push_back(std::make_tuple(info, box, true));
    }

    // change segment ids above branching point
    size_t imax = old_branch->size() > 0 ? old_branch->size() - 1 : 0;

    for (size_t i = 0; i < imax; i++)
    {
        poly = old_branch->get_segment_at(i);
        box  = bg::return_envelope<BBox>(*(poly.get()));

        // remove old version
        info = std::make_tuple(neuron, neurite, old_id, i + branching_point);
        remove_object(box, info, omp_id);

        // readd, associated to new segment id
        info = std::make_tuple(neuron, neurite, old_id, i);
        // add polygon
        geom_add_buffer_[omp_id][info] = poly;
        // add box, so `true` in box buffer
        box_buffer_[omp_id].push_back(std::make_tuple(info, box, true));
    }
}


void SpaceManager::get_objects_in_range(const BPoint &p, double radius,
                                        std::vector<ObjectInfo> &v) const
{
    if (not map_geom_.empty())
    {
        // make box centered on Point and of correct size
        BPoint min_corner(p), max_corner(p);

        bg::add_point(min_corner, bg::make<BPoint>(-radius, -radius));
        bg::add_point(max_corner, bg::make<BPoint>(radius, radius));

        BBox box(min_corner, max_corner);

        // get the values inside the BBox
        std::vector<RtreeValue> returned_values;
        rtree_.query(bgi::intersects(box), std::back_inserter(returned_values));

        //~ bool target, got_target(false);
        // get the associated ObjectInfo
        for (const auto &value : returned_values)
        {
            //~ target = (std::get<0>(value.second) == 5 and
            //std::get<1>(value.second) == "dendrite_2" ~ and
            //std::get<2>(value.second) == 1 and std::get<3>(value.second) ==
            //0);
            //~ got_target += target;
            //~ if (target)
            //~ printf("got it\n");
            v.push_back(value.second);
        }

        //~ if (got_target)
        //~ printf("\n");
    }
}


void SpaceManager::update_rtree()
{
#pragma omp single
    {
        // box buffer is keeping track of the proper order of the addition and
        // removal operations, so we follow it

        for (size_t i = 0; i < box_buffer_.size(); i++)
        {
            const space_tree_map &gmap = geom_add_buffer_[i];
            const auto &v              = box_buffer_[i];

            // first loop on the OpenMP vector
            for (const auto tpl : v)
            {
                // second loop over the operations
                const ObjectInfo &info = std::get<0>(tpl);
                const BBox &box        = std::get<1>(tpl);

                // addition or removal?
                if (std::get<2>(tpl))
                {
                    // addition
                    rtree_.insert(std::make_pair(box, info));
                    auto it = gmap.find(info);
                    if (it == gmap.end())
                    {
                        printf("not in gmap\n");
                        printf("%lu %s %lu %lu\n", std::get<0>(info),
                               std::get<1>(info).c_str(), std::get<2>(info),
                               std::get<3>(info));
                    }
                    map_geom_[info] = gmap.at(info);
                }
                else
                {
                    // removal
                    rtree_.remove(RtreeValue({box, info}));

                    auto it_geom = map_geom_.find(info);
                    if (it_geom == map_geom_.end())
                    {
                        printf("not in map_geom_\n");
                        printf("%lu %s %lu %lu\n", std::get<0>(info),
                               std::get<1>(info).c_str(), std::get<2>(info),
                               std::get<3>(info));
                    }
                    map_geom_.erase(it_geom);
                }
            }

            // clear containers
            box_buffer_[i].clear();
            geom_add_buffer_[i].clear();
        }
    }
}


/*
 * Space sensing
 */

bool SpaceManager::sense(std::vector<double> &directions_weights,
                         std::vector<bool> &wall_presence,
                         const Filopodia &filopodia, const BPoint &position,
                         const Move &move, const std::string &area,
                         double proba_down_move, double max_height_up_move,
                         Affinities aff_values, double substep, double radius,
                         GCPtr gc_ptr, space_tree_map &neighbors)
{
    // Useful values
    double subs_afty    = filopodia.substrate_affinity;
    double len_filo     = filopodia.finger_length;
    double wall_afty    = filopodia.wall_affinity;
    AreaPtr old_area    = areas_[area];
    double old_height   = old_area->get_height();
    double lamel_factor = 2.;

    size_t neuron_id                = gc_ptr->get_neuron_id();
    const std::string &neurite_name = gc_ptr->get_neurite_name();

    double aff_self                 = aff_values.affinity_self;
    double aff_axon_same_neuron     = aff_values.affinity_axon_same_neuron;
    double aff_axon_other_neuron    = aff_values.affinity_axon_other_neuron;
    double aff_dendrite_same_neuron = aff_values.affinity_dendrite_same_neuron;
    double aff_dendrite_other_neuron =
        aff_values.affinity_dendrite_other_neuron;
    double aff_soma_same_neuron  = aff_values.affinity_soma_same_neuron;
    double aff_soma_other_neuron = aff_values.affinity_soma_other_neuron;

    // values used locally inside loop
    double angle, distance, distance1, min_wall_dist;
    bool filo_wall, interacting(false), keep_point, intsct;
    BPoint filo_pos, tmp_pos;
    // intersections of the filopodia
    double x0(position.x()), y0(position.y()), x, y, x1, y1, point_angle;
    std::unordered_map<std::string, AreaPtr> tested_areas;
    std::vector<double> distances, affinities;
    BMultiPoint intersections;
    //~ std::vector<AreaPtr> new_areas;
    BLineString filo_line, tmp_line;
    std::vector<size_t> indices;
    std::vector<bool> is_wall;
    std::string name_new_area;
    size_t n_intersect;
    AreaPtr new_area;
    GEOSGeom border;
    // recursive loop
    std::deque<AreaPtr> area_deque;
    std::deque<BPoint> area_points;
    // computation of the affinity
    double affinity, old_dist, current_dist;
    // loop indices
    unsigned int i, n_angle, s_i;

    // get the properties of the neighboring geometries
    std::vector<ObjectInfo> neighbors_info;
    get_objects_in_range(position, move.module, neighbors_info);

    bool check = false;

    // test the environment for each of the filopodia's angles
    for (n_angle = 0; n_angle < filopodia.size; n_angle++)
    {
        filo_wall = false;

        angle = move.angle + filopodia.directions[n_angle];

        // clear all containers
        distances.clear();
        affinities.clear();
        is_wall.clear();
        tested_areas.clear();
        area_deque.clear();
        area_points.clear();
        //~ new_areas.clear();

        // set position/line
        filo_pos  = BPoint(position.x() + cos(angle) * len_filo,
                          position.y() + sin(angle) * len_filo);
        filo_line = line_from_points(position, filo_pos);

        // set first (current) affinity
        affinities.push_back(subs_afty);

        min_wall_dist = std::numeric_limits<double>::infinity();

        // ============================ //
        // Find GC-neurite interactions //
        // ============================ //

        // in a first time, we get all the intersections between the
        // filopodia and other neurites in ``neighbors``.
        if (interactions_)
        {
            BPolygonPtr other;
            size_t other_neuron, other_node, other_segment;
            std::string other_neurite;

            //~ bool target = (neuron_id == 12 and neurite_name == "dendrite_2"
            //~ and gc_ptr->get_nodeID() == 3);

            for (const auto &info : neighbors_info)
            {
                other         = map_geom_[info];
                other_neuron  = std::get<0>(info);
                other_neurite = std::get<1>(info);
                other_node    = std::get<2>(info);
                other_segment = std::get<3>(info);

                bool other_intersects = false;

                // check for intersections
                if (other == gc_ptr->get_last_segment())
                {
                    // ignore possible intersection with last segment
                    other_intersects = false;
                }
                else if (other_neuron == neuron_id and
                         neurite_name == other_neurite and
                         gc_ptr->get_centrifugal_order() > 1
                         and gc_ptr->get_branch()->size() < 30)
                {
                    // check for potential intersections at branching points
                    size_t nid      = gc_ptr->get_nodeID();
                    bool bneighbors = other_node == nid + 1;
                    bneighbors += other_node == nid + 2;
                    if (nid > 1)
                    {
                        bneighbors += other_node == nid - 1;
                    }
                    if (nid > 2)
                    {
                        bneighbors += other_node == nid - 2;
                    }

                    // we limit to the first segments
                    if (bneighbors and other_segment < 10)
                    {
                        other_intersects = false;
                    }
                }
                else if (intersects(*(other.get()), filo_line))
                {
                    other_intersects = true;

                    intersections.clear();

                    get_intersections(filo_line, *(other.get()), intersections);
                }

                // compute interaction
                if (other_intersects)
                {
                    interacting = true;
                    double other_affinity = 0.;

                    if (other_neurite.empty())
                    {
                        // soma
                        if (other_neuron == neuron_id)
                        {
                            other_affinity = aff_soma_same_neuron;
                        }
                        else
                        {
                            other_affinity = aff_soma_other_neuron;
                        }
                    }
                    else if (other_neuron == neuron_id)
                    {
                        if (other_neurite == neurite_name)
                        {
                            // self-interaction
                            other_affinity = aff_self;
                        }
                        else if (other_neurite == "axon")
                        {
                            other_affinity = aff_axon_same_neuron;
                        }
                        else
                        {
                            other_affinity = aff_dendrite_same_neuron;
                        }
                    }
                    else
                    {
                        if (other_neurite == "axon")
                        {
                            other_affinity = aff_axon_other_neuron;
                        }
                        else
                        {
                            other_affinity = aff_dendrite_other_neuron;
                        }
                    }

                    // store forbidden intersections in neighbors
                    if (std::isnan(other_affinity))
                    {
                        auto info_it = neighbors.find(info);

                        if (info_it == neighbors.end())
                        {
                            neighbors[info] = other;
                        }
                    }

                    if (intersections.empty())
                    {
                        // printf("there are %lu intersections\n",
                        // intersections.size()); printf("polygon is valid:
                        // %i\n", bg::is_valid(*(other.get()))); printf("polygon
                        // intersects line: %i\n", bg::intersects(filo_line,
                        // *(other.get()))); std::cout << bg::wkt(filo_line) <<
                        // std::endl; std::cout << bg::wkt(*(other.get())) <<
                        // std::endl;
                        throw std::runtime_error(
                            "Empty intersection in `sense`");
                    }

                    // append distance and affinities to the containers
                    x = intersections[0].x();
                    y = intersections[0].y();

                    distance = sqrt((x - x0) * (x - x0) + (y - y0) * (y - y0));

                    if (intersections.size() > 1)
                    {
                        // goes through the segment so compute second
                        // intersection
                        x1 = intersections[1].x();
                        y1 = intersections[1].y();
                        distance1 =
                            sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));

                        // save shortest distance with other_affinity
                        is_wall.push_back(false);
                        affinities.push_back(other_affinity);
                        distances.push_back(std::min(distance, distance1));

                        // get containing area after neurite
                        tmp_pos = (distance > distance1) ? BPoint(x, y)
                                                         : BPoint(x1, y1);
                        name_new_area = get_containing_area(tmp_pos);

                        // store longest distance associated to
                        // underlying area
                        distances.push_back(std::max(distance, distance1));

                        if (name_new_area.empty())
                        {
                            is_wall.push_back(true);
                            affinities.push_back(wall_afty);
                            min_wall_dist = distances.back();
                        }
                        else
                        {
                            is_wall.push_back(false);
                            affinities.push_back(
                                areas_[name_new_area]->get_property(
                                    names::substrate_affinity));
                        }
                    }
                    else
                    {
                        // does not go through
                        is_wall.push_back(false);
                        distances.push_back(distance);
                        affinities.push_back(other_affinity);
                    }
                }
            }
        }

        // ======================================= //
        // Find intersections with the environment //
        // ======================================= //

        // in a second time, we get all the intersections between the
        // filopodia and the environment (walls + areas)

        // intersections with areas (and indirectly with walls)
        // "recursively"
        if (intersects(area, filo_line))
        {
            interacting = true;

            area_points.push_back(position);
            area_deque.push_back(old_area);

            while (not area_deque.empty())
            {
                // get new area and its associated point
                new_area      = area_deque.front();
                tmp_pos       = area_points.front();
                name_new_area = new_area->get_name();
                tmp_line      = line_from_points(tmp_pos, filo_pos);

                auto it = tested_areas.find(name_new_area);
                intsct  = it == tested_areas.end();

                // remove from the deque
                area_deque.pop_front();
                area_points.pop_front();

                // test intersections only if not already done
                if (intsct and intersects(name_new_area, tmp_line))
                {
                    // add area into tested areas
                    tested_areas[name_new_area] = new_area;
                    // check intersections
                    intersections.clear();
                    get_intersections(tmp_line, new_area->get_boundary(),
                                      intersections);

                    for (auto p : intersections)
                    {
                        // compute distance
                        x = p.x();
                        y = p.y();
                        distance =
                            sqrt((x - x0) * (x - x0) + (y - y0) * (y - y0));

                        keep_point = std::abs(x - tmp_pos.x()) > epsilon or
                                     std::abs(y - tmp_pos.y()) > epsilon;
                        keep_point *= (min_wall_dist - distance > epsilon);

                        // intersection must be before the wall, we also exclude
                        // the wall intersection itself because we did it in
                        // a previous iteration
                        if (keep_point)
                        {
                            // we have an intersection before the wall
                            // compute the point just after the intersection
                            point_angle = atan2(y - y0, x - x0);
                            tmp_pos     = BPoint(
                                x0 + (distance + epsilon) * cos(point_angle),
                                y0 + (distance + epsilon) * sin(point_angle));

                            // save distance
                            distances.push_back(distance);

                            // get containing area
                            name_new_area = get_containing_area(tmp_pos);
                            it = tested_areas.find(name_new_area);

                            if (name_new_area.empty())
                            {
                                filo_wall = true;
                                //~ new_areas.push_back(nullptr);
                                is_wall.push_back(true);
                                affinities.push_back(wall_afty);
                                // compute distance of first wall
                                min_wall_dist =
                                    std::min(distance, min_wall_dist);
                            }
                            else
                            {
                                //~ new_areas.push_back(areas_[name_new_area]);
                                is_wall.push_back(false);
                                affinities.push_back(
                                    areas_[name_new_area]->get_property(
                                        names::substrate_affinity));
                            }

                            // store the new areas with which we should check
                            // for intersections in the deque
                            if (not name_new_area.empty() and
                                it == tested_areas.end())
                            {
                                area_deque.push_back(areas_[name_new_area]);
                                area_points.push_back(tmp_pos);
                            }
                        }
                    }
                }
            }
        }

        // ===================== //
        // Sort them by distance //
        // ===================== //

        // sort distances and get indices
        indices.resize(distances.size());
        std::iota(indices.begin(), indices.end(), 0);
        auto comparator = [&distances](int a, int b) {
            return distances[a] < distances[b];
        };
        std::sort(indices.begin(), indices.end(), comparator);

        // we are going to add a last entry, push back indices
        indices.push_back(distances.size());
        // add the last point
        distances.push_back(len_filo);
        is_wall.push_back(false);
        // note that we don't need to push-back affinities because they got
        // the additional entry from the start (the first affinity where the
        // filopodia initiates)

        // ============================ //
        // Compute the average affinity //
        // ============================ //

        old_dist = 0.;
        affinity = indices.empty() ? len_filo * subs_afty : 0.;

        for (i = 0; i < indices.size(); i++)
        {
            // sorted index
            s_i          = indices[i];
            current_dist = distances[s_i];

            // if it's a forbidden direction, then affinity goes
            // to NaN and we break
            if (std::isnan(affinities[s_i]))
            {
                affinity = std::nan("");
                break;
            }

            // are we compressing the tip (dist <= radius)?
            if (current_dist <= radius)
            {
                // if its a wall then affinity goes to NaN and we break
                if (is_wall[s_i])
                {
                    affinity = std::nan("");
                    break;
                }
                else
                {
                    affinity += lamel_factor * affinities[s_i] *
                                (current_dist - old_dist);
                }
            } // are we dealing with the lamellipodia?
            else if (current_dist < 0.5 * len_filo)
            {
                affinity +=
                    lamel_factor * affinities[s_i] * (current_dist - old_dist);
            } // are we in-between
            else if (old_dist < 0.5 * len_filo)
            {
                // part for lamellipodia
                affinity += lamel_factor * affinities[s_i] *
                            (0.5 * len_filo - old_dist);
                // part for filopodia
                affinity += affinities[s_i] * (current_dist - 0.5 * len_filo);
            }
            else
            {
                affinity += affinities[s_i] * (current_dist - old_dist);
            }

            // check where we stand: did we reach a wall or do we keep going?
            if (is_wall[s_i])
            {
                // we will break the loop so we jump directly to the last
                // point (the filopodia length)
                affinity += wall_afty * (len_filo - current_dist);
                break;
            }
            else
            {
                old_dist = current_dist;
            }
        }

        affinity                    = std::max(affinity / len_filo, 0.);
        directions_weights[n_angle] = affinity;

        // question: what about the transitions for up/down?
        // answer: move to compute_accessibility! (noob)
    }

#ifndef NDEBUG
    if (check);
#endif

    return interacting;
}


void SpaceManager::check_accessibility(std::vector<double> &directions_weights,
                                       const Filopodia &filopodia,
                                       const BPoint &position, const Move &move,const BPolygonPtr last_segment)
{
    unsigned int n_angle;
    BPoint target_pos;
    double angle;

    // test the environment for each of the filopodia's angles
    //~ for (n_angle = n_min; n_angle < n_max; n_angle++)
    for (n_angle = 0; n_angle < filopodia.size; n_angle++)
    {
        angle = move.angle + filopodia.directions.at(n_angle);

        target_pos = BPoint(position.x() + cos(angle) * move.module,
                           position.y() + sin(angle) * move.module);

        const BLineString &line = line_from_points(position, target_pos);

        if (intersects("environment", line))
        {
            directions_weights[n_angle] = std::nan(""); // cannot escape env
        }
        else if (last_segment != nullptr and
                 bg::covered_by(target_pos, *(last_segment.get())))
        {
            directions_weights[n_angle] = std::nan(""); // cannot cross oneself
        }
    }
}


bool SpaceManager::env_contains(const BPoint &point) const
{
    if (environment_initialized_ == false)
    {
        return 1;
    }

    return bg::within(point, *(environment_manager_->get_environment().get()));
}


bool SpaceManager::is_inside(const BPoint &point, size_t neuron,
                             const std::string &neurite, double radius,
                             BPolygon &polygon) const
{
    std::vector<ObjectInfo> neighbors;
    get_objects_in_range(point, radius, neighbors);

    bool inside = false;

    for (auto n : neighbors)
    {
        if (std::get<0>(n) == neuron and std::get<1>(n) == neurite)
        {
            if (bg::covered_by(point, *(map_geom_.at(n).get())))
            {
                polygon = *(map_geom_.at(n).get());
                inside  = true;
                break;
            }
        }
    }
}


double SpaceManager::get_wall_distance(const BPoint &position, int omp_id) const
{
    //~ GEOSGeometry *boundary;
    //~ double distance;

    //~ boundary = GEOSBoundary_r(context_handler_,
    //~ environment_manager_->get_environment());

    //~ GEOSDistance_r(context_handler_, boundary, geospoint.get(), &distance);

    //~ GEOSGeom_destroy_r(context_handler_, boundary);

    //~ return distance;
}


bool SpaceManager::intersects(const std::string &object_name,
                              const BLineString &line) const
{
    if (environment_initialized_ == false)
    {
        return 0;
    }

    BMultiLineString geom;

    if (object_name == "environment")
    {
        geom = environment_manager_->get_boundary();
    }
    else
    {
        geom = areas_.at(object_name)->get_boundary();
    }

    return bg::crosses(line, geom);
}


bool SpaceManager::intersects(const BPolygon &object,
                              const BLineString &line) const
{
    return bg::crosses(line, object);
}


void SpaceManager::get_intersections(const BLineString &line,
                                     const BMultiLineString &boundary,
                                     BMultiPoint &points) const
{
    if (environment_initialized_)
    {
        bg::intersection(line, boundary, points);
    }
}


void SpaceManager::get_intersections(const BLineString &line,
                                     const BPolygon &geometry,
                                     BMultiPoint &points) const
{
    if (environment_initialized_)
    {
        bg::intersection(line, geometry, points);
    }
}


/**
 * @brief returns the absolute value of the angle widening necessary to unstuck
 */
double SpaceManager::unstuck_angle(const BPoint &position, double current_angle,
                                   double radius, const std::string &area,
                                   int omp_id)
{
    // make the circle with a buffer
    bg::strategy::buffer::distance_symmetric<double> distance_strategy(radius);

    BMultiPolygon tmp;
    bg::buffer(position, tmp, distance_strategy, side_strategy_, join_strategy_,
               end_strategy_, circle_strategy_);

    BLineString circle = BLineString();
    circle.insert(circle.end(), tmp[0].outer().begin(), tmp[0].outer().end());

    BMultiPoint intsct;

    // get the intersections with area if environment is present
    if (environment_initialized_)
    {
        BMultiPolygonPtr a = areas_.at(area)->get_area();

        for (auto &p : *(a.get()))
        {
            if (bg::crosses(circle, p.outer()))
            {
                bg::intersection(circle, p.outer(), intsct);
            }

            for (auto &inner : p.inners())
            {
                if (bg::crosses(circle, inner))
                {
                    bg::intersection(circle, inner, intsct);
                }
            }
        }
    }

    // get neighbouring neurons/neurites
    std::vector<ObjectInfo> neighbors_info;
    get_objects_in_range(position, radius, neighbors_info);

    for (auto info : neighbors_info)
    {
        BPolygonPtr other = map_geom_[info];
        // check for intersections
        if (bg::crosses(*(other.get()), circle))
        {
            get_intersections(circle, *(other.get()), intsct);
        }
    }

    size_t num_intersect = intsct.size();
    double angle_first(0.), angle_last(3.15);

    if (num_intersect)
    {
        BPoint p = intsct.front();

        angle_first =
            fmod(std::abs(atan2(p.y() - position.y(), p.x() - position.x()) -
                          current_angle),
                 M_PI);

        if (num_intersect > 1)
        {
            BPoint p2 = intsct.back();

            angle_last = fmod(
                std::abs(atan2(p2.x() - position.y(), p2.x() - position.x()) -
                         current_angle),
                M_PI);

            if (angle_last < angle_first)
            {
                angle_first = angle_last;
            }
        }
    }

    return angle_first;
}


bool SpaceManager::has_environment() const { return environment_initialized_; }


const BRing &SpaceManager::get_env_border(int omp_id) const
{
    return environment_manager_->get_environment()->at(0).outer();
}


void _get_p_at_dist(const BLineString &line, const BMultiPoint &intersections,
                    double radius, BPoint &p, double &distance)
{
    std::vector<double> distances;
    double x0(line.at(0).x()), y0(line.at(0).y());
    double x, y, theta;

    // find closest intesection
    for (auto p : intersections)
    {
        x = p.x();
        y = p.y();
        distances.push_back(sqrt((x - x0) * (x - x0) + (y - y0) * (y - y0)));
    }

    auto it_min = std::min_element(distances.begin(), distances.end());
    size_t nmin = std::distance(distances.begin(), it_min);

    if (distances[nmin] > radius)
    {
        distance = distances[nmin] - radius;

        x = intersections[nmin].x();
        y = intersections[nmin].y();

        theta = atan2(y - y0, x - x0);

        // go to the intersection-0.5*diameter
        bg::set<0>(p, x0 + distance * cos(theta));
        bg::set<1>(p, y0 + distance * sin(theta));
    }
    else
    {
        distance = 0.;
        bg::set<0>(p, x0);
        bg::set<1>(p, y0);
    }
}


void SpaceManager::get_point_at_distance(const BLineString &line,
                                         const std::string &geom_name,
                                         double radius, BPoint &p,
                                         double &distance)
{
    // get the intersections and distances from position
    BMultiPoint intersections;

    if (geom_name == "environment")
    {
        get_intersections(line, environment_manager_->get_boundary(),
                          intersections);
    }
    else
    {
        get_intersections(line, areas_[geom_name]->get_boundary(),
                          intersections);
    }

    _get_p_at_dist(line, intersections, radius, p, distance);
}


void SpaceManager::get_point_at_distance(const BLineString &line,
                                         const BPolygonPtr polygon,
                                         double radius, BPoint &p,
                                         double &distance)
{
    // get the intersections and distances from position
    BMultiPoint intersections;

    get_intersections(line, *(polygon.get()), intersections);

    _get_p_at_dist(line, intersections, radius, p, distance);
}


void SpaceManager::copy_polygon(BMultiPolygonPtr copy, const BPolygon &p)
{
    bg::clear(*(copy.get()));
    copy->resize(1);

    // outer ring
    auto &ring = copy->at(0).outer();
    ring.insert(ring.end(), p.outer().begin(), p.outer().end());

    // inner rings
    auto &inners = copy->at(0).inners();
    inners.insert(inners.end(), p.inners().begin(), p.inners().end());
}


void SpaceManager::copy_polygon(BMultiPolygonPtr copy, const BMultiPolygon &p)
{
    size_t num_poly = p.size();
    bg::clear(*(copy.get()));
    copy->resize(num_poly);

    for (size_t i = 0; i < num_poly; i++)
    {
        auto &ring = copy->at(i).outer();

        const auto &pring = p.at(i).outer();
        ring.insert(ring.end(), pring.begin(), pring.end());

        auto &inners        = copy->at(i).inners();
        const auto &pinners = p.at(i).inners();
        inners.insert(inners.end(), pinners.begin(), pinners.end());
    }
}


void SpaceManager::set_environment(
    GEOSGeometry *environment, const std::vector<GEOSGeometry *> &areas,
    const std::vector<double> &heights, const std::vector<std::string> &names,
    const std::vector<std::unordered_map<std::string, double>> &properties)
{
    const GEOSGeom env = static_cast<GEOSGeom>(environment);
    assert(GEOSisValid_r(context_handler_, env));

    GEOSWKTWriter *writer = GEOSWKTWriter_create_r(context_handler_);
    char *wkt = GEOSWKTWriter_write_r(context_handler_, writer, environment);

    BPolygon env_tmp;
    bg::read_wkt(std::string(wkt), env_tmp);
    BMultiPolygonPtr b_env = std::make_shared<BMultiPolygon>();
    copy_polygon(b_env, env_tmp);
    delete wkt;

    environment_manager_ = std::unique_ptr<Environment>(new Environment(b_env));

    BPolygon area_tmp;
    BMultiPolygon area_multi_tmp;
    BMultiPolygonPtr b_area;

    for (size_t i = 0; i < areas.size(); i++)
    {
        b_area = std::make_shared<BMultiPolygon>();
        wkt    = GEOSWKTWriter_write_r(context_handler_, writer, areas[i]);

        // check area type (can be multipolygon)
        int area_type = GEOSGeomTypeId_r(context_handler_, areas[i]);

        if (area_type == GEOS_MULTIPOLYGON)
        {
            bg::read_wkt(std::string(wkt), area_multi_tmp);
            copy_polygon(b_area, area_multi_tmp);
        }
        else
        {
            bg::read_wkt(std::string(wkt), area_tmp);
            copy_polygon(b_area, area_tmp);
        }

        delete wkt;

        areas_[names[i]] =
            std::make_shared<Area>(b_area, heights[i], names[i], properties[i]);
    }

    GEOSWKTWriter_destroy_r(context_handler_, writer);

    environment_initialized_ = true;
}


void SpaceManager::get_environment(
    GEOSGeom &environment, std::vector<GEOSGeometry *> &areas,
    std::vector<double> &heights, std::vector<std::string> &names,
    std::vector<std::unordered_map<std::string, double>> &properties) const
{
    GEOSWKTReader *reader = GEOSWKTReader_create_r(context_handler_);
    std::stringstream s;
    std::string wkt;

    GEOSGeom geom_tmp, area;

    if (environment_manager_ != nullptr)
    {
        s << std::setprecision(12)
          << bg::wkt(*(environment_manager_->get_environment().get()));
        geom_tmp =
            GEOSWKTReader_read_r(context_handler_, reader, s.str().c_str());

        environment =
            GEOSGeom_clone_r(context_handler_,
                             GEOSGetGeometryN_r(context_handler_, geom_tmp, 0));

        GEOSGeom_destroy_r(context_handler_, geom_tmp);
    }

    for (auto &a : areas_)
    {
        s.str("");
        s << bg::wkt(*(a.second->get_area().get()));

        geom_tmp =
            GEOSWKTReader_read_r(context_handler_, reader, s.str().c_str());

        if (GEOSGetNumGeometries_r(context_handler_, geom_tmp) == 1)
        {
            // convert to polygon
            area = GEOSGeom_clone_r(
                context_handler_,
                GEOSGetGeometryN_r(context_handler_, geom_tmp, 0));
            // add to container
            areas.push_back(area);
            // delete tmp
            GEOSGeom_destroy_r(context_handler_, geom_tmp);
        }
        else
        {
            // multipolygon: push directly to container and do not destroy
            areas.push_back(geom_tmp);
        }

        // set properties
        heights.push_back(a.second->get_height());
        names.push_back(a.second->get_name());

        std::unordered_map<std::string, double> prop;
        a.second->get_properties(prop);
        properties.push_back(prop);
    }

    GEOSWKTReader_destroy_r(context_handler_, reader);
}


void SpaceManager::init_spatial_grid(int local_num_threads)
{
    for (int i = 0; i < local_num_threads; i++)
    {
    }
    // spatial_grid.push_back(Space::Shape());
}


int SpaceManager::get_region_thread(const BPoint &position) const
{
    return get_region_thread(position.x(), position.y());
}


int SpaceManager::get_region_thread(double x, double y) const
{
    // @TODO
    return 0;
}


std::string SpaceManager::get_containing_area(const BPoint &position) const
{
    if (environment_initialized_ == false)
    {
        return "";
    }

    bool contained = false;

    for (auto &area : areas_)
    {
        contained = bg::within(position, *(area.second->get_area().get()));

        if (contained)
        {
            return area.first;
        }
    }

    // if we get there, it means that this point is out of the environment
    return "";
}


AreaPtr SpaceManager::get_area(const std::string &name) const
{
    auto it = areas_.find(name);

    if (it == areas_.end())
    {
        return nullptr;
    }

    return areas_.at(name);
}


std::vector<std::string> SpaceManager::get_area_names() const
{
    std::vector<std::string> names;

    for (const auto a : areas_)
    {
        names.push_back(a.first);
    }

    return names;
}


void SpaceManager::get_area_properties(
    const std::string &area,
    std::unordered_map<std::string, double> &prop) const
{
    areas_.at(area)->get_properties(prop);
}


void SpaceManager::set_status(const statusMap &config)
{
    /*    int dim = Space::DEFAULT_DIM;*/
    // get_param(config, "dimension", dim);
    /*Space::set_dimension_(dim);*/
    get_param(config, names::interactions, interactions_);
}


void SpaceManager::get_status(statusMap &status) const
{
    set_param(status, "environment_initialized", environment_initialized_, "");
    set_param(status, names::interactions, interactions_, "");
    // set_param(status, "dimension", Space::get_dimension());
}


void SpaceManager::num_threads_changed(int num_omp)
{
    geom_add_buffer_ = std::vector<space_tree_map>(num_omp);
    box_buffer_      = std::vector<std::vector<box_tree_tuple>>(num_omp);
}


GEOSContextHandle_t SpaceManager::get_context_handler() const
{
    return context_handler_;
}

} // namespace growth
