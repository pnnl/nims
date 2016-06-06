#include <iostream>
#include <opencv2/opencv.hpp>

#define NIMS_LOG_DEBUG std::cout

#include "pixelgroup.h"
#include "detections.h"


using namespace std;
using namespace cv;



void print_mat(const Mat& mat)
{
	Size sz = mat.size();
	cout << endl;
	cout << "---------------------------------" << endl;
	for (int y=0; y<sz.height; ++y)
	{
		for (int x=0; x<sz.width; ++x)
			cout << (int)(mat.at<uchar>(Point(x,y))>0);
		cout << endl;
	}
	cout << "---------------------------------" << endl;
}



// 2x3 horizontal rectangle
void make_horiz_rect(const Point2i& offset, vector<Point2i>& hrect)
{
hrect.clear();
for (int x=0;x<3;++x)
{
	hrect.push_back(Point2i(x,0)+offset);
	hrect.push_back(Point2i(x,1)+offset);
}
}

// 3x2 vertical rectangle
void make_vert_rect(const Point2i& offset, vector<Point2i>& vrect)
{
	vrect.clear();
for (int y=0;y<3;++y)
{
	vrect.push_back(Point2i(0,y)+offset);
	vrect.push_back(Point2i(1,y)+offset);
}
}

// 3x2 vertical rotated rectange
void make_rotated_rect(const Point2i& offset, vector<Point2i>& rvrect)
{
rvrect.clear();
rvrect.push_back(Point2i(2,0)+offset);
rvrect.push_back(Point2i(1,1)+offset);
rvrect.push_back(Point2i(2,1)+offset);
rvrect.push_back(Point2i(0,2)+offset);
rvrect.push_back(Point2i(1,2)+offset);
rvrect.push_back(Point2i(0,3)+offset);
}

int main (int argc, char * argv[]) 
{


long num_samples = 15;
long num_beams = 9;

// test image
Mat im(num_samples, num_beams, CV_32FC1, Scalar(0.5));

// binary mask 
Mat msk(num_samples, num_beams, CV_8UC1, Scalar(0));


// 2x3 horizontal rectangle
Point2i offset(2,3);
vector<Point2i> hrect;
make_horiz_rect(offset, hrect);
for (int p=0; p<hrect.size(); ++p)
	msk.at<uchar>(hrect[p]) = 255;
cout << "HORIZONTAL RECT:" << endl;
cout << endl << "offset: x = " << offset.x << ", y = " << offset.y << endl;
print_mat(msk);
cout << endl;

// 3x2 vertical rectangle
offset.x = 5; offset.y = 8;
vector<Point2i> vrect;
make_vert_rect(offset,vrect);
for (int p=0; p<hrect.size(); ++p)
	msk.at<uchar>(vrect[p]) = 255;
cout << "VERTICAL RECT:" << endl;
cout << endl << "offset: x = " << offset.x << ", y = " << offset.y << endl;
print_mat(msk);
cout << endl;

PixelGrouping objects;
int min_size = 1;
group_pixels(im, msk, min_size, objects);

Mat outmsk(num_samples, num_beams, CV_8UC1, Scalar(0));
for (int i=0; i<objects.size(); ++i)
	for (int j=0; j<objects[i].points.size(); ++j)
		outmsk.at<uchar>(objects[i].points[j]) = 255;

cout << endl;
cout << "PixelGrouping -- " << objects.size() << " objects" << endl;
print_mat(outmsk);
cout << endl;

/*
// 3x2 vertical rotated rectange
vector<Point2i> rvrect;
make_rotated_rect(offset, rvrect);

cout << "HORIZONTAL RECT:" << endl;
print_mat(msk);
cout << endl;

cout << endl;
cout << "VERTICAL RECT:" << endl;
print_mat(msk);
cout << endl;

cout << endl;
cout << endl;
cout << "ROTATED RECT:" << endl;
print_mat(msk);
cout << endl;


imb.at<float>(np) = 1.0;
imb.at<float>(fs) = 1.0;
imb.at<float>(ctr) = 1.0;

int min_size = 1;
PixelGrouping objects;
group_pixels(im, msk, min_size, objects);
int n_obj = objects.size();
cout << endl << " number of detected objects: " << n_obj << endl;
cout << endl;

imb.at<float>(np) = 0.0;
imb.at<float>(fs) = 0.0;
imb.at<float>(ctr) = 0.0;

cout << "Expecting ROTATED RECT" << endl;
for (int p=0; p<rvrect.size(); ++p)
{
	imb.at<float>(rvrect[p]) = 1.0;
}

group_pixels(Mat(imb>0.0), min_size, objects);
n_obj = objects.size();
cout << endl << " number of detected objects: " << n_obj << endl;
cout << endl;
*/
	return 0;
}