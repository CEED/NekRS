/*---------------------------------------------------------------------------*\
   Copyright (c) 2019-2021, UCHICAGO ARGONNE, LLC.

   The UChicago Argonne, LLC as Operator of Argonne National
   Laboratory holds copyright in the Software. The copyright holder
   reserves all rights except those expressly granted to licensees,
   and U.S. Government license rights.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the disclaimer below.

   2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the disclaimer (as noted below)
   in the documentation and/or other materials provided with the
   distribution.

   3. Neither the name of ANL nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
   UCHICAGO ARGONNE, LLC, THE U.S. DEPARTMENT OF
   ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
   TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   Additional BSD Notice
   ---------------------
   1. This notice is required to be provided under our contract with
   the U.S. Department of Energy (DOE). This work was produced at
   Argonne National Laboratory under Contract
   No. DE-AC02-06CH11357 with the DOE.

   2. Neither the United States Government nor UCHICAGO ARGONNE,
   LLC nor any of their employees, makes any warranty,
   express or implied, or assumes any liability or responsibility for the
   accuracy, completeness, or usefulness of any information, apparatus,
   product, or process disclosed, or represents that its use would not
   infringe privately-owned rights.

   3. Also, reference herein to any specific commercial products, process,
   or services by trade name, trademark, manufacturer or otherwise does
   not necessarily constitute or imply its endorsement, recommendation,
   or favoring by the United States Government or UCHICAGO ARGONNE LLC.
   The views and opinions of authors expressed
   herein do not necessarily state or reflect those of the United States
   Government or UCHICAGO ARGONNE, LLC, and shall
   not be used for advertising or product endorsement purposes.

\*---------------------------------------------------------------------------*/

#include <mpi.h>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>
#include <cstring>
#include <getopt.h>
#include <cfenv>
#include <limits>
#include <math.h>
#include <unistd.h>

#include "nekrs.hpp"

#define DEBUG

static MPI_Comm comm;

struct cmdOptions
{
  int buildOnly = 0;
  int ciMode = 0;
  int debug = 0;
  int sizeTarget = 0;
  std::string setupFile;
  std::string deviceID;
  std::string backend;
};

static cmdOptions* processCmdLineOptions(int argc, char** argv);

int main(int argc, char** argv)
{
  {
    int request = MPI_THREAD_SINGLE;
    const char* env_val = std::getenv ("NEKRS_MPI_THREAD_MULTIPLE");
    if (env_val)
      if (std::stoi(env_val)) request = MPI_THREAD_MULTIPLE;

    int provided;
    int retval =  MPI_Init_thread(&argc, &argv, request, &provided);
    if (retval != MPI_SUCCESS) {
      std::cout << "FATAL ERROR: Cannot initialize MPI!" << "\n";
      exit(EXIT_FAILURE);
    }
  }

  int rank, size;
  MPI_Comm_dup(MPI_COMM_WORLD, &comm);
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);

  {
    if(!getenv("NEKRS_HOME")) {
      std::cout << "FATAL ERROR: Cannot find env variable NEKRS_HOME!" << "\n";
      MPI_Finalize();
      exit(EXIT_FAILURE);
    }

    std::string bin(getenv("NEKRS_HOME"));
    bin += "/bin/nekrs";
    const char* ptr = realpath(bin.c_str(), NULL);
    if(!ptr) {
      std::cout << "FATAL ERROR: Cannot find " << bin << "!\n";
      MPI_Finalize();
      exit(EXIT_FAILURE);
    }
  }

  cmdOptions* cmdOpt = processCmdLineOptions(argc, argv);

  if (cmdOpt->debug) {
    for(int currRank = 0; currRank < size; ++currRank)
      if(rank == currRank) printf("rank %d: pid<%d>\n", rank, ::getpid());
    fflush(stdout); 
    MPI_Barrier(comm);
    if (rank == 0) std::cout << "Attach debugger, then press enter to continue\n";
    if (rank == 0) std::cin.get(); 
    MPI_Barrier(comm);
  }
  if (cmdOpt->debug) feraiseexcept(FE_ALL_EXCEPT);

  MPI_Barrier(comm);
  const double time0 = MPI_Wtime();

  std::string cacheDir;
  nekrs::setup(comm, cmdOpt->buildOnly, cmdOpt->sizeTarget,
               cmdOpt->ciMode, cacheDir, cmdOpt->setupFile,
               cmdOpt->backend, cmdOpt->deviceID);

  if (cmdOpt->buildOnly) {
    nekrs::finalize();
    MPI_Finalize();
    return EXIT_SUCCESS;
  }

  MPI_Barrier(comm);
  double elapsedTime = (MPI_Wtime() - time0);

  const int runTimeStatFreq = 500;
  const int updCheckFreq = 20;

  int tStep = 0;
  double time = nekrs::startTime();
  int lastStep = nekrs::lastStep(time, tStep, elapsedTime);

  nekrs::udfExecuteStep(time, tStep, /* outputStep */ 0);

  if (rank == 0 && !lastStep) {
    if (nekrs::endTime() > nekrs::startTime()) 
      std::cout << "\ntimestepping to time " << nekrs::endTime() << " ...\n";
    else
      std::cout << "\ntimestepping for " << nekrs::numSteps() << " steps ...\n";
  }
  MPI_Pcontrol(1);
  while (!lastStep) {
    MPI_Barrier(comm);
    const double timeStart = MPI_Wtime();

    ++tStep;
    lastStep = nekrs::lastStep(time, tStep, elapsedTime);

    double dt; 
    if (lastStep && nekrs::endTime() > 0) 
      dt = nekrs::endTime() - time;
    else
      dt = nekrs::dt(tStep);

    int outputStep = nekrs::outputStep(time+dt, tStep);
    if (nekrs::writeInterval() == 0) outputStep = 0;
    if (lastStep) outputStep = 1;
    if (nekrs::writeInterval() < 0) outputStep = 0;
    nekrs::outputStep(outputStep);

    nekrs::runStep(time, dt, tStep);
    time += dt;

    if (outputStep) nekrs::outfld(time); 

    if (tStep%runTimeStatFreq == 0 || lastStep) nekrs::printRuntimeStatistics(tStep);

    MPI_Barrier(comm);
    elapsedTime += (MPI_Wtime() - timeStart);

    if(tStep%updCheckFreq) nekrs::processUpdFile();
  }
  MPI_Pcontrol(0);

  if (rank == 0) {
    std::cout << "elapsedTime: " << elapsedTime << " s\n";
    std::cout << "End\n";
  }
  fflush(stdout);

  nekrs::finalize();
  MPI_Finalize();
  return EXIT_SUCCESS;
}

static cmdOptions* processCmdLineOptions(int argc, char** argv)
{
  int rank;
  MPI_Comm_rank(comm, &rank);

  cmdOptions* cmdOpt = new cmdOptions();

  int err = 0;
  int printHelp = 0;
  std::string helpCat;

  if (rank == 0) {
    while(1) {
      static struct option long_options[] =
      {
        {"setup", required_argument, 0, 's'},
        {"cimode", required_argument, 0, 'c'},
    	{"build-only", required_argument, 0, 'b'},
        {"debug", no_argument, 0, 'd'},
        {"backend", required_argument, 0, 't'},
        {"device-id", required_argument, 0, 'i'},
        {"help", optional_argument, 0, 'h'},
        {0, 0, 0, 0}
      };
      int option_index = 0;
      int c = getopt_long (argc, argv, "", long_options, &option_index);

      if (c == -1)
        break;

      switch(c) {
      case 's':
        cmdOpt->setupFile.assign(optarg);
        break;
      case 'b':
        cmdOpt->buildOnly = 1;
	    cmdOpt->sizeTarget = atoi(optarg);
        break;
      case 'c':
        cmdOpt->ciMode = atoi(optarg);
        if (cmdOpt->ciMode < 1) {
          std::cout << "ERROR: ci test id has to be >0!\n";
          printHelp = 1;
        }
        break;
      case 'd':
        cmdOpt->debug = 1;
        break;
      case 'i':
        cmdOpt->deviceID.assign(optarg);
        break;
      case 't':
        cmdOpt->backend.assign(optarg);
        break;
      case 'h':
        if(!optarg && argv[optind] != NULL && argv[optind][0] != '-') {
          helpCat.assign(argv[optind++]);
        }
        break;
      default:
        err = 1;
      }
    }
  }

  char buf[FILENAME_MAX];
  strcpy(buf, cmdOpt->setupFile.c_str());
  MPI_Bcast(buf, sizeof(buf), MPI_BYTE, 0, comm);
  cmdOpt->setupFile.assign(buf);

  strcpy(buf, cmdOpt->deviceID.c_str());
  MPI_Bcast(buf, sizeof(buf), MPI_BYTE, 0, comm);
  cmdOpt->deviceID.assign(buf);

  strcpy(buf, cmdOpt->backend.c_str());
  MPI_Bcast(buf, sizeof(buf), MPI_BYTE, 0, comm);
  cmdOpt->backend.assign(buf);

  MPI_Bcast(&cmdOpt->buildOnly, sizeof(cmdOpt->buildOnly), MPI_BYTE, 0, comm);
  MPI_Bcast(&cmdOpt->sizeTarget, sizeof(cmdOpt->sizeTarget), MPI_BYTE, 0, comm);
  MPI_Bcast(&cmdOpt->ciMode, sizeof(cmdOpt->ciMode), MPI_BYTE, 0, comm);
  MPI_Bcast(&cmdOpt->debug, sizeof(cmdOpt->debug), MPI_BYTE, 0, comm);

  if(cmdOpt->setupFile.empty()){
    printHelp++;
  } else {
    std::string casepath, casename;
    size_t last_slash = cmdOpt->setupFile.rfind('/') + 1;
    casepath = cmdOpt->setupFile.substr(0,last_slash);
    chdir(casepath.c_str()); 
    casename = cmdOpt->setupFile.substr(last_slash, cmdOpt->setupFile.length() - last_slash);
    if(casepath.length() > 0) chdir(casepath.c_str());
    cmdOpt->setupFile.assign(casename);
  }

  MPI_Bcast(&printHelp, sizeof(printHelp), MPI_BYTE, 0, comm);
  MPI_Bcast(&err, sizeof(err), MPI_BYTE, 0, comm);
  if (err | printHelp) {
    if (rank == 0) {
      if (helpCat == "par") {
        std::string install_dir;
        install_dir.assign(getenv("NEKRS_HOME"));
        std::ifstream f(install_dir + "/include/parHelp.txt");
        if (f.is_open()) std::cout << f.rdbuf();
        f.close();
      } else {
        std::cout << "usage: ./nekrs [--help <par>] "
                  << "--setup <case name> "
                  << "[ --build-only <#procs> ] [ --cimode <id> ] [ --debug ] "
                  << "[ --backend <CPU|CUDA|HIP|OPENCL> ] [ --device-id <id|LOCAL-RANK> ]"
                 << "\n";
      }
    }
    MPI_Finalize();
    exit((err) ? EXIT_FAILURE : EXIT_SUCCESS);
  }

  return cmdOpt;
}
