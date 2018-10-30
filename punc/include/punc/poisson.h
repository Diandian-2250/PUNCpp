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
 * @file		poisson.h
 * @brief		Solvers for Poisson's equation
 */

#ifndef POISSON_H
#define POISSON_H

#include "object.h"
#include <dolfin.h>
#include <boost/optional.hpp>

namespace punc
{

namespace df = dolfin;


/**
 * @brief Boundary condition for the electric potential
 */
class PhiBoundary : public df::Expression
{
public:
    const std::vector<double> &B; ///< A uniform magnetic field
    const std::vector<double> &vd;///< Drift velocity
    std::vector<double> E;        ///< Electric field

    /**
    * @brief Constructor
    * @param B[in] -  the external uniform magnetic field
    * @param vd[in] - the drift velocity
    */
    PhiBoundary(const std::vector<double> &B, const std::vector<double> &vd);

private:
    void eval(df::Array<double> &values, const df::Array<double> &x) const;
};

/**
 * @brief Boundary condition for non-periodic problems
 */
class NonPeriodicBoundary : public df::SubDomain
{
public:
    const std::vector<double> &Ld;
    const std::vector<bool> &periodic;
    NonPeriodicBoundary(const std::vector<double> &Ld,
                        const std::vector<bool> &periodic);

private:
    bool inside(const df::Array<double> &x, bool on_boundary) const;
};

/**
 * @brief Boundary condition for periodic problems
 */
class PeriodicBoundary : public df::SubDomain
{
public:
    const std::vector<double> &Ld;
    const std::vector<bool> &periodic;
    PeriodicBoundary(const std::vector<double> &Ld,
                     const std::vector<bool> &periodic);

    bool inside(const df::Array<double> &x, bool on_boundary) const;

    void map(const df::Array<double> &x, df::Array<double> &y) const;
};

/**
 * @brief Creates a function space in CG1
 * @param mesh[in] - The Mesh 
 * @param constr[in] Constraint to be imposed for periodic problems
 * @return CG1 function space
 * 
 * @see CG1_vector_space, DG0_space
 */
df::FunctionSpace CG1_space(const Mesh &mesh,
                            boost::optional<std::shared_ptr<PeriodicBoundary>> constr = boost::none);

/**
 * @brief Creates a vector function space in CG1
 * @param mesh[in] - The Mesh 
 * @return CG1 vector function space
 * 
 * @see CG1_space, DG0_space
 */
df::FunctionSpace CG1_vector_space(const Mesh &mesh);

/**
 * @brief Creates a function space in DG0
 * @param mesh[in] - The Mesh 
 * @return DG0 function space
 * 
 * @see CG1_space, CG1_vector_space
 */
df::FunctionSpace DG0_space(const Mesh &mesh);

/**
 * @brief Creates a vector function space in DG0
 * @param mesh[in] - The Mesh 
 * @return DG0 vector function space
 * 
 * @see DG0_space, CG1_space, CG1_vector_space
 */
df::FunctionSpace DG0_vector_space(const Mesh &mesh);

/**
 * @brief Solver for Poisson's equation
 */
class PoissonSolver {
private:
    boost::optional<std::vector<df::DirichletBC> &> ext_bc; /// < Exterior boundaries
    bool remove_null_space;                                 /// < Whether or not to remove null space
    std::shared_ptr<df::PETScKrylovSolver> solver;          /// < Linear algebra solver
    std::shared_ptr<df::Form> a;                            /// < Bilinear form
    std::shared_ptr<df::Form> L;                            /// < Linear form
    df::PETScMatrix A;                                      /// < Stiffness matrix
    df::PETScVector b;                                      /// < Load vector
    std::shared_ptr<df::VectorSpaceBasis> null_space;
    std::size_t num_bcs = 0;                                ///< Number of boundaries

public:
    /**
     * @brief Constructor 
     * @param V                 The function space of rho and phi
     * @param ext_bc            Exterior Dirichlet boundary conditions
     * @param circuit           Circuits between objects
     * @param eps0              Vacuum permittivity in simulation parameters
     *                          (depends on normalization scheme)
     * @param method            Method of linear algebra solver
     * @param preconditioner    Preconditioner for matrix equation
     */
    PoissonSolver(const df::FunctionSpace &V, 
                  boost::optional<std::vector<df::DirichletBC>& > ext_bc = boost::none,
                  std::shared_ptr<Circuit> circuit = nullptr,
                  double eps0 = 1,
                  bool remove_null_space = false,
                  std::string method = "",
                  std::string preconditioner = "");

    /**
     * @brief Solves Poisson's equation
     * @param[in,out]   phi          The electric potential
     * @param           rho          Total charge density
     * @param           objects      A vector of objects
     * @param           circuit      The circuitry
     * @see solve_circuit
     *
     * Any objects and circuits are treated as boundary conditions which are
     * applied to the matrix equation upon solving by using their apply-methods.
     * Other pre- and post- computations may be necessary to correctly
     * incorporate circuits.
     */
    void solve(df::Function &phi, const df::Function &rho,
               ObjectVector &objects,
               std::shared_ptr<Circuit> circuit = nullptr);

    /**
     * @brief Solves Poisson's equation and associated circuit equations.
     * @param[in,out]   phi          The electric potential
     * @param           rho          Total charge density
     * @param           objects      A vector of objects
     * @param           circuit      The circuitry
     * @see solve, Circuit::post_solve, Circuit::pre_solve
     * 
     * This is a wrapper that performs all steps necessary to obtain the
     * electric potential given charge density and circuitry.
     */
    void solve_circuit(df::Function &phi, const df::Function &rho,
                      Mesh &mesh,
                      ObjectVector &objects,
                      std::shared_ptr<Circuit> circuit = nullptr);

    /**
     * @brief Calculates the residual of the Poisson solution
     * @param    phi     - The solution of Poisson's equation
     * @return   The residual
     * 
     * The residual is given by 
     * \f[
     * \mathbf{r} =\lVert A\mathbf{\phi} - \mathbf{b} \rVert_{L_2}. 
     * /f]
     *
     */
    double residual(const df::Function &phi);

};

/**
     * @brief Calculates the L2 error-norm 
     * @param    phi           Numerical solution
     * @param    phi_e         The exact solution
     * @return   L2 error-norm
     * 
     * The error is measured by using \f[L_2\f] norm, which is defined by 
     * \f[
     * \lVert\mathbf{e}\rVert_{L_2} =\left(\int_{\Omega}\mathbf{e}\cdot\mathbf{e} \,\dd\mathbf{x}\right)^{\frac{1}{2}}. 
    * /f]
    *
    */
double errornorm(const df::Function &phi, const df::Function &phi_e);

} // namespace punc

#endif // POISSON_H
