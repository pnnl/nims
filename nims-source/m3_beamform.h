/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  m3_beamform.h
 *  
 *  Created by Shari Matzner on 06/06/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef __NIMS_M3_BEAMFORM_H__
#define __NIMS_M3_BEAMFORM_H__

/*
 % BEAMFORM   Beamforms raw data into complex image data.
 %    [IMAGE_DATA,RANGE_LIST] = BEAMFORM(RAW_DATA,SETTINGS,OPTIONS)
 %    carries out beamforming on a single matrix of raw data samples and
 %    returns a matrix of complex (i.e., undetected) samples.
 %
 %    RAW_DATA is an MxN complex matrix of raw data (M range cells; N
 %    elements).
 %
 %    SETTINGS is a structure with fields:
 %       .beam_list        [vector length P of beam angles in degrees, +ve
 %                          clockwise from broadside]
 %       .ref_pulse        [R x 1 complex vector]
 %       .raw_data_header: [header associated with raw_data]
 %       .rx_corrns        [N x 1 complex vector]
 %
 %    OPTIONS is a structure with optional fields:
 %       .azimuth_process  [0 or 1, default 1 (on)]
 %       .apply_rx_corrns  [0 or 1, default 1 (on)]
 %       .range_kaiser_coeff   [default 1.7]
 %       .azimuth_kaiser_coeff [default 1.7]
 %       .sub_array        [vector of elements to use, default 1:N]
 %       .sub_array_center [sub array {x,y,z} center in meters,
 %                          default = {0,0,0}]
 %       .display_plots    [{0,1,2} plotting level: 0=no plots,
 %                                   2=all plots, default 1 (=some plots)]
 %       .title_string     [default '']
 %       .verbose          [0 or 1, default 1 (on)]
 %
 %    IMAGE_DATA is a QxN complex matrix of beamformed data.
 %
 %    RANGE_LIST is a 1xQ vector of ranges corresponding to the rows in
 %    the IMAGE_DATA matrix.
 %
 %    M3 Matlab Toolbox 1.0
 %    Copyright (C) 2013   Kongsberg Mesotech Ltd.
 %
 %    See also M3TOOLBOX, BEAMLISTGENERATION, PLOT_BEAMFORMED_DATA
 */

#include <complex>
#include <iostream>

const std::size_t M = 500; // number of range cells
const std::size_t N = 128; // number of elements
const std::size_t P = 128; // number of beams
const std::size_t Q = 500; // number of image rows (ranges)
const std::size_t R = 500; // length of ref pulse


typedef std::complex<float> RawData[M][N];

struct Header
{
    std::size_t Num_Elements;
    float Min_Range;
    float Max_Range;
    float Velocity_Sound;
    float Sub_Array_Frequency;
    float Raw_Sample_Interval;
    float Sampling_Window_Start_Time;
    long Start_Raw_Sample;
    long End_Raw_Sample;
    float Sub_Array_Geometry_XYZ[N][3];
    float System_Min_SWST;
    float Focal_Tolerance;
};

struct Settings
{
    float beam_list[P];
    std::complex<float> ref_pulse[R];
    Header raw_data_header;
    std::complex<float> rx_corrns[N];
    
}; // Settings

struct Options
{
    int azimuth_process;
    int apply_rx_corrns;
    float range_kaiser_coeff;
    float azimuth_kaiser_coeff;
    bool sub_array[N];
    float sub_array_center[3];
    
    Options() // defaults
    : sub_array{ true }, sub_array_center{ 0.0, 0.0, 0.0 }
    {
        azimuth_process = 1;
        apply_rx_corrns = 1;
        range_kaiser_coeff = 1.7;
        azimuth_kaiser_coeff = 1.7;
        //sub_array = { true };
        //sub_array_center = { 0.0, 0.0, 0.0 };
    };
    
}; // Options

typedef std::complex<float> ImageData[Q][N];
typedef float RangeList[Q];

int beamform(const RawData& raw_data, const Settings& settings, const Options& options,
             ImageData* image_data, RangeList* range_list);

#endif // __NIMS_M3_BEAMFORM_H__
