.. _spatial-interactions:

========================
Spatial interactions
========================

The spatial navigation of growth cones is powered by a geometric engine library, `GEOS
<http://https://trac.osgeo.org/geos>`_, the spatial library is compatible with multithreading library `OpenMP`.
The spatial manager supervises growth cones' elongation and wall interactions.
The spatial implementation is detailed in :ref:`geometry`.
Extension to `MPI` and neurite-neurite interactions are work in progress.




Sensing walls
=============

All models use filopodia to check for the presence of obstacles (walls) on the
path they go.
The obstacle are sensed at each step, through the `growth::GrowthCone` interface.
At present time only one environment can be set and it represents the physical space.
Later chemical space and electromagnetic can be represented with more environment object.

The space is managed with Boost:geometry, read more here `geometry`_.


`growth::GrowthCone` implements:

`growth::GrowthCone::accessible_environment`
verifies the point is contained in the accessible space.
Now it's improved: check the absence of intersection to avoid tunneling.

`growth::GrowthCone::sense`

The prepared geometry feature from GEOS is applied on the `culture` environment
to make the intersection tests efficient.


Sensing other neurites
======================

Grid-based idea
---------------

Grid the culture with a given resolution.
The :cpp:class:`growth::SpaceManager` will store this grid, and each cell will
contain an `unordered_map` containing the neuron id as key and the number of
branches going through the cell as value.
Every time a :cpp:class:`growth::GrowthCone` enters the cell, it adds one to
the value associated to its .


.. Links
