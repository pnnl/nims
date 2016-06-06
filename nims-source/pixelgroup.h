/*
 *  pixelgroup.h
 *   *
 *  Created by Shari Matzner on 6/18/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef __NIMS_PIXELGROUP_H__
#define __NIMS_PIXELGROUP_H__

#include <ostream>
#include <vector>
#include <opencv2/opencv.hpp>

// NOTE:  These functions assume that matrices
//        are stored (0,0) (0,1) (0,2) ... (0,M-1) (1,0) (1,1) ... (N-1,M-1)
//        where M is the number of columns (width) and 
//        N is the number of rows (height).

//TODO: Add flag for row-wise or column-wise indexing
//cv::Point2i idx2pnt(int idx, int mcols);
//int pnt2idx(cv::Point2i pnt, int mcols);

struct Blob
{
	std::vector<cv::Point2i > points;
	std::vector<float>  intensity;

	Blob()
	{

	};

	Blob(std::vector<cv::Point2i> pnts, std::vector<float> I)
	{
		// TODO: make sure vectors are the same size.
		points = pnts;
		intensity = I;
	};
};

typedef  std::vector< Blob > PixelGrouping;
std::ostream& operator<<(std::ostream& strm, const PixelGrouping& pg);

// Get a list of the connected pixels in the binary image.
void group_pixels(const cv::InputArray& im, const cv::InputArray& mask, int min_size, PixelGrouping& blobs);


#endif // __NIMS_PIXELGROUP_H__
