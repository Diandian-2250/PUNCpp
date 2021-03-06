#ifndef PUSHER_H
#define PUSHER_H

#include <dolfin.h>
#include "population.h"

namespace punc
{

namespace df = dolfin;

double accel(Population &pop, std::shared_ptr<df::Function> &E, const double dt);

double boris(Population &pop, std::shared_ptr<df::Function> &E, 
             const std::vector<double> &B, const double dt);

void move_periodic(Population &pop, const double dt, const std::vector<double> &Ld);

void move(Population &pop, const double dt);

std::vector<double> cross(std::vector<double> &v1, std::vector<double> &v2);

}

#endif
