#include <iostream>
#include <cstdlib>

#include <paralution.hpp>

using namespace paralution;

int main(int argc, char* argv[]) {

  if (argc == 1) { 
    std::cerr << argv[0] << " <matrix> [Num threads]" << std::endl;
    exit(1);
  }

  init_paralution();

  if (argc > 2) {
    set_omp_threads_paralution(atoi(argv[2]));
  } 

  info_paralution();

  LocalVector<double> x;
  LocalVector<double> rhs;

  LocalMatrix<double> mat;

  // read from file 
  mat.ReadFileMTX(std::string(argv[1]));

  x.Allocate("x", mat.get_nrow());
  rhs.Allocate("rhs", mat.get_nrow());

  MixedPrecisionDC<LocalMatrix<double>, LocalVector<double>, double,
                   LocalMatrix<float>, LocalVector<float>, float> mp;

  CG<LocalMatrix<float>, LocalVector<float>, float> cg;
  MultiColoredILU<LocalMatrix<float>, LocalVector<float>, float> p;

  double tick, tack;

  rhs.Ones();
  x.Zeros();

  // setup a lower tol for the inner solver
  cg.SetPreconditioner(p);
  cg.Init(1e-5, 1e-2, 1e+20,
          100000);

  // setup the mixed-precision DC
  mp.SetOperator(mat);
  mp.Set(cg);

  mp.Build();

  tick = paralution_time();

  mp.Solve(rhs, &x);

  tack = paralution_time();

  std::cout << "Solver execution:" << (tack-tick)/1000000 << " sec" << std::endl;

  mp.Clear();

  stop_paralution();

  return 0;
}