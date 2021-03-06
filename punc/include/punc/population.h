// Copyright (C) 2018, Diako Darian and Sigvald Marholm
//
// This file is part of PUNC++.
//
// PUNC++ is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// PUNC++ is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// PUNC++. If not, see <http://www.gnu.org/licenses/>.

/**
 * @file		population.h
 * @brief		The plasma species and population
 *
 * 
 * Contains the functionality to handle the plasma particles, species and population
 */

#ifndef POPULATION_H
#define POPULATION_H

#include "mesh.h"
#include "poisson.h"
#include "distributions.h"

#include <dolfin/mesh/Facet.h>
#include <dolfin/mesh/Vertex.h>
#include <dolfin/mesh/Cell.h>
#include <dolfin/mesh/MeshEntityIterator.h>
#include <dolfin/fem/UFC.h>

#include <fstream>
#include <boost/units/systems/si/codata/electromagnetic_constants.hpp>
#include <boost/units/systems/si/codata/electron_constants.hpp>
#include <boost/units/systems/si/codata/physico-chemical_constants.hpp>
#include <boost/units/systems/si/codata/universal_constants.hpp>

namespace punc
{

namespace df = dolfin;

enum class ParticleAmountType {
    in_total,       ///< Total number of simulation particles
    per_cell,       ///< Simulation particles per cell
    per_volume,     ///< Simulation particles per volume (number density)
    phys_per_sim    ///< Physical particles per simulation particle
};

/**
 * @brief Contains the most important physical constants needed in PIC simulations
 */
struct PhysicalConstants
{
    double e = boost::units::si::constants::codata::e / boost::units::si::coulomb;                                    ///< Elementary charge
    double m_e = boost::units::si::constants::codata::m_e / boost::units::si::kilograms;                              ///< Electron mass
    double ratio = boost::units::si::constants::codata::m_e_over_m_p / boost::units::si::dimensionless();             ///< Electron to proton mass ratio
    double m_i = m_e / ratio;                                                                                         ///< Proton mass
    double k_B = boost::units::si::constants::codata::k_B * boost::units::si::kelvin / boost::units::si::joules;      ///< Boltzmann constant
    double eps0 = boost::units::si::constants::codata::epsilon_0 * boost::units::si::meter / boost::units::si::farad; ///< Electric constant
    double amu = boost::units::si::constants::codata::m_u / boost::units::si::kilograms;                              ///< Atomic mass constant
};

/**
 * @brief A simulation particle
 */
template <std::size_t len = 2>
struct Particle
{
    double x[len];  ///< Position
    double v[len];  ///< Velocity
    double q;       ///< Charge
    double m;       ///< Mass
    Particle(const double *x, const double *v, double q, double m);
    Particle(){};
};

template <std::size_t len>
Particle<len>::Particle(const double *x, const double *v,
                      double q, double m) : q(q), m(m)
{
    for (std::size_t i = 0; i < len; i++)
    {
        this->x[i] = x[i];
        this->v[i] = v[i];
    }
}

/**
 * @brief Complete specification of a species.
 */
class Species 
{
public:
    double q;                  ///< Charge of simulation particle
    double m;                  ///< Mass of simulation particle
    double n;                  ///< Density of simulation particles
    size_t num;                ///< Initial number of simulation particles
    std::shared_ptr<Pdf> pdf;  ///< Position distribution function (initially)
    std::shared_ptr<Pdf> vdf;  ///< Velocity distribution function (initially and at boundary)
    double debye;              ///< The Debye length
    double weight;             ///< Statistical weight (number of physical particles per simulation particle)
    
    /**
     * @brief Constructor
     * @param   charge  Charge of physical particle
     * @param   mass    Mass of physical particle
     * @param   density Density of physical particles
     * @param   amount  Amount of simulation particles
     * @param   type    How the amount of simulation particles are specified
     * @param   mesh    The mesh
     * @param   pdf     Position distribution function
     * @param   vdf     Velocity distribution function
     */
    Species(double charge, double mass, double density, double amount,
            ParticleAmountType type, const Mesh &mesh,
            std::shared_ptr<Pdf> pdf, std::shared_ptr<Pdf> vdf, double eps0);
};

/**
 * @brief Creates plasma species.
 */
class CreateSpecies
{
  public:
    double X; ///< Characteristic length - used for normalization
    int g_dim; ///< The geometrical dimension of the physical space
    double volume, num_cells; ///< The volume of the simulation domain and number of cells in the domain
    std::vector<Species> species; ///< A vector containing species
    double T = std::numeric_limits<double>::quiet_NaN(); ///< Characteristic time
    double Q = boost::units::si::constants::codata::e / boost::units::si::coulomb; ///< Characteristic charge - here set to elementary charge
    double M = std::numeric_limits<double>::quiet_NaN();                           ///< Characteristic mass
    double epsilon_0 = boost::units::si::constants::codata::epsilon_0 * boost::units::si::meter / boost::units::si::farad; ///< Electric constant

    /**
     * @brief Constructor
     * @param       mesh    Mesh  
     * @param       X       Characteristic length 
     */
    CreateSpecies(const Mesh &mesh, double X = 1.0);

    /**
     * @brief Creates species without normalization
     * @param[in]   q - species charge  
     * @param[in]   m - species mass 
     * @param[in]   n - species volumetric number density
     * @param[in]   pdf - position distribution function for the species
     * @param[in]   vdf - velocity distribution function for the species
     * @param[in]   npc - number of particles per cell for the species
     * @param[in]   num - total number of particles in the simulation domain for the species
     */
    void create_raw(double q, double m, double n, Pdf &pdf, Pdf &vdf, int npc = 4,
                    int num = 0);

    /**
     * @brief Creates species and normalizes the physical quantities
     * @param[in]   q - species charge  
     * @param[in]   m - species mass 
     * @param[in]   n - species volumetric number density
     * @param[in]   pdf - position distribution function for the species
     * @param[in]   vdf - velocity distribution function for the species
     * @param[in]   npc - number of particles per cell for the species
     * @param[in]   num - total number of particles in the simulation domain for the species
     */
    void create(double q, double m, double n, Pdf &pdf, Pdf &vdf, int npc = 4,
                int num = 0);
};

/**
 * @brief Generic class representing a cell in the simulation domain
 */
template <std::size_t len>
class Cell : public df::Cell
{
  public:
    std::size_t id; ///< Cell index or id
    std::size_t g_dim; ///< geometric dimension of the domain
    std::vector<std::size_t> neighbors; ///< Neighbors of the Cell - all the cells that share a vertex, facet or edge with the cell
    std::vector<signed long int> facet_adjacents; ///< Adjacent facets to the Cell
    std::vector<double> facet_plane_coeffs;       ///< Coefficients of the plane-equation for the facets of the Cell
    std::vector<Particle<len>> particles;         ///< Particles contained in the Cell
    std::array<double, len*(len+1)> vertex_coordinates; /// Vertex coordinates of the Cell
    ufc::cell ufc_cell;                           ///< The underlying UFC cell

    /**
     * @brief Constructor
     * @param[in]   df::Mesh  
     * @param[in]   Cell index/id
     * @param[in]   neighbors - a vector containing the indices of all the cells in the domain  
     */
    Cell(std::shared_ptr<const df::Mesh> &mesh,
         std::size_t id, std::vector<std::size_t> neighbors)
        : df::Cell(*mesh, id), id(id), 
        neighbors(neighbors)
    {
        auto g_dim = mesh->geometry().dim();
        const std::size_t num_vertices = (*this).num_vertices();
        const unsigned int *vertices = (*this).entities(0);

        for (std::size_t i = 0; i < num_vertices; i++)
        {
            for (std::size_t j = 0; j < g_dim; j++)
            {
                vertex_coordinates[i * g_dim + j] = mesh->geometry().x(vertices[i])[j];
            }
        }

        (*this).get_cell_data(ufc_cell);

        init_barycentric_matrix();
    }

    /**
     * @brief Compute barycentric coordinates wrt. cell
     * @param[in]   x   Cartesian coordinates
     * @param[out]  y   Barycentric coordinates
     */
    inline void barycentric(const double *x, double *y) const;

  private:
    double barycentric_matrix[len*(len+1)]; ///< Matrix for transforming to barycentric coordinates
    void init_barycentric_matrix();         ///< Initialize barycentric_matrix

};

/**
 * @brief A collection of Particles
 */
template <std::size_t len>
class Population
{
  public:
    std::shared_ptr<const df::Mesh> mesh;   ///< df::Mesh of the domain
    const std::size_t g_dim;                ///< Number of geometric dimensions
    std::size_t t_dim;                      ///< Number of topological dimensions
    std::size_t num_cells;                  ///< Number of cells in the domain
    std::vector<Cell<len>> cells;           ///< All df::Cells in the domain

    Population(const Mesh &mesh);
    void init_localizer(const df::MeshFunction<std::size_t> &bnd);
    void save_localizer(const std::string &fname);
    void add_particles(const std::vector<double> &xs,
                       const std::vector<double> &vs,
                       double q, double m);
    void add_particles(const std::vector<Particle<len>> &ps);
    signed long int locate(const double *p);
    signed long int relocate(const double *p, signed long int cell_id);
    signed long int relocate_stat(const double *p, signed long int cell_id, int &crossings);
    signed long int relocate_fast(const double *p, signed long int cell_id);
    void update(ObjectVector objects, double dt);
    void update_stat(ObjectVector objects, double dt, double &mean_crossings);
    std::size_t num_of_particles();         ///< Returns number of particles
    std::size_t num_of_positives();         ///< Returns number of positively charged particles
    std::size_t num_of_negatives();         ///< Returns number of negatively charged particles

    /**
     * @brief Calculates mean speed and standard deviation for each species 
     * @param   stats[in, out]   Array containing the mean speed and standard deviation
     *
     * Uses Welford's algorithm, see ref. "Note on a method for calculating 
     * corrected sums of squares and products." Technometrics 4.3 (1962): 419-420,
     * to calculate the mean speed and standard deviation for each species. The 
     * elements of the array stats are organized as follows:
     * 
     *           stats[0]:  Mean speed for electrons
     *           stats[1]: Standard deviation for electrons
     *           stats[2]: Mean speed for ions
     *           stats[3]: Standard deviation for ions
     */
    void statistics(double *stats);        
    
    /**
     * @brief Save particles to file
     * @param   fname   File name
     * @param   binary  Use binary file format
     * @see load_file
     * 
     * Saves particles to file using either binary or ASCII format.
     *
     * Binary files are typically 40% the size of ASCII files, and do not
     * suffer from the loss of precision associated with displaying numbers in
     * base 10. ASCII files display numbers in base 10, but this precision
     * lost should rarely be significant. Binary files merely stores the
     * Particle structs byte-by-bate, and this makes it depend on the platform.
     * Different platform may have different size for the datatypes in Particle,
     * different padding of structs, and different endianness. Reading a binary
     * file on a system where this differs from where the binary file was made
     * will fail. As such, ASCII files are more portable.
     */
    void save_file(const std::string &fname, bool binary=false);

    /**
     * @brief Load particles from file
     * @param   fname   File name
     * @param   binary  Use binary file format
     * @see save_file
     * 
     * Loads particles from binary or ASCII file.
     */
    void load_file(const std::string &fname, bool binary=false);
};

template <std::size_t len>
Population<len>::Population(const Mesh &mesh_)
    : mesh(mesh_.mesh), g_dim(mesh_.mesh->geometry().dim()),
      t_dim(mesh_.mesh->topology().dim()), num_cells(mesh_.mesh->num_cells())
{

    for (df::MeshEntityIterator e(*(mesh), t_dim); !e.end(); ++e)
    {
        std::vector<std::size_t> neighbors;
        auto cell_id = e->index();
        auto num_vertices = e->num_entities(0);
        for (std::size_t i = 0; i < num_vertices; ++i)
        {
            df::Vertex vertex(*mesh, e->entities(0)[i]);
            auto vertex_cells = vertex.entities(t_dim);
            auto num_adj_cells = vertex.num_entities(t_dim);
            for (std::size_t j = 0; j < num_adj_cells; ++j)
            {
                if (cell_id != vertex_cells[j])
                {
                    neighbors.push_back(vertex_cells[j]);
                }
            }
        }
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());

        Cell<len> cell(mesh, cell_id, neighbors);
        cells.emplace_back(cell);
    }

    init_localizer(mesh_.bnd);
    save_localizer("localizer.dat");
}

template <std::size_t len>
void Population<len>::init_localizer(const df::MeshFunction<std::size_t> &bnd)
{
    for (auto &cell : cells)
    {
        std::vector<signed long int> facet_adjacents;
        std::vector<double> facet_plane_coeffs;

        auto cell_id = cell.id;
        auto facets = cell.entities(t_dim - 1);
        auto num_facets = cell.num_entities(t_dim - 1);

        for (std::size_t i = 0; i < num_facets; ++i)
        {
            df::Facet facet(*mesh, cell.entities(t_dim - 1)[i]);
            auto facet_cells = facet.entities(t_dim);
            auto num_adj_cells = facet.num_entities(t_dim);

            for (std::size_t j = 0; j < num_adj_cells; ++j)
            {
                if (cell_id != facet_cells[j])
                {
                    facet_adjacents.push_back(facet_cells[j]);
                }
            }
            if (num_adj_cells == 1)
            {
                facet_adjacents.push_back(-1 * bnd.values()[facets[i]]);
            }

            double dot_product = 0;
            for(std::size_t j = 0; j < g_dim; j++){
                dot_product += facet.midpoint()[j]*cell.normal(i)[j];
            }
            facet_plane_coeffs.push_back(-dot_product);
            for(std::size_t j = 0; j < g_dim; j++){
                facet_plane_coeffs.push_back(cell.normal(i)[j]);
            }

        }

        cells[cell_id].facet_adjacents = facet_adjacents;
        cells[cell_id].facet_plane_coeffs = facet_plane_coeffs;
    }
}

template <std::size_t len>
void Population<len>::save_localizer(const std::string &fname)
{
    FILE *fout = fopen(fname.c_str(), "w");

    for (auto &cell : cells)
    {
        fprintf(fout, "Cell %d\t", cell.id);
        fprintf(fout, "Vertex coordinates:\t");
        for (auto &a : cell.vertex_coordinates) fprintf(fout, "%g\t", a);
        fprintf(fout, "Neighbors:\t");
        for (auto &a : cell.facet_adjacents) fprintf(fout, "%d\t", a);
        fprintf(fout, "Plane coeffs.:\t");
        for (auto &a : cell.facet_plane_coeffs) fprintf(fout, "%g\t", a);
        fprintf(fout, "\n");
    }

    fclose(fout);
}

template <std::size_t len>
void Population<len>::add_particles(const std::vector<double> &xs,
                                    const std::vector<double> &vs,
                                    double q, double m)
{
    std::size_t num_particles = xs.size() / g_dim;
    double xs_tmp[g_dim];
    double vs_tmp[g_dim];

    std::size_t cell_id;
    for (std::size_t i = 0; i < num_particles; ++i)
    {
        for (std::size_t j = 0; j < g_dim; ++j)
        {
            xs_tmp[j] = xs[i * g_dim + j];
            vs_tmp[j] = vs[i * g_dim + j];
        }
        cell_id = locate(xs_tmp);
        if (cell_id >= 0)
        {
            Particle<len> _particles(xs_tmp, vs_tmp, q, m);
            cells[cell_id].particles.push_back(_particles);
        }
    }
}

template <std::size_t len>
void Population<len>::add_particles(const std::vector<Particle<len>> &ps)
{
    for (auto &p : ps){
        std::size_t cell_id = locate(p.x);
        if(cell_id >=0){
            cells[cell_id].particles.push_back(p);
        }
    }
}

template <std::size_t len>
signed long int Population<len>::locate(const double *p)
{
    return punc::locate(mesh, p);
}

template <std::size_t len>
signed long int Population<len>::relocate(const double *p, signed long int cell_id)
{
    // One element for each facet.
    // For 1D and 2D all aren't used, but slightly faster than vector.
    double proj[4];
    double *coeffs = cells[cell_id].facet_plane_coeffs.data();

    for (std::size_t i = 0; i < g_dim + 1; ++i)
    {
        proj[i] = *coeffs++;
        for (std::size_t j=0; j < g_dim; j++){
            proj[i] += *coeffs++ * p[j];
        }
    }

    double proj_max = proj[0];
    std::size_t proj_argmax = 0;
    for(std::size_t i = 1; i < g_dim + 1; i++){
        if(proj[i] > proj_max){
            proj_max = proj[i];
            proj_argmax = i;
        }
    }

    if(proj_max < 0){
        return cell_id;
    } else {
        auto new_cell_id = cells[cell_id].facet_adjacents[proj_argmax];

        // negative new_cell_id indicate that the particle hit a boundary with
        // id (-new_cell_id).
        if(new_cell_id >= 0){
            return relocate(p, new_cell_id);
        } else {
            return new_cell_id;
        }
    }
}

// This is not meant to be used regularly.
// It is just to find out some statistics for the revised PUNC paper,
// namely the number of cell crossing on average for a simulation.
template <std::size_t len>
signed long int Population<len>::relocate_stat(const double *p, signed long int cell_id, int &crossings)
{
    // One element for each facet.
    // For 1D and 2D all aren't used, but slightly faster than vector.
    double proj[4];
    double *coeffs = cells[cell_id].facet_plane_coeffs.data();

    for (std::size_t i = 0; i < g_dim + 1; ++i)
    {
        proj[i] = *coeffs++;
        for (std::size_t j=0; j < g_dim; j++){
            proj[i] += *coeffs++ * p[j];
        }
    }

    double proj_max = proj[0];
    std::size_t proj_argmax = 0;
    for(std::size_t i = 1; i < g_dim + 1; i++){
        if(proj[i] > proj_max){
            proj_max = proj[i];
            proj_argmax = i;
        }
    }

    if(proj_max < 0){
        return cell_id;
    } else {

        crossings++;

        auto new_cell_id = cells[cell_id].facet_adjacents[proj_argmax];

        // negative new_cell_id indicate that the particle hit a boundary with
        // id (-new_cell_id).
        if(new_cell_id >= 0){
            return relocate_stat(p, new_cell_id, crossings);
        } else {
            return new_cell_id;
        }
    }
}
template <std::size_t len>
signed long int Population<len>::relocate_fast(const double *p, signed long int cell_id)
{
    // One element for each facet.
    // For 1D and 2D all aren't used, but slightly faster than vector.
    double proj;
    double *coeffs = cells[cell_id].facet_plane_coeffs.data();

    for (std::size_t i = 0; i < g_dim + 1; ++i)
    {
        proj = *coeffs++;
        for (std::size_t j=0; j < g_dim; j++){
            proj += *coeffs++ * p[j];
        }

        if(proj > 0){
            auto new_cell_id = cells[cell_id].facet_adjacents[i];

            // negative new_cell_id indicate that the particle hit a boundary with
            // id (-new_cell_id).
            if(new_cell_id >= 0){
                return relocate_fast(p, new_cell_id);
            } else {
                return new_cell_id;
            }
        }
    }

    return cell_id;

}


// Not to be used permanently. Only to gather statistics for PUNC paper.
template <std::size_t len>
void Population<len>::update_stat(ObjectVector objects, double dt, double &mean_crossings)
{

    int crossings = 0;

    for(auto object : objects){
        object->current = 0;
    }

    // FIXME: Consider a different mechanism for boundaries than using negative
    // numbers, or at least circumvent the problem of casting num_cells to
    // signed. Not good practice. size_t may overflow to negative numbers upon
    // truncation for large numbers.
    signed long int new_cell_id;
    for (signed long int cell_id = 0; cell_id < (signed long int)num_cells; ++cell_id)
    {
        std::vector<std::size_t> to_delete;
        std::size_t num_particles = cells[cell_id].particles.size();
        for (std::size_t p_id = 0; p_id < num_particles; ++p_id)
        {
            auto particle = cells[cell_id].particles[p_id];

            //new_cell_id = relocate_fast(particle.x, cell_id);

            // STATISTICS ON CROSSINGS
            new_cell_id = relocate_stat(particle.x, cell_id, crossings);

            if (new_cell_id != cell_id)
            {
                to_delete.push_back(p_id);
                if (new_cell_id >= 0)
                {
                    // Particle will actually be checked again if
                    // new_cell_id>cell_id. Probably not worth avoiding.
                    cells[new_cell_id].particles.push_back(particle);
                }
                else
                {
                    // FIXME:
                    // Standard numbering scheme on objects and exterior
                    // boundary would eliminate this loop.
                    for(auto object : objects){
                        if ((std::size_t)(-new_cell_id) == object->bnd_id)
                        {
                            object->current += particle.q;
                        }
                    }
                }
            }
        }
        std::size_t size_to_delete = to_delete.size();
        for (std::size_t it = size_to_delete; it-- > 0;)
        {
            auto p_id = to_delete[it];
            cells[cell_id].particles[p_id] = cells[cell_id].particles.back();
            cells[cell_id].particles.pop_back();
        }
    }

    for(auto object : objects){
        object->charge += object->current;
        object->current /= dt;
    }

    mean_crossings = (double)crossings / num_of_particles();
}

template <std::size_t len>
void Population<len>::update(ObjectVector objects, double dt)
{

    for(auto object : objects){
        object->current = 0;
    }

    // FIXME: Consider a different mechanism for boundaries than using negative
    // numbers, or at least circumvent the problem of casting num_cells to
    // signed. Not good practice. size_t may overflow to negative numbers upon
    // truncation for large numbers.
    signed long int new_cell_id;
    for (signed long int cell_id = 0; cell_id < (signed long int)num_cells; ++cell_id)
    {
        std::vector<std::size_t> to_delete;
        std::size_t num_particles = cells[cell_id].particles.size();
        for (std::size_t p_id = 0; p_id < num_particles; ++p_id)
        {
            auto particle = cells[cell_id].particles[p_id];
            new_cell_id = relocate_fast(particle.x, cell_id);

            if (new_cell_id != cell_id)
            {
                to_delete.push_back(p_id);
                if (new_cell_id >= 0)
                {
                    // Particle will actually be checked again if
                    // new_cell_id>cell_id. Probably not worth avoiding.
                    cells[new_cell_id].particles.push_back(particle);
                }
                else
                {
                    // FIXME:
                    // Standard numbering scheme on objects and exterior
                    // boundary would eliminate this loop.
                    for(auto object : objects){
                        if ((std::size_t)(-new_cell_id) == object->bnd_id)
                        {
                            object->current += particle.q;
                        }
                    }
                }
            }
        }
        std::size_t size_to_delete = to_delete.size();
        for (std::size_t it = size_to_delete; it-- > 0;)
        {
            auto p_id = to_delete[it];
            cells[cell_id].particles[p_id] = cells[cell_id].particles.back();
            cells[cell_id].particles.pop_back();
        }
    }

    for(auto object : objects){
        object->charge += object->current;
        object->current /= dt;
    }
}

template <std::size_t len>
std::size_t Population<len>::num_of_particles()
{
    std::size_t num_particles = 0;
    for (auto &cell : cells)
    {
        num_particles += cell.particles.size();
    }
    return num_particles;
}

template <std::size_t len>
std::size_t Population<len>::num_of_positives()
{
    std::size_t num_positives = 0;
    for (auto &cell : cells)
    {
        for (auto &particle : cell.particles)
        {
            if (particle.q > 0)
            {
                num_positives++;
            }
        }
    }
    return num_positives;
}

template <std::size_t len>
std::size_t Population<len>::num_of_negatives()
{
    std::size_t num_negatives = 0;
    for (auto &cell : cells)
    {
        for (auto &particle : cell.particles)
        {
            if (particle.q < 0)
            {
                num_negatives++;
            }
        }
    }
    return num_negatives;
}

template <std::size_t len>
void Population<len>::statistics(double *stats)
{
    std::size_t m = 0;
    std::size_t n = 0;
    double v;
    double m_e_old = 0;
    double m_i_old = 0;

    for (auto &cell : cells)
    {
        for (auto &particle : cell.particles)
        {
            v = 0;
            for (std::size_t i = 0; i < g_dim; ++i)
            {
                v += particle.v[i] * particle.v[i];
            }
            v = sqrt(v);

            if (particle.q < 0)
            {
                m++;
                if (m == 1)
                {
                    m_e_old = v; 
                    stats[0] = v;
                    stats[1] = 0.0;
                }
                else
                {
                    stats[0] = m_e_old + (v - m_e_old) / m;
                    stats[1] += (v - m_e_old) * (v - stats[0]);

                    m_e_old = stats[0];
                }
            }
            if (particle.q > 0)
            {
                n++;
                if (n == 1)
                {
                    m_i_old = v; 
                    stats[2] = v;
                    stats[3] = 0.0;
                }
                else
                {
                    stats[2] = m_i_old + (v - m_i_old) / n;
                    stats[3] += (v - m_i_old) * (v - stats[2]);

                    m_i_old = stats[2];
                }
            }
        }
    }

    if (m > 0) stats[1] = sqrt(stats[1] / (m - 1));
    if (n > 0) stats[3] = sqrt(stats[3] / (n - 1));
}

template <std::size_t len>
void Population<len>::save_file(const std::string &fname, bool binary)
{
    if(binary){

        FILE *fout = fopen(fname.c_str(), "wb");

        for (auto &cell : cells)
            for (auto &particle : cell.particles)
                fwrite(&particle, sizeof(particle), 1, fout);

        fclose(fout);

    } else {

        FILE *fout = fopen(fname.c_str(), "w");

        for (auto &cell : cells) {
            for (auto &particle : cell.particles) {

                for (std::size_t i = 0; i < g_dim; ++i)
                    fprintf(fout, "%.17g\t", particle.x[i]);
    
                for (std::size_t i = 0; i < g_dim; ++i)
                    fprintf(fout, "%.17g\t", particle.v[i]);
   
                fprintf(fout, "%.17g\t %.17g\t", particle.q, particle.m);
                fprintf(fout, "\n");
            }
        }
        fclose(fout);
    }
}

template <std::size_t len>
void Population<len>::load_file(const std::string &fname, bool binary)
{
    if(binary){

        FILE *fin = fopen(fname.c_str(), "rb");

        std::vector<Particle<len>> ps;
        Particle<len> p;

        while(fread(&p, sizeof(p), 1, fin))
            ps.push_back(p); 

        fclose(fin);
        add_particles(ps);

    } else {

        std::fstream in(fname);
        std::string line;
        std::vector<double> x(g_dim);
        std::vector<double> v(g_dim);
        double q = 0;
        double m = 0;
        std::size_t i;
        while (std::getline(in, line))
        {
            double value;
            std::stringstream ss(line);
            i = 0;
            while (ss >> value)
            {
                if (i < g_dim) x[i] = value;
                else if (i >= g_dim && i < 2 * g_dim) v[i % g_dim] = value;
                else if (i == 2 * g_dim) q = value;
                else if (i == 2 * g_dim + 1) m = value;
                ++i;
            }
            add_particles(x, v, q, m);
        }
    }
}


template <>
inline void Cell<3>::barycentric(const double *x, double *y) const {
    auto A = barycentric_matrix;
    y[0] = A[0]  + A[1] *x[0] + A[2] *x[1] + A[3] *x[2];
    y[1] = A[4]  + A[5] *x[0] + A[6] *x[1] + A[7] *x[2];
    y[2] = A[8]  + A[9] *x[0] + A[10]*x[1] + A[11]*x[2];
    // y[3] = A[12] + A[13]*x[0] + A[14]*x[1] + A[15]*x[2];
    y[3] = 1 - y[0] - y[1] - y[2];
}

template <>
inline void Cell<2>::barycentric(const double *x, double *y) const {
    auto A = barycentric_matrix;
    y[0] = A[0]  + A[1] *x[0] + A[2] *x[1];
    y[1] = A[3]  + A[4] *x[0] + A[5] *x[1];
    // y[2] = A[6]  + A[7] *x[0] + A[8]*x[1];
    y[2] = 1 - y[0] - y[1];
}

template <>
inline void Cell<1>::barycentric(const double *x, double *y) const {
    auto A = barycentric_matrix;
    y[0] = A[0]  + A[1] *x[0];
    // y[1] = A[2]  + A[3] *x[0];
    y[1] = 1 - y[0];
}

/**
 * @brief Returns the minimum plasma period of all species
 * @param   species     All species
 * @param   eps0        The permittivity of vacuum
 * @return              The minmium plasma period
 *
 * The plasma period of a species s is defined as
 * \f[
 *  T_{ps} = \frac{2\pi}{\omega_{ps}} = 2\pi\frac{\varepsilon_0 m_s}{q_s^2 n_s}
 * \f]
 */
double min_plasma_period(const std::vector<Species> &species, double eps0);

/**
 * @brief Returns the minimum gyroperiod of all species
 * @param   species     All species
 * @param   B           The magnetic flux density vector
 * @return              The minimum gyro period
 *
 * The gyro period of a species s is defined as
 * \f[
 *  T_{gs} = \frac{2\pi}{\omega_{gs}} = 2\pi\frac{m_s}{q_s B}
 * \f]
 */
double min_gyro_period(const std::vector<Species> &species,
                       const std::vector<double> &B);

/**
 * @brief Returns the maximum expected speed of any particle in the system
 * @param   species     All species
 * @param   k           Number of st. devs of velocity to account for
 * @param   phi_min     The minimum expected potential in the domain
 * @param   phi_max     The maximum expected potential in the domain
 * @return              The maximum expected speed
 *
 * If neglecting particles in the tail of the velocity distributions above
 * k standard deviations, the fastest particles entering the domain have a 
 * speed of
 *
 * \f[
 *  v_{0,s} = v_{D,s} + k*v_{th,s}
 * \f]
 *
 * because the thermal velocity aligns with the drift velocity every now and
 * then. In addition negative/positive charges may be accelerated towards 
 * points of high/low potentials, typically an object with specified voltage.  
 * This speed is computed from energy conservation:
 *
 * \f[
 *  v_{1,s}^2 = v_{0,s} + \frac{2q_s \Delta\phi}{m_s}
 * \f]
 */
double max_speed(const std::vector<Species> &species,
                 double v_range, double phi_min, double phi_max);

} // namespace punc

#endif // POPULATION_H
