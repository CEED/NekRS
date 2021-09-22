#include "nrs.hpp"
#include "bcMap.hpp"
#include "meshNekReader.hpp"
#include <string>

void meshVOccaSetup3D(mesh_t* mesh, occa::properties &kernelInfo);
mesh_t *createMeshV(MPI_Comm comm,
                    int N,
                    int cubN,
                    mesh_t* meshT,
                    occa::properties& kernelInfo);

occa::properties populateMeshProperties(int N)
{
  occa::properties meshProperties = platform->kernelInfo;
  const int Nq = N+1;
  const int Np = Nq * Nq * Nq;
  const int Nfp = Nq * Nq;
  constexpr int Nfaces {6};

  constexpr int Nvgeo {12};
  constexpr int Nggeo {7};
  constexpr int Nsgeo {17};

  meshProperties["defines/" "p_dim"] = 3;
  meshProperties["defines/" "p_Nfields"] = 1;
  meshProperties["defines/" "p_N"] = N;
  meshProperties["defines/" "p_Nq"] = Nq;
  meshProperties["defines/" "p_Np"] = Np;
  meshProperties["defines/" "p_Nfp"] = Nfp;
  meshProperties["defines/" "p_Nfaces"] = Nfaces;
  meshProperties["defines/" "p_NfacesNfp"] = Nfp * Nfaces;

  meshProperties["defines/" "p_Nvgeo"] = Nvgeo;
  meshProperties["defines/" "p_Nsgeo"] = Nsgeo;
  meshProperties["defines/" "p_Nggeo"] = Nggeo;

  meshProperties["defines/" "p_NXID"] = NXID;
  meshProperties["defines/" "p_NYID"] = NYID;
  meshProperties["defines/" "p_NZID"] = NZID;
  meshProperties["defines/" "p_SJID"] = SJID;
  meshProperties["defines/" "p_IJID"] = IJID;
  meshProperties["defines/" "p_IHID"] = IHID;
  meshProperties["defines/" "p_WSJID"] = WSJID;
  meshProperties["defines/" "p_WIJID"] = WIJID;
  meshProperties["defines/" "p_STXID"] = STXID;
  meshProperties["defines/" "p_STYID"] = STYID;
  meshProperties["defines/" "p_STZID"] = STZID;
  meshProperties["defines/" "p_SBXID"] = SBXID;
  meshProperties["defines/" "p_SBYID"] = SBYID;
  meshProperties["defines/" "p_SBZID"] = SBZID;

  meshProperties["defines/" "p_G00ID"] = G00ID;
  meshProperties["defines/" "p_G01ID"] = G01ID;
  meshProperties["defines/" "p_G02ID"] = G02ID;
  meshProperties["defines/" "p_G11ID"] = G11ID;
  meshProperties["defines/" "p_G12ID"] = G12ID;
  meshProperties["defines/" "p_G22ID"] = G22ID;
  meshProperties["defines/" "p_GWJID"] = GWJID;

  meshProperties["defines/" "p_RXID"] = RXID;
  meshProperties["defines/" "p_SXID"] = SXID;
  meshProperties["defines/" "p_TXID"] = TXID;

  meshProperties["defines/" "p_RYID"] = RYID;
  meshProperties["defines/" "p_SYID"] = SYID;
  meshProperties["defines/" "p_TYID"] = TYID;

  meshProperties["defines/" "p_RZID"] = RZID;
  meshProperties["defines/" "p_SZID"] = SZID;
  meshProperties["defines/" "p_TZID"] = TZID;

  meshProperties["defines/" "p_JID"] = JID;
  meshProperties["defines/" "p_JWID"] = JWID;
  meshProperties["defines/" "p_IJWID"] = IJWID;
  return meshProperties;
}
void loadKernels(mesh_t* mesh);

mesh_t *createMesh(MPI_Comm comm,
                   int N,
                   int cubN,
                   bool cht,
                   occa::properties& kernelInfo)
{
  mesh_t *mesh = new mesh_t();
  platform->options.getArgs("MESH INTEGRATION ORDER", mesh->nAB);
  int rank, size;
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);

  mesh->cht  = cht;

  // get mesh from nek
  meshNekReaderHex3D(N, mesh);

  if (platform->comm.mpiRank == 0)
    printf("generating mesh ... ");

  if (mesh->Nelements * mesh->Nvgeo * cubN > std::numeric_limits<int>::max()) {
    if (platform->comm.mpiRank == 0) printf("FATAL ERROR: Local element count too large!");
    ABORT(EXIT_FAILURE);
  }

  mesh->Nfields = 1; // TW: note this is a temporary patch (halo exchange depends on nfields)

  // connect elements using parallel sort
  meshParallelConnect(mesh);

  // connect elements to boundary faces
  meshConnectBoundary(mesh);

  // load reference (r,s,t) element nodes
  meshLoadReferenceNodesHex3D(mesh, N, cubN);
  if (platform->comm.mpiRank == 0) {
    if (cubN)
      printf("Nq: %d cubNq: %d\n", mesh->Nq, mesh->cubNq);
    else
      printf("Nq: %d\n", mesh->Nq);
  }

  mesh->Nlocal = mesh->Nelements * mesh->Np;

  loadKernels(mesh);

  // set up halo exchange info for MPI (do before connect face nodes)
  meshHaloSetup(mesh);

  // compute physical (x,y) locations of the element nodes
  meshPhysicalNodesHex3D(mesh);

  meshHaloPhysicalNodes(mesh);

  // compute geometric factors
  meshGeometricFactorsHex3D(mesh);

  // connect face nodes (find trace indices)
  meshConnectFaceNodes3D(mesh);

  // compute surface geofacs (including halo)
  meshSurfaceGeometricFactorsHex3D(mesh);

  // global nodes
  meshGlobalIds(mesh);
  bcMap::check(mesh);

  meshOccaSetup3D(mesh, platform->options, kernelInfo);

  meshParallelGatherScatterSetup(mesh, mesh->Nelements * mesh->Np, mesh->globalIds, platform->comm.mpiComm, 0);
  oogs_mode oogsMode = OOGS_AUTO; 
  //if(platform->device.mode() == "Serial" || platform->device.mode() == "OpenMP") oogsMode = OOGS_DEFAULT;
  mesh->oogs = oogs::setup(mesh->ogs, 1, mesh->Nelements * mesh->Np, ogsDfloat, NULL, oogsMode);

  // build mass + inverse mass matrix
  for(dlong e = 0; e < mesh->Nelements; ++e)
    for(int n = 0; n < mesh->Np; ++n)
      mesh->LMM[e * mesh->Np + n] = mesh->vgeo[e * mesh->Np * mesh->Nvgeo + JWID * mesh->Np + n];
  mesh->o_LMM.copyFrom(mesh->LMM, mesh->Nelements * mesh->Np * sizeof(dfloat));
  mesh->computeInvLMM();

  if(platform->options.compareArgs("MOVING MESH", "TRUE")){
    const int maxTemporalOrder = 3;
    mesh->coeffAB = (dfloat*) calloc(maxTemporalOrder, sizeof(dfloat));
    mesh->o_coeffAB = platform->device.malloc(maxTemporalOrder * sizeof(dfloat), mesh->coeffAB);
  }

  mesh->fluid = mesh;
  if(mesh->cht) mesh->fluid = createMeshV(comm, N, cubN, mesh, kernelInfo); 

  return mesh;
}

/*
mesh_t* duplicateMesh(MPI_Comm comm,
                      int N,
                      int cubN,
                      mesh_t* meshT,
                      occa::device device,
                      occa::properties& kernelInfo)
{
  mesh_t* mesh = new mesh_t[1];

  // shallow copy
  memcpy(mesh, meshT, sizeof(*meshT));

  mesh->Nfields = 1; // TW: note this is a temporary patch (halo exchange depends on nfields)

  // load reference (r,s,t) element nodes
  meshLoadReferenceNodesHex3D(mesh, N, cubN);
  if (platform->comm.mpiRank == 0)
    printf("Nq: %d cubNq: %d \n", mesh->Nq, mesh->cubNq);

  loadKernels(mesh);

  meshHaloSetup(mesh);
  meshPhysicalNodesHex3D(mesh);
  meshHaloPhysicalNodes(mesh);
  meshGeometricFactorsHex3D(mesh);
  meshConnectFaceNodes3D(mesh);
  meshSurfaceGeometricFactorsHex3D(mesh);
  meshGlobalIds(mesh);

  bcMap::check(mesh);
  meshOccaSetup3D(mesh, platform->options, kernelInfo);

  meshParallelGatherScatterSetup(mesh, mesh->Nelements * mesh->Np, mesh->globalIds, platform->comm.mpiComm, 0);
  oogs_mode oogsMode = OOGS_AUTO; 
  //if(platform->device.mode() == "Serial" || platform->device.mode() == "OpenMP") oogsMode = OOGS_DEFAULT;
  mesh->oogs = oogs::setup(mesh->ogs, 1, mesh->Nelements * mesh->Np, ogsDfloat, NULL, oogsMode);

  // build mass + inverse mass matrix
  for(dlong e = 0; e < mesh->Nelements; ++e)
    for(int n = 0; n < mesh->Np; ++n)
      mesh->LMM[e * mesh->Np + n] = mesh->vgeo[e * mesh->Np * mesh->Nvgeo + JWID * mesh->Np + n];
  mesh->o_LMM.copyFrom(mesh->LMM, mesh->Nelements * mesh->Np * sizeof(dfloat));
  mesh->computeInvLMM();

  if(platform->options.compareArgs("MOVING MESH", "TRUE")){
    const int maxTemporalOrder = 3;
    mesh->coeffAB = (dfloat*) calloc(maxTemporalOrder, sizeof(dfloat));
    mesh->o_coeffAB = platform->device.malloc(maxTemporalOrder * sizeof(dfloat), mesh->coeffAB);
  }

  return mesh;
}
*/

mesh_t *createMeshV(
                    MPI_Comm comm,
                    int N,
                    int cubN,
                    mesh_t* meshT,
                    occa::properties& kernelInfo)
{
  mesh_t *mesh = new mesh_t();

  // shallow copy
  memcpy(mesh, meshT, sizeof(*meshT));
  mesh->cht = 0;

  // find EToV and boundaryInfo
  meshNekReaderHex3D(N, mesh);
  free(mesh->elementInfo);
  mesh->elementInfo = meshT->elementInfo;

  mesh->Nlocal = mesh->Nelements * mesh->Np;

  mesh->Nfields = 1; // temporary patch (halo exchange depends on nfields)

  // find mesh->EToP, mesh->EToE and mesh->EToF, required mesh->EToV
  meshParallelConnect(mesh);

  // find mesh->EToB, required mesh->EToV and mesh->boundaryInfo
  meshConnectBoundary(mesh);

  // set up halo exchange info for MPI (do before connect face nodes)
  meshHaloSetup(mesh);

  //meshPhysicalNodesHex3D(mesh);
  mesh->x = meshT->x;
  mesh->y = meshT->y;
  mesh->z = meshT->z;

  meshHaloPhysicalNodes(mesh);

  // meshGeometricFactorsHex3D(mesh);
  mesh->vgeo = meshT->vgeo;
  mesh->cubvgeo = meshT->cubvgeo;
  mesh->ggeo = meshT->ggeo;

  // connect face nodes (find trace indices)
  // find vmapM, vmapP, mapP based on EToE and EToF
  meshConnectFaceNodes3D(mesh);

  // meshGlobalIds(mesh);
  mesh->globalIds = meshT->globalIds;

  bcMap::check(mesh);

  meshVOccaSetup3D(mesh, kernelInfo);

  meshParallelGatherScatterSetup(mesh, mesh->Nelements * mesh->Np, mesh->globalIds, platform->comm.mpiComm, 0);
  oogs_mode oogsMode = OOGS_AUTO; 
  //if(platform->device.mode() == "Serial" || platform->device.mode() == "OpenMP") oogsMode = OOGS_DEFAULT;
  mesh->oogs = oogs::setup(mesh->ogs, 1, mesh->Nelements * mesh->Np, ogsDfloat, NULL, oogsMode);

  mesh->computeInvLMM();

  return mesh;
}

void meshVOccaSetup3D(mesh_t* mesh, occa::properties &kernelInfo)
{
  if(mesh->totalHaloPairs > 0) {
    // copy halo element list to DEVICE
    mesh->o_haloElementList =
      platform->device.malloc(mesh->totalHaloPairs * sizeof(dlong), mesh->haloElementList);

    // temporary DEVICE buffer for halo (maximum size Nfields*Np for dfloat)
    mesh->o_haloBuffer =
      platform->device.malloc(mesh->totalHaloPairs * mesh->Np * mesh->Nfields ,  sizeof(dfloat));

    // node ids
    mesh->o_haloGetNodeIds =
      platform->device.malloc(mesh->Nfp * mesh->totalHaloPairs * sizeof(dlong), mesh->haloGetNodeIds);

    mesh->o_haloPutNodeIds =
      platform->device.malloc(mesh->Nfp * mesh->totalHaloPairs * sizeof(dlong), mesh->haloPutNodeIds);
  }

  mesh->o_EToB =
    platform->device.malloc(mesh->Nelements * mesh->Nfaces * sizeof(int),
                        mesh->EToB);
  mesh->o_vmapM =
    platform->device.malloc(mesh->Nelements * mesh->Nfp * mesh->Nfaces * sizeof(dlong),
                        mesh->vmapM);
  mesh->o_vmapP =
    platform->device.malloc(mesh->Nelements * mesh->Nfp * mesh->Nfaces * sizeof(dlong),
                        mesh->vmapP);
  mesh->o_invLMM =
    platform->device.malloc(mesh->Nelements * mesh->Np ,  sizeof(dfloat));
}

void loadKernels(mesh_t* mesh)
{
  const std::string meshPrefix = "mesh-";
  if(platform->options.compareArgs("MOVING MESH", "TRUE")){
    {
        mesh->velocityDirichletKernel =
          platform->kernels.getKernel(meshPrefix + "velocityDirichletBCHex3D");
        mesh->geometricFactorsKernel =
          platform->kernels.getKernel(meshPrefix + "geometricFactorsHex3D");
        mesh->surfaceGeometricFactorsKernel =
          platform->kernels.getKernel(meshPrefix + "surfaceGeometricFactorsHex3D");
        mesh->nStagesSumVectorKernel =
          platform->kernels.getKernel(meshPrefix + "nStagesSumVector");
    }
  }
}
