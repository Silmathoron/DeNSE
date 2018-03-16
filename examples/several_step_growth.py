#!/usr/bin/env python
#-*- coding:utf-8 -*-

from pprint import pprint

import numpy as np
import matplotlib.pyplot as plt

import nngt
import NetGrowth as ng


neuron_params = {
    "use_critical_resource": False,
    "growth_cone_model": "persistent_random_walk",
    "use_uniform_branching": False,
    "use_van_pelt": False,

    "rw_persistence_length": 20.,
    "rw_memory_tau": 100.,
    "sensing_angle": 0.1433,

    "speed_growth_cone": 0.01,

    "filopodia_wall_affinity": 0.05,
    "filopodia_finger_length": 20.,
    "filopodia_min_number": 30
}

dendrite_params = {
    "speed_growth_cone": 0.001,
}



if __name__ =='__main__':
    # ~ kernel={"seeds":[33, 64, 84, 65, 68, 23],
            # ~ "num_local_threads": 6,
            # ~ "resolution": 30.}
    kernel={
        "seeds":[35],
        "num_local_threads": 1,
        "resolution": 30.,
        "environment_required": False
    }

    ng.SetKernelStatus(kernel)

    '''
    Create neurons
    '''

    neuron_params['position'] = (0, 0)

    gids = ng.CreateNeurons(n=1, growth_cone_model='random_walk',
                            params = neuron_params,
                            dendrites_params=dendrite_params,
                            num_neurites=3)

    '''
    Create recorders
    '''

    gids_rec = ng.CreateRecorders(gids, "length", levels="growth_cone")

    '''
    Simulate first non-branching period
    '''

    ng.Simulate(hours=20)
    ng.PlotNeuron(show_nodes=True, show=True)


    '''
    Change the parameters to include growth cone splitting
    '''

    print("\nVan Pelt branching ON\n")

    vp_params = {
        "use_van_pelt": True,
        "gc_split_angle_mean": 30.,
        "gc_split_angle_std": 5.,
        "B" : 900.,
        "E" : 0.1,
        "S" : 1.5, # large S leads to core dump
        "T" : 8.6e2,
    }

    dend_params = vp_params.copy()
    #~ dend_params["T"] = 1e-8

    ng.SetStatus(gids, params=vp_params, dendrites_params=dend_params)
    # pprint(ng.GetStatus(gids))

    ng.Simulate(hours=12, days=1)
    print("Simulation done")
    ng.PlotNeuron(show_nodes=True, show=True)


    '''
    Change the parameters to include lateral branching
    '''

    print("\nLateral branching ON\n")

    lat_params = {
        "use_van_pelt": False,
        "use_uniform_branching": True,
        "speed_growth_cone": 0.005,
        "uniform_branching_rate": 0.00005,
        "lateral_branching_angle_mean": 45.,
        "lateral_branching_angle_std": 5.,
    }

    dlat_params = lat_params.copy()
    dlat_params.update({
        "uniform_branching_rate": 0.0001,
        "speed_growth_cone": 0.003,
    })


    ng.SetStatus(gids, params=lat_params, dendrites_params=dlat_params)

    ng.Simulate(days=1)
    ng.PlotNeuron(show_nodes=True, show=True)

    #~ pprint(ng.GetStatus(gids_rec))
