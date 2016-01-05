/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  test_pixelgroup.cpp
 *  
 *  Created by Shari Matzner on 11/04/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#include <iostream> // cout, cin, cerr
#include <string>   // for strings
//#include <opencv2/opencv.hpp>

#include "pixelgroup.h"

using namespace std;
//using namespace cv;


int main (int argc, char * const argv[]) {
	//--------------------------------------------------------------------------
    // PARSE COMMAND LINE
	//
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
  	

PixelGrouping pg;

cout << pg << endl;

cout << "Pixelgrouping is empty: " << pg.empty() << endl;
cout << "Pixelgrouping size is " << pg.size() << endl;

// Get a list of the connected pixels in the binary image.
void group_pixels(const cv::InputArray& imb, int min_size, PixelGrouping& groups);

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

	   
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
