#include "population.h"

namespace punc
{

SpeciesList::SpeciesList(std::shared_ptr<const df::Mesh> &mesh,
                         std::vector<Facet> &facets, double X) : facets(facets), X(X)
{
    this->D = mesh->geometry().dim();
    auto one = std::make_shared<df::Constant>(1);
    this->volume = punc::volume(mesh);
    this->num_cells = mesh->num_cells();
}

void SpeciesList::append_raw(double q, double m, double n, int npc, double vth,
                             std::vector<double> &vd,
                             std::function<double(std::vector<double> &)> pdf,
                             double pdf_max)
{
    double num = npc * this->num_cells;
    double w = (n / num) * this->volume;
    q *= w;
    m *= w;
    n /= w;

    Species s(q, m, n, num, vth, vd, pdf, pdf_max, facets);
    species.emplace_back(s);
}

void SpeciesList::append(double q, double m, double n, int npc, double vth,
                         std::vector<double> &vd,
                         std::function<double(std::vector<double> &)> pdf,
                         double pdf_max)
{
    if (std::isnan(this->T))
    {
        double wp = sqrt((n * q * q) / (epsilon_0 * m));
        this->T = 1.0 / wp;
    }
    if (std::isnan(this->M))
    {
        this->M = (this->T * this->T * this->Q * this->Q) /
                  (this->epsilon_0 * pow(this->X, this->D));
    }
    q /= this->Q;
    m /= this->M;
    n *= pow(this->X, this->D);
    if (vth == 0.0)
    {
        vth = std::numeric_limits<double>::epsilon();
    }
    vth /= (this->X / this->T);
    for (int i = 0; i < this->D; ++i)
    {
        vd[i] /= (this->X / this->T);
    }
    this->append_raw(q, m, n, npc, vth, vd, pdf, pdf_max);
}

Population::Population(std::shared_ptr<const df::Mesh> &mesh,
                       std::shared_ptr<df::MeshFunction<std::size_t>> &bnd)
{
    this->mesh = mesh;
    num_cells = mesh->num_cells();
    std::vector<Cell> cells(num_cells);
    this->cells = cells;
    this->tdim = mesh->topology().dim();
    this->gdim = mesh->geometry().dim();
    
    this->mesh->init(0, this->tdim);
    for (df::MeshEntityIterator e(*(this->mesh), this->tdim); !e.end(); ++e)
    {
        std::vector<std::size_t> neighbors;
        auto cell_id = e->index();
        auto vertices = e->entities(0);
        auto num_vertices = e->num_entities(0);
        for (std::size_t i = 0; i < num_vertices; ++i)
        {
            df::Vertex vertex(*mesh, e->entities(0)[i]);
            auto vertex_cells = vertex.entities(tdim);
            auto num_adj_cells = vertex.num_entities(tdim);
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

        Cell cell(cell_id, neighbors);
        this->cells[cell_id] = cell;
    }

    this->init_localizer(bnd);
}

void Population::init_localizer(std::shared_ptr<df::MeshFunction<std::size_t>> &bnd)
{
    mesh->init(tdim - 1, tdim);
    for (df::MeshEntityIterator e(*mesh, tdim); !e.end(); ++e)
    {
        std::vector<signed long int> facet_adjacents;
        std::vector<double> facet_normals;
        std::vector<double> facet_mids;

        auto cell_id = e->index();
        df::Cell single_cell(*mesh, cell_id);
        auto facets = e->entities(tdim - 1);
        auto num_facets = e->num_entities(tdim - 1);
        for (std::size_t i = 0; i < num_facets; ++i)
        {
            df::Facet facet(*mesh, e->entities(tdim - 1)[i]);
            auto facet_cells = facet.entities(tdim);
            auto num_adj_cells = facet.num_entities(tdim);
            for (std::size_t j = 0; j < num_adj_cells; ++j)
            {
                if (cell_id != facet_cells[j])
                {
                    facet_adjacents.push_back(facet_cells[j]);
                }
            }
            if (num_adj_cells == 1)
            {
                facet_adjacents.push_back(-1 * bnd->values()[facets[i]]);
            }

            for (std::size_t j = 0; j < gdim; ++j)
            {
                facet_mids.push_back(facet.midpoint()[j]);
                facet_normals.push_back(single_cell.normal(i)[j]);
            }
        }

        cells[cell_id].facet_adjacents = facet_adjacents;
        cells[cell_id].facet_normals = facet_normals;
        cells[cell_id].facet_mids = facet_mids;
    }
}

void Population::add_particles(std::vector<double> &xs, std::vector<double> &vs,
                               double q, double m)
{
    std::size_t num_particles = xs.size() / gdim;
    std::vector<double> xs_tmp(gdim);
    std::vector<double> vs_tmp(gdim);
    std::size_t cell_id;
    for (std::size_t i = 0; i < num_particles; ++i)
    {
        for (std::size_t j = 0; j < gdim; ++j)
        {
            xs_tmp[j] = xs[i * gdim + j];
            vs_tmp[j] = vs[i * gdim + j];
        }
        cell_id = locate(xs_tmp);
        if (cell_id >= 0)
        {
            Particle particle(xs_tmp, vs_tmp, q, m);
            cells[cell_id].particles.push_back(particle);
        }
    }
}

signed long int Population::locate(std::vector<double> &p)
{
    return punc::locate(mesh, p);
}

signed long int Population::relocate(std::vector<double> &p, signed long int cell_id)
{
    df::Cell _cell_(*mesh, cell_id);
    df::Point point(gdim, p.data());
    if (_cell_.contains(point))
    {
        return cell_id;
    }
    else
    {
        std::vector<double> proj(gdim + 1);
        for (std::size_t i = 0; i < gdim + 1; ++i)
        {
            proj[i] = 0.0;
            for (std::size_t j = 0; j < gdim; ++j)
            {
                proj[i] += (p[j] - cells[cell_id].facet_mids[i * gdim + j]) *
                        cells[cell_id].facet_normals[i * gdim + j];
            }
        }
        auto projarg = std::distance(proj.begin(), std::max_element(proj.begin(), proj.end()));
        auto new_cell_id = cells[cell_id].facet_adjacents[projarg];
        if (new_cell_id >= 0)
        {
            return relocate(p, new_cell_id);
        }
        else
        {
            return new_cell_id;
        }
    }
}
void Population::update(boost::optional<std::vector<Object>& > objects)
{
    std::size_t num_objects = 0;
    if(objects)
    {
        num_objects = objects->size();
    }
    signed long int new_cell_id;
    for (signed long int cell_id = 0; cell_id < num_cells; ++cell_id)
    {
        std::vector<std::size_t> to_delete;
        std::size_t num_particles = cells[cell_id].particles.size();
        for (std::size_t p_id = 0; p_id < num_particles; ++p_id)
        {
            auto particle = cells[cell_id].particles[p_id];
            new_cell_id = relocate(particle.x, cell_id);
            if (new_cell_id != cell_id)
            {
                to_delete.push_back(p_id);
                if (new_cell_id >= 0)
                {
                    cells[new_cell_id].particles.push_back(particle);
                }else{
                    for (auto i = 0; i < num_objects; ++i)
                    {
                        if(-new_cell_id == objects.get()[i].id)
                        {
                            objects.get()[i].charge += particle.q;
                        }
                    }
                }
            }
        }
        std::size_t size_to_delete = to_delete.size();
        for (std::size_t it = size_to_delete; it-- > 0;)
        {
            auto p_id = to_delete[it];
            if (p_id == num_particles - 1)
            {
                cells[cell_id].particles.pop_back();
            }
            else
            {
                std::swap(cells[cell_id].particles[p_id], cells[cell_id].particles.back());
                cells[cell_id].particles.pop_back();
            }
        }
    }
}

std::size_t Population::num_of_particles()
{
    std::size_t num_particles = 0;
    for (std::size_t cell_id = 0; cell_id < num_cells; ++cell_id)
    {
        num_particles += cells[cell_id].particles.size();
    }
    return num_particles;
}

std::size_t Population::num_of_positives()
{
    std::size_t num_positives = 0;
    for (std::size_t cell_id = 0; cell_id < num_cells; ++cell_id)
    {
        std::size_t num_particles = cells[cell_id].particles.size();
        for (std::size_t p_id = 0; p_id < num_particles; ++p_id)
        {
            auto particle = cells[cell_id].particles[p_id];
            if (particle.q > 0)
            {
                num_positives++;
            }
        }
    }
    return num_positives;
}

std::size_t Population::num_of_negatives()
{
    std::size_t num_negatives = 0;
    for (std::size_t cell_id = 0; cell_id < num_cells; ++cell_id)
    {
        std::size_t num_particles = cells[cell_id].particles.size();
        for (std::size_t p_id = 0; p_id < num_particles; ++p_id)
        {
            auto particle = cells[cell_id].particles[p_id];
            if (particle.q < 0)
            {
                num_negatives++;
            }
        }
    }
    return num_negatives;
}

void Population::save_vel(const std::string &fname)
{
    FILE *fout = fopen(fname.c_str(), "w");
    for (std::size_t cell_id = 0; cell_id < num_cells; ++cell_id)
    {
        std::size_t num_particles = cells[cell_id].particles.size();
        if (num_particles > 0)
        {
            for (std::size_t p_id = 0; p_id < num_particles; ++p_id)
            {
                auto particle = cells[cell_id].particles[p_id];
                for (std::size_t i = 0; i < gdim; ++i)
                {
                    fprintf(fout, "%.17g\t", particle.v[i]);
                }
                fprintf(fout, "\n");
            }
        }
    }
    fclose(fout);
}

void Population::save_file(const std::string &fname)
{
    FILE *fout = fopen(fname.c_str(), "w");
    for (std::size_t cell_id = 0; cell_id < num_cells; ++cell_id)
    {
        std::size_t num_particles = cells[cell_id].particles.size();
        if (num_particles > 0)
        {
            for (std::size_t p_id = 0; p_id < num_particles; ++p_id)
            {
                auto particle = cells[cell_id].particles[p_id];
                for (std::size_t i = 0; i < gdim; ++i)
                {
                    fprintf(fout, "%.17g\t", particle.x[i]);
                }
                for (std::size_t i = 0; i < gdim; ++i)
                {
                    fprintf(fout, "%.17g\t", particle.v[i]);
                }
                fprintf(fout, "%.17g\t %.17g\t", particle.q, particle.m);
                fprintf(fout, "\n");
            }
        }
    }
    fclose(fout);
}

void Population::load_file(const std::string &fname)
{
    std::fstream in(fname);
    std::string line;
    std::vector<double> x(gdim);
    std::vector<double> v(gdim);
    double q, m;
    int i;
    while (std::getline(in, line))
    {
        double value;
        std::stringstream ss(line);
        i = 0;
        while (ss >> value)
        {
            if (i < gdim)
            {
                x[i] = value;
            }
            else if (i >= gdim && i < 2 * gdim)
            {
                v[i % gdim] = value;
            }
            else if (i == 2 * gdim)
            {
                q = value;
            }
            else if (i == 2 * gdim + 1)
            {
                m = value;
            }
            ++i;
            add_particles(x,v,q,m);
        }
    }
}

}
