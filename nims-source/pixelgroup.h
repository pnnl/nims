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

cv::Point2i idx2pnt(int idx, int mcols);

struct PixelGrouping
{
	int connectivity; // 4-connected or 8-connected neighbors
	int image_width;
	int image_height;
	std::vector< std::vector<int> > pixel_idx; // indices of pixels in group
	
	bool empty() const { return (pixel_idx.empty()); };
	int size() const { return (pixel_idx.size()); };
	int size(int grp) const { return ( grp<pixel_idx.size() ? pixel_idx[grp].size() : -1); };
	cv::Rect bounding_box(int idx_grp) const 
	{
		std::vector<int> x;
		std::vector<int> y;
		for (int i=0;i<pixel_idx[idx_grp].size();++i)
		{
			cv::Point2i pnt = idx2pnt(pixel_idx[idx_grp][i],image_width);
			x.push_back(pnt.x);
			y.push_back(pnt.y);
		}
		cv::Rect bb;
		bb.x = *(std::min_element(x.begin(), x.end()));
		bb.y = *(std::min_element(y.begin(), y.end()));
		bb.width = *(std::max_element(x.begin(), x.end())) - bb.x + 1;
		bb.height = *(std::max_element(y.begin(), y.end())) - bb.y + 1;
		
		return bb;											
	};
};
std::ostream& operator<<(std::ostream& strm, const PixelGrouping& fh);	

// Get a list of the connected pixels in the binary image.
void group_pixels(const cv::InputArray& imb, int min_size, PixelGrouping& groups);


#endif // __NIMS_PIXELGROUP_H__
