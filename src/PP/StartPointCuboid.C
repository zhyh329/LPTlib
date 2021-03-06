/*
 * LPTlib
 * Lagrangian Particle Tracking library
 *
 * Copyright (c) 2012-2014 Advanced Institute for Computational Science, RIKEN.
 * All rights reserved.
 *
 */

#include "StartPointCuboid.h"
#include "LPT_LogOutput.h"
#include "Utility.h"

namespace PPlib
{
std::string Cuboid::TextPrint(const REAL_TYPE& RefLength, const double& RefTime) const
{
    std::ostringstream oss;
    oss<<"Cuboid"<<std::endl;
    oss<<"Coord1               = "<<this->Coord1[0]*RefLength<<","<<this->Coord1[1]*RefLength<<","<<this->Coord1[2]*RefLength<<std::endl;
    oss<<"Coord2               = "<<this->Coord2[0]*RefLength<<","<<this->Coord2[1]*RefLength<<","<<this->Coord2[2]*RefLength<<std::endl;
    oss<<"SumStartPoints       = "<<this->SumStartPoints<<std::endl;
    oss<<"NumStartPoints       = "<<this->NumStartPoints[0]<<","<<this->NumStartPoints[1]<<","<<this->NumStartPoints[2]<<std::endl;
    oss<<this->PrintTimeAndID(RefTime);
    return oss.str();
}

void Cuboid::ReadText(std::istream& stream, const REAL_TYPE& RefLength, const double& RefTime)
{
    std::string work;
    //Coord1
    std::getline(stream, work, '=');
    std::getline(stream, work, ',');
    this->Coord1[0] = std::atof(work.c_str())/RefLength;
    std::getline(stream, work, ',');
    this->Coord1[1] = std::atof(work.c_str())/RefLength;
    std::getline(stream, work);
    this->Coord1[2] = std::atof(work.c_str())/RefLength;
    //Coord2
    std::getline(stream, work, '=');
    std::getline(stream, work, ',');
    this->Coord2[0] = std::atof(work.c_str())/RefLength;
    std::getline(stream, work, ',');
    this->Coord2[1] = std::atof(work.c_str())/RefLength;
    std::getline(stream, work);
    this->Coord2[2] = std::atof(work.c_str())/RefLength;

    //SumStartPoints
    std::getline(stream, work, '=');
    std::getline(stream, work);
    this->SumStartPoints = std::atoi(work.c_str());

    //NumStartPoints
    std::getline(stream, work, '=');
    std::getline(stream, work, ',');
    this->NumStartPoints[0] = std::atof(work.c_str());
    std::getline(stream, work, ',');
    this->NumStartPoints[1] = std::atof(work.c_str());
    std::getline(stream, work);
    this->NumStartPoints[2] = std::atof(work.c_str());

    this->ReadTimeAndID(stream, RefTime);
}

void Cuboid::ShrinkX(std::vector<StartPoint*>* StartPoints, int* N, const int& NB, const std::vector<REAL_TYPE>& coord_x)
{
    if(*N%NB != 0)
    {
        //x方向にN%NBだけ縮める
        int RemN = *N%NB;
        *N                     -= RemN;
        this->NumStartPoints[0] = *N;
        this->Coord2[0]         = coord_x[*N];

        //x方向の余り領域をCuboidのオブジェクトとして生成し、StartPointsに格納
        REAL_TYPE NReminderCoord1[3]         = {coord_x[*N+1], this->Coord1[1], this->Coord1[2]};
        int       NReminderNumStartPoints[3] = {RemN, NumStartPoints[1], NumStartPoints[2]};
        StartPoints->push_back(CuboidFactory(NReminderCoord1, this->Coord2, NReminderNumStartPoints, StartTime, ReleaseTime, TimeSpan, ParticleLifeTime));
    }
}

void Cuboid::ShrinkY(std::vector<StartPoint*>* StartPoints, int* M, const int& MB, const std::vector<REAL_TYPE>& coord_y)
{
    if(*M%MB != 0)
    {
        //y方向に*M%MBだけ縮める
        int RemM = *M%MB;
        *M                     -= RemM;
        this->NumStartPoints[1] = *M;
        this->Coord2[1]         = coord_y[*M];

        //y方向の余り領域をCuboidのオブジェクトとして生成し、StartPointsに格納
        REAL_TYPE MReminderCoord1[3]         = {this->Coord1[0], coord_y[*M+1], this->Coord1[2]};
        int       MReminderNumStartPoints[3] = {NumStartPoints[0], RemM, NumStartPoints[2]};
        StartPoints->push_back(CuboidFactory(MReminderCoord1, this->Coord2, MReminderNumStartPoints, StartTime, ReleaseTime, TimeSpan, ParticleLifeTime));
    }
}

void Cuboid::ShrinkZ(std::vector<StartPoint*>* StartPoints, int* K, const int& KB, const std::vector<REAL_TYPE>& coord_z)
{
    if(*K%KB != 0)
    {
        //z方向にK%KBだけ縮める
        int RemK = *K%KB;
        *K                     -= RemK;
        this->NumStartPoints[2] = *K;
        this->Coord2[1]         = coord_z[*K];
        //z方向の余り領域をCuboidのオブジェクトとして生成し、StartPointsに格納
        REAL_TYPE KReminderCoord1[3]         = {this->Coord1[0], this->Coord1[1], coord_z[*K]};
        int       KReminderNumStartPoints[3] = {NumStartPoints[0], NumStartPoints[1], RemK};
        StartPoints->push_back(CuboidFactory(KReminderCoord1, this->Coord2, KReminderNumStartPoints, StartTime, ReleaseTime, TimeSpan, ParticleLifeTime));
    }
}

void Cuboid::Divider(std::vector<StartPoint*>* StartPoints, const int& MaxNumStartPoints)
{
    //オブジェクトの開始点数が引数で指定された数以下の場合は、自分自身のコピーを格納したvectorを返す
    if(MaxNumStartPoints >= GetSumStartPoints())
    {
        Cuboid* NewCuboid = CuboidFactory(this->Coord1, this->Coord2, this->NumStartPoints, this->StartTime, this->ReleaseTime, this->TimeSpan, this->ParticleLifeTime);
        StartPoints->push_back(NewCuboid);
        return;
    }else if(MaxNumStartPoints <= 0){
        // MaxNumStartPointsが0以下の時はエラーメッセージを出力して終了
        LPT::LPT_LOG::GetInstance()->WARN("illegal MaxNumStartPoints. this StartPointSetting will be deleted ", this);
        return;
    }

    int N = NumStartPoints[0];
    int M = NumStartPoints[1];
    int K = NumStartPoints[2];

    //各方向の分割後のブロックサイズ(NB, MB, KB)を決める
    int NB = -1;
    int MB = -1;
    int KB = -1;
    utility::DetermineBlockSize(&NB, &MB, &KB, MaxNumStartPoints, N, M, K);
    LPT::LPT_LOG::GetInstance()->INFO("MaxNumStartPoints=", MaxNumStartPoints);
    LPT::LPT_LOG::GetInstance()->INFO("N=", N);
    LPT::LPT_LOG::GetInstance()->INFO("M=", M);
    LPT::LPT_LOG::GetInstance()->INFO("K=", K);
    LPT::LPT_LOG::GetInstance()->INFO("NB=", NB);
    LPT::LPT_LOG::GetInstance()->INFO("MB=", MB);
    LPT::LPT_LOG::GetInstance()->INFO("KB=", KB);

    //元の領域の格子点座標を各軸毎に取得
    std::vector<REAL_TYPE> coord_x;
    utility::DivideLine1D(&coord_x, NumStartPoints[0], Coord1[0], Coord2[0]);
    std::vector<REAL_TYPE> coord_y;
    utility::DivideLine1D(&coord_y, NumStartPoints[1], Coord1[1], Coord2[1]);
    std::vector<REAL_TYPE> coord_z;
    utility::DivideLine1D(&coord_z, NumStartPoints[2], Coord1[2], Coord2[2]);

    //座標をCoord1の方が小さくなるように書き換え
    //京でfront(), back()が使えなかったのでbegin, rbeginをdereferenceして使っている
    Coord1[0] = *coord_x.begin();
    Coord1[1] = *coord_y.begin();
    Coord1[2] = *coord_z.begin();
    Coord2[0] = *coord_x.rbegin();
    Coord2[1] = *coord_y.rbegin();
    Coord2[2] = *coord_z.rbegin();

    //各方向をNB, MB, KBの倍数になるように縮小する
    ShrinkX(StartPoints, &N, NB, coord_x);
    ShrinkY(StartPoints, &M, MB, coord_y);
    ShrinkZ(StartPoints, &K, KB, coord_z);

    //残った部分をNB*MB*KBづつの小領域に分割する
    int NewNumStartPoints[3] = {NB, MB, KB};
    for(std::vector<REAL_TYPE>::iterator it_z = coord_z.begin()+(KB-1); it_z < coord_z.end(); it_z += KB)
    {
        for(std::vector<REAL_TYPE>::iterator it_y = coord_y.begin()+(MB-1); it_y < coord_y.end(); it_y += MB)
        {
            for(std::vector<REAL_TYPE>::iterator it_x = coord_x.begin()+(NB-1); it_x < coord_x.end(); it_x += NB)
            {
                REAL_TYPE Coord1[3] = {*(it_x-NB+1), *(it_y-MB+1), *(it_z-KB+1)};
                REAL_TYPE Coord2[3] = {*it_x, *it_y, *it_z};
                StartPoints->push_back(CuboidFactory(Coord1, Coord2, NewNumStartPoints, this->StartTime, this->ReleaseTime, this->TimeSpan, this->ParticleLifeTime));
            }
        }
    }
}

void Cuboid::GetGridPointCoord(std::vector<REAL_TYPE>& Coords)
{
    std::vector<REAL_TYPE> coord_x;
    utility::DivideLine1D(&coord_x, NumStartPoints[0], Coord1[0], Coord2[0]);
    std::vector<REAL_TYPE> coord_y;
    utility::DivideLine1D(&coord_y, NumStartPoints[1], Coord1[1], Coord2[1]);
    std::vector<REAL_TYPE> coord_z;
    utility::DivideLine1D(&coord_z, NumStartPoints[2], Coord1[2], Coord2[2]);

    for(std::vector<REAL_TYPE>::iterator it_x = coord_x.begin(); it_x != coord_x.end(); ++it_x)
    {
        for(std::vector<REAL_TYPE>::iterator it_y = coord_y.begin(); it_y != coord_y.end(); ++it_y)
        {
            for(std::vector<REAL_TYPE>::iterator it_z = coord_z.begin(); it_z != coord_z.end(); ++it_z)
            {
                Coords.push_back(*it_x);
                Coords.push_back(*it_y);
                Coords.push_back(*it_z);
            }
        }
    }
    LPT::LPT_LOG::GetInstance()->LOG("Number of grid points = ", Coords.size()/3);
}
} // namespace PPlib