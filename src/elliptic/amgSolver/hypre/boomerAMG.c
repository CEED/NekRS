#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "omp.h"

#include "boomerAMG.h"

static double boomerAMGParam[BOOMERAMG_NPARAM];

#ifdef HYPRE

#include "_hypre_utilities.h"
#include "HYPRE_parcsr_ls.h"
#include "_hypre_parcsr_ls.h"
#include "HYPRE.h"

typedef struct hypre_data {
  MPI_Comm comm;
  HYPRE_Solver solver;
  HYPRE_IJMatrix A;
  HYPRE_IJVector b;
  HYPRE_IJVector x;
  HYPRE_BigInt ilower;
  HYPRE_BigInt *ii;
  HYPRE_Real *bb;
  HYPRE_Real *xx;
  int nRows;
  int Nthreads;
} hypre_data;
static hypre_data *data;


int boomerAMGSetup(int nrows,
                   int nz, const long long int *Ai, const long long int *Aj, const double *Av,
                   const int null_space, const MPI_Comm ce, int Nthreads, int deviceID,
                   const int useFP32, const double *param)
{
  data = (hypre_data*) malloc(sizeof(struct hypre_data));

  data->Nthreads = Nthreads;   

  MPI_Comm comm;
  MPI_Comm_dup(ce, &comm);
  data->comm = comm;
  int rank;
  MPI_Comm_rank(comm,&rank);

  if(sizeof(HYPRE_Real) != ((useFP32) ? sizeof(float) : sizeof(double))) {
    if(rank == 0) printf("HYPRE has not been built to support FP32.\n");
    MPI_Abort(ce, 1);
  } 

  long long rowStart = nrows;
  MPI_Scan(MPI_IN_PLACE, &rowStart, 1, MPI_LONG_LONG, MPI_SUM, ce);
  rowStart -= nrows;

  data->nRows = nrows;
  HYPRE_BigInt ilower = (HYPRE_BigInt) rowStart;
  data->ilower = ilower;
  HYPRE_BigInt iupper = ilower + (HYPRE_BigInt) nrows - 1; 

  HYPRE_IJMatrixCreate(comm,ilower,iupper,ilower,iupper,&data->A);
  HYPRE_IJMatrix A_ij = data->A;
  HYPRE_IJMatrixSetObjectType(A_ij,HYPRE_PARCSR);
  HYPRE_IJMatrixInitialize(A_ij);

  int i;
  for(i=0; i<nz; i++) 
  {
    HYPRE_BigInt mati = (HYPRE_BigInt)(Ai[i]);
    HYPRE_BigInt matj = (HYPRE_BigInt)(Aj[i]);
    HYPRE_Real matv = (HYPRE_Real) Av[i]; 
    HYPRE_Int ncols = 1; // number of columns per row
    HYPRE_IJMatrixSetValues(A_ij, 1, &ncols, &mati, &matj, &matv);
  }
  HYPRE_IJMatrixAssemble(A_ij);
  //HYPRE_IJMatrixPrint(A_ij, "matrix.dat");

  // Create AMG solver
  HYPRE_BoomerAMGCreate(&data->solver);
  HYPRE_Solver solver = data->solver;

  int uparam = (int) param[0];
 
  // Set AMG parameters
  if (uparam) {
    int i;
    for (i = 0; i < BOOMERAMG_NPARAM; i++)
        boomerAMGParam[i] = param[i+1]; 
  } else {
    boomerAMGParam[0]  = 10;   /* coarsening */
    boomerAMGParam[1]  = 6;    /* interpolation */
    boomerAMGParam[2]  = 1;    /* number of cycles */
    boomerAMGParam[3]  = 6;    /* smoother for crs level */
    boomerAMGParam[4]  = 3;    /* sweeps */
    boomerAMGParam[5]  = -1;   /* smoother */
    boomerAMGParam[6]  = 1;    /* sweeps   */
    boomerAMGParam[7]  = 0.25; /* threshold */
    boomerAMGParam[8]  = 0.0;  /* non galerkin tolerance */
  }

  HYPRE_BoomerAMGSetCoarsenType(solver,boomerAMGParam[0]);
  HYPRE_BoomerAMGSetInterpType(solver,boomerAMGParam[1]);

  //HYPRE_BoomerAMGSetKeepTranspose(solver, 1);
  //HYPRE_BoomerAMGSetChebyFraction(solver, 0.2); 
  if (boomerAMGParam[5] > 0) {
    HYPRE_BoomerAMGSetCycleRelaxType(solver, boomerAMGParam[5], 1);
    HYPRE_BoomerAMGSetCycleRelaxType(solver, boomerAMGParam[5], 2);
  } 
  HYPRE_BoomerAMGSetCycleRelaxType(solver, 9, 3);

  HYPRE_BoomerAMGSetCycleNumSweeps(solver, boomerAMGParam[6], 1);
  HYPRE_BoomerAMGSetCycleNumSweeps(solver, boomerAMGParam[6], 2);
  HYPRE_BoomerAMGSetCycleNumSweeps(solver, 1, 3);

  if (null_space) {
    HYPRE_BoomerAMGSetMinCoarseSize(solver, 2);
    HYPRE_BoomerAMGSetCycleRelaxType(solver, boomerAMGParam[3], 3);
    HYPRE_BoomerAMGSetCycleNumSweeps(solver, boomerAMGParam[4], 3);
  }

  HYPRE_BoomerAMGSetStrongThreshold(solver,boomerAMGParam[7]);

  if (boomerAMGParam[8] > 1e-3) {
    HYPRE_BoomerAMGSetNonGalerkinTol(solver,boomerAMGParam[8]);
    HYPRE_BoomerAMGSetLevelNonGalerkinTol(solver,0.0 , 0);
    HYPRE_BoomerAMGSetLevelNonGalerkinTol(solver,0.01, 1);
    HYPRE_BoomerAMGSetLevelNonGalerkinTol(solver,0.05, 2);
  }

  HYPRE_BoomerAMGSetAggNumLevels(solver, boomerAMGParam[9]); 

  HYPRE_BoomerAMGSetMaxIter(solver,boomerAMGParam[2]); // number of V-cycles
  HYPRE_BoomerAMGSetTol(solver,0);

  HYPRE_BoomerAMGSetPrintLevel(solver,1);

  // Create and initialize rhs and solution vectors
  HYPRE_IJVectorCreate(comm,ilower,iupper,&data->b);
  HYPRE_IJVector b = data->b;
  HYPRE_IJVectorSetObjectType(b,HYPRE_PARCSR);
  HYPRE_IJVectorInitialize(b);
  HYPRE_IJVectorAssemble(b);

  HYPRE_IJVectorCreate(comm,ilower,iupper,&data->x);
  HYPRE_IJVector x = data->x;
  HYPRE_IJVectorSetObjectType(x,HYPRE_PARCSR);
  HYPRE_IJVectorInitialize(x);
  HYPRE_IJVectorAssemble(x);

  // Perform AMG setup
  HYPRE_ParVector par_b;
  HYPRE_ParVector par_x;
  HYPRE_IJVectorGetObject(b,(void**) &par_b);
  HYPRE_IJVectorGetObject(x,(void**) &par_x);
  HYPRE_ParCSRMatrix par_A;
  HYPRE_IJMatrixGetObject(data->A,(void**) &par_A);

  int _Nthreads = 1;
  #pragma omp parallel
  {
    int tid = omp_get_thread_num();
    if(tid==0) _Nthreads = omp_get_num_threads();
  }
  omp_set_num_threads(data->Nthreads);
  HYPRE_BoomerAMGSetup(solver,par_A,par_b,par_x);
  omp_set_num_threads(_Nthreads);

  data->ii = (HYPRE_BigInt*) malloc(data->nRows*sizeof(HYPRE_BigInt));
  data->bb = (HYPRE_Real*) malloc(data->nRows*sizeof(HYPRE_Real));
  data->xx = (HYPRE_Real*) malloc(data->nRows*sizeof(HYPRE_Real));
  for(i=0;i<data->nRows;++i) 
    data->ii[i] = ilower + (HYPRE_BigInt)i;

  return 0;
}

int boomerAMGSolve(void *x, void *b)
{
  int i; 
  int err;

  const HYPRE_Real *xx = (HYPRE_Real*) x; 
  const HYPRE_Real *bb = (HYPRE_Real*) b; 

  HYPRE_ParVector par_x;
  HYPRE_ParVector par_b;
  HYPRE_ParCSRMatrix par_A;

  HYPRE_IJVectorSetValues(data->b,data->nRows,data->ii,bb);
  HYPRE_IJVectorAssemble(data->b);
  HYPRE_IJVectorGetObject(data->b,(void**) &par_b);

  HYPRE_IJVectorAssemble(data->x);
  HYPRE_IJVectorGetObject(data->x,(void **) &par_x);

  HYPRE_IJMatrixGetObject(data->A,(void**) &par_A);

  int _Nthreads = 1;
  #pragma omp parallel
  {
    int tid = omp_get_thread_num();
    if(tid==0) _Nthreads = omp_get_num_threads();
  }
  omp_set_num_threads(data->Nthreads);
  err = HYPRE_BoomerAMGSolve(data->solver,par_A,par_b,par_x);
  if(err > 0) { 
    int rank;
    MPI_Comm_rank(data->comm,&rank);
    if(rank == 0) printf("HYPRE_BoomerAMGSolve failed!\n");
    return 1;
  }
  omp_set_num_threads(_Nthreads);

  HYPRE_IJVectorGetValues(data->x,data->nRows,data->ii,(HYPRE_Real*)xx);

  return 0; 
}

void boomerAMGFree()
{
  HYPRE_BoomerAMGDestroy(data->solver);
  HYPRE_IJMatrixDestroy(data->A);
  HYPRE_IJVectorDestroy(data->x);
  HYPRE_IJVectorDestroy(data->b);
  free(data);
}

// Just to fix a hypre linking error
void hypre_blas_xerbla() {
}
void hypre_blas_lsame() {
}

#else
int boomerAMGSetup(int nrows,
                  int nz, const long long int *Ai, const long long int *Aj, const double *Av,
                  const int null_space, const MPI_Comm ce, int Nthreads, int deviceID
                  const double *param)
{
  int rank;
  MPI_Comm_rank(ce,&rank);
  if(rank == 0) printf("ERROR: Recompile with HYPRE support!\n");
  return 1;
}
int boomerAMGSolve(void *x, void *b)
{
  int rank;
  MPI_Comm_rank(ce,&rank);
  if(rank == 0) printf("ERROR: Recompile with HYPRE support!\n");
  return 1;
}
void boomerAMGFree()
{
  int rank;
  MPI_Comm_rank(ce,&rank);
  if(rank == 0) printf("ERROR: Recompile with HYPRE support!\n");
  MPI_Abort(ce, 1);
}
#endif
