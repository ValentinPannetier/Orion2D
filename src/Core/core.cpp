#include "core.hpp"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>

Point &
Mesh::InsertPoint (Point &point)
{
    points.push_back ({point [0], point [1]});
    return points.back ();
}

Point &
Mesh::InsertPoint (real_t x, real_t y)
{
    points.push_back ({x, y});
    return points.back ();
}

void
Mesh::InsertTriangle (idx_t i, idx_t j, idx_t k)
{
    Triangle nc;
    nc [0] = std::min (std::min (i, j), k);
    nc [2] = std::max (std::max (i, j), k);
    nc [1] = i + j + k - nc [0] - nc [2];

    triangles.push_back (nc);

    Point circumcenter = {0, 0};
    Point masscenter   = {0, 0};

    areas.push_back (0);
    circumcenters.push_back (circumcenter);
    masscenters.push_back (masscenter);
    radius.push_back (0);
    qualities.push_back (0);

    return UpdateTriangle (triangles.size () - 1);
}

void
Mesh::UpdateTriangle (ul_t id)
{
    Triangle &tri = triangles [id];

    Point &pA = points [static_cast<ul_t> (tri [0])];
    Point &pB = points [static_cast<ul_t> (tri [1])];
    Point &pC = points [static_cast<ul_t> (tri [2])];

    Point u_AB = pB - pA;
    Point u_AC = pC - pA;
    Point u_BC = pC - pB;

    real_t dAB = Norm (u_AB);
    real_t dAC = Norm (u_AC);
    real_t dBC = Norm (u_BC);

    real_t sum = dAB * dAB + dAC * dAC + dBC * dBC;

    real_t det = u_AB [0] * u_AC [1] - u_AB [1] * u_AC [0];

    /////////////////////////////////////////////////////////////////////////////////////////////
    //
    // Aire du triangle ABC = 0.5 * ||AB^AC|| = 0.5 * ||[0, 0, z_ABC]||
    // Composante z de AB^AC = u^v
    // Aire du triangle ABC (maille K)
    //
    /////////////////////////////////////////////////////////////////////////////////////////////
    areas [id] = 0.5 * std::fabs (det);

    /////////////////////////////////////////////////////////////////////////////////////////////
    //
    // Circumcenter
    //
    /////////////////////////////////////////////////////////////////////////////////////////////
    real_t f = u_AB [0] * ((pA [0] + pB [0]) / 2.) + u_AB [1] * ((pA [1] + pB [1]) / 2.);
    real_t g = u_AC [0] * ((pA [0] + pC [0]) / 2.) + u_AC [1] * ((pA [1] + pC [1]) / 2.);

    circumcenters [id][0] = (u_AC [1] * f - u_AB [1] * g) / det;
    circumcenters [id][1] = (u_AB [0] * g - u_AC [0] * f) / det;

    /////////////////////////////////////////////////////////////////////////////////////////////
    //
    // Radius
    //
    /////////////////////////////////////////////////////////////////////////////////////////////
    radius [id] = Norm (pA - circumcenters [id]);

    /////////////////////////////////////////////////////////////////////////////////////////////
    //
    // Quality
    //
    /////////////////////////////////////////////////////////////////////////////////////////////
    qualities [id] = (MATH_SQRT3 / (12.0 * areas [id])) * sum;

    /////////////////////////////////////////////////////////////////////////////////////////////
    //
    // Mass center
    //
    /////////////////////////////////////////////////////////////////////////////////////////////
    //    masscenters [id] = ...

    return;
}

void
Mesh::InsertEdgeByTriangle (idx_t i, idx_t j)
{
    Edge ne;
    ne [0] = std::min (i, j);
    ne [1] = i + j - ne [0];

    edgesbytriangles.push_back (ne);

    return;
}

void
Mesh::InsertEdgeByTriangle (Edge e)
{
    Edge ne;
    ne [0] = std::min (e [0], e [1]);
    ne [1] = e [0] + e [1] - ne [0];

    edgesbytriangles.push_back (ne);

    return;
}

void
Mesh::InsertEdgeByPoint (idx_t i, idx_t j)
{
    Edge ne;
    ne [0] = std::min (i, j);
    ne [1] = i + j - ne [0];

    edgesbypoints.push_back (ne);

    return;
}

void
Mesh::InsertEdgeByPoint (Edge e)
{
    Edge ne;
    ne [0] = std::min (e [0], e [1]);
    ne [1] = e [0] + e [1] - ne [0];

    edgesbypoints.push_back (ne);

    return;
}

void
PurgeInvalids (Mesh *mesh)
{
    ul_t numTriangles = mesh->triangles.size ();

    ul_t cursor = 0;
    for (ul_t id = 0; id < numTriangles; ++id)
        if (IsValid (mesh->triangles [id]))
        {
            mesh->triangles [cursor]     = mesh->triangles [id];
            mesh->areas [cursor]         = mesh->areas [id];
            mesh->circumcenters [cursor] = mesh->circumcenters [id];
            mesh->masscenters [cursor]   = mesh->masscenters [id];
            mesh->radius [cursor]        = mesh->radius [id];
            mesh->qualities [cursor]     = mesh->qualities [id];

            cursor++;
        }

    mesh->triangles.resize (cursor);
    mesh->areas.resize (cursor);
    mesh->circumcenters.resize (cursor);
    mesh->masscenters.resize (cursor);
    mesh->radius.resize (cursor);
    mesh->qualities.resize (cursor);

    return;
}

void
PrintStatistics (Mesh *mesh, const char *name)
{
    BEGIN << "Mesh stats " << name << ENDLINE;
    INFOS << " Number of points                     : " << mesh->points.size () << ENDLINE;
    INFOS << " Number of edges [point-connections]  : " << mesh->edgesbypoints.size () << ENDLINE;
    INFOS << " Number of edges [cell-connections]   : " << mesh->edgesbytriangles.size () << ENDLINE;
    INFOS << " Number of triangles                  : " << mesh->triangles.size () << ENDLINE;

    ENDFUN;
    return;
}

void
BuildEdges (Mesh *mesh)
{
    BEGIN << "Build edges " << ENDLINE;

    /////////////////////////////////////////////////////////////////////////////////////////////
    // Definitions
    /////////////////////////////////////////////////////////////////////////////////////////////

    // The object in the hash map
    struct hash_t
    {
        idx_t data = -1;
    };

    // the type of the key
    using key_t = ul_t;
    // the hash map
    using map_t = std::map<key_t, hash_t>;

    ul_t numPoints    = mesh->points.size ();
    ul_t numTriangles = mesh->triangles.size ();

    // b = numPoints / dim
    // sum_{k=0} ^{nbrPoint_cell} b^k * cell[k]
    // i + numPoints * j

    auto hashFun = [numPoints] (idx_t i, idx_t j) -> key_t {
        return static_cast<key_t> (i) + static_cast<key_t> (numPoints) * static_cast<key_t> (j);
    };

    // list identities
    const std::vector<std::array<ul_t, 2>> listIdx = {{0, 1}, {1, 2}, {0, 2}};

    map_t theMap;

    /////////////////////////////////////////////////////////////////////////////////////////////
    // Loop to build edges
    /////////////////////////////////////////////////////////////////////////////////////////////
    for (ul_t idTri = 0; idTri < numTriangles; ++idTri)
    {
        Triangle &tri = mesh->triangles [idTri];

        if (!IsValid (tri))
            continue;

        /////////////////////////////////////////////////////////////////////////////////////////////
        // [0, 1] [1, 2] [0, 2]
        /////////////////////////////////////////////////////////////////////////////////////////////
        // theMap = {key_1 : value_1, key_2 : value_2, ....}
        /////////////////////////////////////////////////////////////////////////////////////////////
        for (std::array<ul_t, 2> couple : listIdx)
        {
            hash_t &obj = theMap [hashFun (tri [couple [0]], tri [couple [1]])];

            if (obj.data == -1)
                obj.data = static_cast<idx_t> (idTri);
            else
                mesh->InsertEdgeByTriangle (obj.data, static_cast<idx_t> (idTri));
        }
    }

    INFOS << "Hash map size : " << theMap.size () << ENDLINE;
    INFOS << "Edges with triangle id : " << mesh->edgesbytriangles.size () << ENDLINE;

    //    /////////////////////////////////////////////////////////////////////////////////////////////
    //    // Put the new edges by triangles
    //    /////////////////////////////////////////////////////////////////////////////////////////////
    //    mesh->edgesbytriangles.clear ();
    //    mesh->edgesbytriangles.reserve (theMap.size ());

    //    for (std::pair<key_t, hash_t> obj : theMap)
    //        mesh->InsertEdgeByTriangle (obj.second.data);

    /////////////////////////////////////////////////////////////////////////////////////////////
    // Put the new edges by points
    /////////////////////////////////////////////////////////////////////////////////////////////
    mesh->edgesbypoints.clear ();
    mesh->edgesbypoints.reserve (theMap.size ());

    for (ul_t idTri = 0; idTri < numTriangles; ++idTri)
    {
        Triangle &tri = mesh->triangles [idTri];

        if (!IsValid (tri))
            continue;

        mesh->InsertEdgeByPoint (tri [0], tri [1]);
        mesh->InsertEdgeByPoint (tri [1], tri [2]);
        mesh->InsertEdgeByPoint (tri [0], tri [2]);
    }

    std::sort (std::begin (mesh->edgesbypoints), std::end (mesh->edgesbypoints));

    mesh->edgesbypoints.erase (std::unique (std::begin (mesh->edgesbypoints), std::end (mesh->edgesbypoints)),
                               std::end (mesh->edgesbypoints));

    INFOS << "Edges with point id : " << mesh->edgesbypoints.size () << ENDLINE;

    STATUS << "Done !" << ENDLINE;
    ENDFUN;

    return;
}
