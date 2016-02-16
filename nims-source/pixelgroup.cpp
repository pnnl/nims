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
long find(InputArray src, vector<unsigned>& out) {
	
	long Nfound = 0;
	Mat matsrc;
	src.getMat().convertTo(matsrc, CV_8U); // get Mat headers
	Size matsize = matsrc.size();
	out.clear();
	
	// TODO: Make sure src is a single channel.
	
	if ( matsrc.isContinuous() )
	{
		matsize.width *= matsize.height;
		matsize.height = 1;
	}
	
	for (int i=0; i<matsize.height; i++)
	{
		uchar *dat = matsrc.ptr(0);
		for (long j=0; j<matsize.width; ++j)
		{
			if ( dat[j] )
			{
				out.push_back(j);
				++Nfound;
			}
		}
	}
	return(Nfound);
}

void getNeighbors(int idx, int mrows, int mcols, vector<unsigned>& outidx) {
	outidx.clear();
	int y = floor(idx/mcols);
	int x = idx - y*mcols;
	
	if (y>0)
	{
		if (x>0)       outidx.push_back((y-1)*mcols + x-1); // left, up
        outidx.push_back((y-1)*mcols + x);   // up
		if (x<mcols-1) outidx.push_back((y-1)*mcols + x+1); // right, up
	}
	
	if (x>0)           outidx.push_back(y*mcols + (x-1));   // left
	if (x<mcols-1)     outidx.push_back(y*mcols + (x+1));   // right
	
	if (y<mrows-1)
	{
		if (x>0)       outidx.push_back((y+1)*mcols + x-1); // left, down
        outidx.push_back((y+1)*mcols + x);   // down
		if (x<mcols-1) outidx.push_back((y+1)*mcols + x+1); // right, down
	}
	
}


std::ostream& operator<<(std::ostream& strm, const PixelGrouping& pg)
{
	int n = pg.size();
	strm << "number of groups: " << n << endl;
	for (int j=0;j<n;++j)
	{
		int nj = pg[j].size();
		cout << "group " << j << ": " << nj << " pixels" << endl;
		strm << "     ";
		for (int k=0;k<nj;++k)
		{
			strm << "(" << pg[j][k].x << ", " << pg[j][k].y << ") "; 
		} // for each pixel in group
		strm << endl;
	} // for each group
	
	return strm;
}

// Get a list of the connected pixels in the binary image.
void group_pixels(const cv::InputArray &imb, int min_size, PixelGrouping& blobs)
{

	Mat matimb = imb.getMat();
    CV_Assert( matimb.type() == CV_8UC1 ); // input must be binary image

	blobs.clear();
	vector<unsigned>   pixListIdx; // index of nonzero pixels
	long Npix = find(matimb, pixListIdx);
	
	if (Npix==0) return;

	unordered_set<int> ungroupedIdx(pixListIdx.begin(),pixListIdx.end());  // ungrouped pixels
	vector<unsigned>  ineighbors;     // list of pixel neighbors
	int               ipix;           // pixel index
	
	while (!ungroupedIdx.empty()) // while ungrouped pixels
	{
		// start a new group
		//cout << endl << "starting a new group " << endl;
		//cout << "ungrouped pixels: " << ungroupedIdx.size() << endl;
		
		stack<unsigned>   pixReady;  // set of candidate pixels
		vector<Point2i>  groupPix;  // pixels in group
		
		// Pick an initial pixel, mark it as found and put it in the Ready list.
		pixReady.push(*ungroupedIdx.begin());
		ungroupedIdx.erase(ungroupedIdx.begin());
						
		while (!pixReady.empty())
		{
			//cout << "pixels ready: " << pixReady.size() << endl;
			ipix = pixReady.top();  // pick a pixel in the Ready list
			pixReady.pop();         // remove it from Ready
			//cout << "grouping pixel " << ipix << endl;
			
			// add it to the current group
			groupPix.push_back(idx2pnt(ipix, matimb.cols));
			
			// find all its neighbors
			getNeighbors(ipix,matimb.rows,matimb.cols,ineighbors);
			//cout << "neighbors (" << ineighbors.size() << "):" << endl;
			for (int j=0;j<ineighbors.size();j++)
			{
				//cout << ineighbors[j] << " ";
				if (matimb.reshape(0,1).at<unsigned char>(ineighbors[j])>0 )
				{
					// mark neighbor as found
					if ( ungroupedIdx.erase(ineighbors[j])>0 )
					{
						// add neighbor to the Ready list
						pixReady.push(ineighbors[j]);
					} // if ungrouped
				} // if neighbor valid
			} // for each neighbor
		} // pixels ready
		// save group
		if (groupPix.size() >= min_size)
		    blobs.push_back(groupPix);
		
	} // ungrouped pixels
	
	
}
