#include <dolfin.h>
#include "../../punc/include/punc.h"

using namespace punc;

int main()
{
    df::set_log_level(df::WARNING);

    std::string fname{"/home/diako/Documents/cpp/punc/mesh/3D/laframboise_sphere_in_cube_res1"};
    auto mesh = load_mesh(fname);
    auto dim = mesh->geometry().dim();
    auto tdim = mesh->topology().dim();

    auto boundaries = load_boundaries(mesh, fname);
    auto tags = get_mesh_ids(boundaries);
    std::size_t ext_bnd_id = tags[1];

    auto facet_vec = exterior_boundaries(boundaries, ext_bnd_id);

    std::vector<double> Ld = get_mesh_size(mesh);
    std::vector<double> vd(dim);
    for (std::size_t i = 0; i<dim; ++i)
    {
        vd[i] = 0.0;
    }

    PhysicalConstants constants;
    double e = constants.e;
    double me = constants.m_e;
    double mi = constants.m_i;
    double kB = constants.k_B;
    double eps0 = constants.eps0;

    int npc = 4;
    double ne = 1.0e10;
    double debye = 1.0;
    double Rp = 1.0*debye;
    double X = Rp;
    double Te = e*e*debye*debye*ne/(eps0*kB);
    double wpe = sqrt(ne*e*e/(eps0*me));
    double vthe = debye*wpe;
    double vthi = vthe/sqrt(1836.);

    double Vlam  = kB*Te/e;
    double Ilam  = -e*ne*Rp*Rp*sqrt(8.*M_PI*kB*Te/me);
    double Iexp  = 1.987*Ilam;

    double dt = 0.05;
    std::size_t steps = 3000;

    CreateSpecies create_species(mesh, facet_vec, X);

    auto pdf = [](std::vector<double> t)->double{return 1.0;};
    create_species.create(-e, me, ne, npc, vthe, vd, pdf, 1.0);
    create_species.create(e, mi, ne, npc, vthi, vd, pdf, 1.0);

    auto species = create_species.species;

    double Inorm  = create_species.Q/create_species.T;
    double Vnorm  = (create_species.M/create_species.Q)*(create_species.X/create_species.T)*(create_species.X/create_species.T);
    Inorm /= fabs(Ilam);
    Vnorm /= Vlam;

    double cap_factor = 1.;
    double current_collected = Iexp;
    double imposed_potential = 1.0/Vnorm;
    eps0 = 1.0;

    printf("Q:  %e\n", create_species.Q);
    printf("T:  %e\n", create_species.T);

    printf("Inorm:  %e\n", Inorm);
    printf("Vnorm:  %e\n", Vnorm);

    printf("Laframboise voltage:  %e\n", Vlam);
    printf("Laframboise current:  %e\n", Ilam);
    printf("Expected current:     %e\n", Iexp);
    printf("Imposed potential:    %e\n", imposed_potential);

    auto V = function_space(mesh);
    auto dv_inv = voronoi_volume_approx(V);

    auto u0 = std::make_shared<df::Constant>(0.0);
    df::DirichletBC bc(std::make_shared<df::FunctionSpace>(V), u0, 
		std::make_shared<df::MeshFunction<std::size_t>>(boundaries), ext_bnd_id);
    std::vector<df::DirichletBC> ext_bc = {bc};

    PoissonSolver poisson(V, ext_bc);
    ESolver esolver(V);

	Object object(V, boundaries, tags[2]);
	object.set_potential(0.0);
	std::vector<Object> int_bc = {object};

	boost_matrix inv_capacity = capacitance_matrix(V, int_bc, boundaries, ext_bnd_id);

    reset_objects(int_bc);

    Population pop(mesh, boundaries);

    load_particles(pop, species);

    std::vector<double> KE(steps);
    std::vector<double> PE(steps);
    std::vector<double> TE(steps);
    std::vector<double> num_e(steps);
    std::vector<double> num_i(steps);
    std::vector<double> num_tot(steps);
    // std::vector<double> num_particles_outside(steps - 1);
    // std::vector<double> num_injected_particles(steps - 1);
    std::vector<double> potential(steps, 0.0);
    std::vector<double> current_measured(steps, 0.0);
    std::vector<double> obj_charge(steps, 0.0);

    double old_charge = 0.0;
    Timer timer;
    std::vector<double> dist(steps),rsetobj(steps),pois(steps),efil(steps),upd(steps);
    std::vector<double> objpoten(steps),pot(steps),ace(steps),mv(steps), inj(steps), pnum(steps);
    boost::optional<double> opt = NAN;
    num_i[0] = pop.num_of_positives();
    num_e[0] = pop.num_of_negatives();
    auto num3 = pop.num_of_particles();
    std::cout << "Num positives:  " << num_i[0];
    std::cout << ", num negatives: " << num_e[0];
    std::cout << " total: " << num3 << '\n';

    std::ofstream file;
    file.open("history.dat", std::ofstream::out | std::ofstream::app);
    for(int i=0; i<steps;++i)
    {
        std::cout<<"step: "<< i<<'\n';
        auto rho = distribute(V, pop, dv_inv); 
        dist[i] = timer.elapsed();
        timer.reset();
        reset_objects(int_bc);  
        rsetobj[i]= timer.elapsed();
        timer.reset();
        auto phi = poisson.solve(rho, int_bc); 
        pois[i] = timer.elapsed();
        timer.reset();

        auto E = esolver.solve(phi); 
        efil[i] = timer.elapsed();
        timer.reset();
        compute_object_potentials(int_bc, E, inv_capacity, mesh); 
        objpoten[i] = timer.elapsed();
        timer.reset();

        potential[i] = int_bc[0].potential*Vnorm; 

        auto phi1 = poisson.solve(rho, int_bc); 
        pois[i] += timer.elapsed();
        timer.reset();

        auto E1 = esolver.solve(phi1); 
        efil[i] += timer.elapsed();
        timer.reset();

        PE[i] = particle_potential_energy(pop, phi1); 
        pot[i] = timer.elapsed();
        timer.reset();

        old_charge = int_bc[0].charge;               

        KE[i] = accel(pop, E1, (1.0-0.5*(i == 1))*dt); 
        ace[i] = timer.elapsed();
        if(i==0)
        {
            KE[i] = kinetic_energy(pop);
        }
        timer.reset();
        move(pop, dt);          
        mv[i] = timer.elapsed();
        timer.reset();

        pop.update(int_bc);      
        upd[i]= timer.elapsed();

        file<<i<<"\t"<<num_e[i]<<"\t"<<num_i[i]<<"\t"<<KE[i]<<"\t"<<PE[i]<<"\t"<<potential[i]<<"\t"<<current_measured[i]<<'\n';

        current_measured[i] = ((int_bc[0].charge-old_charge)/dt)*Inorm;
        int_bc[0].charge -= current_collected*dt;                       
        obj_charge[i] = int_bc[0].charge;
        timer.reset();

        inject_particles(pop, species, facet_vec, dt);         
        inj[i] = timer.elapsed();

        timer.reset();
        num_e[i] = pop.num_of_negatives();
        num_i[i] = pop.num_of_positives();
        num_tot[i] = pop.num_of_particles();
        pnum[i] = timer.elapsed();
        timer.reset();
    }

    file.close();

    auto time_dist = std::accumulate(dist.begin(), dist.end(), 0.0);
    auto time_rsetobj = std::accumulate(rsetobj.begin(), rsetobj.end(), 0.0);
    auto time_pois = std::accumulate(pois.begin(), pois.end(), 0.0);
    auto time_efil = std::accumulate(efil.begin(), efil.end(), 0.0);
    auto time_upd = std::accumulate(upd.begin(), upd.end(), 0.0);
    auto time_objpoten = std::accumulate(objpoten.begin(), objpoten.end(), 0.0);
    auto time_pot = std::accumulate(pot.begin(), pot.end(), 0.0);
    auto time_ace = std::accumulate(ace.begin(), ace.end(), 0.0);
    auto time_mv = std::accumulate(mv.begin(), mv.end(), 0.0);
    auto time_inj = std::accumulate(inj.begin(), inj.end(), 0.0);
    auto time_pnum = std::accumulate(pnum.begin(), pnum.end(), 0.0);

    std::cout<<"-----Measured time for each task----------"<<'\n';
    std::cout<<"Dist:             "<<  time_dist<<'\n';
    std::cout<<"reset objects:    "<< time_rsetobj <<'\n';
    std::cout<<"pois:             "<<  time_pois<<'\n';
    std::cout<<"efield:           "<< time_efil <<'\n';
    std::cout<<"update:           "<< time_upd <<'\n';
    std::cout<<"move:             "<< time_mv <<'\n';
    std::cout<<"inject:           "<< time_inj <<'\n';
    std::cout<<"accel:            "<< time_ace <<'\n';
    std::cout<<"potential         "<< time_pot <<'\n';
    std::cout<<"object potential: "<< time_objpoten <<'\n';
    std::cout<<"particles:        "<< time_pnum <<'\n';

    for(int i=0;i<KE.size(); ++i)
    {
        TE[i] = PE[i] + KE[i];
    }
    std::ofstream out;
    out.open("PE.txt");
    for (const auto &e : PE) out << e << "\n";
    out.close();
    std::ofstream out1;
    out1.open("KE.txt");
    for (const auto &e : KE) out1 << e << "\n";
    out1.close();
    std::ofstream out2;
    out2.open("TE.txt");
    for (const auto &e : TE) out2 << e << "\n";
    out2.close();
    out.open("num_e.txt");
    for (const auto &e : num_e) out << e << "\n";
    out.close();
    out.open("num_i.txt");
    for (const auto &e : num_i)
        out << e << "\n";
    out.close();
    out.open("num_tot.txt");
    for (const auto &e : num_tot)
        out << e << "\n";
    out.close();
    out.open("potential.txt");
    for (const auto &e : potential) out << e << "\n";
    out.close();
    out.open("current.txt");
    for (const auto &e : current_measured)
        out << e << "\n";
    out.close();
    out.open("charge.txt");
    for (const auto &e : obj_charge)
        out << e << "\n";
    out.close();

    return 0;
}
