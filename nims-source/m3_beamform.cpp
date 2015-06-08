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

#include "m3_beamform.h"

using namespace std;

extern int rangeCompression(const RawData& raw_data,
                            std::complex<float> ref_pulse[],
                            long num_raw_samples,
                            long num_comp_samples,
                            size_t num_elements,
                            float range_kaiser_coeff,
                            std::complex<float> correct_coef[N],
                            ImageData range_compressed);

struct ElementPosition
{
    float xCoordinate;
    float yCoordinate;
};

/*
extern int AzimuthProcess2(echoRangeCompressed(:,options.sub_array),
                           sampleCompStep,
                           elementPositions,
                           settings.beam_list,
                           focalTolerance,
                           systemMinRange,
                           rangeMinActual,
                           MAX_FOCAL_ZONES,
                           MUM_MAX_RANGE,
                           lambda,
                           Focusing, 
                           options.azimuth_kaiser_coeff);
*/
int beamform(const RawData& raw_data, const Settings& settings, const Options& options,
             ImageData* image_data, RangeList* range_list)
{
/*
if ~isfield(options, 'sub_array_center')
    % default -- geometric center of array
    options.sub_array_center = ...
        mean(settings.raw_data_header.Sub_Array_Geometry_XYZ(options.sub_array, :), 1);
end
*/

/*
% This is the data processing algorithm for the raw data of the M3.
% 1. starting from raw data, then processed by the range compression,
% azimuth process (beamformer with dymanic focusing).
% 2. It is an equivalent Matlab version to the M3 hostsoftware. 
*/
// TODO:  Probably don't need to copy.
Header header = settings.raw_data_header;

//[M N] = size(rawData);
if ( N != header.Num_Elements )
{
    cerr << "Raw data dimension does not match number of elements" << endl;
    return -1;
}

    // TODO:  Make sure float is sufficient precision.
float rangeMin = header.Min_Range;
float rangeMax = header.Max_Range;
float c        = header.Velocity_Sound;
float fCarrier = header.Sub_Array_Frequency;
float lambda   = c/fCarrier;

float sampleRawInterval = header.Raw_Sample_Interval; //% in seconds
float sampleWindowStartTime = header.Sampling_Window_Start_Time; //%2*rangeMin/c //% in seconds

//% number of raw samples in the sampling window
long numRawSamples = header.End_Raw_Sample-header.Start_Raw_Sample+1;

size_t numElements=header.Num_Elements;


/*
% % actual number of processed samples in window length
% 
% % note that without deliberate interpolation in the range processor, that
% % the sampling interval in the processed data is the same as that in the
% % raw data
% 
*/
float sampleCompInterval = sampleRawInterval;
long numCompSamples=numRawSamples-sizeof(settings.ref_pulse); // should be length of refPulse

    std::complex<float> correctionCoef[N] = { 1 };
if ( options.apply_rx_corrns==1 )
    for (int i=0; i<N; ++i)
        correctionCoef[i] = settings.rx_corrns[i];
 

clog << "Range Processing ..." << endl;
    
ImageData echoRangeCompressed;
rangeCompression(raw_data,
                settings.ref_pulse,
                numRawSamples,
                numCompSamples,
                numElements,
                options.range_kaiser_coeff,
                correctionCoef,
                echoRangeCompressed);
/*
%******************************************************************%
%******************************************************************%

if options.display_plots == 2
    % Display the range-compressed data
    figure;
    imagesc(abs(echoRangeCompressed));
    title('Range-compressed data amplitude');
    set(gca, 'XDir', 'reverse', 'YDir', 'normal');
end

%******************************************************************%
%******************************************************************%
*/
//% perform Azimuth process (include beamforming and dynamic focusing)
float systemMinRange = header.System_Min_SWST*header.Velocity_Sound/2;
float focalTolerance = header.Focal_Tolerance; //%the default setup is 22.5.
//%focalTolerance = 45; %for testing
float sampleCompStep = c*sampleCompInterval/2;
float rangeMinActual=(sampleWindowStartTime-0.000025)*c/2; //%the actual sampling range is closer than the min range in the setup
//% here 25us is TXWST, tx window start time, the acoustic signal start after
//% TXWST, so the rangle of the first sample should be calculated in this
//% way, which is always smaller than 0.2.
for (int i=0; i<numCompSamples; ++i)
{
    range_list[i] = rangeMinActual + i*sampleCompStep;
}

if ( options.azimuth_process == 1 )
{
    int MUM_MAX_RANGE=500;
    int MAX_FOCAL_ZONES=128;
    int Focusing=1;  //%%%%%%%1 or 0 for dynamic focusing
    
    // only used for azimuth processing ?
    //% array parameters for linear array
    ElementPosition elementPositions[N];
    for (int i; i<N; ++i)
    {
        elementPositions[i].xCoordinate =
        header.Sub_Array_Geometry_XYZ[i][0] - options.sub_array_center[0];
        elementPositions[i].yCoordinate =  //% previously = 0.039370?
        header.Sub_Array_Geometry_XYZ[i][1] - options.sub_array_center[1];
    }
    
    clog << "Azimuth Processing ..." << endl;
/*    image_data = AzimuthProcess2(echoRangeCompressed(:,options.sub_array), ...
                                 sampleCompStep, ...
                                 elementPositions, ...
                                 settings.beam_list, ...
                                 focalTolerance, ...
                                 systemMinRange, ...
                                 rangeMinActual, ...
                                 MAX_FOCAL_ZONES, ...
                                 MUM_MAX_RANGE, ...
                                 lambda, ...
                                 Focusing, ...
                                 options.azimuth_kaiser_coeff);
*/
} else
{
    image_data = echoRangeCompressed;
    clog << "Azimuth Processing OFF" << endl;
}
    
    return 0;
}
