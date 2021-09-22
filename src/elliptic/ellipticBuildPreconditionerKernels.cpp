/*

   The MIT License (MIT)

   Copyright (c) 2017 Tim Warburton, Noel Chalmers, Jesse Chan, Ali Karakus

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

 */

#include "elliptic.h"
#include <string>
#include "platform.hpp"
#include "linAlg.hpp"

void ellipticBuildPreconditionerKernels(elliptic_t* elliptic)
{
  
  mesh_t* mesh      = elliptic->mesh;

  std::string prefix = "Hex3D";
  std::string kernelName;

  MPI_Barrier(platform->comm.mpiComm);
  double tStartLoadKernel = MPI_Wtime();
  if(platform->comm.mpiRank == 0) printf("loading elliptic preconditioner kernels ... ");
  fflush(stdout);

  const std::string orderSuffix = std::string("_") + std::to_string(mesh->N);

  {
    kernelName = "mask";
    mesh->maskKernel =
      platform->kernels.getKernel(kernelName + orderSuffix);

    mesh->maskPfloatKernel =
      platform->kernels.getKernel(kernelName + orderSuffix + "pfloat");
                                 kernelName = "fusedCopyDfloatToPfloat";
    elliptic->fusedCopyDfloatToPfloatKernel =
      platform->kernels.getKernel(kernelName + orderSuffix);
    kernelName = "copyDfloatToPfloat";
    elliptic->copyDfloatToPfloatKernel =
      platform->kernels.getKernel(kernelName + orderSuffix);

    kernelName = "copyPfloatToDfloat";
    elliptic->copyPfloatToDPfloatKernel =
      platform->kernels.getKernel(kernelName + orderSuffix);

    kernelName = "scaledAdd";
    elliptic->scaledAddPfloatKernel =
      platform->kernels.getKernel(kernelName + orderSuffix);

    kernelName = "dotMultiply";
    elliptic->dotMultiplyPfloatKernel =
      platform->kernels.getKernel(kernelName + orderSuffix);

    kernelName = "updateSmoothedSolutionVec";
    elliptic->updateSmoothedSolutionVecKernel =
      platform->kernels.getKernel(kernelName + orderSuffix);

    kernelName = "updateChebyshevSolutionVec";
    elliptic->updateChebyshevSolutionVecKernel =
      platform->kernels.getKernel(kernelName + orderSuffix);

    kernelName = "updateIntermediateSolutionVec";
    elliptic->updateIntermediateSolutionVecKernel =
      platform->kernels.getKernel(kernelName + orderSuffix);
  }

  MPI_Barrier(platform->comm.mpiComm);
  if(platform->comm.mpiRank == 0) printf("done (%gs)\n", MPI_Wtime() - tStartLoadKernel);
  fflush(stdout);

}
