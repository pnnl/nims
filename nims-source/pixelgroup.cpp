/*
 *  pixelgroup.cpp
 *  FIshVideoTracker
 *
 *  Created by Shari Matzner on 3/26/13.
 *  Copyright 2013 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

//#include <c++/4.2.1/tr1/unordered_set>
#include <unordered_set>
#include <stack>

#include "pixelgroup.h"

using namespace cv;
using namespace std;
using std::unordered_set;

// TODO:  Make array versions of these functions (indx2pnt and pnt2idx).
Point2i idx2pnt(int idx, int mcols) {
	Point2i pnt(-1,-1);
	
	pnt.y = floor(idx/mcols);
	pnt.x = idx - pnt.y*mcols;
	
	return(pnt);
}

int pnt2idx(Point2i pnt, int mcols) {
	int idx = -1;
	
	idx = pnt.y*mcols + pnt.x;
	return(idx);
}

// Similar to Matlab find function
// src must be a logical array, matrix, or vector.
// dst is a vector of indices of the nonzero elements
// of src.

long find(InputArray src, vector<Point2i>& out) {

	// input must be binary image of unsigned char
 	Mat matsrc = src.getMat();
   CV_Assert( matsrc.type() == CV_8UC1 ); 
	
	long Nfound = 0;
	Size matsize = matsrc.size();
	out.clear();
	
	
	for (int i=0; i<matsize.height; i++) // y
	{
		uchar *dat = matsrc.ptr(i);
		for (int j=0; j<matsize.width; ++j) // x
		{
			if ( dat[j] )
			{
				out.push_back(Point2i(j,i));
				++Nfound;
			}
		}
	}
	return(Nfound);
}



void getNeighbors(Point2i pnt, Size2i sz, vector<Point2i>& out) {
	out.clear();
	
	if (pnt.x > 0)
	{
		if (pnt.y > 0) out.push_back(Point(pnt.x-1,pnt.y-1));
		out.push_back(Point(pnt.x-1,pnt.y));
		if ((pnt.y+1) < sz.height) out.push_back(Point(pnt.x-1,pnt.y+1));
	}

	if (pnt.y > 0) out.push_back(Point(pnt.x,pnt.y-1));
	if ((pnt.y+1) < sz.height) out.push_back(Point(pnt.x,pnt.y+1));

	if ((pnt.x+1) < sz.width)
	{
		if (pnt.y > 0) out.push_back(Point(pnt.x+1,pnt.y-1));
		out.push_back(Point(pnt.x+1,pnt.y));
		if ((pnt.y+1) < sz.height) out.push_back(Point(pnt.x+1,pnt.y+1));		
	}
}


std::ostream& operator<<(std::ostream& strm, const PixelGrouping& pg)
{
	int n = pg.size();
	strm << "number of groups: " << n << endl;
	for (int j=0;j<n;++j)
	{
		int nj = pg[j].points.size();
		cout << "group " << j << ": " << nj << " pixels" << endl;
		strm << "     ";
		for (int k=0;k<nj;++k)
		{
			strm << "(" << pg[j].points[k].x << ", " << pg[j].points[k].y << ") "; 
		} // for each pixel in group
		strm << endl;
	} // for each group
	
	return strm;
}

// TODO:  TEST this function!
// Get a list of the connected pixels in the binary image.
void group_pixels(const cv::InputArray& im, const cv::InputArray& mask, int min_size, PixelGrouping& blobs)
{

	Mat matim = im.getMat();
	CV_Assert( matim.type() == CV_32FC1 );

	Mat matmask = mask.getMat();
    CV_Assert( matmask.type() == CV_8UC1 ); // mask must be binary image
	CV_Assert( matmask.size() == matim.size() );

Size2i im_sz = matim.size();

	blobs.clear();
	// TODO:  Check for 2D or 3D, maybe template
	vector<Point2i> pixList;
	long Npix = find(matmask, pixList);

	if (Npix==0) return;

	vector<Point2i>  neighbors;     // list of pixel neighbors
	Point2i          pix;           // pixel index
	
	while (!pixList.empty()) // while ungrouped pixels
	{
		if (matmask.at<unsigned char>(*pixList.begin()) == 0)
		{
			pixList.erase(pixList.begin());
			continue;
		}
		stack<Point2i>   pixReady;  // set of candidate pixels
		vector<Point2i>  groupPix;  // pixels in group
		
		// Pick an initial pixel, mark it as found and put it in the Ready list.
		pixReady.push(*pixList.begin());
		pixList.erase(pixList.begin());
						
		while (!pixReady.empty())
		{
			//cout << "pixels ready: " << pixReady.size() << endl;
			pix = pixReady.top();  // pick a pixel in the Ready list
			pixReady.pop();         // remove it from Ready
			//cout << "grouping pixel " << ipix << endl;
			
			// add it to the current group
			groupPix.push_back(pix);
			
			// find all its neighbors
			getNeighbors(pix, im_sz, neighbors);
			//cout << "neighbors (" << ineighbors.size() << "):" << endl;
			for (int j=0;j<neighbors.size();++j)
			{
				if (matmask.at<unsigned char>(neighbors[j])>0 )
				{
					// mark neighbor as found
					matmask.at<unsigned char>(neighbors[j]) = 0;
					// add neighbor to the Ready list
					pixReady.push(neighbors[j]);
				} // if neighbor valid
			} // for each neighbor
		} // pixels ready
		// save group
		if (groupPix.size() >= min_size)
		{
			vector<float> intensity;
		    for (int p=0;p<groupPix.size();++p)
		    	intensity.push_back(matim.at<float>(groupPix[p]));
		    blobs.push_back(Blob(groupPix,intensity));
		}
	} // ungrouped pixels
	
}
