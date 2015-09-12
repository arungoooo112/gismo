/** @file poissonVector.cpp

    @brief Poisson example with command line arguments.

    This file is part of the G+Smo library.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): A. Mantzaflaris, J. Sogn
*/

// This example solving the Poisson equation using gismo
// for the gsAssemblerBase.h structure

# include <gismo.h>

using std::cout;
using std::endl;
using namespace gismo;

// Getting the input (defined after the main function)
bool parse_input( int argc, char *argv[], int & numRefine, int & numElevate,
                  int & Dirichlet, int & DG, bool & plot, int & plot_pts,
                  gsMultiPatch<> * & geo, gsPoissonPde<> * & ppde,
                  gsMultiBasis<> & bases );

int main(int argc, char *argv[])
{

    /////////////////// Input ///////////////////
    int numRefine;  // defaults to 2
    int numElevate; // defaults to -1
    int Dirichlet;  // defaults to 0
    int DG;         // defaults to 0
    bool plot;      // defaults to false
    int plot_pts;   // defaults to 1000
    gsMultiPatch<> * patches ; // defaults to BSplineCube
    gsPoissonPde<> * ppde ;
    gsMultiBasis<> bases;// not yet given by input

    bool success = parse_input(argc, argv, numRefine, numElevate, Dirichlet,
                               DG, plot, plot_pts, patches, ppde, bases);
    if ( ! success )
      return 0;

    /////////////////// Print info ///////////////////
    cout<<"Type "<< argv[0]<< " -h, to get the list of command line options.\n\n";
    cout<<"Domain: "<< *patches <<"\n";
    cout<< "Number of patches are " << patches->nPatches() << endl;
    cout<<"Source function "<< *ppde->rhs() << endl;
    cout<<"Exact solution "<< *ppde->solution() <<".\n" << endl;
    cout<<"p-refinent steps before solving: "<< numElevate <<"\n";
    cout<<"h-refinent steps before solving: "<< numRefine <<"\n";

    cout<< * ppde <<"\n";

    /////////////////// Setup boundary conditions ///////////////////
    // Define Boundary conditions
    gsBoundaryConditions<> bcInfo;

    // Create Dirichlet boundary conditions for all boundaries
    for (gsMultiPatch<>::const_biterator
         bit = patches->bBegin(); bit != patches->bEnd(); ++bit)
    {
        bcInfo.addCondition( *bit, condition_type::dirichlet, ppde->solution() );
    }
    // Put Boundary conditions in a vector
    std::vector<gsBoundaryConditions<>* > vecBcInfo;
    vecBcInfo.push_back(&bcInfo);


    //gsFunction<> g1 =  ppde->solution();
    //gsFunction<> f1 = ppde->rhs();


    /////////////////// Refinement h and p ///////////////////
    if ( bases.nBases() == 0 )
        bases = gsMultiBasis<>(*patches);

    // Elevate and refine the solution space
    if ( numElevate > -1 )
    {
        // get maximum degree
        int tmp = bases.maxDegree(0);

        // Elevate all degrees uniformly
        tmp += numElevate;
        for (size_t j = 0; j < bases.nBases(); ++j )
                bases[j].setDegree(tmp);
    }

    // Refining the basis
    for (size_t j = 0; j < bases.nBases(); ++j )
        for (int i = 0; i < numRefine; ++i)
            bases[j].uniformRefine();

    cout << "Discrete. Space 0: "<< bases[0] << endl;


    /////////////////// Setup solver ///////////////////
    //Initialize Solver

    // gsPoissonAssembler<real_t> PoissonAssembler(*patches,bases,bcInfo,*ppde->rhs(),
    //                                             dirichlet::elimination, iFace::glue);
    gsPoissonAssembler<real_t> PoissonAssembler(*ppde->rhs());

    gsAssemblerOptions options;
    //Use Nitsche's method for Dirichlet boundaries
    if ( Dirichlet == 1)
    {
        cout<<"Using Nitsche's method for Dirichlet boundaries.\n";
        options.dirStrategy = dirichlet::nitsche;
    }

    if ( DG == 1)
    {
        cout<<"Using DG method for patch interfaces.\n";
        options.intStrategy = iFace::dg;
    }

    PoissonAssembler.initialize(*patches, bases, bcInfo, options);

    // Generate system matrix and load vector
    std::cout<<"Assembling...\n";
    PoissonAssembler.assemble();

    // Initialize the conjugate gradient solver
    std::cout<<"Solving...\n";
    gsSparseSolver<>::CGDiagonal solver( PoissonAssembler.matrix() );
    gsMatrix<> solVector = solver.solve( PoissonAssembler.rhs() );

    // Construct the solution
    // Construct the solution as a scalar field
    gsField<>::uPtr sol = safe(PoissonAssembler.constructSolution(solVector));

    // Plot solution in paraview
    int result = 0;
    if (plot)
    {
        // Write approximate and exact solution to paraview files
        std::cout<<"Plotting in Paraview...\n";
        gsWriteParaview<>(*sol, "poisson2d", plot_pts);
        //gsField<> exact( PoissonSolver.patches(), g, false );
        //gsWriteParaview<>( exact, "poisson2d_exact", plot_pts);

        // Run paraview
        result = system("paraview poisson2d.pvd &");
    }
    //delete tbasis;

    cout << "Test is done: Cleaning up..." << endl; //freeAll(m_bconditions);

    delete patches;

    cout << "Test is done: Exiting" << endl;

    delete ppde;

    return  result;
}


bool parse_input( int argc, char *argv[], int & numRefine, int & numElevate,
                  int & Dirichlet, int & DG, bool & plot, int & plot_pts,
                  gsMultiPatch<> *& geo, gsPoissonPde<> *& ppde,
                  gsMultiBasis<> &  bases )
{
  std::string fn_pde("");
  std::string fn("");
  std::string fn_basis("");
  bool arg_dirich = false;
  bool arg_dg = false;
  plot = false;
  plot_pts = 1000;
  numElevate = -1;
  numRefine = 2;
  
  gsCmdLine cmd("Solves Poisson's equation with an isogeometric discretization.");
  cmd.addString("p","pde","File containing a poisson PDE (.xml)", fn_pde);
  cmd.addSwitch("nitsche", "Use the Nitsche's method for Dirichlet sides", arg_dirich);
  cmd.addSwitch("discGalerkin", "Use Discontiouous Galerkin method for patch interfaces", arg_dg);
  cmd.addSwitch("plot", "Plot result in ParaView format", plot);
  cmd.addInt("s","plotSamples", "Number of sample points to use for plotting", plot_pts);
  cmd.addInt("e","degreeElevation", 
             "Number of degree elevation steps to perform before solving (0: equalize degree in all directions)",
             numElevate);
  cmd.addInt("r","uniformRefine", "Number of Uniform h-refinement steps to perform before solving",
             numRefine);
  cmd.addString("b","basis","File containing basis for discretization (.xml)", fn_basis);
  cmd.addString("g","geometry","File containing Geometry (.xml, .axl, .txt)", fn);
  bool ok = cmd.getValues(argc,argv);
  if (!ok) {
    cout << "Error parsing command line!\n";
    return false; 
  }
    
    if ( arg_dirich )
        Dirichlet  = 1;
    else
        Dirichlet  = 0;

    if ( arg_dg )
    {
        DG  = 1;
        Dirichlet  = 1;// use Nitsche for boundary
    }
    else
        DG  = 0;

    

    if ( ! fn_basis.empty() )
    {
    gsBasis<> * bb = gsReadFile<>( fn_basis );
    cout << "Got basis: "<< * bb<<"\n";
    bases.addBasis(bb);
    //cout << "Warning: basis ignored.\n";
    }

    if (numRefine<0)
    {
      cout << "Number of refinements must be non-negative, setting to zero.\n";
      numRefine = 0;
    }
    if (numElevate<-1)
    {
      cout << "Number of elevations must be non-negative, ignoring parameter.\n";
      numElevate = -1;
    }

    if ( fn_pde.empty() )
    {
        fn_pde = GISMO_DATA_DIR;
        if ( !fn.empty() )
        {
            geo = gsReadFile<>( fn );
            if ( ! geo )
              {
            gsWarn<< "Did not find any geometry in "<<fn<<", quitting.\n";
            return false;
              }
            switch ( geo->geoDim() )
            {
            case 1:
                fn_pde+="/pde/poisson1d_sin.xml";
                break;
            case 2:
                fn_pde+="/pde/poisson2d_sin.xml";
                break;
            case 3:
                fn_pde+="/pde/poisson3d_sin.xml";
                break;
            default:
                return false;
            }
        }
        else
        fn_pde+="/pde/poisson2d_sin.xml";
    }
    ppde = gsReadFile<>(fn_pde);
    if ( !ppde )
      {
    gsWarn<< "Did not find any PDE in "<< fn<<", quitting.\n";
    return false;
      }

    if ( fn.empty() )
    {
      fn = GISMO_DATA_DIR;
      switch ( ppde->m_compat_dim )
	    {
	    case 1:
	      fn+= "domain1d/bspline1d_01.xml";
	      break;
	    case 2:
	      fn+= "domain2d/square.xml";
	      break;
	    case 3:
	      fn+= "domain3d/cube.xml";
	      break;
	    default:
	      return false;
	    }
    }
    geo = gsReadFile<>( fn );
    if ( !geo )
      {
    cout << "Did not find any geometries in "<< fn<<", quitting.\n";
    return false;
      }

 
  return true;
}
