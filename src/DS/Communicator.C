#include <iostream>
#include <vector>
#include <mpi.h>

#include "Cache.h"
#include "Communicator.h"
#include "DecompositionManager.h"
#include "DSlib.h"
#include "LPT_LogOutput.h"

namespace DSlib
{
  int Isend(double *buf, int count, int dest, int tag, MPI_Comm comm, MPI_Request * request);
  int Isend(float *buf, int count, int dest, int tag, MPI_Comm comm, MPI_Request * request);
  int Irecv(double *buf, int count, int source, int tag, MPI_Comm comm, MPI_Request * request);
  int Irecv(float *buf, int count, int source, int tag, MPI_Comm comm, MPI_Request * request);

  int Isend(double *buf, int count, int dest, int tag, MPI_Comm comm, MPI_Request * request)
  {
    return (MPI_Isend(buf, count, MPI_DOUBLE, dest, tag, comm, request));
  }
  int Isend(float *buf, int count, int dest, int tag, MPI_Comm comm, MPI_Request * request)
  {
    return (MPI_Isend(buf, count, MPI_FLOAT, dest, tag, comm, request));
  }
  int Irecv(double *buf, int count, int source, int tag, MPI_Comm comm, MPI_Request * request)
  {
    return (MPI_Irecv(buf, count, MPI_DOUBLE, source, tag, comm, request));
  }
  int Irecv(float *buf, int count, int source, int tag, MPI_Comm comm, MPI_Request * request)
  {
    return (MPI_Irecv(buf, count, MPI_FLOAT, source, tag, comm, request));
  }

  void Communicator::CommRequestsAlltoall()
  {
    int ierr = MPI_Alltoall(SendRequestCounts, 1, MPI_INT, RecvRequestCounts, 1, MPI_INT, Comm);

    if(ierr != MPI_SUCCESS)
      LPT::LPT_LOG::GetInstance()->ERROR("MPI_Alltoall returns error in DSlib::Communicator::CommRequestsAlltoall() ierr = ", ierr);
  }

  void Communicator::CommRequest(DSlib * ptrDSlib)
  {
    int tag = 0;
    MPI_Request *array_of_requests = new MPI_Request[NumProcs];
    MPI_Request *array_of_requests2 = new MPI_Request[NumProcs];

    for(int rank = 0; rank < NumProcs; rank++) {
      RequestedBlockIDs.at(rank)->resize(RecvRequestCounts[rank]);
    }

    //粒子プロセスから流体プロセスに必要なデータブロックのID配列(RequestID)を送る
    //prepare to recieve P2P request
    for(int rank = 0; rank < NumProcs; rank++) {
      int src = (MyRank + rank) >= NumProcs ? (MyRank + rank) - NumProcs : (MyRank + rank);

      if(RecvRequestCounts[src] > 0) {
        int ierr = MPI_Irecv(&*(RequestedBlockIDs.at(src)->begin()), RecvRequestCounts[src], MPI_LONG, src, tag, Comm, &array_of_requests[src]);
      }
    }
    //Send P2P request
    for(int rank = 0; rank < NumProcs; rank++) {
      int dest = (MyRank - rank) < 0 ? (MyRank - rank) + NumProcs : (MyRank - rank);

      if(SendRequestCounts[dest] > 0) {
        int ierr = MPI_Isend(&*(ptrDSlib->RequestQueues.at(dest)->begin()), SendRequestCounts[dest], MPI_LONG, dest, tag, Comm, &array_of_requests2[dest]);
      }
    }

    MPI_Status *array_of_statuses = new MPI_Status[NumProcs];

    for(int rank = 0; rank < NumProcs; rank++) {
      int src = (MyRank + rank) >= NumProcs ? (MyRank + rank) - NumProcs : (MyRank + rank);

      if(RecvRequestCounts[src] > 0) {
        int ierr = MPI_Wait(&array_of_requests[src], &array_of_statuses[src]);
      }
    }

    for(int rank = 0; rank < NumProcs; rank++) {
      LPT::LPT_LOG::GetInstance()->INFO("rank = ", rank);
      if(ptrDSlib->RequestQueues.at(rank)->size() != 0) {
        LPT::LPT_LOG::GetInstance()->INFO("Request IDs = ", &*(ptrDSlib->RequestQueues.at(rank)->begin()), ptrDSlib->RequestQueues.at(rank)->size());
      }
    }

/// @attention  MPI_Waitallすると受信数が0の相手に対してIrecvを発行していないので、エラーになる
///  MPI_Waitall(NumProcs, array_of_requests, array_of_statuses);
    delete[]array_of_requests;
    delete[]array_of_requests2;
    delete[]array_of_statuses;
  }

  void Communicator::CommDataP2F()
  {
  }

  void Communicator::CommDataF2P(REAL_TYPE * Data, REAL_TYPE * v00, int *Mask, const int &vlen, DecompositionManager * ptrDM, std::list < CommDataBlockManager * >*SendBuff, std::list < CommDataBlockManager * >*RecvBuff)
  {
    //流体プロセスから粒子プロセスに要求されたデータブロックを1つづつ送る
    //prepare to recieve Datablocks
    int count = vlen * ptrDM->GetLargestBlockSize();

    int RecvBuffMemSize = 0;

    for(int rank = 0; rank < NumProcs; rank++) {
      int src = (MyRank + rank) >= NumProcs ? (MyRank + rank) - NumProcs : (MyRank + rank);
      int tag = 0;

      for(int i = 0; i < SendRequestCounts[src]; i++) {
        CommDataBlockManager *tmp = new CommDataBlockManager;

        tmp->Buff = new REAL_TYPE[count];
        tmp->Header = new CommDataBlockHeader;

        RecvBuffMemSize += count;
        int ierr = Irecv(tmp->Buff, count, src, tag++, Comm, &(tmp->Request0));

        if(ierr != MPI_SUCCESS)
          LPT::LPT_LOG::GetInstance()->ERROR("return value from Irecv = ", ierr);
        int ierr2 = MPI_Irecv(tmp->Header, 1, MPI_DataBlockHeader, src, tag++, Comm, &(tmp->Request1));

        if(ierr2 != MPI_SUCCESS)
          LPT::LPT_LOG::GetInstance()->ERROR("return value from MPI_Irecv = ", ierr2);
        RecvBuff->push_back(tmp);
      }
    }
    RecvBuffMemSize *= sizeof(REAL_TYPE);
    LPT::LPT_LOG::GetInstance()->LOG("Memory size for Recv Buffer = ", RecvBuffMemSize);

    //Send Datablocks
    int SendBuffMemSize = 0;

    for(int rank = 0; rank < NumProcs; rank++) {
      int dest = (MyRank - rank) < 0 ? (MyRank - rank) + NumProcs : (MyRank - rank);
      int tag = 0;

      for(int i = 0; i < RecvRequestCounts[dest]; i++) {
        CommDataBlockManager *tmp = new CommDataBlockManager;

        tmp->Buff = new REAL_TYPE[count];
        tmp->Header = new CommDataBlockHeader;
        int SendSize;

        CommPacking(RequestedBlockIDs.at(dest)->at(i), Data, Mask, vlen, tmp->Buff, tmp->Header, &SendSize, MyRank);

        SendBuffMemSize += count;
        int ierr = Isend(tmp->Buff, SendSize, dest, tag++, Comm, &(tmp->Request0));

        if(ierr != MPI_SUCCESS)
          LPT::LPT_LOG::GetInstance()->ERROR("return value from Isend = ", ierr);
        int ierr2 = MPI_Isend(tmp->Header, 1, MPI_DataBlockHeader, dest, tag++, Comm, &(tmp->Request1));

        if(ierr2 != MPI_SUCCESS)
          LPT::LPT_LOG::GetInstance()->ERROR("return value from MPI_Isend = ", ierr2);
        SendBuff->push_back(tmp);
      }
    }

    SendBuffMemSize *= sizeof(REAL_TYPE);
    LPT::LPT_LOG::GetInstance()->LOG("Memory size for Send Buffer = ", RecvBuffMemSize);
  }

  void Communicator::MakeSendRequestCounts(const int &SubDomainID, DSlib * ptrDSlib)
  {
    SendRequestCounts[SubDomainID] = ptrDSlib->CountRequestQueues(SubDomainID);
  }

  void Communicator::CommPacking(const long &BlockID, REAL_TYPE * Data, int *Mask, const int &vlen, REAL_TYPE * SendBuff, CommDataBlockHeader * Header, int *SendSize, const int &MyRank)
  {
    DecompositionManager *ptrDM = DecompositionManager::GetInstance();
    int halo = ptrDM->GetGuideCellSize();
    int SubDomainSize[3];

    SubDomainSize[0] = ptrDM->GetSubDomainSizeX(MyRank) + 2 * ptrDM->GetGuideCellSize();
    SubDomainSize[1] = ptrDM->GetSubDomainSizeY(MyRank) + 2 * ptrDM->GetGuideCellSize();
    SubDomainSize[2] = ptrDM->GetSubDomainSizeZ(MyRank) + 2 * ptrDM->GetGuideCellSize();

    Header->BlockID = BlockID;
    Header->SubDomainID = MyRank;
    Header->BlockSize[0] = ptrDM->GetBlockSizeX(BlockID) + 2 * ptrDM->GetGuideCellSize();
    Header->BlockSize[1] = ptrDM->GetBlockSizeY(BlockID) + 2 * ptrDM->GetGuideCellSize();
    Header->BlockSize[2] = ptrDM->GetBlockSizeZ(BlockID) + 2 * ptrDM->GetGuideCellSize();
    Header->Pitch[0] = ptrDM->Getdx();
    Header->Pitch[1] = ptrDM->Getdy();
    Header->Pitch[2] = ptrDM->Getdz();
    Header->Origin[0] = ptrDM->GetBlockOriginX(BlockID);
    Header->Origin[1] = ptrDM->GetBlockOriginY(BlockID);
    Header->Origin[2] = ptrDM->GetBlockOriginZ(BlockID);
    Header->OriginCell[0] = ptrDM->GetBlockOriginCellX(BlockID);
    Header->OriginCell[1] = ptrDM->GetBlockOriginCellY(BlockID);
    Header->OriginCell[2] = ptrDM->GetBlockOriginCellZ(BlockID);

    int BlockLocalOffset = ptrDM->GetBlockLocalOffset(BlockID, MyRank);

    int indexS = 0;

    // 袖領域も含めて転送する
    for(int i = 0; i < vlen; i++) {
      for(int l = 0; l < Header->BlockSize[2]; l++) {
        for(int k = 0; k < Header->BlockSize[1]; k++) {
#pragma ivdep
          for(int j = 0; j < Header->BlockSize[0]; j++) {
            SendBuff[indexS + j] = Data[BlockLocalOffset + ptrDM->Convert4Dto1D(j, k, l, i, SubDomainSize[0], SubDomainSize[1], SubDomainSize[2])] * Mask[BlockLocalOffset + ptrDM->Convert3Dto1D(j, k, l, SubDomainSize[0], SubDomainSize[1])];
          }
          indexS += Header->BlockSize[0];
        }
      }
    }

    *SendSize = indexS;

  }

} // namespace DSlib